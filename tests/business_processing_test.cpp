#include "TcpServer.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <vector>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

// Read-only test access, used only on the loop thread. No production API added.
struct TcpServerTestAccess {
    static size_t connections(const TcpServer& s) { return s.connections_.size(); }
    static size_t pending(const TcpServer& s) { return s.pendingRequests_; }
    static int onlyFd(const TcpServer& s) { return s.connections_.begin()->first; }
    static std::weak_ptr<int> lifetime(const TcpServer& s) { return s.lifetime_; }
};

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void tick(EventLoop& loop) {
    loop.queueInLoop([&] { loop.stop(); });
    loop.run();
}
template<class Predicate>
void until(EventLoop& loop, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        require(std::chrono::steady_clock::now() < deadline, "bounded wait expired");
        tick(loop);
        std::this_thread::sleep_for(1ms);
    }
}

struct Client {
    int fd;
    explicit Client(int port) : fd(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) {
        require(fd >= 0, "socket failed");
        timeval timeout{2, 0};
        require(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0,
                "send timeout failed");
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        require(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0,
                "connect failed");
    }
    ~Client() { if (fd >= 0) close(fd); }
    Client(const Client&) = delete;
    void sendAll(const std::string& text) {
        size_t offset = 0;
        while (offset < text.size()) {
            auto n = send(fd, text.data() + offset, text.size() - offset, MSG_NOSIGNAL);
            if (n < 0 && errno == EINTR) continue;
            require(n > 0, "send failed");
            offset += static_cast<size_t>(n);
        }
    }
    void reset() {
        linger option{1, 0};
        require(setsockopt(fd, SOL_SOCKET, SO_LINGER, &option, sizeof(option)) == 0,
                "linger failed");
        close(fd);
        fd = -1;
    }
    void expect(EventLoop& loop, const std::string& expected) {
        std::string result;
        until(loop, [&] {
            char buffer[1024];
            auto n = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (n > 0) result.append(buffer, static_cast<size_t>(n));
            else if (n == 0) throw std::runtime_error("unexpected EOF");
            else require(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR,
                         "recv failed");
            require(result.size() <= expected.size(), "unexpected extra response");
            return result.size() == expected.size();
        });
        require(result == expected, "response mismatch or ordering failure");
    }
    void expectEof(EventLoop& loop) {
        until(loop, [&] {
            char c;
            auto n = recv(fd, &c, 1, MSG_DONTWAIT);
            if (n == 0) return true;
            require(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK), "expected EOF");
            return false;
        });
    }
    void expectEmpty() {
        char c;
        require(recv(fd, &c, 1, MSG_DONTWAIT) == -1 &&
                (errno == EAGAIN || errno == EWOULDBLOCK), "stale response leaked");
    }
};

struct Gate {
    std::promise<void> promise;
    std::shared_future<void> ready = promise.get_future().share();
    std::atomic<unsigned> entered{0}, finished{0};
    bool released = false;
    void release() { if (!released) { released = true; promise.set_value(); } }
    ~Gate() { release(); }
    std::string process(const std::string& request) {
        ++entered;
        // Even a test failure must never leave worker shutdown blocked forever.
        if (ready.wait_for(3s) != std::future_status::ready)
            throw std::runtime_error("gate timeout");
        ++finished;
        return RequestProcessor::process(request);
    }
};

void processorTest() {
    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
        {"CALC 0", "RESULT 0\n"}, {"CALC 1", "RESULT 0\n"},
        {"CALC 2", "RESULT 1\n"}, {"CALC 10", "RESULT 4\n"},
        {"CALC 1000000", "RESULT 78498\n"}, {"HELLO", "ERR command\n"}})
        require(RequestProcessor::process(item.first) == item.second, "processor result");
    for (const auto& invalid : {"CALC ", "CALC -1", "CALC +1", "CALC 2x",
                               "CALC 1000001", "CALC 4294967296", "CALC 2 3"})
        require(RequestProcessor::process(invalid) == "ERR range 0..1000000\n",
                "invalid number accepted");
}

void successTest() {
    EventLoop loop;
    const auto loopThread = std::this_thread::get_id();
    std::atomic<unsigned> calls{0};
    std::atomic<bool> wrongThread{false};
    TcpServer server(loop, 0, [&](const std::string& request) {
        if (std::this_thread::get_id() == loopThread) wrongThread = true;
        ++calls;
        return RequestProcessor::process(request);
    });
    server.start();
    Client client(server.port());
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 1; });
    client.sendAll("CAL");
    tick(loop);
    require(calls == 0, "partial frame dispatched");
    client.sendAll("C 1000000\nCALC 10\nCALC 0\n");
    require(shutdown(client.fd, SHUT_WR) == 0, "half-close failed");
    client.expect(loop, "RESULT 78498\nRESULT 4\nRESULT 0\n");
    client.expectEof(loop);
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 0; });
    require(calls == 3 && !wrongThread, "task count or worker thread incorrect");
}

void multiclientTest() {
    EventLoop loop;
    Gate gate;
    TcpServer server(loop, 0, [&](const std::string& request) {
        return gate.process(request);
    });
    server.start();
    Client first(server.port()), second(server.port());
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 2; });
    first.sendAll("CALC 10");
    second.sendAll("CALC ");
    tick(loop);
    require(gate.entered == 0 && TcpServerTestAccess::pending(server) == 0,
            "incomplete frames dispatched across clients");
    first.sendAll("\nCALC 2\nCALC 0\n");
    second.sendAll("100\nCALC 20\nCALC 1\n");
    until(loop, [&] { return gate.entered == 2; });
    require(TcpServerTestAccess::pending(server) == 2,
            "pipelined requests exceeded one active task per connection");
    first.expectEmpty();
    second.expectEmpty();
    require(shutdown(first.fd, SHUT_WR) == 0, "first half-close failed");
    require(shutdown(second.fd, SHUT_WR) == 0, "second half-close failed");
    gate.release();
    first.expect(loop, "RESULT 4\nRESULT 1\nRESULT 0\n");
    second.expect(loop, "RESULT 25\nRESULT 8\nRESULT 0\n");
    first.expectEof(loop);
    second.expectEof(loop);
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 0; });
    require(gate.finished == 6 && TcpServerTestAccess::pending(server) == 0,
            "multi-client requests lost or duplicated");
}

void disconnectTest() {
    EventLoop loop;
    Gate gate;
    TcpServer server(loop, 0, [&](const std::string& request) {
        if (request == "CALC 1000000") return gate.process(request);
        return RequestProcessor::process(request);
    });
    server.start();
    Client old(server.port());
    old.sendAll("CALC 1000000\n");
    until(loop, [&] { return gate.entered == 1; });
    const int oldFd = TcpServerTestAccess::onlyFd(server);
    old.reset();
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 0; });
    require(TcpServerTestAccess::pending(server) == 1, "task finished before disconnect");
    Client fresh(server.port());
    until(loop, [&] { return TcpServerTestAccess::connections(server) == 1; });
    require(TcpServerTestAccess::onlyFd(server) == oldFd, "fd reuse scenario not exercised");
    fresh.sendAll("CALC 10\n");
    fresh.expect(loop, "RESULT 4\n"); // second worker serves while first is blocked
    gate.release();
    until(loop, [&] { return TcpServerTestAccess::pending(server) == 0; });
    require(gate.finished == 1, "old task not completed");
    fresh.expectEmpty(); // old result must not go to the reused fd
    fresh.sendAll("CALC 2\n");
    fresh.expect(loop, "RESULT 1\n");

    // A blocked task must not permit unbounded buffering of complete lines.
    Gate overflowGate;
    TcpServer bounded(loop, 0, [&](const std::string& request) {
        return overflowGate.process(request);
    });
    bounded.start();
    Client flooding(bounded.port());
    flooding.sendAll("CALC 10\n");
    until(loop, [&] { return overflowGate.entered == 1; });
    std::string pipeline;
    for (int i = 0; i < 512; ++i) pipeline += "CALC 10\n";
    require(pipeline.size() == TcpConnection::kInputLimit, "input limit test setup");
    flooding.sendAll(pipeline);
    flooding.expectEof(loop);
    until(loop, [&] { return TcpServerTestAccess::connections(bounded) == 0; });
    require(overflowGate.entered == 1, "per-connection task bound exceeded");
    overflowGate.release();
    until(loop, [&] { return TcpServerTestAccess::pending(bounded) == 0; });
}

void shutdownTest() {
    EventLoop loop;
    Gate gate;
    auto server = std::make_unique<TcpServer>(loop, 0, [&](const std::string& request) {
        return gate.process(request);
    });
    server->start();
    std::vector<std::unique_ptr<Client>> clients;
    for (int i = 0; i < 3; ++i) {
        clients.push_back(std::make_unique<Client>(server->port()));
        clients.back()->sendAll("CALC 10\n");
    }
    until(loop, [&] { return gate.entered == 2 && TcpServerTestAccess::pending(*server) == 3; });
    auto lifetime = TcpServerTestAccess::lifetime(*server);
    const int port = server->port();
    std::atomic<bool> observedDestruction{false}, listenerClosed{false};
    std::thread releaseDuringDestruction([&] {
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (!lifetime.expired() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
        observedDestruction = lifetime.expired();
        // The destructor is still waiting for this gate. New connections must
        // already be refused, rather than accepted into an unserviced backlog.
        const int probe = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (probe >= 0) {
            timeval timeout{1, 0};
            if (setsockopt(probe, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0) {
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port = htons(port);
                listenerClosed = connect(probe, reinterpret_cast<sockaddr*>(&address),
                                         sizeof(address)) == -1 && errno == ECONNREFUSED;
            }
            close(probe);
        }
        gate.release();
    });
    // Two running + one queued task. Destructor must drain/join without the loop.
    server.reset();
    releaseDuringDestruction.join();
    require(listenerClosed, "listener remained open while joining workers");
    require(observedDestruction && gate.finished == 3, "shutdown did not join all tasks");
    for (auto& client : clients) client->expectEof(loop);
    // Keep the loop alive and execute old queued callbacks after server destruction.
    // A new server also makes accidental use of recycled server memory detectable.
    TcpServer next(loop, 0, RequestProcessor::process);
    next.start();
    tick(loop);
    require(TcpServerTestAccess::pending(next) == 0, "stale callback touched new server");
    Client fresh(next.port());
    fresh.sendAll("CALC 10\n");
    fresh.expect(loop, "RESULT 4\n");
}

void limitsTest() {
    EventLoop loop;
    Gate gate;
    TcpServer server(loop, 0, [&](const std::string& request) { return gate.process(request); });
    server.start();
    std::vector<std::unique_ptr<Client>> clients;
    for (int i = 0; i < 65; ++i) {
        clients.push_back(std::make_unique<Client>(server.port()));
        clients.back()->sendAll("CALC 10\n");
        until(loop, [&] { return TcpServerTestAccess::connections(server) == clients.size(); });
        if (i < 64)
            until(loop, [&] { return TcpServerTestAccess::pending(server) == clients.size(); });
    }
    clients.back()->expect(loop, "ERR busy\n");
    require(TcpServerTestAccess::pending(server) == 64, "task limit exceeded");
    gate.release();
    for (size_t i = 0; i < 64; ++i) clients[i]->expect(loop, "RESULT 4\n");
    until(loop, [&] { return TcpServerTestAccess::pending(server) == 0; });
    clients.back()->sendAll("CALC 2\n");
    clients.back()->expect(loop, "RESULT 1\n");
    Client tooLong(server.port());
    tooLong.sendAll(std::string(65, 'x'));
    tooLong.expectEof(loop);
}

void errorsTest() {
    EventLoop loop;
    TcpServer server(loop, 0, [](const std::string& request) -> std::string {
        if (request == "CALC 1") throw std::runtime_error("expected business failure");
        if (request == "CALC 2") return std::string(129, 'x');
        return RequestProcessor::process(request);
    });
    server.start();
    Client client(server.port());
    client.sendAll("CALC 1\nCALC 2\nCALC -1\nHELLO\nCALC 10\nunfinished");
    require(shutdown(client.fd, SHUT_WR) == 0, "half-close failed");
    client.expect(loop, "ERR processing\nERR response too large\nERR range 0..1000000\nERR command\nRESULT 4\n");
    client.expectEof(loop);
}

int main(int argc, char** argv) {
    alarm(12); // process hard limit, independent of CTest
    try {
        require(argc == 2, "test case required");
        const std::string mode(argv[1]);
        if (mode == "processor") processorTest();
        else if (mode == "success") successTest();
        else if (mode == "multiclient") multiclientTest();
        else if (mode == "disconnect") disconnectTest();
        else if (mode == "shutdown") shutdownTest();
        else if (mode == "limits") limitsTest();
        else if (mode == "errors") errorsTest();
        else throw std::runtime_error("unknown test case");
        std::cout << "PASS business " << mode << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
