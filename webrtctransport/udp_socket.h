#pragma once

#include <functional>
#include <memory>

#define KBuffSize 5000

namespace muduo {
namespace net {
class Channel;
class EventLoop;
}  // namespace net
}  // namespace muduo
typedef std::function<void(char* buf, int len, struct sockaddr_in* remoteAddr)>
    UdpSocketReadCallback;
class UdpSocket : public std::enable_shared_from_this<UdpSocket> {
 public:
  typedef std::shared_ptr<UdpSocket> Ptr;
  UdpSocket(std::string ip, muduo::net::EventLoop* loop);
  ~UdpSocket();
  void Start();
  int Send(char* buf, int len, const struct sockaddr_in& remoteAddr);
  void SetReadCallback(UdpSocketReadCallback cb) { read_callback_ = cb; }
  uint16_t GetPort() { return port_; }

 private:
  UdpSocketReadCallback read_callback_;
  void handleRead();
  int fd_;
  uint16_t port_;
  std::string ip_;
  std::shared_ptr<muduo::net::Channel> channel_;
  muduo::net::EventLoop* loop_;
  char buf_[KBuffSize];
};