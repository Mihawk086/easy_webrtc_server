//
// Created by xueyuegui on 19-8-25.
//

//#include "SignalSesssion.h"
#include "SignalServer.h"
#include "SignalSesssion.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "FFmpegSrc.h"
#include "MyLoop.h"



using namespace rapidjson;

SignalSesssion::SignalSesssion(boost::asio::io_service *ptr_io_service ,SignalServer* server,connection_hdl hdl)
:m_server(server),m_ioservice(ptr_io_service),m_hdl(hdl)
{
}

void SignalSesssion::on_Open() {

    m_webrtctransport.reset(new WebRtcTransport(MyLoop::GetLoop()));
    m_webrtctransport->Start();
    Send(m_webrtctransport->GetLocalSdp());
    FFmpegSrc::GetInsatance()->AddClient(m_webrtctransport);
}

void SignalSesssion::on_Close() {

}

void SignalSesssion::on_Message(websocket_server::message_ptr msg) {

    std::string str = msg->get_payload();
    Document d;
    if(d.Parse(str.c_str()).HasParseError()){
        printf("parse error!\n");
    }
    if(!d.IsObject()){
        printf("should be an object!\n");
        return ;
    }
    if(d.HasMember("sdp")){
        Value &m = d["sdp"];
        std::string strSdp = m.GetString();
      
        return ;
    }
    if(d.HasMember("candidate")){
        Value &v1 = d["candidate"];
        Value &v2 = d["sdpMLineIndex"];
        Value &v3 = d["sdpMid"];
        std::string strCandidate = v1.GetString();
        int mLineIndex = v2.GetInt();
        std::string mid = v3.GetString();

        std::string cand; //parse from candidate
        cand = strCandidate;
        std::string sdp = m_strRemoteSdp;
        sdp += "a=";
        sdp += cand;
        sdp += "\r\n";
        
        return ;
    }
}


void SignalSesssion::Send(std::string szsdp){
    m_server->GetServer()->send(m_hdl, szsdp, websocketpp::frame::opcode::text);
}

void SignalSesssion::TaskInLoop(std::function<void()> func) {
    m_ioservice->post(func);
}




