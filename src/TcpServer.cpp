#include "TcpServer.h"
#include"setNotBlocking.h"
#include <iostream>
#include <unistd.h>
#include<cstdio>
#include<cerrno>
#include<memory>
#include<utility>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<stdexcept>

TcpServer::TcpServer(EventLoop&loop,int port):loop_(loop),server_fd(-1),port_(port),handleAccept_(nullptr){}


TcpServer::~TcpServer()
{
    lifetime_.reset();

    for(auto&connection:connections_)
    {
        loop_.removeChannel(connection.first);
    }
    if(server_fd!= -1)
    {
        loop_.removeChannel(server_fd);
        close(server_fd);
    }


    delete handleAccept_;

}


void TcpServer::initSocket()
{
    //server_fd=socket(AF_INET,SOCK_STREAM,0)
    /*
    if(server_Fd==-1)
    {
    }
    int opt=1;
    setsockopt(server_fd,sol_soket,so_reuseaddr,&opt,sizeof(opt))
    
    */
  server_fd =
        socket(AF_INET,
            SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,
            0
        );


    if(server_fd == -1)
    {
        throw std::runtime_error(
            "socket failed"
        );
    }

setNotBlocking(server_fd);

    int opt = 1;


    if (setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    ) == -1) throw std::runtime_error("setsockopt failed");

/*

sockaddr_in addr{}
addr.sin_family=af_inet;
addr.sin_port=htons(8080)
addr.sin_addr.s_addr=htonl(inaddr_loopback)
*/


    sockaddr_in addr{};


    addr.sin_family =
        AF_INET;


    addr.sin_port =
        htons(port_);



    addr.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

/*
if(bind(server_Fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==-1)
{}

if(listen(server_fd,10)==-1)
{
}
*/


    if(
        bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        )
        == -1
    )
    {
        throw std::runtime_error(
            "bind failed"
        );
    }



    if(
        listen(
            server_fd,
            10
        )
        == -1
    )
    {
        throw std::runtime_error(
            "listen failed"
        );
    }

}


void TcpServer::start()
{
    if (server_fd != -1) throw std::runtime_error("server already started");
    initSocket();

    handleAccept_=new Channel(server_fd);

    handleAccept_->setReadCallback([this](){
        handleAccept();
    });

    loop_.addChannel(handleAccept_);
}


void TcpServer::handleAccept()
{
   while(true)
   {
    int client_fd=accept4(server_fd,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC);
    if(client_fd==-1)
    {
        if(errno==EAGAIN||errno==EWOULDBLOCK)
        {
            //当前连接队列为空
            break;

        }
        if(errno==EINTR)
        {
            continue;
        }
        perror("accept");
        break;
    }

    std::unique_ptr<TcpConnection> connection;
    try
    {
        connection=std::make_unique<TcpConnection>(client_fd,loop_);
    }
    catch(...)
    {
        close(client_fd);
        throw;
    }
    connection->setCloseCallback([this,client_fd](){
        loop_.removeChannel(client_fd);
        loop_.queueInLoop([this,client_fd,alive=std::weak_ptr<int>(lifetime_)](){
            if (alive.expired()) return;
            connections_.erase(client_fd);
            std::cout<<"client_fd is closed\n";
        });
    });
    //get the channel
    Channel* client_channel=connection->channel();
    //注册到eventloop
    loop_.addChannel(client_channel);
    //保存连接对象
    try
    {
        connections_[client_fd]=std::move(connection);
    }
    catch(...)
    {
        loop_.removeChannel(client_fd);
        throw;
    }
    std::cout<<"new client:"<<client_fd<<"\n";
   }
}