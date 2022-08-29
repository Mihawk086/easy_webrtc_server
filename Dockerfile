FROM ubuntu:20.04

ENV TZ=Asia/Kolkata \
    DEBIAN_FRONTEND=noninteractive

RUN sed -i s@/archive.ubuntu.com/@/mirrors.aliyun.com/@g /etc/apt/sources.list && \
	sed -i s@/security.ubuntu.com/@/mirrors.aliyun.com/@g /etc/apt/sources.list && \
	apt-get clean && \
	apt-get update

RUN apt-get -y update && \
	apt-get install -y \
		libssl-dev \
    	libboost-all-dev \
		libavutil-dev \
		libavformat-dev \
		libavcodec-dev \
		pkg-config \
		cmake \
		automake \
		build-essential \
		wget \
		git \
		gdb \
		net-tools \
		ffmpeg \
		vim 

RUN cd /home && \
	wget https://github.com/cisco/libsrtp/archive/v2.3.0.tar.gz && \
	tar xfv v2.3.0.tar.gz && \
	cd libsrtp-2.3.0 && \
	./configure --prefix=/usr --enable-openssl && \
	make shared_library && \
	make install

RUN cd /home && \
	git clone https://github.com/chenshuo/muduo.git && \
	cd muduo && \
	mkdir build && \
  cd build && \
	cmake .. && \
	make && \
  make install && \
  cp /home/muduo/muduo/net/*.h /usr/local/include/muduo/net/

RUN cd /home && \
	git clone https://github.com/Mihawk086/easy_webrtc_server.git && \
	cd easy_webrtc_server && \
	mkdir build && \
  cd build && \
	cmake .. && \
	make


EXPOSE 8000
EXPOSE 10000/udp