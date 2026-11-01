#pragma once 
#include"EventLoop.h"
#include"Channel.h"
#include"TcpConnection.h"
#include "RequestProcessor.h"
#include "ThreadPool.h"

#include<memory>
#include<unordered_map>

class TcpServer{
    friend struct TcpServerTestAccess;
    private:
   std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
   EventLoop& loop_;
   int server_fd;
   int port_;
   Channel*handleAccept_;
   std::unordered_map<int,std::unique_ptr<TcpConnection>>  connections_;

   RequestProcessor::Handler processor_;
   std::unique_ptr<ThreadPool> workers_;
   std::size_t pendingRequests_ = 0; // loop-thread-only; includes queued results
   static constexpr std::size_t kMaxPendingRequests = 64;
   void submitRequest(int fd, std::weak_ptr<int> connectionLife, std::string request);

   void initSocket();
   void handleAccept();
    public:
TcpServer(const TcpServer&) = delete;
TcpServer& operator=(const TcpServer&) = delete;

    // Construct/use/destroy on the loop thread (or while the loop is stopped).
    // EventLoop must outlive this server. An empty processor preserves echo mode.
    TcpServer(EventLoop&loop,int port, RequestProcessor::Handler processor = {});
    ~TcpServer();
    void start();
    int port() const { return port_; }

};