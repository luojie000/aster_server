#pragma once
#include<sys/epoll.h>
#include<vector>

class Epoll
{
public:
Epoll();
~Epoll();

void addFd(int fd,uint32_t events);
void removeFd(int fd);


//wait
int wait(std::vector<epoll_event>&events,int timeout=-1);
private:
int epoll_fd;
};