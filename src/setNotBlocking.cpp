#include"setNotBlocking.h"
#include<stdexcept>
#include<string>
#include<fcntl.h>

void setNotBlocking(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    if(flags==-1)
    {
        throw std::runtime_error("fcntl F_GETFL failed");
    }
    if(fcntl(fd,F_SETFL,flags|O_NONBLOCK)==-1)
    {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}
