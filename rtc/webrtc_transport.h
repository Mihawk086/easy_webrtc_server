#pragma once

#include <memory>
#include <string>

#include "common/utils.h"
#include "dtls_transport.h"
#include "ice_server.h"
#include "srtp_session.h"
#include "stun_packet.h"
#include "udp_socket.h"

namespace muduo {
namespace net {
class EventLoop;
}
}  // namespace muduo

class WebRtcTransport : public std::enable_shared_from_this<WebRtcTransport>,
                        RTC::DtlsTransport::Listener,
                        RTC::IceServer::Listener {
 public:
  WebRtcTransport(muduo::net::EventLoop* loop, std::string ip);
  ~WebRtcTransport();

  void Start();
  std::string GetLocalSdp();
  void OnIceServerCompleted();
  void OnDtlsCompleted(std::string client_key, std::string server_key,
                       RTC::SrtpSession::CryptoSuite srtp_crypto_suite);

  void OnInputDataPacket(char* buf, int len, struct sockaddr_in* remote_address);
  void SendUdpPacket(char* buf, int len, struct sockaddr_in* remote_address);
  void SendUdpPacket(char* buf, int len);
  void EncryptAndSendRtpPacket(char* buf, int len);
  std::string GetSHA256Fingerprint();

 public:
  void OnIceServerSendStunPacket(const RTC::IceServer* iceServer, const RTC::StunPacket* packet,
                                 RTC::TransportTuple* tuple) override;
  void OnIceServerSelectedTuple(const RTC::IceServer* iceServer,
                                RTC::TransportTuple* tuple) override;
  void OnIceServerConnected(const RTC::IceServer* iceServer) override;
  void OnIceServerCompleted(const RTC::IceServer* iceServer) override;
  void OnIceServerDisconnected(const RTC::IceServer* iceServer) override;

 public:
  void OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport) override;
  void OnDtlsTransportConnected(const RTC::DtlsTransport* dtlsTransport,
                                RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                uint8_t* srtpLocalKey, size_t srtpLocalKeyLen,
                                uint8_t* srtpRemoteKey, size_t srtpRemoteKeyLen,
                                std::string& remoteCert) override;
  void OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport) override;
  void OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport) override;
  void OnDtlsTransportSendData(const RTC::DtlsTransport* dtlsTransport, const uint8_t* data,
                               size_t len) override;
  void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport* dtlsTransport,
                                              const uint8_t* data, size_t len) override;

 private:
  std::shared_ptr<RTC::IceServer> ice_server_;
  std::shared_ptr<RTC::DtlsTransport> dtls_transport_;
  std::shared_ptr<RTC::SrtpSession> srtp_session_;
  UdpSocket::Ptr udp_socket_;
  muduo::net::EventLoop* loop_;

  char protect_buf_[65536];
  bool is_ready_;
  std::string ip_;
  struct sockaddr_in remote_socket_address_;
};
