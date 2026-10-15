#pragma once
#include"Epoll.h"
#include"Channel.h"
#include<functional>
#include<unordered_map>

class EventLoop{
private:
Epoll epoll;
bool running;
std::unordered_map<int,Channel*> channels;

public:
EventLoop();
void run();
void addChannel(Channel* channel);
};