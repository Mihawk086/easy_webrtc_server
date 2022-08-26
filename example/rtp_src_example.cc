#include <cstdio>
#include <iostream>
#include <map>

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

static std::map<std::string, std::shared_ptr<WebRtcTransport>> g_rtc_sessions;

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

std::vector<std::string> Split(const string& s, const char* delim) {
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

class Transport : public TransportInterface {
 public:
  Transport(const std::shared_ptr<UdpConnection>& con) : con_(con) {}
  bool SendPacket(const uint8_t* data, size_t len, const struct sockaddr_in& remote_address) {
    con_->Send(data, len);
  }

 private:
  std::shared_ptr<UdpConnection> con_;
};

int main(int argc, char* argv[]) {
  if (fork() == 0) {
    execlp("ffmpeg", "ffmpeg", "-re", "-f", "lavfi", "-i", "testsrc2=size=640*480:rate=25",
           "-vcodec", "libx264", "-profile:v", "baseline", "-f", "rtp", "rtp://127.0.0.1:56000",
           NULL);
    return 0;
  }
  // ffmpeg -re -f lavfi -i testsrc2=size=640*480:rate=25 -vcodec libx264 -profile:v baseline -f
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

  UdpServer rtp_recieve_server(&loop, muduo::net::InetAddress("127.0.0.1", 56000), "rtp_server");
  UdpServer rtc_server(&loop, muduo::net::InetAddress("0.0.0.0", 10000), "rtc_server");
  HttpServer http_server(&loop, muduo::net::InetAddress("0.0.0.0", 8000), "webrtc", TcpServer::kReusePort);

  rtp_recieve_server.SetPacketCallback([](UdpServer* server, const uint8_t* buf, size_t len,
                                          const muduo::net::InetAddress& peer_addr,
                                          muduo::Timestamp timestamp) {
    rtp_header* rtp_hdr = (rtp_header*)buf;
    rtp_hdr->ssrc = htonl(12345678);
    for (auto& session : g_rtc_sessions) {
      session.second->EncryptAndSendRtpPacket(buf, len);
    }
  });
  rtc_server.SetPacketCallback([](UdpServer* server, const uint8_t* buf, size_t len,
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
    auto it = g_rtc_sessions.find(use_name);
    if (it == g_rtc_sessions.end()) {
      std::cout << "no rtc session" << std::endl;
      return;
    }
    auto connection = server->GetOrCreatConnection(peer_addr);
    if (!connection) {
      std::cout << "get connection error" << std::endl;
    }
    connection->SetPacketCallback(
        [rtc_session = it->second](const std::shared_ptr<UdpConnection>& con, const uint8_t* buf,
                                   size_t len, const muduo::net::InetAddress& peer_addr,
                                   muduo::Timestamp) {
          struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr.getSockAddr();
          rtc_session->OnInputDataPacket(buf, len, remote_sockaddr_in);
        });
    connection->Start();
    struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr.getSockAddr();
    it->second->OnInputDataPacket(buf, len, remote_sockaddr_in);
    auto transport = std::shared_ptr<Transport>(new Transport(connection));
    it->second->SetTransport(transport);
  });

  http_server.setHttpCallback([&loop, port, ip](const HttpRequest& req, HttpResponse* resp) {
    if (req.path() == "/webrtc") {
      resp->setStatusCode(HttpResponse::k200Ok);
      resp->setStatusMessage("OK");
      resp->setContentType("text/plain");
      resp->addHeader("Access-Control-Allow-Origin", "*");
      std::shared_ptr<WebRtcTransport> rtc_session(new WebRtcTransport(&loop, ip, port));
      g_rtc_sessions.insert(std::make_pair(rtc_session->GetidentifyID(), rtc_session));
      resp->setBody(rtc_session->GetLocalSdp());
      std::cout << rtc_session->GetLocalSdp() << std::endl;
    }
  });
  loop.runInLoop([&]() {
    rtp_recieve_server.Start();
    rtc_server.Start();
    http_server.start();
  });
  loop.loop();
}
