#pragma once

#include <functional>
#include <memory>

#include "muduo/base/Timestamp.h"
#include "muduo/net/InetAddress.h"

class UdpConnection;
class UdpServer;

typedef std::function<void(UdpServer*, const uint8_t* buf, size_t len,
                           const muduo::net::InetAddress& peer_addr, muduo::Timestamp)>
    ServerPacketCallback;
typedef std::function<void(const std::shared_ptr<UdpConnection>&, const uint8_t* buf, size_t len,
                           const muduo::net::InetAddress& peer_addr, muduo::Timestamp)>
    PacketCallback;