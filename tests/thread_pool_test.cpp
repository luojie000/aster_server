
#include "ThreadPool.h"

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    constexpr int workerCount = 4;
    constexpr int producerCount = 4;
    constexpr int tasksPerProducer = 250;
    constexpr int totalTasks =
        producerCount * tasksPerProducer;

    ThreadPool pool(workerCount);

    std::atomic<int> completed{0};

    std::mutex futuresMutex;
    std::vector<std::future<int>> futures;

    std::vector<std::thread> producers;

    // 多个线程同时提交任务
    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back([&, p]()
        {
            for (int i = 0; i < tasksPerProducer; ++i)
            {
                auto future = pool.submit([&, p, i]()
                {
                    completed.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                    return p * tasksPerProducer + i;
                });

                std::lock_guard<std::mutex> lock(futuresMutex);

                futures.push_back(std::move(future));
            }
        });
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    // 确认所有任务的 future 都已经保存
    if (static_cast<int>(futures.size()) != totalTasks)
    {
        std::cerr << "Incorrect future count\n";
        return 1;
    }

    // 验证每个任务都返回了正确且唯一的编号
    std::vector<bool> seen(totalTasks, false);

    for (auto& future : futures)
    {
        const int result = future.get();

        if (result < 0 || result >= totalTasks)
        {
            std::cerr << "Invalid task result\n";
            return 1;
        }

        if (seen[result])
        {
            std::cerr << "Duplicate task result\n";
            return 1;
        }

        seen[result] = true;
    }

    if (completed.load() != totalTasks)
    {
        std::cerr << "Some tasks were lost\n";
        return 1;
    }

    // 验证任务异常能够通过 future 传递
    auto failedTask = pool.submit([]() -> int
    {
        throw std::runtime_error("expected test exception");
    });

    bool exceptionCaught = false;

    try
    {
        (void)failedTask.get();
    }
    catch (const std::runtime_error&)
    {
        exceptionCaught = true;
    }

    if (!exceptionCaught)
    {
        std::cerr << "Task exception was not propagated\n";
        return 1;
    }

    // 停止线程池
    pool.stop();

    // 停止后应拒绝新任务
    bool rejected = false;

    try
    {
        (void)pool.submit([]()
        {
            return 1;
        });
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }

    if (!rejected)
    {
        std::cerr << "Task accepted after shutdown\n";
        return 1;
    }

    std::cout
        << "PASS: "
        << totalTasks
        << " tasks completed, results verified, "
        << "exception propagated, shutdown verified\n";

    return 0;
}