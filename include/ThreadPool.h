
#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(std::size_t threadCount);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务，并返回用于获取结果的 future
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ResultType = std::invoke_result_t<F, Args...>;

        // packaged_task 将函数包装成可异步执行的任务
        auto task = std::make_shared<
            std::packaged_task<ResultType()>
        >(
            std::bind(
                std::forward<F>(f),
                std::forward<Args>(args)...
            )
        );

        // 提前取得结果通道
        std::future<ResultType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_)
            {
                throw std::runtime_error(
                    "submit on stopped ThreadPool"
                );
            }

            // 队列中存放统一类型的无参数任务
            tasks_.emplace([task]()
            {
                (*task)();
            });
        }

        // 通知一个等待任务的工作线程
        condition_.notify_one();

        return result;
    }

    // 停止接收任务，完成已接收的任务并等待工作线程退出
    void stop();

private:
    void workerLoop();

    std::vector<std::thread> workers_;

    std::queue<std::function<void()>> tasks_;

    std::mutex mutex_;

    std::condition_variable condition_;

    // 只能在 mutex_ 保护下读写
    bool stopping_ = false;

    // 防止多个外部线程同时执行 stop() 和 join()
    std::mutex stopMutex_;
};
