#include "TcpServer.h"
#include"setNotBlocking.h"
#include "Logger.h"
#include <string>
#include <unistd.h>
#include<cstdio>
#include<cerrno>
#include<memory>
#include<utility>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<stdexcept>

TcpServer::TcpServer(EventLoop&loop,int port, RequestProcessor::Handler processor)
    : loop_(loop), server_fd(-1), port_(port), handleAccept_(nullptr),
      processor_(std::move(processor))
{
    if (processor_) workers_ = std::make_unique<ThreadPool>(2);
}


TcpServer::~TcpServer()
{
    // Stop accepting connections before waiting for business work. Otherwise
    // clients can still connect into the listening backlog during a slow join.
    if (server_fd != -1) {
        loop_.removeChannel(server_fd);
        close(server_fd);
        server_fd = -1;
    }
    delete handleAccept_;
    handleAccept_ = nullptr;
    lifetime_.reset();
    // Workers only enqueue results; they never wait for the loop. Join before
    // destroying connections, and crucially before the owner destroys EventLoop.
    if (workers_) workers_->stop();

    for(auto&connection:connections_)
    {
        loop_.removeChannel(connection.first);
    }

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
    if (port_ < 0 || port_ > 65535) throw std::invalid_argument("port must be 0..65535");
    try
    {
    initSocket();

    handleAccept_=new Channel(server_fd);

    handleAccept_->setReadCallback([this](){
        handleAccept();
    });

    loop_.addChannel(handleAccept_);
    sockaddr_in bound{};
    socklen_t length = sizeof(bound);
    if (getsockname(server_fd, reinterpret_cast<sockaddr*>(&bound), &length) == -1)
        throw std::runtime_error("getsockname failed");
    port_ = ntohs(bound.sin_port);
    }
    catch (...)
    {
        loop_.removeChannel(server_fd);
        delete handleAccept_;
        handleAccept_ = nullptr;
        if (server_fd != -1) close(server_fd);
        server_fd = -1;
        throw;
    }
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
        const int error = errno;
        Logger::instance().log(LogLevel::Error, "accept failed errno=" + std::to_string(error));
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
            Logger::instance().log(LogLevel::Info, "connection closed fd=" + std::to_string(client_fd));
        });
    });
    if (processor_) {
        connection->setRequestCallback(
            [this, client_fd, token = connection->lifetimeToken()](std::string request) {
                submitRequest(client_fd, token, std::move(request));
            });
    }
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
    Logger::instance().log(LogLevel::Info, "connection accepted fd=" + std::to_string(client_fd));
   }
}

void TcpServer::submitRequest(int fd, std::weak_ptr<int> connectionLife,
                              std::string request)
{
    // This closure may be carried by a worker, but only EXECUTES in EventLoop.
    // Never capture a TcpConnection*. fd alone is insufficient: it can be reused.
    auto deliver = [this, alive = std::weak_ptr<int>(lifetime_), connectionLife, fd]
                   (std::string response, bool counted) {
        if (alive.expired()) return;
        if (counted) --pendingRequests_;
        const auto token = connectionLife.lock();
        if (!token) return;
        const auto it = connections_.find(fd);
        if (it == connections_.end() || it->second->lifetimeToken().lock() != token)
            return;
        it->second->completeRequest(std::move(response));
    };
    if (pendingRequests_ >= kMaxPendingRequests) {
        // Still deferred: never reenter TcpConnection from its request callback.
        loop_.queueInLoop([deliver] { deliver("ERR busy\n", false); });
        return;
    }
    ++pendingRequests_;
    try {
        workers_->submit([request = std::move(request), processor = processor_,
                          loop = &loop_, deliver] {
            std::string response;
            try {
                response = processor(request);
                if (response.size() > TcpConnection::kResponseLimit)
                    response = "ERR response too large\n";
            } catch (...) {
                response = "ERR processing\n";
            }
            loop->queueInLoop([deliver, response = std::move(response)]() mutable {
                deliver(std::move(response), true);
            });
        });
    } catch (...) {
        --pendingRequests_;
        loop_.queueInLoop([deliver] { deliver("ERR unavailable\n", false); });
    }
}
