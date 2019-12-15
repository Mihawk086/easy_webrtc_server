//
// Created by xueyuegui on 19-12-6.
//

#include "MyLoop.h"

xop::EventLoop *MyLoop::GetLoop() {
    static MyLoop s_MyLoop;
    return s_MyLoop.getLoop();
}

MyLoop::MyLoop() {
    m_thread.reset(new std::thread([this](){
        m_loop.loop();
    }));
}

xop::EventLoop *MyLoop::getLoop() {
    return &m_loop;
}
