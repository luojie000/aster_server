#include "Logger.h"


#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace
{
const char* levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";

    case LogLevel::Info:
        return "INFO";

    case LogLevel::Warn:
        return "WARN";

    case LogLevel::Error:
        return "ERROR";
    }

    return "UNKNOWN";
}
}



Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger(): worker_(&Logger::workerLoop, this){}

Logger::~Logger()
{
    stop();
}



void Logger::log(LogLevel level, std::string message)
{
    // 1. 限制单条日志的长度
    constexpr char suffix[] = "...[truncated]";
    constexpr std::size_t suffixBytes = sizeof(suffix) - 1;

    if (message.size() > kMaxMessageBytes)
    {
        message.resize(kMaxMessageBytes - suffixBytes);
        message += suffix;
    }

    // 2. 检查队列并添加日志
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Logger 正在停止，不再接收新日志
        if (stopping_)
        {
            ++dropped_;
            return;
        }

        // 检查队列条数和总正文大小
        if (queue_.size() >= kMaxQueueEntries ||
            message.size() > kMaxQueueBytes - queuedBytes_)
        {
            ++dropped_;
            return;
        }

        // 记录这条日志的正文大小
        const std::size_t messageBytes = message.size();

        // 将日志放入队列
        queue_.push_back(
            LogItem{level, std::move(message)}
        );

        // 更新等待队列的正文大小
        queuedBytes_ += messageBytes;
    }

    // 3. 释放锁之后，通知后台线程
    cv_.notify_one();
}



void Logger::workerLoop()
{
    while(true)
    {
        std::deque<LogItem> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock,[this](){
                return stopping_||!queue_.empty();
            });
            if(stopping_&&queue_.empty())
            return;
        batch.swap(queue_);
        queuedBytes_=0;
        }

//开始处理输出
        for (const auto& item : batch)
{
    const auto now = std::chrono::system_clock::now();

    const std::time_t time =std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

    if (localtime_r(&time, &localTime) == nullptr)
    {
        localTime = {};
    }

    std::ostringstream line;

    line << std::put_time(
                &localTime,
                "%Y-%m-%d %H:%M:%S")
         << " [" << levelToString(item.level) << "] "
         << item.message << '\n';

    std::clog << line.str();
}

std::clog.flush();
    }
}



void Logger::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    cv_.notify_all();

    if (worker_.joinable())
    {
        worker_.join();
    }
}



uint64_t Logger::droppedCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
}
