# easy_webrtc_server
简单的webrtc流媒体服务器
联系方式：qq864733526
微信：Mihawk086
# 依赖库
``` 
1、openssl 1.1以上 
2、ffmpeg，ffmpeg_src_example例子用到ffmpeg库
sudo apt install libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavresample-dev libavutil-dev libpostproc-dev libswresample-dev libswscale-dev    
3、muduo网络库：https://github.com/chenshuo/muduo
4、srtp https://github.com/cisco/libsrtp。srtp必须--enable-openssl或者修改cmake的ENABLE_OPENSSL为ON
```  
# 目录说明
* webrtchtml webrtc视频播放的网页 
* rtc webrtc协议，包括stun，dtls，srtp  
* main ffmpeg_src_example，ffmpeg编码带有日期时间的h264流，http信令服务器交换sdp  
* main rtp_src_example，转发rtp例子，直接将56000的udp端口接收到的rtp流通过webrtc协议转发

# 使用说明
ubuntu18.04安装依赖库openssl1.1以上、srtp、ffmpeg
安装muduo网络库
```  
mkdir build  
cd build   
cmake ..  
make  
```  
## ffmpeg_src_example
运行程序，第一个参数为IP地址：
./ffmpeg_src_example 服务器IP地址   
打开webrtchtml/index.html 输入IP地址，播放视频 

## rtp_src_example
运行程序，第一个参数为IP地址：
./rtp_src_example 服务器IP地址
### 推屏幕采集rtp流
在windows运行命令 ffmpeg -f gdigrab -i desktop -framerate 25 -s 640x480 -pix_fmt yuv420p  -vcodec libx264 -profile baseline -tune zerolatency  -g 25 -f rtp rtp://192.168.2.128:56000
将屏幕采集的的rtp流推到服务器，ip改为自己服务器的地址，port为56000
### 推摄像头rtp流
在windows运行命令ffmpeg -f dshow -i video="USB2.0 PC CAMERA" -pix_fmt yuv420p -s 640x480  -vcodec libx264 -profile baseline -tune zerolatency  -g 25 -f rtp rtp://192.168.2.128:56000
将摄像头的rtp流推到服务器，ip改为自己服务器的地址，port为56000

打开webrtchtml/index.html 输入IP地址，播放视频 


# 原理说明
* muduo不支持udp，本项目基于muduo的Channel类简单封装一个udp通信的类；  
* 基于muduo_http建立一个http信令服务器，交换webrtc所需要的sdp信息。  
* 网页上打开一个http连接，服务器建立一个WebRtcTransport，底层是一个UdpSocket。  
* WebRtcTransport生成sdp信息，通过http协议传到前端。    
* sdp信息包括媒体信息如编码格式、ssrc等，stun协议需要的ice-ufrag、ice-pwd、candidate,dtls需要的fingerprint。  
* 前端通过candidate获取ip地址和端口号，通过udp协议连接到服务器的。  
* 服务器收到udp报文，先后通过类UdpSocket接收报文；StunPacket和IceServer解析stun协议，此处的Stun协议解析，只要收到stun request，验证账户密码成功，就认为连接成功。
* stun协议交互成功后，通过DtlsTransport进行dtls握手；交换密钥后就可以初始化SrtpChannel。此处没有通过签名验证客户端的证书，所以省略了前端返回sdp的步骤。  
* FFmpeg生成h264流通过RtpMaker生成rtp流，通过SrtpChannel加密，通过UdpSocket发送，前端就可以看到视频。  
