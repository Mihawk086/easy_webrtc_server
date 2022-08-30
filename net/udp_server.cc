#include "net/udp_server.h"

#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/Socket.h"
#include "muduo/net/SocketsOps.h"

using namespace muduo;
using namespace muduo::net;

static const int BUF_SIZE = 4 * 1024;

UdpServer::UdpServer(EventLoop* loop, const InetAddress& listen_addr, const std::string& name,
                     int num_threads)
    : loop_(CHECK_NOTNULL(loop)), thread_pool_(new EventLoopThreadPool(loop, name)) {
  size_ = BUF_SIZE;
  buf_ = new char[size_];
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  socket_ = std::unique_ptr<Socket>(new Socket(fd));
  socket_->setReuseAddr(true);
  socket_->setReusePort(true);
  socket_->bindAddress(listen_addr);
  channel_ = std::unique_ptr<Channel>(new Channel(loop, socket_->fd()));
  channel_->setReadCallback(std::bind(&UdpServer::HandleRead, this, _1));
  thread_pool_->setThreadNum(num_threads);
}

UdpServer::~UdpServer() {}

static int DissolveUdpSock(int fd) {
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  if (-1 == getsockname(fd, (struct sockaddr*)&addr, &addr_len)) {
    return -1;
  }
  addr.ss_family = AF_UNSPEC;
  if (-1 == ::connect(fd, (struct sockaddr*)&addr, addr_len)) {
    return -1;
  }
  return 0;
}

void UdpServer::HandleRead(Timestamp receive_time) {
  loop_->assertInLoopThread();
  struct sockaddr_in remote_addr;
  unsigned int addr_len = sizeof(remote_addr);
  size_t recvLen =
      recvfrom(socket_->fd(), buf_, size_, 0, (struct sockaddr*)&remote_addr, &addr_len);
  if (callback_) {
    callback_(this, (const uint8_t*)buf_, recvLen, InetAddress(remote_addr), receive_time);
  }
}

UdpConnectionPtr UdpServer::GetOrCreatConnection(const InetAddress& remote_addr) {
  std::string key = remote_addr.toIpPort();
  if (auto it = connections_.find(key); it != connections_.end()) {
    return it->second;
  }
  EventLoop* io_loop = thread_pool_->getNextLoop();
  InetAddress local_addr(sockets::getLocalAddr(socket_->fd()));
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  auto connection =
      std::shared_ptr<UdpConnection>(new UdpConnection(io_loop, key, fd, local_addr, remote_addr));
  connections_[key] = connection;
  DissolveUdpSock(socket_->fd());
  return connection;
}

void UdpServer::Start() {
  thread_pool_->start(thread_init_callback_);
  loop_->runInLoop(std::bind(&Channel::enableReading, channel_.get()));
}
