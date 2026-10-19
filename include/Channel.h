#pragma once
#include<functional>


class Channel
{
    public:
    uint32_t events()const;
    void enableWriting();
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
    uint32_t events_=EPOLLIN;
    EventCallback readCallback_;
    EventCallback writeCallback_;
};