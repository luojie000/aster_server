#include "TcpServer.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<stdexcept>

TcpServer::TcpServer(EventLoop&loop,int port):loop_(loop),port_(port),server_fd(-1),handleAccept_(nullptr){}


TcpServer::~TcpServer()
{

    if(server_fd!= -1)
    {
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
            SOCK_STREAM,
            0
        );


    if(server_fd == -1)
    {
        throw std::runtime_error(
            "socket failed"
        );
    }



    int opt = 1;


    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

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
    initSocket();

    handleAccept_=new Channel(server_fd);

    handleAccept_->setReadCallback([this](){
        handleAccept();
    });

    loop_.addChannel(handleAccept_);
}


void TcpServer::handleAccept()
{
    int client_fd=accept(server_fd,nullptr,nullptr);
    if(client_fd==-1)
    {
        std::cerr<<"client failed\n";
        return ;
    }
    std::cout<<"new client:"<<client_fd<<"\n";
}