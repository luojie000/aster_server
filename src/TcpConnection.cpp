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
    size_t received = 0;

    while (true)
    {
        // 缓冲区已经装满，先处理已有数据
        if (received == sizeof(buffer))
        {
            outputBuffer_.append(buffer, received);
            received = 0;
        }

        ssize_t n = recv(
            fd_,
            buffer + received,
            sizeof(buffer) - received,
            0
        );

        if (n > 0)
        {
            // 累计本次成功读取的字节数
            received += static_cast<size_t>(n);
            continue;
        }

        if (n == 0)
        {
            // 保存本轮已经读取但尚未追加的数据
            if (received > 0)
            {
                outputBuffer_.append(buffer, received);
            }

            std::cout
                << "client fd=" << fd_
                << " disconnected\n";

            if (closeCallback_)
            {
                closeCallback_();
            }

            return;
        }

        // n == -1

        if (errno == EINTR)
        {
            // 系统调用被信号中断，重新读取
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 非阻塞 Socket 当前没有更多数据
            break;
        }

        // 其他错误
        perror("recv");

        if (received > 0)
        {
            outputBuffer_.append(buffer, received);
        }

        if (closeCallback_)
        {
            closeCallback_();
        }

        return;
    }

    // 保存本轮最后剩余的数据
    if (received > 0)
    {
        outputBuffer_.append(buffer, received);
    }

    std::cout
        << "client fd=" << fd_
        << ", pending output: "
        << outputBuffer_.size()
        << " bytes\n";

    // 读取完成后，尝试发送积累的数据
    if (!outputBuffer_.empty())
    {
        handleWrite();
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
  