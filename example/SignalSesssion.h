//
// Created by xueyuegui on 19-8-25.
//

#ifndef MYWEBRTC_SIGNALSESSSION_H
#define MYWEBRTC_SIGNALSESSSION_H

#include <string>
#include <memory>
#include <functional>

#include <boost/asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <websocketpp/common/thread.hpp>


#include "dtls/DtlsSocket.h"
#include "webrtctransport/WebRtcTransport.h"


using namespace erizo;

typedef websocketpp::server<websocketpp::config::asio> websocket_server;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;



class SignalServer;
class SignalSesssion :public std::enable_shared_from_this<SignalSesssion>{
public:

    SignalSesssion(boost::asio::io_service* ptr_io_service,SignalServer* server,connection_hdl hdl);

    void on_Open();
    void on_Close();
    void on_Message(websocket_server::message_ptr msg);
    void Send(std::string str);

    void TaskInLoop(std::function<void()> func);

private:
    std::shared_ptr<WebRtcTransport> m_webrtctransport;
    connection_hdl m_hdl;
    std::string m_strRemoteSdp;
    SignalServer* m_server;
    boost::asio::io_service* m_ioservice;

};


#endif //MYWEBRTC_SIGNALSESSSION_H
