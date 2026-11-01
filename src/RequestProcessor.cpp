#include "RequestProcessor.h"
#include <charconv>
#include <vector>

std::string RequestProcessor::process(const std::string& request)
{
    if (request.compare(0, 5, "CALC ") != 0)
        return "ERR command\n";
    unsigned number = 0;
    const auto begin = request.data() + 5;
    const auto end = request.data() + request.size();
    const auto parsed = std::from_chars(begin, end, number);
    if (parsed.ec != std::errc{} || parsed.ptr != end || number > kMaxNumber)
        return "ERR range 0..1000000\n";

    // Sieve of Eratosthenes: bounded CPU work and bounded task-local memory.
    std::vector<bool> prime(number + 1, true);
    for (unsigned p = 2; p * p <= number; ++p) {
        if (!prime[p]) continue;
        for (unsigned multiple = p * p; multiple <= number; multiple += p)
            prime[multiple] = false;
    }
    unsigned count = 0;
    for (unsigned p = 2; p <= number; ++p)
        if (prime[p]) ++count;
    return "RESULT " + std::to_string(count) + "\n";
}
