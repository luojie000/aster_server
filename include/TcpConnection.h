#pragma once
#include"Channel.h"
#include<string>

class TcpConnection
{

    public:
TcpConnection(int fd,EventLoop&loop);
~TcpConnection();

int fd()const;
Channel* channel()const;
void handleRead();
void setCloseCallback(std::function<void()> cb);
void handleWrite();

    private:

    int fd_;
    Channel* channel_;
    std::function<void()> closeCallback_;
    std::string outputBuffer_;

    EventLoop&loop_;
};