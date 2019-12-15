#include "UdpSocket.h"

#include <iostream>
using namespace xop;

#define BUFF_SIZE 5000
static char s_buf[BUFF_SIZE];

UdpSocket::UdpSocket(std::string ip,int16_t port, xop::EventLoop* loop)
	:m_loop(loop),m_strIP(ip),
	m_port(port)
{
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (!SocketUtil::bind(m_fd, ip.c_str(), port))
    {
        SocketUtil::close(m_fd);
        std::cout << "create UdpSocket" << std::endl;
    }
    SocketUtil::setReuseAddr(m_fd);
    SocketUtil::setReusePort(m_fd);
    SocketUtil::setNonBlock(m_fd);
    m_channel.reset(new xop::Channel(m_fd));
    m_channel->setReadCallback([this]() { this->handleRead(); });
    m_channel->enableReading();
    loop->updateChannel(m_channel);
}


UdpSocket::~UdpSocket()
{
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

}
