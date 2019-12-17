//
// Created by xueyuegui on 19-12-7.
//

#include "WebRtcTransport.h"
#include "Utils.hpp"

using namespace erizo;

WebRtcTransport::WebRtcTransport(xop::EventLoop *loop,std::string strIP)
        :m_loop(loop),m_bReady(false){
    m_strIP = strIP;
    m_pDtlsTransport.reset(new MyDtlsTransport(true));
    m_pUdpSocket.reset(new UdpSocket(m_strIP,loop));
    m_Srtp.reset(new SrtpChannel());
    m_IceServer.reset(new IceServer(Utils::Crypto::GetRandomString(4), Utils::Crypto::GetRandomString(24)));
    m_pUdpSocket->setReadCallback([this](char* buf, int len, struct sockaddr_in* remoteAddr){
        this->onInputDataPacket(buf,len,remoteAddr);
    });
    m_IceServer->SetIceServerCompletedCB([this](){
        this->OnIceServerCompleted();
    });
    m_IceServer->SetSendCB([this](char* buf, int len, struct sockaddr_in* remoteAddr) {
        this->WritePacket(buf, len,remoteAddr);
    });
    m_pDtlsTransport->SetHandshakeCompletedCB([this](std::string clientKey, std::string serverKey){
        this->OnDtlsCompleted(clientKey,serverKey);
    });
    m_pDtlsTransport->SetOutPutCB([this](char* buf ,int len){
        this->WritePacket(buf,len);
    });
    m_rtpmaker.SetRtpCallBack([this](char* buf ,int len) {
        this->WritRtpPacket(buf,len);
        });
}

WebRtcTransport::~WebRtcTransport() {

}

void WebRtcTransport::Start() {
    m_pUdpSocket->Start();
}

std::string WebRtcTransport::GetLocalSdp() {
    char szsdp[1024 * 10] = { 0 };
    int nssrc = 12345678;
    sprintf(szsdp, "v=0\r\no=- 1495799811084970 1495799811084970 IN IP4 %s\r\ns=Streaming Test\r\nt=0 0\r\n"
                   "a=group:BUNDLE video\r\na=msid-semantic: WMS janus\r\n"
                   "m=video 1 RTP/SAVPF 96\r\nc=IN IP4 %s\r\na=mid:video\r\na=sendonly\r\na=rtcp-mux\r\n"
                   "a=ice-ufrag:%s\r\n"
                   "a=ice-pwd:%s\r\na=ice-options:trickle\r\na=fingerprint:sha-256 %s\r\na=setup:actpass\r\na=connection:new\r\n"
                   "a=rtpmap:96 H264/90000\r\n"
                   "a=ssrc:%d cname:janusvideo\r\n"
                   "a=ssrc:%d msid:janus janusv0\r\n"
                   "a=ssrc:%d mslabel:janus\r\n"
                   "a=ssrc:%d label:janusv0\r\n"
                   "a=candidate:%s 1 udp %u %s %d typ %s\r\n",
            m_strIP.c_str(), m_strIP.c_str(),
            m_IceServer->GetUsernameFragment().c_str(), m_IceServer->GetPassword().c_str(),
            m_pDtlsTransport->GetMyFingerprint().c_str(),
            nssrc, nssrc, nssrc, nssrc,
            "4", 12345678, m_strIP.c_str(), m_pUdpSocket->GetPort(),
            "host"
    );
    return std::string(szsdp);
}

void WebRtcTransport::OnIceServerCompleted() {
    m_RemoteSockaddr = *m_IceServer->GetSelectAddr();
	m_pDtlsTransport->Start();
}

void WebRtcTransport::OnDtlsCompleted(std::string clientKey, std::string serverKey) {
    m_Srtp->setRtpParams(clientKey, serverKey);
    m_bReady = true;
}

void WebRtcTransport::onInputDataPacket(char *buf, int len, struct sockaddr_in *remoteAddr) {
    if (RTC::StunPacket::IsStun((const uint8_t*)buf, len)) {
        RTC::StunPacket* packet = RTC::StunPacket::Parse((const uint8_t*)buf, len);
        if (packet == nullptr)
        {
            std::cout << "parse stun error" << std::endl;
            return;
        }
        m_IceServer->ProcessStunPacket(packet, remoteAddr);
    }
    if(MyDtlsTransport::isDtlsPacket(buf,len)){
        m_pDtlsTransport->InputData(buf,len);
    }
}

void WebRtcTransport::WritePacket(char *buf, int len, struct sockaddr_in *remoteAddr) {
    m_pUdpSocket->Send(buf,len,*remoteAddr);
}

void WebRtcTransport::WritePacket(char *buf, int len) {
    m_pUdpSocket->Send(buf,len,m_RemoteSockaddr);
}

void WebRtcTransport::WritRtpPacket(char *buf, int len) {
    if(m_bReady) {
        memcpy(m_ProtectBuf, buf, len);
        int length = len;
        m_Srtp->protectRtp(m_ProtectBuf, &length);
        m_pUdpSocket->Send(m_ProtectBuf, length, m_RemoteSockaddr);
    }
}

void WebRtcTransport::WriteH264Frame(char* buf, int len, uint32_t timestamp)
{
    if (m_bReady) {
        m_rtpmaker.InputH264Frame(buf, len, timestamp);
    }
}


