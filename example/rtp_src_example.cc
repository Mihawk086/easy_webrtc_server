extern "C" {
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}
#include <signal.h>
#include <sys/prctl.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

#include "common/utils.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"
#include "net/udp_connection.h"
#include "net/udp_server.h"
#include "rtc/dtls_transport.h"
#include "rtc/srtp_session.h"
#include "rtc/stun_packet.h"
#include "rtc/transport_interface.h"
#include "rtc/webrtc_transport.h"

using namespace muduo;
using namespace muduo::net;

static std::vector<std::string> Split(const string& s, const char* delim) {
  std::vector<std::string> ret;
  size_t last = 0;
  auto index = s.find(delim, last);
  while (index != string::npos) {
    if (index - last >= 0) {
      ret.push_back(s.substr(last, index - last));
    }
    last = index + strlen(delim);
    index = s.find(delim, last);
  }
  if (!s.size() || s.size() - last >= 0) {
    ret.push_back(s.substr(last));
  }
  return ret;
}

class NetworkTransport : public TransportInterface {
 public:
  NetworkTransport(const std::shared_ptr<UdpConnection>& con) : connection_(con) {}
  ~NetworkTransport() {}
  bool SendPacket(const uint8_t* data, size_t len, const struct sockaddr_in& remote_address) {
    connection_->Send(data, len);
  }
  std::shared_ptr<UdpConnection> connection() { return connection_; }

 private:
  std::shared_ptr<UdpConnection> connection_;
};

class WebRTCSession {
 public:
  WebRTCSession(const std::shared_ptr<WebRtcTransport>& webrtc_transport)
      : webrtc_transport_(webrtc_transport), is_ready_(false) {}
  ~WebRTCSession() {}
  void SetNetworkTransport(const std::shared_ptr<NetworkTransport>& transport) {
    network_transport_ = transport;
    webrtc_transport_->SetNetworkTransport(transport);
    auto connection = transport->connection();
    connection->SetPacketCallback(
        [this](const std::shared_ptr<UdpConnection>& con, const uint8_t* buf, size_t len,
               const muduo::net::InetAddress& peer_addr, muduo::Timestamp) {
          struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr.getSockAddr();
          webrtc_transport_->OnInputDataPacket(buf, len, remote_sockaddr_in);
        });
    connection->Start();
    SetLoop(connection->loop());
    is_ready_.store(true);
  }
  void SetLoop(muduo::net::EventLoop* loop) { loop_ = loop; }
  muduo::net::EventLoop* loop() { return loop_; }
  std::atomic<bool>& is_ready() { return is_ready_; }
  std::shared_ptr<WebRtcTransport> webrtc_transport() { return webrtc_transport_; }

 private:
  std::shared_ptr<WebRtcTransport> webrtc_transport_;
  std::shared_ptr<NetworkTransport> network_transport_;
  muduo::net::EventLoop* loop_;
  std::atomic<bool> is_ready_;
};

class WebRTCSessionFactory {
 public:
  WebRTCSessionFactory() {}
  ~WebRTCSessionFactory() {}
  std::shared_ptr<WebRTCSession> CreateWebRTCSession(const std::string& ip, uint16_t port) {
    std::shared_ptr<WebRtcTransport> rtc_transport(new WebRtcTransport(ip, port));
    std::shared_ptr<WebRTCSession> rtc_session(new WebRTCSession(rtc_transport));
    {
      std::lock_guard<std::mutex> guard(mutex_);
      rtc_sessions.insert(std::make_pair(rtc_transport->GetidentifyID(), rtc_session));
    }
    return rtc_session;
  }
  std::shared_ptr<WebRTCSession> GetWebRTCSession(const std::string& key) {
    std::shared_ptr<WebRTCSession> ptr = nullptr;
    {
      std::lock_guard<std::mutex> guard(mutex_);
      auto it = rtc_sessions.find(key);
      if (it == rtc_sessions.end()) {
        ptr = nullptr;
        return ptr;
      }
      ptr = it->second;
    }
    return ptr;
  }
  void GetAllReadyWebRTCSession(std::vector<std::shared_ptr<WebRTCSession>>* sessions) {
    {
      std::lock_guard<std::mutex> guard(mutex_);
      for (const auto& session : rtc_sessions) {
        if (session.second->is_ready().load()) {
          sessions->push_back(session.second);
        }
      }
    }
    return;
  }

 private:
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<WebRTCSession>> rtc_sessions;
};

static int WriteRtpCallback(void* opaque, uint8_t* buf, int buf_size) {
  WebRTCSessionFactory* webrtc_session_factory = (WebRTCSessionFactory*)opaque;
  std::vector<std::shared_ptr<WebRTCSession>> all_sessions;
  std::shared_ptr<uint8_t> shared_buf(new uint8_t[buf_size]);
  memcpy(shared_buf.get(), buf, buf_size);
  webrtc_session_factory->GetAllReadyWebRTCSession(&all_sessions);
  for (const auto& session : all_sessions) {
    session->loop()->runInLoop([session, shared_buf, buf_size]() {
      session->webrtc_transport()->EncryptAndSendRtpPacket(shared_buf.get(), buf_size);
    });
  }
  return buf_size;
}

int H2642Rtp(const char* in_filename, void* opaque) {
  const AVOutputFormat* ofmt = NULL;
  AVIOContext* avio_ctx = NULL;
  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVPacket* pkt = NULL;
  int ret, i;
  int in_stream_index = 0, out_stream_index = 0;
  int stream_mapping_size = 0;
  int64_t pts = 0;
  uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
  size_t buffer_size, avio_ctx_buffer_size = 4096;

  const AVBitStreamFilter* abs_filter = NULL;
  AVBSFContext* abs_ctx = NULL;
  abs_filter = av_bsf_get_by_name("h264_mp4toannexb");
  av_bsf_alloc(abs_filter, &abs_ctx);

  avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
  if (!avio_ctx_buffer) {
    ret = AVERROR(ENOMEM);
    goto end;
  }
  avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 1, opaque, NULL,
                                WriteRtpCallback, NULL);
  if (!avio_ctx) {
    ret = AVERROR(ENOMEM);
    goto end;
  }
  avio_ctx->max_packet_size = 1400;

  pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Could not allocate AVPacket\n");
    return 1;
  }

  if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
    fprintf(stderr, "Could not open input file '%s'", in_filename);
    goto end;
  }

  if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream information");
    goto end;
  }

  av_dump_format(ifmt_ctx, 0, in_filename, 0);

  avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtp", NULL);
  if (!ofmt_ctx) {
    fprintf(stderr, "Could not create output context\n");
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  ofmt = ofmt_ctx->oformat;
  av_opt_set_int(ofmt_ctx->priv_data, "ssrc", 12345678, 0);

  for (i = 0; i < ifmt_ctx->nb_streams; i++) {
    AVStream* out_stream;
    AVStream* in_stream = ifmt_ctx->streams[i];
    AVCodecParameters* in_codecpar = in_stream->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }
    out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream) {
      fprintf(stderr, "Failed allocating output stream\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }
    ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    if (ret < 0) {
      fprintf(stderr, "Failed to copy codec parameters\n");
      goto end;
    }

    avcodec_parameters_copy(abs_ctx->par_in, in_codecpar);
    av_bsf_init(abs_ctx);

    out_stream->codecpar->codec_tag = 0;
    in_stream_index = i;
    out_stream_index = out_stream->index;
    break;
  }

  av_dump_format(ofmt_ctx, 0, NULL, 1);

  ofmt_ctx->pb = avio_ctx;

  ret = avformat_write_header(ofmt_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file\n");
    goto end;
  }

  while (1) {
    AVStream *in_stream, *out_stream;

    ret = av_read_frame(ifmt_ctx, pkt);
    if (ret < 0) {
      for (size_t i = 0; i < ifmt_ctx->nb_streams; i++) {
        av_seek_frame(ifmt_ctx, i, 0, AVSEEK_FLAG_BYTE);
      }
      continue;
    }
    if (pkt->stream_index != in_stream_index) {
      continue;
    }

    in_stream = ifmt_ctx->streams[in_stream_index];
    out_stream = ofmt_ctx->streams[0];
    // log_packet(ifmt_ctx, pkt, "in");
    pts += 40;
    pkt->pts = pts;
    pkt->dts = pts;

    av_bsf_send_packet(abs_ctx, pkt);
    av_bsf_receive_packet(abs_ctx, pkt);

    /* copy packet */
    av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
    pkt->pos = -1;
    // log_packet(ofmt_ctx, pkt, "out");
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    ret = av_interleaved_write_frame(ofmt_ctx, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0) {
      fprintf(stderr, "Error muxing packet\n");
      break;
    }
  }

  av_write_trailer(ofmt_ctx);
end:
  av_packet_free(&pkt);

  avformat_close_input(&ifmt_ctx);

  /* close output */
  if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);

  av_bsf_free(&abs_ctx);

  if (ret < 0 && ret != AVERROR_EOF) {
    fprintf(stderr, "Error occurred: %d\n", ret);
    return 1;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  // ffmpeg -re -f lavfi -i testsrc2=size=640*480:rate=25 -vcodec libx264 -profile:v baseline
  // -keyint_min 60 -g 60 -sc_threshold 0 -f rtp rtp://127.0.0.1:56000
  // ffmpeg -re -stream_loop -1 -i  test.mp4 -vcodec copy -bsf:v h264_mp4toannexb -ssrc 12345678 -f
  // rtp rtp://127.0.0.1:56000

  std::string ip("127.0.0.1");
  uint16_t port = 10000;
  if (argc == 3) {
    ip = argv[1];
    port = atoi(argv[2]);
  }

  Utils::Crypto::ClassInit();
  RTC::DtlsTransport::ClassInit();
  RTC::DepLibSRTP::ClassInit();
  RTC::SrtpSession::ClassInit();
  EventLoop loop;
  WebRTCSessionFactory webrtc_session_factory;

  std::thread flv_2_rtp_thread(
      [&webrtc_session_factory]() { H2642Rtp("./test.h264", &webrtc_session_factory); });

  UdpServer rtc_server(&loop, muduo::net::InetAddress("0.0.0.0", port), "rtc_server", 2);
  HttpServer http_server(&loop, muduo::net::InetAddress("0.0.0.0", 8000), "http_server",
                         TcpServer::kReusePort);
  UdpServer rtp_server(&loop, muduo::net::InetAddress("127.0.0.1", 56000), "rtc_server", 0);

  rtp_server.SetPacketCallback([&webrtc_session_factory](UdpServer* server, const uint8_t* buf,
                                                         size_t buf_size,
                                                         const muduo::net::InetAddress& peer_addr,
                                                         muduo::Timestamp timestamp) {
    std::vector<std::shared_ptr<WebRTCSession>> all_sessions;
    std::shared_ptr<uint8_t> shared_buf(new uint8_t[buf_size]);
    memcpy(shared_buf.get(), buf, buf_size);
    webrtc_session_factory.GetAllReadyWebRTCSession(&all_sessions);
    for (const auto& session : all_sessions) {
      session->loop()->runInLoop([session, shared_buf, buf_size]() {
        session->webrtc_transport()->EncryptAndSendRtpPacket(shared_buf.get(), buf_size);
      });
    }
    return;
  });

  rtc_server.SetPacketCallback([&webrtc_session_factory](UdpServer* server, const uint8_t* buf,
                                                         size_t len,
                                                         const muduo::net::InetAddress& peer_addr,
                                                         muduo::Timestamp timestamp) {
    if (!RTC::StunPacket::IsStun(buf, len)) {
      std::cout << "receive not stun packet" << std::endl;
      return;
    }
    RTC::StunPacket* packet = RTC::StunPacket::Parse(buf, len);
    if (packet == nullptr) {
      std::cout << "parse stun error" << std::endl;
      return;
    }
    auto vec = Split(packet->GetUsername(), ":");
    std::string use_name = vec[0];
    auto session = webrtc_session_factory.GetWebRTCSession(use_name);
    if (!session) {
      std::cout << "no rtc session" << std::endl;
      return;
    }
    auto connection = server->GetOrCreatConnection(peer_addr);
    if (!connection) {
      std::cout << "get connection error" << std::endl;
      return;
    }
    auto transport = std::shared_ptr<NetworkTransport>(new NetworkTransport(connection));
    session->SetNetworkTransport(transport);
    struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr.getSockAddr();
    std::shared_ptr<uint8_t> shared_buf(new uint8_t[len]);
    memcpy(shared_buf.get(), buf, len);
    session->loop()->runInLoop([session, shared_buf, len, remote_sockaddr_in]() {
      session->webrtc_transport()->OnInputDataPacket(shared_buf.get(), len, remote_sockaddr_in);
    });
  });

  http_server.setHttpCallback(
      [&loop, &webrtc_session_factory, port, ip](const HttpRequest& req, HttpResponse* resp) {
        if (req.path() == "/webrtc") {
          resp->setStatusCode(HttpResponse::k200Ok);
          resp->setStatusMessage("OK");
          resp->setContentType("text/plain");
          resp->addHeader("Access-Control-Allow-Origin", "*");
          auto rtc_session = webrtc_session_factory.CreateWebRTCSession(ip, port);
          resp->setBody(rtc_session->webrtc_transport()->GetLocalSdp());
          std::cout << rtc_session->webrtc_transport()->GetLocalSdp() << std::endl;
        }
      });
  loop.runInLoop([&]() {
    rtp_server.Start();
    rtc_server.Start();
    http_server.start();
  });
  loop.loop();
}
