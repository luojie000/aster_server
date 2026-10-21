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
EventLoop(const EventLoop&) = delete;
EventLoop& operator=(const EventLoop&) = delete;
EventLoop();
void run();
// Single-threaded API: callbacks and stop must be invoked on the loop thread.
void stop() { running = false; }


void addChannel(Channel* channel);
void updateChannel(Channel* channel);


void removeChannel(int fd);
void queueInLoop(std::function<void()> cb);
};