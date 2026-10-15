#pragma once
#include<functional>


class Channel
{
    public:
    using EventCallback=std::function<void()>;
    Channel(int fd);
    int fd()const;
    void setReadCallback(EventCallback cb);
    void handleEvent();


    private:
    int fd_;
    EventCallback readCallback_;
};