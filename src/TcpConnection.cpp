#include"TcpConnection.h"

#include<iostream>
#include<string>
#include<utility>
#include<cerrno>
#include<cstdio>

#include<sys/socket.h>
#include<unistd.h>

TcpConnection::TcpConnection(int fd,EventLoop&loop):fd_(fd),channel_(std::make_unique<Channel>(fd)),loop_(loop)
{
channel_->setReadCallback([this](){
    handleRead();
});

channel_->setWriteCallback([this](){
    handleWrite();
});
}

TcpConnection::~TcpConnection()
{
    loop_.removeChannel(fd_);
    if(fd_!=-1)
    {
        close(fd_);
    }
}

int TcpConnection::fd()const{
    return fd_;

}

Channel*TcpConnection::channel()const{
    return channel_.get();
}

void TcpConnection::setCloseCallback(std::function<void()> cb)
{
    closeCallback_=std::move(cb);
}


void TcpConnection::handleClose()
{
    if (closed_) return;
    closed_ = true;
    loop_.removeChannel(fd_);
    if (closeCallback_) closeCallback_();
}

void TcpConnection::handleRead()
{
    if (closed_ || readClosed_) return;
    char buffer[4096];
    while (true) {
        const ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);
        if (n > 0) { outputBuffer_.append(buffer, static_cast<size_t>(n)); continue; }
        if (n == 0) {
            readClosed_ = true;
            channel_->disableReading();
            loop_.updateChannel(channel_.get());
            break;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        handleClose();
        return;
    }
    handleWrite();
}

void TcpConnection::handleWrite()
{
    if (closed_) return;
    size_t sendindex = 0;

    while (sendindex < outputBuffer_.size())
    {
        ssize_t n = send(
            fd_,
            outputBuffer_.data() + sendindex,
            outputBuffer_.size() - sendindex,
            MSG_NOSIGNAL
        );

        if (n > 0)
        {
            sendindex += static_cast<size_t>(n);
        }
        else if (n == -1 && errno == EINTR)
        {
            continue;
        }
        else if (
            n == -1 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)
        )
        {
            break;
        }
        else
        {
            handleClose();
            return;
        }
    }

    outputBuffer_.erase(0, sendindex);

    if (!outputBuffer_.empty())
    {
        // 还有数据没有发送完
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
            loop_.updateChannel(channel_.get());
        }
    }
    else
    {
        // 数据已经全部发送完
        if (channel_->isWriting())
        {
            channel_->disableWriting();
            loop_.updateChannel(channel_.get());
        }
    }
    if (readClosed_ && outputBuffer_.empty()) handleClose();
}
  