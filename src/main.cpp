#include "EventLoop.h"
#include "TcpServer.h"
#include <charconv>
#include <iostream>
#include <string_view>
#include <stdexcept>

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
        EventLoop loop;
        TcpServer server(loop, port);
        server.start();
        std::cerr << "LISTENING " << server.port() << std::endl;
        loop.run();
    } catch (const std::exception& error) {
        std::cerr << "aster_server: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
