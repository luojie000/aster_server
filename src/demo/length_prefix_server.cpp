#include<iostream>
#include<unistd.h>
#include<cerrno>
#include<string>
#include<cstdint>
#include<cstring>
#include<sys/socket.h>
#include<netinet/in.h>

int recvExact(int fd,void*buffer,std::size_t length)
{
    char*p=static_cast<char*>(buffer);
    std::size_t received=0;
    while(received<length)
    {
        ssize_t n=recv(fd,p+received,length-received,0);
        if(n>0)
        {
            received+=static_cast<std::size_t>(n);
        }
        else if(n==0)
        {
if(received==0)
{
    return 0;
}
return -1;
        }
        else
        {
            if(errno==EINTR)
            {
                continue;
            }
            return -1;
        }
    }
    return 1;
}
int main()
{
int server_fd=socket(AF_INET,SOCK_STREAM,0);
//int server_fd=socket(AF_INET,SOCK_STREAM,0);

//BIND
/*
if(server_fd==-1)
std::cerr<<strerror(errno)
return 1
bind
int opt=1
setsocket(server_fd,sol_socket,reuseaddr,$opt,size(opt))
sockaddr_in addr{}
addr.sin_family=af_inet
add.sin_port=htons(8080)
add.sin_addr.s_addr=htons(inaddr_loopback)
if(bind(server_fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==-1)
std::cerr<<
close(fd)
return 1
if(listen(server_fd,nullptr,nullptr)==-1)
{
}

*/
if(server_fd==-1)
{
std::cerr<<"socket:"<<strerror(errno)<<"\n";
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
    std::cerr<<"bind:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}

if(listen(server_fd,10)==-1)
{
    std::cerr<<"listen:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}
std::cout<<"listening on 127.0.0.1:8080\n";

int client_fd=accept(server_fd,nullptr,nullptr);
if(client_fd==-1)
{
    std::cerr<<"accept:"<<strerror(errno)<<"\n";
    close(server_fd);
    return 1;
}

while(true)
{
uint32_t network_length=0;
int result=recvExact(client_fd,&network_length,sizeof(network_length));
if(result==-1)
{
    std::cerr<<"failed to receive length\n";
    break;
}
else if(result==0)
{
    std::cout<<"client disconnected\n";
    break;
}

//信息长度
uint32_t length=ntohl(network_length);
std::cout<<"message length:"<<length<<"\n";

//check the message
if(length>4096)
{
    std::cerr<<"message too large\n";
    break;
}

//get the message
std::string message(length,'\0');
if(length>0)
{
    result=recvExact(client_fd,message.data(),length);
    if(result!=1)
    {
        std::cerr<<"incomplete message\n";
        break;
    }
}

//tackle with the message
std::cout<<"received message:"<<message<<"\n";
}

close(client_fd);
close(server_fd);
    return 0;
}