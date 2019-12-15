//
// Created by xueyuegui on 19-8-25.
//

#ifndef MYWEBRTC_SIGNALSERVER_H
#define MYWEBRTC_SIGNALSERVER_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <websocketpp/common/thread.hpp>

#include "SignalSesssion.h"

#include <unordered_map>
#include <memory>
#include <map>

typedef websocketpp::server<websocketpp::config::asio> websocket_server;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;


class SignalServer {
public:
    SignalServer() = delete;
    SignalServer(SignalServer &other) = delete;
    SignalServer operator=(SignalServer &other) = delete;

    SignalServer(boost::asio::io_service* ptr_io_service, uint16_t port);
    websocket_server* GetServer(){
        return &m_server;
    };
    void Start();
    void Stop();
    std::shared_ptr<SignalSesssion>& GetSessionFromhdl(connection_hdl hdl);

    void onOpen(connection_hdl);
    void onClose(connection_hdl);
    void onMessage(connection_hdl,websocket_server::message_ptr);
private:
    websocket_server m_server;
    connection_hdl hdl_;
    std::map<connection_hdl,std::shared_ptr<SignalSesssion>,std::owner_less<connection_hdl>> m_mapClients;
    boost::asio::io_service* m_ioservice;
    int16_t m_iPort;

    std::function<void(connection_hdl)> m_open_cb;
    std::function<void(connection_hdl)> m_close_cb;
    std::function<void(connection_hdl,websocket_server::message_ptr)>  m_message_cb;

};


#endif //MYWEBRTC_SIGNALSERVER_H
