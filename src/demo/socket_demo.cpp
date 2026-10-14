#include<iostream>
#include<sys/socket.h>
#include<cerrno>
#include<cstring>

#include<unistd.h>
#include<netinet/in.h>

int main()
{
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1)
    {
        std::cerr<<"socket creation failed:"<<strerror(errno)<<"\n";
        return 1;
    }
    std::cout<<"socket created successfully\n";
    std::cout<<"file descriptor:"<<server_fd<<"\n";
    std::cout<<"pid:"<<getpid()<<"\n";

//bind
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8080);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(server_fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==-1)
{
    std::cerr<<"bind failed:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}
std::cout<<"bind success:127.0.0.1:8080\n";


//listen
if(listen(server_fd,128)==-1)
{
    std::cerr<<"listen failed:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}


//client
int client_fd=accept(server_fd,nullptr,nullptr);
if(client_fd==-1)
{
    std::cerr<<"accept failed:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}
std::cout<<"client connected successfully\n";

//循环接收数据并回显
char buffer[1024];
while(true)
{
    ssize_t n=recv(client_fd,buffer,sizeof(buffer),0);
    if(n>0)
    {
        std::cout<<"received:  "<<n<<"  bytes:";
        std::cout.write(buffer,n);
        std::cout<<"\n";
        ssize_t sent=0;
        while(sent<n)
        {
            ssize_t result=send(client_fd,buffer+sent,static_cast<size_t>(n-sent),MSG_NOSIGNAL);
            if(result>0)
            {
                sent+=result;
            }
            else if(result==-1&&errno==EINTR)
            {
                continue;
            }
            else{
                std::cerr<<"send failed:"<<strerror(errno)<<"\n";
                close(client_fd);
                close(server_fd);
                return 1;
            }
        }

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
    close(server_fd);
    close(client_fd);
    std::cout<<"socket closed\n";
    return 0;
}