#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "common/utils.h"
#include "dtls_transport.h"
#include "ice_server.h"
#include "srtp_session.h"
#include "stun_packet.h"
#include "transport_interface.h"

class WebRtcTransport : public std::enable_shared_from_this<WebRtcTransport>,
                        RTC::DtlsTransport::Listener,
                        RTC::IceServer::Listener {
 public:
  WebRtcTransport(std::string ip, uint16_t port);
  ~WebRtcTransport();

  void Start();
  std::string GetLocalSdp();
  std::string GetPublishSdp();
  std::string GetidentifyID() { return ice_server_->GetUsernameFragment(); }
  void SetNetworkTransport(std::shared_ptr<TransportInterface> transport) {
    transport_ = transport;
  }
  void SetRTPChannel(std::shared_ptr<RTPChannelInterface> channel) { rtp_channel_ = channel; }
  void OnIceServerCompleted();
  void OnDtlsCompleted(std::string client_key, std::string server_key,
                       RTC::SrtpSession::CryptoSuite srtp_crypto_suite);

  void OnInputDataPacket(const uint8_t* buf, size_t len, const struct sockaddr_in& remote_address);
  bool SendPacket(const uint8_t* data, size_t len, const struct sockaddr_in& remote_address);
  void EncryptAndSendRtpPacket(const uint8_t* data, size_t len);
  std::string GetSHA256Fingerprint();

 private:
  void OnIceServerSendStunPacket(const RTC::IceServer* iceServer, const RTC::StunPacket* packet,
                                 RTC::TransportTuple* tuple) override;
  void OnIceServerSelectedTuple(const RTC::IceServer* iceServer,
                                RTC::TransportTuple* tuple) override;
  void OnIceServerConnected(const RTC::IceServer* iceServer) override;
  void OnIceServerCompleted(const RTC::IceServer* iceServer) override;
  void OnIceServerDisconnected(const RTC::IceServer* iceServer) override;

 private:
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
  std::shared_ptr<TransportInterface> transport_;
  std::shared_ptr<RTPChannelInterface> rtp_channel_;

  std::atomic<bool> is_ready_;
  std::string ip_;
  uint16_t port_;
  struct sockaddr_in remote_socket_address_;
};
