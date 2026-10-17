#pragma once 
#include"EventLoop.h"
#include"Channel.h"
#include"TcpConnection.h"

#include<memory>
#include<unordered_map>

class TcpServer{
    private:
   EventLoop& loop_;
   int server_fd;
   int port_;
   Channel*handleAccept_;
   std::unordered_map<int,std::unique_ptr<TcpConnection>>  connections_;

   void initSocket();
   void handleAccept();
    public:

    TcpServer(EventLoop&loop,int port);
    ~TcpServer();
    void start();

};