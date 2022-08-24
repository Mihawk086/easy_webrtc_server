#include <cstdio>
#include <iostream>
#include <map>

#include "common/utils.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"
#include "rtc/dtls_transport.h"
#include "rtc/srtp_session.h"
#include "rtc/udp_socket.h"
#include "rtc/webrtc_transport.h"

using namespace muduo;
using namespace muduo::net;

static std::map<int, std::shared_ptr<WebRtcTransport>> s_rtc_sessions;
static int s_sessionid = 1;

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

int main(int argc, char* argv[]) {
  if (fork() == 0) {
    execlp("ffmpeg", "ffmpeg", "-f", "lavfi", "-i", "testsrc=duration=120:size=1280x720:rate=30",
           "-f", "rtp", "rtp://127.0.0.1:56000", NULL);
    return 0;
  }
  std::string ip = "0.0.0.0";
  if (argc > 1) {
    ip = argv[1];
  }
  Utils::Crypto::ClassInit();
  RTC::DtlsTransport::ClassInit();
  RTC::DepLibSRTP::ClassInit();
  RTC::SrtpSession::ClassInit();

  int threads_num = 0;
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8088), "webrtc", TcpServer::kReusePort);
  UdpSocket udp_socket(&loop, "127.0.0.1", 56000);
  udp_socket.SetReadCallback([](char* buf, int len, struct sockaddr_in* remoteAddr) {
    rtp_header* rtp_hdr = (rtp_header*)buf;
    rtp_hdr->ssrc = htonl(12345678);
    for (auto& session : s_rtc_sessions) {
      session.second->EncryptAndSendRtpPacket(buf, len);
    }
  });
  udp_socket.Start();
  server.setHttpCallback([&loop](const HttpRequest& req, HttpResponse* resp) {
    if (req.path() == "/webrtc") {
      resp->setStatusCode(HttpResponse::k200Ok);
      resp->setStatusMessage("OK");
      resp->setContentType("text/plain");
      resp->addHeader("Access-Control-Allow-Origin", "*");
      std::shared_ptr<WebRtcTransport> session(new WebRtcTransport(&loop, "127.0.0.1"));
      s_rtc_sessions.insert(std::make_pair(s_sessionid, session));
      s_sessionid++;
      session->Start();
      resp->setBody(session->GetLocalSdp());
      std::cout << session->GetLocalSdp() << std::endl;
    }
  });
  server.setThreadNum(threads_num);
  server.start();
  loop.loop();
}
