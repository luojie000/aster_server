#include"TcpConnection.h"

#include<iostream>
#include<string>
#include<utility>
#include<cerrno>
#include<cstdio>

#include<sys/socket.h>
#include<unistd.h>

TcpConnection::TcpConnection(int fd,EventLoop&loop):fd_(fd),loop_(loop),channel_(new Channel(fd))
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
    delete channel_;
    if(fd_!=-1)
    {
        close(fd_);
    }
}

int TcpConnection::fd()const{
    return fd_;

}

Channel*TcpConnection::channel()const{
    return channel_;
}

void TcpConnection::setCloseCallback(std::function<void()> cb)
{
    closeCallback_=std::move(cb);
}

void TcpConnection::handleRead()
{
    char buffer[4096];

    ssize_t n;

    do
    {
        n = recv(fd_, buffer, sizeof(buffer), 0);
    }
    while (n == -1 && errno == EINTR);

    if (n > 0)
    {
        std::string message(buffer, static_cast<size_t>(n));

        // 把新数据追加到尚未发送的缓冲区
        outputBuffer_ += message;

        std::cout
            << "client fd=" << fd_
            << ", message: " << message
            << "\n";

        // 尝试发送
        handleWrite();
    }
    else if (n == 0)
    {
        std::cout << "client fd=" << fd_
                  << " disconnected\n";

        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("recv");

        if (closeCallback_)
        {
            closeCallback_();
        }
    }
}



void TcpConnection::handleWrite()
{
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
            if (closeCallback_)
            {
                closeCallback_();
            }
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
            loop_.updateChannel(channel_);
        }
    }
    else
    {
        // 数据已经全部发送完
        if (channel_->isWriting())
        {
            channel_->disableWriting();
            loop_.updateChannel(channel_);
        }
    }
}
