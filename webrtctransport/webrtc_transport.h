#pragma once

#include <memory>
#include <string>

#include "dtls_transport.h"
#include "ice_server.h"
#include "rtp_maker.h"
#include "stun_packet.h"
#include "udp_socket.h"

namespace muduo {
namespace net {
class EventLoop;
}
}  // namespace muduo

class WebRtcTransport : public std::enable_shared_from_this<WebRtcTransport> {
 public:
  WebRtcTransport(muduo::net::EventLoop* loop, std::string ip);
  ~WebRtcTransport();

  void Start();
  std::string GetLocalSdp();
  void OnIceServerCompleted();
  void OnDtlsCompleted(std::string client_key, std::string server_key);

  void OnInputDataPacket(char* buf, int len, struct sockaddr_in* remote_address);
  void WritePacket(char* buf, int len, struct sockaddr_in* remote_address);
  void WritePacket(char* buf, int len);
  void WritRtpPacket(char* buf, int len);
  void WriteH264Frame(char* buf, int len, uint32_t timestamp);

 private:
  IceServer::Ptr ice_server_;
  DtlsTransport::Ptr dtls_transport_;
  std::shared_ptr<erizo::SrtpChannel> srtp_channel_;

  UdpSocket::Ptr udp_socket_;
  muduo::net::EventLoop* loop_;

  char protect_buf_[65536];
  bool is_ready_;
  std::string ip_;
  struct sockaddr_in remote_socket_address_;
  RtpMaker rtp_maker_;
};
