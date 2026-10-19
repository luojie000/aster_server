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
std::vector<std::function<void()>> pendingCallbacks;

public:
EventLoop();
void run();


void addChannel(Channel* channel);
void updateChannel(Channel* channel);


void removeChannel(int fd);
void queueInLoop(std::function<void()> cb);
};