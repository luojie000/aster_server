#include<iostream>
#include<string>
#include<cstring>
#include<unistd.h>
#include<cerrno>
#include<netinet/in.h>
#include<sys/socket.h>
#include<cstdint>
#include<fcntl.h>

bool setNonBlocking(int fd)
{
    //get the attitude
    int flags=fcntl(fd,F_GETFL,0);
    if(flags==-1)
    {
        return false;
    }

    //添加非阻塞属性
    flags |=O_NONBLOCK;
    if(fcntl(fd,F_SETFL,flags)==-1)
    {
        return false;
    }
    return true;
}

int main()
{
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1)
    {
        std::cerr<<"failed to server:"<<strerror(errno)<<"\n";
        return 1;
    }
    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8080);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(server_fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==-1)
    {
        std::cerr<<"failed to bind:"<<strerror(errno)<<"\n";
        close(server_fd);
        return 1;
    }

    //listen
    if(listen(server_fd,10)==-1)
    {
        std::cerr<<"failed to listen:"<<strerror(errno)<<"\n";
        close(server_fd);
        return 1;
    }

std::cout<<"listening on 127.0.0.1:8080\n";

if(!setNonBlocking(server_fd))
{
    std::cerr<<"failed to nonblock\n";
    close(server_fd);
    return 1;
}
    
char buffer[1024];

    while(true)
    {
        int client_fd=accept(server_fd,nullptr,nullptr);
        if(client_fd==-1)
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK)
            {
                std::cout<<"no client connetction"<<"\n";
                sleep(1);
                continue;
            }
            else
            {
            std::cerr<<"failed to accept:"<<strerror(errno)<<"\n";
            close(server_fd);
            break;
            }
        }
        else
        {
            std::cout<<"connected success\n";
             std::size_t received=0;
while(true)
{
   ssize_t n=recv(client_fd,received+buffer,sizeof(buffer)-received,0);
   if(n>0)
   {
   std::cout.write(received+buffer,n);
   std::cout<<"\n";
   }
   else if(n==0)
    {
        std::cout<<"client disconnected\n";
        break;
    }
    else if(errno==EINTR)
    {
        continue;
    }
    else
    {
        std::cerr<<"recv failed:"<<strerror(errno)<<"\n";
        break;
    }

}
            close(client_fd);
        }
    }
    close(server_fd);
    return 0;

}