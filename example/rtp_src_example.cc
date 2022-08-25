#include <cstdio>
#include <iostream>
#include <map>

#include "common/utils.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"
#include "net/udp_socket.h"
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

class UdpSession : public TransportInterface {
 public:
  UdpSession(EventLoop* loop, const std::string& ip, uint16_t port)
      : loop_(loop), socket_(loop, ip, port) {}

  bool SendPacket(const uint8_t* data, size_t len, const struct sockaddr_in& remote_address) {}

 public:
  UdpSocket socket_;

 private:
  EventLoop* loop_;
};

class UdpServer {
 public:
  UdpServer(EventLoop* loop, const std::string& ip, uint16_t port)
      : socket_(loop, ip, port), loop_(loop) {
    socket_.SetReadCallback(
        [this](const uint8_t* buf, size_t len, const struct sockaddr_in& remote_address) {
          this->ReadPacket(buf, len, remote_address);
        });
  }
  void Start() { socket_.Start(); }
  void ReadPacket(const uint8_t* buf, size_t len, const struct sockaddr_in& remote_address) {
    if (RTC::StunPacket::IsStun(buf, len)) {
      RTC::StunPacket* packet = RTC::StunPacket::Parse(buf, len);
      if (packet == nullptr) {
        std::cout << "parse stun error" << std::endl;
        return;
      }

      std::shared_ptr<UdpSession> udp_session;
      std::string key = ::inet_ntoa(remote_address.sin_addr) + std::string(":") +
                        std::to_string(remote_address.sin_port);
      if (auto it = udp_sessions_.find(key); it != udp_sessions_.end()) {
        udp_session = it->second;
      } else {
        auto session =
            std::shared_ptr<UdpSession>(new UdpSession(loop_, socket_.GetIP(), socket_.GetPort()));
        udp_sessions_.insert(std::make_pair(key, session));
        udp_session = session;
      }

      auto vec = Split(packet->GetUsername(), ":");
      std::string use_name = vec[0];
      if (auto it = g_rtc_sessions.find(use_name); it != g_rtc_sessions.end()) {
        udp_session->socket_.BindRemote(remote_address);
        udp_session->socket_.SetReadCallback(
            [rtc_session = it->second](const uint8_t* data, size_t len,
                                       const struct sockaddr_in& remote_address) {
              rtc_session->OnInputDataPacket(data, len, remote_address);
            });
        it->second->SetTransport(udp_session.get());
        udp_session->socket_.Start();
        it->second->OnInputDataPacket(buf, len, remote_address);
      }
    }
  }

 private:
  std::map<std::string, std::shared_ptr<UdpSession>> udp_sessions_;
  UdpSocket socket_;
  EventLoop* loop_;
};

int main(int argc, char* argv[]) {
  // if (fork() == 0) {
  //   execlp("ffmpeg", "ffmpeg", "-f", "lavfi", "-i", "testsrc=duration=120:size=1280x720:rate=30",
  //          "-f", "rtp", "rtp://127.0.0.1:56000", NULL);
  //   return 0;
  // }

  Utils::Crypto::ClassInit();
  RTC::DtlsTransport::ClassInit();
  RTC::DepLibSRTP::ClassInit();
  RTC::SrtpSession::ClassInit();
  EventLoop loop;

  UdpSocket rtp_recieve_udp_socket(&loop, "127.0.0.1", 56000);
  rtp_recieve_udp_socket.SetReadCallback(
      [](const uint8_t* buf, size_t len, const struct sockaddr_in& remote_addr) {
        rtp_header* rtp_hdr = (rtp_header*)buf;
        rtp_hdr->ssrc = htonl(12345678);
        for (auto& session : g_rtc_sessions) {
          session.second->EncryptAndSendRtpPacket(buf, len);
        }
      });
  rtp_recieve_udp_socket.Start();

  UdpServer udp_server(&loop, "0.0.0.0", 10000);
  udp_server.Start();

  int threads_num = 0;
  HttpServer http_server(&loop, InetAddress(8088), "webrtc", TcpServer::kReusePort);
  http_server.setHttpCallback([&loop](const HttpRequest& req, HttpResponse* resp) {
    if (req.path() == "/webrtc") {
      resp->setStatusCode(HttpResponse::k200Ok);
      resp->setStatusMessage("OK");
      resp->setContentType("text/plain");
      resp->addHeader("Access-Control-Allow-Origin", "*");
      std::shared_ptr<WebRtcTransport> rtc_session(new WebRtcTransport(&loop, "127.0.0.1", 10000));
      g_rtc_sessions.insert(std::make_pair(rtc_session->GetidentifyID(), rtc_session));
      resp->setBody(rtc_session->GetLocalSdp());
      std::cout << rtc_session->GetLocalSdp() << std::endl;
    }
  });

  http_server.setThreadNum(threads_num);
  http_server.start();
  loop.loop();
}
