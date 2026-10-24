#pragma once
#include"Channel.h"
#include<string>
#include<memory>
#include"EventLoop.h"
class EventLoop;

class TcpConnection
{

    public:
TcpConnection(int fd,EventLoop&loop);
~TcpConnection();

int fd()const;
Channel* channel()const;
void handleRead(int fd);
void setCloseCallback(std::function<void()> cb);
void handleWrite();

    private:

    int fd_;
    std::unique_ptr<Channel> channel_;
    bool closed_ = false;
    bool readClosed_ = false;
    void handleClose();
    std::function<void()> closeCallback_;
    std::string outputBuffer_;

    EventLoop&loop_;
};