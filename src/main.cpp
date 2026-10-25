#include "EventLoop.h"
#include "TcpServer.h"
#include "Logger.h"
#include <cerrno>
#include <pthread.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <charconv>
#include <iostream>
#include <string_view>
#include <stdexcept>

namespace {
// Block before creating Logger so its worker inherits the mask. The loop reads
// signals as ordinary fd events; no C++ or logging runs in a signal handler.
class ShutdownSignals {
public:
    ShutdownSignals() {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        if (pthread_sigmask(SIG_BLOCK, &mask, &previous_) != 0)
            throw std::runtime_error("pthread_sigmask failed");
        fd_ = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd_ == -1) {
            pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
            throw std::runtime_error("signalfd failed");
        }
    }
    ~ShutdownSignals() {
        close(fd_);
        pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }
    int fd() const { return fd_; }
    ShutdownSignals(const ShutdownSignals&) = delete;
    ShutdownSignals& operator=(const ShutdownSignals&) = delete;
private:
    sigset_t previous_{};
    int fd_ = -1;
};

struct LoggerLifetime {
    Logger& logger = Logger::instance();
    ~LoggerLifetime() { logger.stop(); }
};
}

int main(int argc, char** argv)
{
    try {
        int port = 8080;
        if (argc > 2) throw std::invalid_argument("usage: aster_server [port: 0..65535]");
        if (argc == 2) {
            const std::string_view value(argv[1]);
            auto result = std::from_chars(value.data(), value.data() + value.size(), port);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                throw std::invalid_argument("invalid port");
        }
        ShutdownSignals signals;
        LoggerLifetime logging;
        {
            EventLoop loop;
            Channel signalChannel(signals.fd());
            signalChannel.setReadCallback([&] {
                signalfd_siginfo info{};
                while (true) {
                    const auto n = read(signals.fd(), &info, sizeof(info));
                    if (n == sizeof(info)) {
                        logging.logger.log(LogLevel::Info, "shutdown requested signal=" +
                                           std::to_string(info.ssi_signo));
                        loop.stop();
                        continue;
                    }
                    if (n == -1 && errno == EINTR) continue;
                    if (n == -1 && errno == EAGAIN) break;
                    throw std::runtime_error("signalfd read failed");
                }
            });
            loop.addChannel(&signalChannel);
            TcpServer server(loop, port);
            server.start();
            // Keep this synchronous and first: test/startup consumers parse it.
            std::cerr << "LISTENING " << server.port() << std::endl;
            loop.run();
        } // Server/connections and pending loop callbacks die before Logger stops.
        logging.logger.log(LogLevel::Info, "server stopped");
    } catch (const std::exception& error) {
        std::cerr << "aster_server: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
