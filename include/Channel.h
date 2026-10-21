#pragma once
#include<functional>
#include<cstdint>
#include<sys/epoll.h>


class Channel
{
    public:
    uint32_t events()const;
    void enableWriting();
    void disableReading() { events_ &= ~(EPOLLIN | EPOLLRDHUP); }
    void disableWriting();
    bool isWriting()const;

    using EventCallback=std::function<void()>;
    Channel(int fd);
    int fd()const;
    void setReadCallback(EventCallback cb);
    void setWriteCallback(EventCallback cb);
    void handleEvent(uint32_t revents);


    private:
    int fd_;
    uint32_t events_=EPOLLIN|EPOLLRDHUP;
    EventCallback readCallback_;
    EventCallback writeCallback_;
};