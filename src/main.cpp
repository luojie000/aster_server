#include"EventLoop.h"
#include"TcpServer.h"

int main()
{
    EventLoop loop;
    TcpServer server(loop,8080);

    server.start();
    loop.run();
return 0;
}