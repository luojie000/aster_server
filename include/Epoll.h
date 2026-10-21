#pragma once
#include<sys/epoll.h>
#include<vector>

class Epoll
{
public:
Epoll(const Epoll&) = delete;
Epoll& operator=(const Epoll&) = delete;
Epoll();
~Epoll();

void addFd(int fd,uint32_t events);
void removeFd(int fd);
void modifyFd(int fd,uint32_t events);


//wait
int wait(std::vector<epoll_event>&events,int timeout=-1);
private:
int epoll_fd_;
};