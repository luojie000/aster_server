#include"Channel.h"
#include<utility>

Channel::Channel(int fd):fd_(fd){}

int Channel::fd() const
{
    return fd_;
}

void Channel::events()const
{
    return events_;
}

void Channel::enableWriting()
{
    event_|=EPOLLOUT;
}

void Channel::disableWriting()
{
    events_&=~EPOLLOUT;
}

void Channel::isWriting()const
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
    if (revents & (EPOLLERR | EPOLLHUP))
    {
        // 错误和挂断事件需要单独处理，
        // 后续增加对应的错误/关闭回调。
    }

    if ((revents & (EPOLLIN | EPOLLRDHUP)) && readCallback_)
    {
        readCallback_();
    }

    if ((revents & EPOLLOUT) && writeCallback_)
    {
        writeCallback_();
    }
}
