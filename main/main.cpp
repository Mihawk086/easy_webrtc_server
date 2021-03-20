#include <iostream>
#include <map>

#include "FFmpegSrc.h"
#include "dtls/DtlsSocket.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"
#include "webrtctransport/Utils.hpp"
#include "webrtctransport/WebRtcTransport.h"

using namespace muduo;
using namespace muduo::net;

static std::map<int, std::shared_ptr<WebRtcTransport>> s_WebRTCSession;
static int s_sessionid = 1;

int main(int argc, char* argv[]) {
  std::string strIP = "192.168.127.128";
  if (argc > 1) {
    strIP = argv[1];
  }
  FFmpegSrc::GetInsatance()->Start();
  Utils::Crypto::ClassInit();
  dtls::DtlsSocketContext::Init();

  int threads_num = 0;
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8000), "webrtc", TcpServer::kReusePort);
  server.setHttpCallback([&loop, &strIP](const HttpRequest& req, HttpResponse* resp) {
    if (req.path() == "/webrtc") {
      resp->setStatusCode(HttpResponse::k200Ok);
      resp->setStatusMessage("OK");
      resp->setContentType("text/plain");
      resp->addHeader("Access-Control-Allow-Origin", "*");
      std::shared_ptr<WebRtcTransport> session(new WebRtcTransport(&loop, strIP));
      s_WebRTCSession.insert(std::make_pair(s_sessionid, session));
      s_sessionid++;
      session->Start();

      FFmpegSrc::GetInsatance()->AddClient(session);
      resp->setBody(session->GetLocalSdp());
    }
  });
  server.setThreadNum(threads_num);
  server.start();
  loop.loop();
}
