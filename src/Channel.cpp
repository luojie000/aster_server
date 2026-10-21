#include"Channel.h"
#include<utility>

Channel::Channel(int fd):fd_(fd){}

int Channel::fd() const
{
    return fd_;
}

uint32_t Channel::events()const
{
    return events_;
}

void Channel::enableWriting()
{
    events_|=EPOLLOUT;
}

void Channel::disableWriting()
{
    events_&=~EPOLLOUT;
}

bool Channel::isWriting()const
{
    return (events_&EPOLLOUT)!=0;
}

void Channel::setReadCallback(EventCallback cb)
{
    readCallback_=std::move(cb);
}

void Channel::setWriteCallback(EventCallback cb)
{
    writeCallback_=std::move(cb);
}


void Channel::handleEvent(uint32_t revents)
{
    if ((revents & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) && readCallback_)
    {
        readCallback_();
    }

    if ((revents & EPOLLOUT) && writeCallback_)
    {
        writeCallback_();
    }
}
