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
void handleRead();
void setCloseCallback(std::function<void()> cb);
void handleWrite();
// Pending application bytes; kernel socket buffers are separate.
static constexpr size_t kOutputLimit = 1024 * 1024;
static constexpr size_t kResumeLevel = kOutputLimit / 2;
size_t pendingOutputBytes() const { return outputBuffer_.size(); }
bool readPaused() const { return readPaused_; }

    private:

    int fd_;
    std::unique_ptr<Channel> channel_;
    bool closed_ = false;
    bool readClosed_ = false;
    bool readPaused_ = false;
    void handleClose();
    std::function<void()> closeCallback_;
    std::string outputBuffer_;

    EventLoop&loop_;
};