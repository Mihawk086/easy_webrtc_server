#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "muduo/net/EventLoop.h"
#include "net/udp_connection.h"
#include "net/udp_server.h"
#include "rtc/stun_packet.h"
#include "rtc/transport_interface.h"
#include "rtc/webrtc_transport.h"

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
  void SetNetworkTransport(const std::shared_ptr<NetworkTransport>& transport);
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
  std::shared_ptr<WebRTCSession> CreateWebRTCSession(const std::string& ip, uint16_t port);
  std::shared_ptr<WebRTCSession> GetWebRTCSession(const std::string& key);
  void GetAllReadyWebRTCSession(std::vector<std::shared_ptr<WebRTCSession>>* sessions);
  static void HandlePacket(WebRTCSessionFactory* factory, UdpServer* server, const uint8_t* buf,
                           size_t len, const muduo::net::InetAddress& peer_addr,
                           muduo::Timestamp timestamp);

 private:
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<WebRTCSession>> rtc_sessions;
};