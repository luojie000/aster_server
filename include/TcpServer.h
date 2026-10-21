#pragma once 
#include"EventLoop.h"
#include"Channel.h"
#include"TcpConnection.h"

#include<memory>
#include<unordered_map>

class TcpServer{
    private:
   std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
   EventLoop& loop_;
   int server_fd;
   int port_;
   Channel*handleAccept_;
   std::unordered_map<int,std::unique_ptr<TcpConnection>>  connections_;

   void initSocket();
   void handleAccept();
    public:
TcpServer(const TcpServer&) = delete;
TcpServer& operator=(const TcpServer&) = delete;

    TcpServer(EventLoop&loop,int port);
    ~TcpServer();
    void start();

};