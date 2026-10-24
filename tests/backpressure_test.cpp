#include "TcpConnection.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cerrno>
void require(bool b, const char* m) { if (!b) throw std::runtime_error(m); }
struct Peer {
    int pair[2];
    std::unique_ptr<TcpConnection> connection;
    Peer(EventLoop& loop) {
        require(socketpair(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0,pair)==0,"socketpair");
        int size=4096;
        require(setsockopt(pair[0],SOL_SOCKET,SO_SNDBUF,&size,sizeof(size))==0,"sndbuf");
        connection=std::make_unique<TcpConnection>(pair[0],loop);
        loop.addChannel(connection->channel());
    }
    ~Peer() { connection.reset(); close(pair[1]); }
};
void tick(EventLoop& loop) { loop.queueInLoop([&]{loop.stop();}); loop.run(); }
void echo(EventLoop& loop, Peer& peer) {
    require(send(peer.pair[1],"hello",5,MSG_NOSIGNAL)==5,"normal send");
    std::string received;
    for(int i=0;i<1000 && received.size()<5;++i) {
        tick(loop); char b[5]; auto n=recv(peer.pair[1],b,sizeof(b),0);
        if(n>0) received.append(b,n);
    }
    require(received=="hello","normal echo failed");
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"case required");
        int mode=std::stoi(argv[1]);
        std::cout.setstate(std::ios::failbit); // suppress only the test's payload logging
        EventLoop loop;
        Peer slow(loop);
        if(mode==1) { echo(loop,slow); }
        else {
            std::string payload(4096,'x'); size_t sent=0,received=0,peak=0;
            for(int i=0;i<20000 && !slow.connection->readPaused();++i) {
                auto n=send(slow.pair[1],payload.data(),payload.size(),MSG_NOSIGNAL);
                if(n>0) sent+=n;
                else require(errno==EAGAIN || errno==EWOULDBLOCK,"fill send failed");
                tick(loop);
                peak=std::max(peak,slow.connection->pendingOutputBytes());
                require(peak<=TcpConnection::kOutputLimit,"hard limit exceeded");
            }
            require(slow.connection->readPaused(),"backpressure not reached");
            require(!(slow.connection->channel()->events() & EPOLLIN),"read interest still enabled");
            for(int i=0;i<1000;++i) {
                auto n=send(slow.pair[1],payload.data(),payload.size(),MSG_NOSIGNAL);
                if(n>0) sent+=n;
                tick(loop);
                require(slow.connection->pendingOutputBytes()<=TcpConnection::kOutputLimit,"continued accumulation exceeded limit");
                require(slow.connection->readPaused(),"premature resume");
            }
            if(mode==4) { Peer normal(loop); echo(loop,normal); require(slow.connection->readPaused(),"slow client no longer paused"); }
            if(mode==3) {
                for(int i=0;i<100000 && (received<sent || slow.connection->readPaused());++i) {
                    char b[16384]; auto n=recv(slow.pair[1],b,sizeof(b),0);
                    if(n>0) { require(std::string(b,n)==std::string(n,'x'),"data corrupted"); received+=n; }
                    tick(loop);
                    require(slow.connection->pendingOutputBytes()<=TcpConnection::kOutputLimit,"recovery exceeded limit");
                }
                require(received==sent,"recovery lost data");
                require(!slow.connection->readPaused(),"backpressure not released");
                require(slow.connection->channel()->events() & EPOLLIN,"read interest not restored");
                echo(loop,slow);
            }
            std::cerr<<"peak application pending bytes="<<peak<<" limit="<<TcpConnection::kOutputLimit<<"\n";
        }
        std::cerr<<"PASS backpressure case "<<mode<<"\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
