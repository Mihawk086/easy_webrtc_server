//
// Created by xueyuegui on 19-12-7.
//

#ifndef MYWEBRTC_WEBRTCTRANSPORT_H
#define MYWEBRTC_WEBRTCTRANSPORT_H

#include <memory>
#include <string>

#include "IceServer.h"
#include "MuduoUdpSocket.h"
#include "MyDtlsTransport.h"
#include "RtpMaker.h"
#include "StunPacket.hpp"

namespace muduo {
namespace net {
class EventLoop;
}
}  // namespace muduo

class WebRtcTransport : public std::enable_shared_from_this<WebRtcTransport> {
 public:
  WebRtcTransport(muduo::net::EventLoop* loop2, std::string strIP);
  ~WebRtcTransport();

  void Start();
  std::string GetLocalSdp();
  void OnIceServerCompleted();
  void OnDtlsCompleted(std::string clientKey, std::string serverKey);

  void onInputDataPacket(char* buf, int len, struct sockaddr_in* remoteAddr);
  void WritePacket(char* buf, int len, struct sockaddr_in* remoteAddr);
  void WritePacket(char* buf, int len);
  void WritRtpPacket(char* buf, int len);
  void WriteH264Frame(char* buf, int len, uint32_t timestamp);

 private:
  IceServer::Ptr m_IceServer;
  MyDtlsTransport::Ptr m_pDtlsTransport;
  std::shared_ptr<erizo::SrtpChannel> m_Srtp;

  MuduoUdpSocket::Ptr m_pMuduoUdpSocket;
  muduo::net::EventLoop* m_MuduoLoop;

  char m_ProtectBuf[65536];
  bool m_bReady;
  std::string m_strIP;
  struct sockaddr_in m_RemoteSockaddr;
  RtpMaker m_rtpmaker;
};

#endif  // MYWEBRTC_WEBRTCTRANSPORT_H