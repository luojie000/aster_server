#include"Channel.h"

Channel::Channel(int fd):fd_(fd){}

int Channel::fd() const
{
    return fd_;
}

void Channel::setReadCallback(EventCallback cb)
{
    readCallback_=std::move(cb);
}

void Channel::handleEvent()
{
    if(readCallback_)
    {
        readCallback_();
    }
}