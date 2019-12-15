#pragma once

#include <functional>

class RtpMaker
{
public:
	RtpMaker();
	~RtpMaker();
	void InputH264Frame(char* buf,int len, uint32_t timestamp);
	void SetRtpCallBack(std::function<void(char* buf, int len)> cb) { m_RtpCallBack = cb; };
	void Setssrc(uint32_t ssrc) { m_ssrc = ssrc; };
private:
	std::function<void(char* buf, int len )> m_RtpCallBack;
	char m_buf[5000];
	uint16_t m_seq = 0;
	uint32_t m_ssrc = 12345678;
};



