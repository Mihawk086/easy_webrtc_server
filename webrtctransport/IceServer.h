#pragma once

#include <functional>
#include <memory>

#include "StunPacket.hpp"
#include "logger.h"

typedef std::function<void(char* buf, int len, struct sockaddr_in* remoteAddr)> UDPSOCKETSENDCB;

class IceServer
{
	DECLARE_LOGGER();
public:
	enum class IceState
	{
		NEW = 1,
		CONNECTED,
		COMPLETED,
		DISCONNECTED
	};
    typedef std::shared_ptr<IceServer> Ptr;
	IceServer();
	IceServer(const std::string& usernameFragment, const std::string& password);
	const std::string& GetUsernameFragment() const;
	const std::string& GetPassword() const;
	void SetUsernameFragment(const std::string& usernameFragment);
	void SetPassword(const std::string& password);
	IceState GetState() const;
	void ProcessStunPacket(RTC::StunPacket* packet, struct sockaddr_in* remoteAddr);
	void HandleTuple(struct sockaddr_in* remoteAddr, bool hasUseCandidate);
	~IceServer();
	void SetSendCB(UDPSOCKETSENDCB send_cb) {
		m_send_cb = send_cb;
	}
	void SetIceServerCompletedCB(std::function<void()> cb) {
		m_IceServerCompletedCB = cb;
	};
	struct sockaddr_in* GetSelectAddr() { return &m_remoteAddr; }
private:
	UDPSOCKETSENDCB m_send_cb;
	std::function<void()> m_IceServerCompletedCB;
	std::string usernameFragment;
	std::string password;
	std::string oldUsernameFragment;
	std::string oldPassword;
	IceState state{ IceState::NEW };
	struct sockaddr_in m_remoteAddr;

};

