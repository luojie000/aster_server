#pragma once
#include"Channel.h"

class TcpConnection
{

    public:
TcpConnection(int fd);
~TcpConnection();

int fd()const;
Channel* channel()const;
void handleRead();
void setCloseCallback(std::function<void()> cb);

    private:

    int fd_;
    Channel* channel_;
    std::function<void()> closeCallback_;
};