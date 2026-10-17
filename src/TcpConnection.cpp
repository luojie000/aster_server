#include"TcpConnection.h"

#include<iostream>
#include<string>
#include<utility>
#include<cerrno>
#include<cstdio>

#include<sys/socket.h>
#include<unistd.h>

TcpConnection::TcpConnection(int fd):fd_(fd),channel_(new Channel(fd))
{
channel_->setReadCallback([this](){
    handleRead();
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
        n=recv(fd_,buffer,sizeof(buffer),0);
    }while(n==-1&&errno==EINTR);
    if(n>0)
    {
        std::string message(buffer,n);
        std::cout<<"client fd="<<fd_<<",message:"<<message<<"\n";
    }
    else if(n==0)
    {
        //Z暂时只打印，后续完善连接清理
        std::cout<<"client fd="<<fd_<<" disconnection\n";
        if(closeCallback_)
        {
            closeCallback_();
        }
    }
    else
    {
        //非阻塞不一定是错误
        if(errno!=EAGAIN&&errno!=EWOULDBLOCK)
        {
            perror("recv");
            if(closeCallback_)
            {
                closeCallback_();
            }
        }
    }
}