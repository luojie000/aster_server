#include "Logger.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

int main()
{
    // 在启动后台线程之前接管 clog，stop() 返回后才读取结果。
    std::ostringstream captured;
    auto* original = std::clog.rdbuf(captured.rdbuf());
    auto& logger = Logger::instance();
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << '\n';
        if (!ok) ++failures;
    };

    check(logger.droppedCount() == 0, "initial droppedCount = 0");
    logger.log(LogLevel::Debug, "debug message");
    logger.log(LogLevel::Info, "hello logger");
    logger.log(LogLevel::Warn, "warning message");
    logger.log(LogLevel::Error, "error message");
    logger.log(LogLevel::Info, std::string(9000, 'x'));

    // 不用 sleep 等待：stop 应排空已接收的日志，并等待后台线程退出。
    logger.stop();
    const std::string output = captured.str();
    check(logger.droppedCount() == 0, "accepted messages are not dropped");
    check(std::count(output.begin(), output.end(), '\n') == 5,
          "workerLoop outputs all 5 messages before stop returns");

    const char* expected[] = {
        "[DEBUG] debug message", "[INFO] hello logger",
        "[WARN] warning message", "[ERROR] error message"
    };
    std::istringstream lines(output);
    std::string line;
    bool ordered = true;
    for (const auto* message : expected) {
        std::getline(lines, line);
        // 时间戳占 19 字符，后面还有一个空格。
        ordered = ordered && line.size() >= 20 && line.substr(20) == message;
        std::cout << line << '\n';
    }
    check(ordered, "log levels, message contents and FIFO order");
    std::getline(lines, line);
    const std::string suffix = "...[truncated]";
    const std::string truncated = std::string(8192 - suffix.size(), 'x') + suffix;
    check(line.size() >= 20 && line.substr(20) == "[INFO] " + truncated,
          "oversized message is truncated to 8192 bytes");

    logger.stop();
    check(captured.str() == output && logger.droppedCount() == 0,
          "sequential stop calls are safe");
    logger.log(LogLevel::Info, "must not be printed");
    check(logger.droppedCount() == 1 && captured.str() == output,
          "log after stop is rejected and droppedCount becomes 1");

    // 后台线程已经退出，可以安全恢复 clog。
    std::clog.rdbuf(original);
    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << '\n';
    return failures == 0 ? 0 : 1;
}
