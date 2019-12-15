#pragma once

#include "net/SocketUtil.h"
#include "net/Channel.h"
#include "net/EventLoop.h"


#include <memory>
#include <functional>

typedef std::function<void(char* buf, int len, struct sockaddr_in* remoteAddr)> UDPSOCKETREADCB;

class UdpSocket
{
public:
    typedef std::shared_ptr<UdpSocket> Ptr;

	UdpSocket(std::string ip,int16_t port, xop::EventLoop* loop);
	~UdpSocket();
	void Start();
	int Send(char* buf,int len,  const struct sockaddr_in& remoteAddr);
	void setReadCallback(UDPSOCKETREADCB cb) {
		m_ReadCB = cb;
	}
private:
	void handleRead();
	SOCKET m_fd;
	int16_t m_port;
	std::string m_strIP;
	std::string m_strIpAddr;
	std::shared_ptr<xop::Channel> m_channel;
	xop::EventLoop* m_loop;
	UDPSOCKETREADCB m_ReadCB;

};

