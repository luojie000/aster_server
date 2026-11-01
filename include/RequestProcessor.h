#pragma once
#include <functional>
#include <string>

// Pure business work: no sockets, Channel, EventLoop or connection access.
class RequestProcessor {
public:
    // Custom processors must be bounded and safe to invoke concurrently.
    using Handler = std::function<std::string(const std::string&)>;
    static constexpr unsigned kMaxNumber = 1000000;
    static std::string process(const std::string& request);
};
