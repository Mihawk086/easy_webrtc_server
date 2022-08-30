#pragma once

#include <boost/any.hpp>
#include <map>
#include <memory>
#include <string>

#include "muduo/base/Timestamp.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/InetAddress.h"
#include "net/callbacks.h"
#include "net/udp_connection.h"

namespace muduo {
namespace net {
class Channel;
class EventLoop;
class Socket;
class EventLoopThreadPool;
}  // namespace net
}  // namespace muduo

class UdpServer {
 public:
  typedef std::function<void(muduo::net::EventLoop*)> ThreadInitCallback;

  UdpServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listen_addr,
            const std::string& name, int num_threads);
  ~UdpServer();
  void SetPacketCallback(ServerPacketCallback cb) { callback_ = cb; }
  void SetThreadInitCallback(const ThreadInitCallback& cb) { thread_init_callback_ = cb; }
  UdpConnectionPtr GetOrCreatConnection(const muduo::net::InetAddress& remote_addr);
  void Start();

 private:
  void HandleRead(muduo::Timestamp receiveTime);

 private:
  typedef std::map<std::string, UdpConnectionPtr> ConnectionMap;

  muduo::net::EventLoop* loop_;
  std::shared_ptr<muduo::net::EventLoopThreadPool> thread_pool_;
  ConnectionMap connections_;
  std::unique_ptr<muduo::net::Socket> socket_;
  std::unique_ptr<muduo::net::Channel> channel_;
  char* buf_;
  size_t size_;
  ThreadInitCallback thread_init_callback_;
  ServerPacketCallback callback_;
};