#include "UdpSocket.h"
#include "PortManager.h"
#include <iostream>
using namespace xop;

#define BUFF_SIZE 5000
static char s_buf[BUFF_SIZE];

UdpSocket::UdpSocket(std::string ip, xop::EventLoop* loop)
	:m_loop(loop),m_strIP(ip)
{
   
}


UdpSocket::~UdpSocket()
{
    SOCKET fd = m_channel->fd();
    if (fd > 0)
    {
        SocketUtil::close(fd);
        m_loop->removeChannel(m_channel);
    }
    PortManager::GetInstance()->DelPort(m_port);
}

int UdpSocket::Send(char * buf, int len, const sockaddr_in & remoteAddr)
{
	int ret = sendto(m_fd, (const char*)buf, len, 0,
		(struct sockaddr *)&remoteAddr,
		sizeof(remoteAddr));

	if (ret < 0)
	{
		std::cout << "send error" << std::endl;
		return -1;
	}

	return ret;
}


void UdpSocket::handleRead()
{
	struct sockaddr_in remoteAddr;
    unsigned int nAddrLen = sizeof(remoteAddr);
	int recvLen = recvfrom(m_fd, s_buf, BUFF_SIZE, 0, (struct sockaddr *)&remoteAddr, &nAddrLen);
	if (m_ReadCB) {
		m_ReadCB(s_buf,recvLen,&remoteAddr);
	}
}

void UdpSocket::Start() {
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    int16_t nPort = 0;
    nPort = PortManager::GetInstance()->GetPort();
    if (!SocketUtil::bind(m_fd, m_strIP.c_str(), nPort))
    {
        SocketUtil::close(m_fd);
        std::cout << "create UdpSocket" << std::endl;
    }
    m_port = nPort;
    SocketUtil::setReuseAddr(m_fd);
    SocketUtil::setReusePort(m_fd);
    SocketUtil::setNonBlock(m_fd);
    m_channel.reset(new xop::Channel(m_fd));
    m_channel->setReadCallback([this]() { this->handleRead(); });
    m_channel->enableReading();
    m_loop->updateChannel(m_channel);
}
