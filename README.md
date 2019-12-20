# easy-webrtc-server
简单的webrtc流媒体服务器
# 依赖库
```  
sudo apt-get install liblog4cxx-dev  
sudo apt install libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavresample-dev libavutil-dev libpostproc-dev   libswresample-dev libswscale-dev  
sudo apt-get install libboost-all-dev  
```  
# 目录说明
* http 基于boost的简单http库
* dtls 封装openssl  
* net  一个类似muduo网络库  
* rapidjson 腾讯json库，解析json用  
* thirdparty 第三方库，srtp2和openssl版本1.02的头文件和静态库  
* webrtchtml webrtc视频播放的网页 
* webrtctransport webrtc协议，包括stun，dtls，srtp  
* websocketpp websocket网络库，依赖boost  
* example ffmpeg编码带有日期时间的h264流，websocket信令服务器交换sdp  

# 使用说明
ubuntu18.04安装依赖库  
```  
mkdir build  
cd build   
cmake ..  
make  
```  
运行程序   
修改webrtchtml/js/main.js下websocket的ip地址   
打开webrtchtml/index.html 播放视频 

# 原理说明
* 类似muduo网络库封装一个udpsocket；  
* 基于boost建立一个websocket信令服务器，交换webrtc所需要的sdp信息。  
* 网页上打开一个websocket连接，服务器建立一个WebRtcTransport，底层是一个udpsocket。  
* WebRtcTransport生成sdp信息，通过websocket传到前端。    
* sdp信息包括媒体信息如编码格式、ssrc等，stun协议需要的ice-ufrag、ice-pwd、candidate,dtls需要的fingerprint。  
* 前端通过candidate获取ip地址和端口号，通过udp协议连接到服务器的。  
* 服务器收到udp报文，先后通过类UdpSocket接收报文；StunPacket和IceServer解析stun协议；stun协议交互成功后  
通过MyDtlsTransport进行dtls握手；交换密钥后就可以初始化SrtpChannel。  
* FFmpeg生成h264流通过RtpMaker生成rtp流，通过SrtpChannel加密，通过udpsocket发送，前端就可以看到视频。  
