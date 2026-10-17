#include"EventLoop.h"

#include<iostream>
#include<stdexcept>
#include<utility>

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
            std::cout<<"fd"<<fd<<":active"<<"\n";
            auto it=channels.find(fd);
            if(it!=channels.end())
            {
                it->second->handleEvent();
            }
        }
        std::vector<std::function<void()>> callbacks;
        callbacks.swap(pendingCallbacks);
        for(auto&callback:callbacks)
        {
            callback();
        }
    }
}

void EventLoop::removeChannel(int fd)
{
    auto it=channels.find(fd);
    if(it!=channels.end())
    {
        epoll.removeFd(fd);
        channels.erase(it);
    }
}

void EventLoop::queueInLoop(std::function<void()> cb)
{
    pendingCallbacks.push_back(std::move(cb));
}

void EventLoop::addChannel(Channel*channel)
{
    int fd=channel->fd();
    if(channels.find(fd)!=channels.end())
    {
        throw std::runtime_error("channel already exists");
    }
    channels[fd]=channel;
    try
    {
        epoll.addFd(fd,EPOLLIN);
    }
    catch(...)
    {
        channels.erase(fd);
        throw;
    }

}