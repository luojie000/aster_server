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
static constexpr std::size_t kOutputLimit = 1024 * 1024;
std::size_t pendingOutputBytes() const { return outputBuffer_.size(); }
bool readPaused() const { return readPaused_; }
void setCloseCallback(std::function<void()> cb);
void handleWrite();

// Configure before registering the Channel. All methods below
// belong to the loop thread, including completeRequest(). Empty callback = echo.
using RequestCallback = std::function<void(std::string)>;
void setRequestCallback(RequestCallback cb);
void completeRequest(std::string response);
std::weak_ptr<int> lifetimeToken() const { return lifetime_; }
static constexpr std::size_t kInputLimit = 4096;
static constexpr std::size_t kLineLimit = 64;
static constexpr std::size_t kResponseLimit = 128;

    private:

    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    RequestCallback requestCallback_;
    std::string inputBuffer_;
    bool processingRequest_ = false;
    void dispatchRequest();

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