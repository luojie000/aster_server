#include"EventLoop.h"

#include<iostream>

EventLoop::EventLoop():running(false){}

void EventLoop::run()
{
    running=true;
    while(running)
    {
        std::vector<epoll_event> events;
        int n=epoll.wait(events);
        std::cout<<"events:"<<n<<"\n";
        for(auto&event:events)
        {
            int fd=event.data.fd;
            std::cout<<"fd:"<<fd<<"active"<<"\n";
            auto it=channels.find(fd);
            if(it!=channels.end())
            {
                it->second->handleEvent();
            }
        }
    }
}

void EventLoop::addChannel(Channel*channel)
{
    int fd=channel->fd();
    channels[fd]=channel;
    epoll.addFd(fd,EPOLLIN);

}