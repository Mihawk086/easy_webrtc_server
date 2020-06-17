#pragma once

#include <memory>
#include <functional>

#define BUFF_SIZE 5000

namespace muduo {
	namespace net {
		class Channel;
		class EventLoop;
	}
}
typedef std::function<void(char* buf, int len, struct sockaddr_in* remoteAddr)> MUDUO_UDPSOCKET_READCB;
class MuduoUdpSocket :public std::enable_shared_from_this<MuduoUdpSocket> {
public:
	typedef std::shared_ptr<MuduoUdpSocket> Ptr;
	MuduoUdpSocket(std::string ip, muduo::net::EventLoop* loop);
	~MuduoUdpSocket();
	void Start();
	int Send(char* buf, int len, const struct sockaddr_in& remoteAddr);
	void setReadCallback(MUDUO_UDPSOCKET_READCB cb) {
		m_ReadCB = cb;
	}
	uint16_t GetPort() { return m_port; }
private:
	MUDUO_UDPSOCKET_READCB m_ReadCB;
	void handleRead();
	int m_fd;
	uint16_t m_port;
	std::string m_strIP;
	std::string m_strIpAddr;
	std::shared_ptr<muduo::net::Channel> m_channel;
	muduo::net::EventLoop* m_loop;
	char m_buf[BUFF_SIZE];
};