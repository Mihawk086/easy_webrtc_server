#include "net/udp_connection.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/Socket.h"
#include "muduo/net/SocketsOps.h"

using namespace muduo;
using namespace muduo::net;

static const int BUF_SIZE = 4 * 1024;

void UdpConnection::HandleRead(muduo::Timestamp receive_time) {
  loop_->assertInLoopThread();
  struct sockaddr_in remote_addr;
  unsigned int addr_len = sizeof(remote_addr);
  size_t recvLen =
      recvfrom(socket_->fd(), buf_, size_, 0, (struct sockaddr*)&remote_addr, &addr_len);
  if (callback_) {
    callback_(shared_from_this(), (const uint8_t*)buf_, recvLen, InetAddress(remote_addr),
              receive_time);
  }
}

UdpConnection::UdpConnection(muduo::net::EventLoop* loop, const std::string& name, int sockfd,
                             const muduo::net::InetAddress& local_addr,
                             const muduo::net::InetAddress& peer_addr)
    : loop_(CHECK_NOTNULL(loop)),
      name_(name),
      local_addr_(local_addr),
      peer_addr_(peer_addr),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)) {
  size_ = BUF_SIZE;
  buf_ = new char[size_];
  socket_->setReuseAddr(true);
  socket_->setReusePort(true);
  socket_->bindAddress(local_addr);
  channel_->setReadCallback(std::bind(&UdpConnection::HandleRead, this, _1));
  sockets::connect(sockfd, peer_addr.getSockAddr());
}

UdpConnection::~UdpConnection() {}

void UdpConnection::Start() {
  loop_->runInLoop(std::bind(&Channel::enableReading, channel_.get()));
}

void UdpConnection::Send(const void* data, size_t len) {
  loop_->assertInLoopThread();
  ::write(socket_->fd(), data, len);
}
