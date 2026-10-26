#pragma once
#include"Epoll.h"
#include"Channel.h"
#include<functional>
#include<unordered_map>
#include<mutex>

class EventLoop{
private:
Epoll epoll;
bool running;

int wakeupFd_;
std::mutex pendingMutex_;

std::unordered_map<int,Channel*> channels;
std::vector<std::function<void()>> pendingCallbacks;

public:
EventLoop(const EventLoop&) = delete;
~EventLoop();
EventLoop& operator=(const EventLoop&) = delete;
EventLoop();
void run();
// run(), stop() and Channel operations belong to the loop thread.
// queueInLoop() is thread-safe; its callbacks execute on the loop thread.
void stop() { running = false; }


void addChannel(Channel* channel);
void updateChannel(Channel* channel);


void removeChannel(int fd);
void queueInLoop(std::function<void()> cb);
};