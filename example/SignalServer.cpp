//
// Created by xueyuegui on 19-8-25.
//

#include "SignalServer.h"

SignalServer::SignalServer(boost::asio::io_service *ptr_io_service, uint16_t port)
:m_iPort(port),m_ioservice(ptr_io_service){
    m_server.init_asio(ptr_io_service);
    m_server.set_reuse_addr(true);
    m_server.set_open_handler(bind(&SignalServer::onOpen,this,::_1));
    m_server.set_close_handler(bind(&SignalServer::onClose,this,::_1));
    m_server.set_message_handler(bind(&SignalServer::onMessage,this,::_1,::_2));
}

void SignalServer::Start() {
    m_server.listen(m_iPort);
    m_server.start_accept();
}

void SignalServer::Stop() {
    m_server.stop();
}

void SignalServer::onOpen(connection_hdl con) {
    auto iter = m_mapClients.find(con);
    if(iter == m_mapClients.end()){
        auto client = std::make_shared<SignalSesssion>(m_ioservice,this,con);
        m_mapClients.insert(std::make_pair(con,client));
        client->on_Open();
    }else{
        return;
    }
}

void SignalServer::onClose(connection_hdl hdl) {
    auto ptr = GetSessionFromhdl(hdl);
    ptr->on_Close();
}

void SignalServer::onMessage(connection_hdl hdl, websocket_server::message_ptr msg) {
    auto ptr = GetSessionFromhdl(hdl);
    ptr->on_Message(msg);
}

std::shared_ptr<SignalSesssion> &SignalServer::GetSessionFromhdl(connection_hdl hdl) {
    auto iter = m_mapClients.find(hdl);
    if(iter == m_mapClients.end()){
        throw std::invalid_argument("No data available for session");
    }
    return iter->second;
}
