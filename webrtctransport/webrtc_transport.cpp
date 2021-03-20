#include "webrtc_transport.h"

#include <iostream>

#include "muduo/net/EventLoop.h"
#include "utils.h"

using namespace erizo;

WebRtcTransport::WebRtcTransport(muduo::net::EventLoop* loop, std::string ip)
    : loop_(loop_), is_ready_(false) {
  ip_ = ip;
  dtls_transport_.reset(new DtlsTransport(true));
  udp_socket_.reset(new UdpSocket(ip_, loop));
  srtp_channel_.reset(new SrtpChannel());
  ice_server_.reset(
      new IceServer(Utils::Crypto::GetRandomString(4), Utils::Crypto::GetRandomString(24)));
  udp_socket_->SetReadCallback([this](char* buf, int len, struct sockaddr_in* remote_address) {
    this->OnInputDataPacket(buf, len, remote_address);
  });
  ice_server_->SetIceServerCompletedCB([this]() { this->OnIceServerCompleted(); });
  ice_server_->SetSendCB([this](char* buf, int len, struct sockaddr_in* remote_address) {
    this->WritePacket(buf, len, remote_address);
  });
  dtls_transport_->SetHandshakeCompletedCB([this](std::string client_key, std::string server_key) {
    this->OnDtlsCompleted(client_key, server_key);
  });
  dtls_transport_->SetOutPutCB([this](char* buf, int len) { this->WritePacket(buf, len); });
  rtp_maker_.SetRtpCallBack([this](char* buf, int len) { this->WritRtpPacket(buf, len); });
}

WebRtcTransport::~WebRtcTransport() {}

void WebRtcTransport::Start() {
  if (udp_socket_) {
    udp_socket_->Start();
  }
}

std::string WebRtcTransport::GetLocalSdp() {
  char szsdp[1024 * 10] = {0};
  int nssrc = 12345678;
  uint16_t nport = 0;
  if (udp_socket_) {
    nport = udp_socket_->GetPort();
  }
  sprintf(szsdp,
          "v=0\r\no=- 1495799811084970 1495799811084970 IN IP4 %s\r\ns=Streaming Test\r\nt=0 0\r\n"
          "a=group:BUNDLE video\r\na=msid-semantic: WMS janus\r\n"
          "m=video 1 RTP/SAVPF 96\r\nc=IN IP4 %s\r\na=mid:video\r\na=sendonly\r\na=rtcp-mux\r\n"
          "a=ice-ufrag:%s\r\n"
          "a=ice-pwd:%s\r\na=ice-options:trickle\r\na=fingerprint:sha-256 "
          "%s\r\na=setup:actpass\r\na=connection:new\r\n"
          "a=rtpmap:96 H264/90000\r\n"
          "a=ssrc:%d cname:janusvideo\r\n"
          "a=ssrc:%d msid:janus janusv0\r\n"
          "a=ssrc:%d mslabel:janus\r\n"
          "a=ssrc:%d label:janusv0\r\n"
          "a=candidate:%s 1 udp %u %s %d typ %s\r\n",
          ip_.c_str(), ip_.c_str(), ice_server_->GetUsernameFragment().c_str(),
          ice_server_->GetPassword().c_str(), dtls_transport_->GetMyFingerprint().c_str(), nssrc,
          nssrc, nssrc, nssrc, "4", 12345678, ip_.c_str(), nport, "host");
  return std::string(szsdp);
}

void WebRtcTransport::OnIceServerCompleted() {
  remote_socket_address_ = *ice_server_->GetSelectAddr();
  dtls_transport_->Start();
}

void WebRtcTransport::OnDtlsCompleted(std::string client_key, std::string server_key) {
  srtp_channel_->setRtpParams(client_key, server_key);
  is_ready_ = true;
}

void WebRtcTransport::OnInputDataPacket(char* buf, int len, struct sockaddr_in* remote_address) {
  if (RTC::StunPacket::IsStun((const uint8_t*)buf, len)) {
    RTC::StunPacket* packet = RTC::StunPacket::Parse((const uint8_t*)buf, len);
    if (packet == nullptr) {
      std::cout << "parse stun error" << std::endl;
      return;
    }
    ice_server_->ProcessStunPacket(packet, remote_address);
  }
  if (DtlsTransport::IsDtlsPacket(buf, len)) {
    dtls_transport_->InputData(buf, len);
  }
}

void WebRtcTransport::WritePacket(char* buf, int len, struct sockaddr_in* remote_address) {
  if (udp_socket_) {
    udp_socket_->Send(buf, len, *remote_address);
  }
}

void WebRtcTransport::WritePacket(char* buf, int len) {
  if (udp_socket_) {
    udp_socket_->Send(buf, len, remote_socket_address_);
  }
}

void WebRtcTransport::WritRtpPacket(char* buf, int len) {
  if (is_ready_) {
    memcpy(protect_buf_, buf, len);
    int length = len;
    srtp_channel_->protectRtp(protect_buf_, &length);
    if (udp_socket_) {
      udp_socket_->Send(protect_buf_, length, remote_socket_address_);
    }
  }
}

void WebRtcTransport::WriteH264Frame(char* buf, int len, uint32_t timestamp) {
  if (is_ready_) {
    rtp_maker_.InputH264Frame(buf, len, timestamp);
  }
}
