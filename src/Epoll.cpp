#include"Epoll.h"
#include<unistd.h>
#include<cstring>
#include<stdexcept>
#include<cerrno>

Epoll::Epoll()
{
    epoll_fd_=epoll_create1(EPOLL_CLOEXEC);
    if(epoll_fd_==-1)
    {
        throw std::runtime_error("epoll_create failed");
    }
}

Epoll::~Epoll()
{
    close(epoll_fd_);
}

void Epoll::addFd(int fd,uint32_t events)
{
    epoll_event event{};
    event.events=events;
    event.data.fd=fd;
    if(epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,fd,&event)==-1)
    {
        throw std::runtime_error("epoll add failed");
    }
}

void Epoll::removeFd(int fd)
{
epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,fd,nullptr);
}

void Epoll::modifyFd(int fd, uint32_t events)
{
    epoll_event ev{};
    //epoll_event ev{};
    //ev.events=events;
    //ev.data.fd=fd;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_,EPOLL_CTL_MOD,fd, &ev) == -1)
    {
        throw std::runtime_error("epoll modify failed");
    }
}

int Epoll::wait(std::vector<epoll_event>&events,int timeout)
{
    events.resize(1024);
    int n;
    do
    {
        n=epoll_wait(epoll_fd_,events.data(),events.size(),timeout);
    }while(n==-1&&errno==EINTR);
    if(n<0)
    {
        throw std::runtime_error("epoll wait failed");
    }
    events.resize(n);
    return n;
}