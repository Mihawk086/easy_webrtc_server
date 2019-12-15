//
// Created by xueyuegui on 19-12-6.
//

#ifndef MYWEBRTC_MYLOOP_H
#define MYWEBRTC_MYLOOP_H

#include "net/EventLoop.h"
#include <thread>
#include <memory>
class MyLoop {
public:
    static xop::EventLoop* GetLoop();
    xop::EventLoop* getLoop();
private:
    MyLoop();
    xop::EventLoop m_loop;
    std::shared_ptr<std::thread> m_thread;
};


#endif //MYWEBRTC_MYLOOP_H
