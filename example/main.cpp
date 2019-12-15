//
// Created by xueyuegui on 19-8-25.
//

#include <boost/asio.hpp>
#include <thread>

#include "SignalServer.h"
#include "webrtctransport/Utils.hpp"
#include "FFmpegSrc.h"

int main(){
    FFmpegSrc::GetInsatance()->Start();
	Utils::Crypto::ClassInit();
    dtls::DtlsSocketContext::Init();
    boost::asio::io_service ioService;
    SignalServer wserver(&ioService, 3000);
    wserver.Start();
    ioService.run();
}
