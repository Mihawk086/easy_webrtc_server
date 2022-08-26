#pragma once

#include <boost/any.hpp>
#include <memory>
#include <string>

#include "muduo/base/Timestamp.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/InetAddress.h"
#include "net/callbacks.h"

namespace muduo {
namespace net {
class Channel;
class EventLoop;
class Socket;
}  // namespace net
}  // namespace muduo

class UdpConnection : public std::enable_shared_from_this<UdpConnection> {
 public:
  UdpConnection(muduo::net::EventLoop* loop, const std::string& name, int sockfd,
                const muduo::net::InetAddress& local_addr,
                const muduo::net::InetAddress& peer_addr);
  ~UdpConnection();

  muduo::net::EventLoop* loop() const { return loop_; }
  const std::string& name() const { return name_; }
  const muduo::net::InetAddress& local_address() const { return local_addr_; }
  const muduo::net::InetAddress& peer_address() const { return peer_addr_; }
  void SetPacketCallback(PacketCallback cb) { callback_ = cb; }
  void Send(const void* data, size_t len);
  void Start();
  
 private:
  void HandleRead(muduo::Timestamp receive_time);

 private:
  muduo::net::EventLoop* loop_;
  std::string name_;
  std::unique_ptr<muduo::net::Socket> socket_;
  std::unique_ptr<muduo::net::Channel> channel_;
  muduo::net::Buffer input_buffer_;
  boost::any context_;
  const muduo::net::InetAddress local_addr_;
  const muduo::net::InetAddress peer_addr_;
  PacketCallback callback_;
  char* buf_;
  size_t size_;
};

typedef std::shared_ptr<UdpConnection> UdpConnectionPtr;