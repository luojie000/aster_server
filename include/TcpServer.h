#pragma once 
#include"EventLoop.h"
#include"Channel.h"

class TcpServer{
    private:
   EventLoop& loop_;
   int server_fd;
   int port_;
   Channel*handleAccept_;

   void initSocket();
   void handleAccept();
    public:

    TcpServer(EventLoop&loop,int port);
    ~TcpServer();
    void start();

};