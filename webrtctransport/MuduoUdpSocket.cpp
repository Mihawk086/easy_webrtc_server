#include "MuduoUdpSocket.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Socket.h"
#include "muduo/net/SocketsOps.h"

#include <iostream>

using namespace muduo;
using namespace muduo::net;

#define BUFF_SIZE 5000
static char s_buf[BUFF_SIZE];

MuduoUdpSocket::MuduoUdpSocket(std::string ip, EventLoop* loop)
	:m_loop(loop), m_strIP(ip)
{
}

MuduoUdpSocket::~MuduoUdpSocket()
{
	int fd = m_channel->fd();
	if (fd > 0)
	{
		sockets::close(fd);
        m_loop->queueInLoop([this](){
            m_loop->removeChannel(m_channel.get());
            });
	}
}

void MuduoUdpSocket::Start()
{
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_strIP.c_str());
    addr.sin_port = 0;
    ::bind(m_fd, (struct sockaddr*)&addr, sizeof addr);
    
    {
        struct sockaddr addr;
        struct sockaddr_in* addr_v4;
        socklen_t addr_len = sizeof(addr);
        if (0 == ::getsockname(m_fd, &addr, &addr_len)) {
            if (addr.sa_family == AF_INET) {
                addr_v4 = (sockaddr_in*)&addr;
                m_port =  ntohs(addr_v4->sin_port);
            }
        }
    }
    
    m_loop->queueInLoop([this]() {
        m_channel.reset(new Channel(m_loop, m_fd));
        m_channel->setReadCallback([this](Timestamp time) { this->handleRead(); });
        m_channel->enableReading();
        m_loop->updateChannel(m_channel.get());
        });
}

int MuduoUdpSocket::Send(char* buf, int len, const sockaddr_in& remoteAddr)
{
    int ret = sendto(m_fd, (const char*)buf, len, 0,
        (struct sockaddr*)&remoteAddr,
        sizeof(remoteAddr));

    if (ret < 0)
    {
        std::cout << "send error" << std::endl;
        return -1;
    }

    return ret;
}

void MuduoUdpSocket::handleRead()
{
    struct sockaddr_in remoteAddr;
    unsigned int nAddrLen = sizeof(remoteAddr);
    int recvLen = recvfrom(m_fd, s_buf, BUFF_SIZE, 0, (struct sockaddr*)&remoteAddr, &nAddrLen);
    if (m_ReadCB) {
        m_ReadCB(s_buf, recvLen, &remoteAddr);
    }
}
