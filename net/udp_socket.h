#pragma once

#include <netinet/in.h>

#include <functional>
#include <memory>

#define KBuffSize 5000

namespace muduo {
namespace net {
class Channel;
class EventLoop;
}  // namespace net
}  // namespace muduo

typedef std::function<void(const uint8_t* buf, size_t len, const struct sockaddr_in& remote_addr)>
    UdpSocketReadCallback;
class UdpSocket : public std::enable_shared_from_this<UdpSocket> {
 public:
  typedef std::shared_ptr<UdpSocket> Ptr;
  UdpSocket(muduo::net::EventLoop* loop, std::string ip, uint16_t port = 0);
  ~UdpSocket();
  void BindRemote(const struct sockaddr_in& remote_addr) {
    is_connect_remote_ = true;
    remote_addr_ = remote_addr;
  }
  void Start();
  int Send(const uint8_t* buf, size_t len, const struct sockaddr_in& remote_addr);
  void SetReadCallback(UdpSocketReadCallback cb) { read_callback_ = cb; }
  uint16_t GetPort() { return port_; }
  std::string GetIP() { return ip_; }

 private:
  void HandleRead();

 private:
  UdpSocketReadCallback read_callback_;
  struct sockaddr_in remote_addr_;
  bool is_connect_remote_;
  int fd_;
  uint16_t port_;
  std::string ip_;
  std::shared_ptr<muduo::net::Channel> channel_;
  muduo::net::EventLoop* loop_;
  char buf_[KBuffSize];
};