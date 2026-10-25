#pragma once
#include<string>
#include <cstdint>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>

enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error
};


struct LogItem
{
    LogLevel level;
    std::string message;
};

class Logger
{
public:
    static Logger& instance();

    void log(LogLevel level, std::string message);

    // Owner-thread shutdown only: first stop/join all producers, then call stop.
    // Sequential repeated calls are safe; concurrent stop calls are unsupported.
    void stop();

    uint64_t droppedCount() const;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::deque<LogItem> queue_;

    std::size_t queuedBytes_ = 0;

    std::size_t dropped_ = 0;

    bool stopping_ = false;

    std::thread worker_;

    static constexpr std::size_t kMaxQueueEntries = 4096;
    static constexpr std::size_t kMaxQueueBytes = 1024 * 1024;
    static constexpr std::size_t kMaxMessageBytes = 8192;
};