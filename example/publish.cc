extern "C" {
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
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
#include "rtp/rtp_packet.h"
#include "rtp/video_rtp_depacketizer_h264.h"
#include "session/webrtc_session.h"

using namespace muduo;
using namespace muduo::net;

class RTPChannel : public RTPChannelInterface {
 public:
  void OnRTP(const uint8_t* data, size_t len) {
    webrtc::RtpPacket rtp_packet;
    webrtc::VideoRtpDepacketizerH264 depacketizer;
    rtp_packet.Parse(data, len);
    depacketizer.Parse(&rtp_packet);
  }
};

int main(int argc, char* argv[]) {
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

  UdpServer rtc_server(&loop, muduo::net::InetAddress("0.0.0.0", port),
                       "rtc_server", 2);
  HttpServer http_server(&loop, muduo::net::InetAddress("0.0.0.0", 8000),
                         "http_server", TcpServer::kReusePort);

  rtc_server.SetPacketCallback(
      [&webrtc_session_factory](UdpServer* server, const uint8_t* buf,
                                size_t len,
                                const muduo::net::InetAddress& peer_addr,
                                muduo::Timestamp timestamp) {
        WebRTCSessionFactory::HandlePacket(&webrtc_session_factory, server, buf,
                                           len, peer_addr, timestamp);
      });

  std::map<std::string, std::shared_ptr<RTPChannel>> rtp_channels;

  http_server.setHttpCallback([&loop, &webrtc_session_factory, &rtp_channels,
                               port,
                               ip](const HttpRequest& req, HttpResponse* resp) {
    if (req.path() == "/webrtc") {
      resp->setStatusCode(HttpResponse::k200Ok);
      resp->setStatusMessage("OK");
      resp->setContentType("text/plain");
      resp->addHeader("Access-Control-Allow-Origin", "*");
      auto rtc_session = webrtc_session_factory.CreateWebRTCSession(ip, port);
      resp->setBody(rtc_session->webrtc_transport()->GetPublishSdp());
      auto rtp_channel = std::shared_ptr<RTPChannel>(new RTPChannel());
      rtp_channels[rtc_session->webrtc_transport()->GetidentifyID()] =
          rtp_channel;
      rtc_session->webrtc_transport()->SetRTPChannel(rtp_channel);
      std::cout << rtc_session->webrtc_transport()->GetPublishSdp()
                << std::endl;
    }
  });
  loop.runInLoop([&]() {
    rtc_server.Start();
    http_server.start();
  });
  loop.loop();
}
