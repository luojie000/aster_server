#include "EventLoop.h"
#include <iostream>
int main() {
    EventLoop loop;
    int calls = 0;
    loop.queueInLoop([&] {
        ++calls;
        loop.queueInLoop([&] { ++calls; loop.stop(); });
    });
    loop.run();
    if (calls != 2) return 1;
    std::cout << "PASS callbacks before run and nested callbacks\n";
}
