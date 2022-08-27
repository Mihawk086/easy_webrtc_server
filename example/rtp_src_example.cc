#include <atomic>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>

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

typedef struct rtp_header {
  uint16_t csrccount : 4;
  uint16_t extension : 1;
  uint16_t padding : 1;
  uint16_t version : 2;
  uint16_t type : 7;
  uint16_t markerbit : 1;
  uint16_t seq_number;
  uint32_t timestamp;
  uint32_t ssrc;
  uint32_t csrc[16];
} rtp_header;

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

int main(int argc, char* argv[]) {
  if (fork() == 0) {
    execlp("ffmpeg", "ffmpeg", "-re", "-f", "lavfi", "-i", "testsrc2=size=640*480:rate=25",
           "-vcodec", "libx264", "-profile:v", "baseline", "-keyint_min", "60", "-g", "60",
           "-sc_threshold", "0", "-f", "rtp", "rtp://127.0.0.1:56000", NULL);
    return 0;
  }
  // ffmpeg -re -f lavfi -i testsrc2=size=640*480:rate=25 -vcodec libx264 -profile:v baseline
  // -keyint_min 60 -g 60 -sc_threshold 0 -f rtp rtp://127.0.0.1:56000

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

  UdpServer rtp_recieve_server(&loop, muduo::net::InetAddress("127.0.0.1", 56000), "rtp_server");
  UdpServer rtc_server(&loop, muduo::net::InetAddress("0.0.0.0", 10000), "rtc_server");
  HttpServer http_server(&loop, muduo::net::InetAddress("0.0.0.0", 8000), "http_server",
                         TcpServer::kReusePort);

  rtp_recieve_server.SetPacketCallback(
      [&webrtc_session_factory](UdpServer* server, const uint8_t* buf, size_t len,
                                const muduo::net::InetAddress& peer_addr,
                                muduo::Timestamp timestamp) {
        rtp_header* rtp_hdr = (rtp_header*)buf;
        rtp_hdr->ssrc = htonl(12345678);
        std::vector<std::shared_ptr<WebRTCSession>> all_sessions;
        std::shared_ptr<uint8_t> shared_buf(new uint8_t[len]);
        memcpy(shared_buf.get(), buf, len);
        webrtc_session_factory.GetAllReadyWebRTCSession(&all_sessions);
        for (const auto& session : all_sessions) {
          session->loop()->runInLoop([session, shared_buf, len]() {
            session->webrtc_transport()->EncryptAndSendRtpPacket(shared_buf.get(), len);
          });
        }
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
    }
    auto transport = std::shared_ptr<NetworkTransport>(new NetworkTransport(connection));
    session->SetNetworkTransport(transport);
    struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr.getSockAddr();
    session->webrtc_transport()->OnInputDataPacket(buf, len, remote_sockaddr_in);
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
    rtp_recieve_server.Start();
    rtc_server.Start();
    http_server.start();
  });
  loop.loop();
}
