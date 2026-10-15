#include"Epoll.h"
#include<unistd.h>
#include<cstring>
#include<stdexcept>

Epoll::Epoll()
{
    epoll_fd=epoll_create(1);
    if(epoll_fd==-1)
    {
        throw std::runtime_error("epoll_create failed");
    }
}

Epoll::~Epoll()
{
    close(epoll_fd);
}

void Epoll::addFd(int fd,uint32_t events)
{
    epoll_event event{};
    event.events=events;
    event.data.fd=fd;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event)==-1)
    {
        throw std::runtime_error("epoll add failed");
    }
}

void Epoll::removeFd(int fd)
{
epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,nullptr);
}

int Epoll::wait(std::vector<epoll_event>&events,int timeout)
{
    events.resize(1024);
    int n=epoll_wait(epoll_fd,events.data(),events.size(),timeout);
    if(n<0)
    {
        throw std::runtime_error("epoll wait failed");
    }
    events.resize(n);
    return n;
}