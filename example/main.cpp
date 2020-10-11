#include <thread>
#include <map>
#include <signal.h>
#include <iostream>

#include "muduo/net/http/HttpServer.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"
#include "webrtctransport/Utils.hpp"
#include "webrtctransport/WebRtcTransport.h"
#include "dtls/DtlsSocket.h"

#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Rtmp/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"
#include "Player/PlayerProxy.h"
#include "Common/MediaSource.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;
using namespace muduo;
using namespace muduo::net;

typedef struct rtp_header
{
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t version:2;
	uint16_t padding:1;
	uint16_t extension:1;
	uint16_t csrccount:4;
	uint16_t markerbit:1;
	uint16_t type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t csrccount:4;
    uint16_t extension:1;
    uint16_t padding:1;
    uint16_t version:2;
    uint16_t type:7;
    uint16_t markerbit:1;
#endif

    uint16_t seq_number;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[16];
} rtp_header;

static std::map<int, std::shared_ptr<WebRtcTransport>> s_WebRTCSession;
static std::shared_ptr<mediakit::RtspMediaSource::RingType::RingReader> s_reader;
static int s_sessionid = 1;

void GetZLMedia() {
    auto poller = EventPollerPool::Instance().getPoller();

    poller->sync([=](){
        MediaInfo info;
        info._schema = "rtsp";
        info._app = "live";
        info._streamid = "2";
        auto src = mediakit::MediaSource::find(info._schema,
                                               info._vhost,
                                               info._app,
                                               info._streamid);
        if(src){
            std::cout<<"src find"<<std::endl;
            std::shared_ptr<RtspMediaSource> rtspsrc = dynamic_pointer_cast<RtspMediaSource>(src);
            auto reader = rtspsrc->getRing()->attach(poller, true);
            reader->setDetachCB([](){
                std::cout<<"Detach"<<std::endl;
            });

            reader->setReadCB([=](const RtspMediaSource::RingDataType &pack){
                while (!pack->empty())
                {
                    auto it = pack->front();
                    BufferRtp::Ptr buffer(new BufferRtp(it,0));
                    rtp_header *header = (rtp_header *)buffer->data();
                    header->ssrc = htonl(12345678);
                    if(it->type == TrackVideo){
                        for (auto item: s_WebRTCSession)
                        {
                            item.second->WritRtpPacket(buffer->data(),buffer->size());
                        }
                        
                    }
                    pack->pop_front();
                }
            });
            s_reader = reader;
        }

    });
}



int main(int argc, char* argv[]){
    toolkit::Logger::Instance().add(std::make_shared<ConsoleChannel>());
    toolkit::Logger::Instance().add(std::make_shared<FileChannel>());
    toolkit::Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    //加载配置文件，如果配置文件不存在就创建一个
    mediakit::loadIniConfig();

    toolkit::TcpServer::Ptr rtspSrv(new toolkit::TcpServer());
    toolkit::TcpServer::Ptr httpSrv(new toolkit::TcpServer());
    rtspSrv->start<RtspSession>(8554);//默认554
    httpSrv->start<HttpSession>(8080);//默认80
	
	PlayerProxy::Ptr player(new PlayerProxy(DEFAULT_VHOST,"live","2"));
 	player->play("rtsp://192.168.127.128:554/live");

    std::string strIP = "192.168.127.128";
    if (argc > 1)
    {
        strIP = argv[1];
    }
    Utils::Crypto::ClassInit();
    dtls::DtlsSocketContext::Init();

    int numThreads = 0;
    EventLoop loop;
    HttpServer server(&loop, InetAddress(8000), "webrtc",muduo::net::TcpServer::kReusePort);
    //server.setHttpCallback(onRequest);
    GetZLMedia();
    server.setHttpCallback([&loop,&strIP](const HttpRequest& req,
        HttpResponse* resp)
        {
            if (req.path() == "/webrtc") {
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("text/plain");
                resp->addHeader("Access-Control-Allow-Origin", "*");
                std::shared_ptr<WebRtcTransport> session(new WebRtcTransport(&loop, strIP));
                s_WebRTCSession.insert(std::make_pair(s_sessionid, session));
                s_sessionid++;
                session->Start();
                resp->setBody(session->GetLocalSdp());
            }
        });
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();
}