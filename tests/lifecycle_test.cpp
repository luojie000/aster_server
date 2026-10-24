#include "EventLoop.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include <sys/socket.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <memory>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
size_t fdCount() {
    size_t n = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) ++n;
    return n;
}
int main() {
    try {
        const auto before = fdCount();
        {
            EventLoop loop;
            std::unique_ptr<TcpServer> retry;
            {
                TcpServer first(loop, 0);
                first.start();
                check(first.port() > 0, "ephemeral port missing");
                retry = std::make_unique<TcpServer>(loop, first.port());
                bool rejected = false;
                try { retry->start(); } catch (const std::runtime_error&) { rejected = true; }
                check(rejected, "occupied port accepted");
            }
            retry->start(); // failed start must release its socket and allow retry
            bool duplicate = false;
            try { retry->start(); } catch (const std::runtime_error&) { duplicate = true; }
            check(duplicate, "duplicate start accepted");
            for (int port : {-1, 65536}) {
                TcpServer bad(loop, port);
                bool rejected = false;
                try { bad.start(); } catch (const std::invalid_argument&) { rejected = true; }
                check(rejected, "out-of-range port accepted");
            }
        }
        check(fdCount() == before, "server lifecycle leaked fd");
        {
            EventLoop loop;
            int sockets[2];
            check(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets) == 0, "socketpair failed");
            int closed = 0;
            {
                TcpConnection connection(sockets[0], loop);
                connection.setCloseCallback([&] { ++closed; loop.queueInLoop([&] { loop.stop(); }); });
                loop.addChannel(connection.channel());
                check(send(sockets[1], "echo", 4, MSG_NOSIGNAL) == 4, "send failed");
                shutdown(sockets[1], SHUT_WR);
                loop.run();
                char data[4];
                check(recv(sockets[1], data, 4, MSG_WAITALL) == 4 && std::string(data,4) == "echo", "half-close lost data");
                connection.handleRead();
                connection.handleWrite();
                check(closed == 1, "close callback repeated");
            }
            close(sockets[1]);
        }
        check(fdCount() == before, "connection lifecycle leaked fd");
        std::cout << "PASS startup retry, invalid ports, duplicate start, half-close and fd cleanup\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
