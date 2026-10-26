#include "EventLoop.h"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;
constexpr int kBatches = 2;
constexpr int kBatchSize = 64;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void hardTimeout(int) {
    // Only async-signal-safe operations: even a deadlocked join is bounded.
    constexpr char message[] = "FAIL hard timeout (10s): wakeup or thread join stalled\n";
    const auto ignored = ::write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)ignored;
    ::_exit(124);
}

void waitUntilBlocked(pid_t tid) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    const auto path = "/proc/self/task/" + std::to_string(tid) + "/syscall";
    while (std::chrono::steady_clock::now() < deadline) {
        // Linux reports the current syscall of a blocked thread, or "running".
        // Observe the real epoll_wait, rather than assuming a sleep was enough.
        std::ifstream state(path);
        require(state.is_open(), "cannot inspect loop thread syscall in /proc");
        long syscallNumber = -1;
        if ((state >> syscallNumber) && syscallNumber == SYS_epoll_wait) return;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("loop thread did not block in epoll_wait within 2s");
}
} // namespace

int main() {
    struct sigaction action {};
    action.sa_handler = hardTimeout;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, nullptr) != 0) return 1;
    alarm(10); // Also protects direct execution outside CTest.

    try {
        EventLoop loop;
        std::array<std::atomic<int>, kBatches * kBatchSize> calls {};
        for (auto& count : calls) count.store(0);
        std::atomic<bool> wrongThread {false};
        std::atomic<bool> stopCalled {false};
        std::exception_ptr loopError;
        std::exception_ptr workerError;
        std::promise<pid_t> started;
        auto ready = started.get_future();

        std::thread loopThread([&] {
            try {
                started.set_value(static_cast<pid_t>(::syscall(SYS_gettid)));
                loop.run();
            } catch (...) {
                loopError = std::current_exception();
            }
        });
        const auto loopId = loopThread.get_id();

        // Catch every setup/worker error before stopping and joining the loop.
        try {
            const auto tid = ready.get();
            std::thread worker([&] {
                try {
                    for (int batch = 0; batch < kBatches; ++batch) {
                        waitUntilBlocked(tid);
                        auto completed = std::make_shared<std::promise<void>>();
                        auto completion = completed->get_future();
                        for (int i = 0; i < kBatchSize; ++i) {
                            const int index = batch * kBatchSize + i;
                            loop.queueInLoop([&, index] {
                                if (std::this_thread::get_id() != loopId) wrongThread = true;
                                ++calls[index];
                            });
                        }
                        // The marker follows this worker's batch in the FIFO queue.
                        // Shared ownership keeps it alive if the worker times out.
                        loop.queueInLoop([&, completed] {
                            if (std::this_thread::get_id() != loopId) wrongThread = true;
                            completed->set_value();
                        });
                        require(completion.wait_for(2s) == std::future_status::ready,
                                "queued callbacks did not finish within 2s");
                        completion.get();
                        for (int i = 0; i < kBatchSize; ++i) {
                            require(calls[batch * kBatchSize + i] == 1,
                                    "callback missing, duplicated, or batch marker ran too early");
                        }
                    }
                    // Verify the notification was drained and the loop can idle again.
                    waitUntilBlocked(tid);
                } catch (...) {
                    workerError = std::current_exception();
                }
            });
            worker.join();
        } catch (...) {
            workerError = std::current_exception();
        }

        if (workerError) {
            try { std::rethrow_exception(workerError); }
            catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; }
            catch (...) { std::cerr << "FAIL unknown worker error\n"; }
        }
        try {
            // stop() is loop-thread-only; never write its running flag here.
            loop.queueInLoop([&] {
                if (std::this_thread::get_id() != loopId) {
                    wrongThread = true;
                    return;
                }
                stopCalled = true;
                loop.stop();
            });
        } catch (...) {
            if (!workerError) workerError = std::current_exception();
        }
        loopThread.join(); // The alarm bounds cleanup even if wakeup is broken.

        if (loopError) std::rethrow_exception(loopError);
        if (workerError) std::rethrow_exception(workerError);
        require(!wrongThread, "callback executed outside the loop thread");
        require(stopCalled, "shutdown callback was not executed");
        for (const auto& count : calls) require(count == 1, "callback was not executed exactly once");
        alarm(0);
        std::cout << "PASS two blocked-loop wakeups, 128 callbacks exactly once on the loop thread, "
                     "idle recovery and safe shutdown/join\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
