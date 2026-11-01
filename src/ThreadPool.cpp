
#include "ThreadPool.h"

#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(std::size_t threadCount)
{
    if (threadCount == 0)
    {
        throw std::invalid_argument(
            "ThreadPool requires at least one worker"
        );
    }

    try
    {
        for (std::size_t i = 0; i < threadCount; ++i)
        {
            workers_.emplace_back(
                [this]()
                {
                    workerLoop();
                }
            );
        }
    }
    catch (...)
    {
        // 如果创建部分线程后失败，通知已创建的线程退出
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }

        condition_.notify_all();

        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        throw;
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // 没有任务且尚未停止时，工作线程进入等待状态
            condition_.wait(
                lock,
                [this]()
                {
                    return stopping_ || !tasks_.empty();
                }
            );

            // 停止信号已发出，并且任务队列已清空
            if (stopping_ && tasks_.empty())
            {
                return;
            }

            // 从队列取出一个任务
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        // 不持有队列锁执行任务
        // 用户任务抛出的异常由 packaged_task 保存到 future
        task();
    }
}

void ThreadPool::stop()
{
    // 串行化多个外部线程的 stop() 调用
    std::lock_guard<std::mutex> stopLock(stopMutex_);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    // 唤醒所有工作线程，让它们检查停止状态
    condition_.notify_all();

    // 等待所有工作线程完成任务并退出
    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers_.clear();
}
