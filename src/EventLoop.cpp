#include"EventLoop.h"

#include<iostream>
#include<unistd.h>
#include<stdexcept>
#include<utility>
#include <sys/eventfd.h>

EventLoop::EventLoop():running(false),wakeupFd_(-1){
    wakeupFd_=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(wakeupFd_==-1)
    {
        throw std::runtime_error("eventfd create failed");
    }

    try{
        epoll.addFd(wakeupFd_,EPOLLIN);
    }
    catch(...)
    {
        close(wakeupFd_);
        wakeupFd_=-1;
        throw;
    }
    /*
     // 创建用于唤醒 EventLoop 的 eventfd
    wakeupFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    if (wakeupFd_ == -1)
    {
        throw std::runtime_error("eventfd create failed");
    }

    // 将 eventfd 注册到 epoll
    try
    {
        epoll.addFd(wakeupFd_, EPOLLIN);
    }
    catch (...)
    {
        close(wakeupFd_);
        wakeupFd_ = -1;
        throw;
    }
        */
}


EventLoop::~EventLoop()
{
  if (wakeupFd_ != -1)
    {
        epoll.removeFd(wakeupFd_);
        close(wakeupFd_);
    }   
}

void EventLoop::run()
{
    running=true;
    while(running)
    {
        std::vector<epoll_event> events;
        epoll.wait(events,-1);
        for(auto&event:events)
        {
            int fd=event.data.fd;

            if (fd == wakeupFd_)
            {
                while (true)
                {
                    std::uint64_t counter = 0;

                    ssize_t n = read(
                        wakeupFd_,
                        &counter,
                        sizeof(counter)
                    );

                    // 读取成功：eventfd 计数器已经清零。
                    // 继续检查是否又有新的通知。
                    if (n == sizeof(counter))
                    {
                        continue;
                    }

                    // 被信号中断，重新读取
                    if (n == -1 && errno == EINTR)
                    {
                        continue;
                    }

                    // 当前没有更多通知可读
                    if (n == -1 &&
                        (errno == EAGAIN ||
                         errno == EWOULDBLOCK))
                    {
                        break;
                    }

                    // 其他异常或非预期的读取结果
                    throw std::runtime_error(
                        "eventfd read failed"
                    );
                }

                // eventfd 不是普通的客户端 Channel
                continue;
            }




            auto it=channels.find(fd);
            if(it!=channels.end())
            {
                it->second->handleEvent(event.events);
            }
        }
        std::vector<std::function<void()>> callbacks;

        {
            // 保护 pendingCallbacks，避免多线程数据竞争
            std::lock_guard<std::mutex> lock(
                pendingMutex_
            );

            // 将共享队列中的任务转移到本地队列
            callbacks.swap(pendingCallbacks);
        }

       
        for(auto&callback:callbacks)
        {
            callback();
        }
    }
}

void EventLoop::removeChannel(int fd)
{
    auto it=channels.find(fd);
    if(it!=channels.end())
    {
        epoll.removeFd(fd);
        channels.erase(it);
    }
}

void EventLoop::updateChannel(Channel*channel)
{
    if(channel==nullptr)
    {
        return ;
    }
    int fd=channel->fd();
    auto it=channels.find(fd);
    if(it==channels.end()||it->second!=channel)
    {
        return;
    }
    epoll.modifyFd(fd,channel->events());
}

void EventLoop::queueInLoop(
    std::function<void()> cb
)
{
    // 第一步：将回调安全地放入任务队列
    {
        std::lock_guard<std::mutex> lock(
            pendingMutex_
        );

        pendingCallbacks.push_back(
            std::move(cb)
        );
    }

    // 第二步：向 eventfd 写入通知
    const std::uint64_t one = 1;

    while (true)
    {
        ssize_t n = write(
            wakeupFd_,
            &one,
            sizeof(one)
        );

        // 写入成功，通知已发送
        if (n == sizeof(one))
        {
            break;
        }

        // 被信号中断，重新尝试
        if (n == -1 && errno == EINTR)
        {
            continue;
        }

        // eventfd 计数器已满，说明它已经可读，
        // 不需要再写入一次通知
        if (n == -1 &&
            (errno == EAGAIN ||
             errno == EWOULDBLOCK))
        {
            break;
        }

        // 其他写入错误
        throw std::runtime_error(
            "eventfd write failed"
        );
    }
}
void EventLoop::addChannel(Channel*channel)
{
    int fd=channel->fd();
    if(channels.find(fd)!=channels.end())
    {
        throw std::runtime_error("channel already exists");
    }
    channels[fd]=channel;
    try
    {
        epoll.addFd(fd,channel->events());
    }
    catch(...)
    {
        channels.erase(fd);
        throw;
    }

}