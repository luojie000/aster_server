#include<cstring>
#include<cerrno>
#include<unistd.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/epoll.h>
#include<iostream>

bool setNonBlocking(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    if(flags==-1)
    {
        return false;
    }
    flags|=O_NONBLOCK;
    return fcntl(fd,F_SETFL,flags)!=-1;
}

int main()
{
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8080);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    bind(server_fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
    listen(server_fd,10);
    setNonBlocking(server_fd);
    

    //set the epoll
    int epfd=epoll_create(1);
    if(epfd==-1)
    {
        std::cerr<<"epoll_create failed\n";
        return 1;
    }

    //add the listening socket
    epoll_event event{};

    //可读状态
    event.events=EPOLLIN;
    event.data.fd=server_fd;

    epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&event);
    std::cout<<"epoll server start\n";
    epoll_event events[1024];
    while(true)
    {
int n=epoll_wait(epfd,events,1024,-1);
for(int i=0;i<n;i++)
{
int fd=events[i].data.fd;
if(fd==server_fd)
{
    while(true)
    {
        int client_fd=accept(server_fd,nullptr,nullptr);
        if(client_fd == -1)
            {

                        if(errno == EAGAIN)
                        {
                            std::cout<<"reback\n";
                            break;
                        }

                        else
                        {
                            perror("accept");
                            break;
                        }

            }
            setNonBlocking(client_fd);
            epoll_event client_event{};
            client_event.events=EPOLLIN;
            client_event.data.fd=client_fd;
            epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&client_event);
            std::cout<<"new client:"<<client_fd<<"\n";
    }
}
else
{
    char buffer[1024];
    ssize_t len=recv(fd,buffer,sizeof(buffer),0);
    if(len<=0)
    {
        close(fd);
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
        std::cout<<"client close\n";
    }
    else
    {
        std::cout<<"recv:"<<std::string(buffer,len)<<"\n";
        send(fd,buffer,len,0);
    }
}
}
    }
  close(server_fd);
  return 0;
}