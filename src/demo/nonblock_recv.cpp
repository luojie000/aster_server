#include <iostream>

#include <cstring>
#include <cerrno>

#include <fcntl.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <sys/socket.h>
#include <netinet/in.h>



bool setNonBlocking(int fd)
{

    int flags =
        fcntl(
            fd,
            F_GETFL,
            0
        );


    if(flags == -1)
        return false;


    flags |= O_NONBLOCK;


    return
        fcntl(
            fd,
            F_SETFL,
            flags
        )
        != -1;
}




int main()
{

    int server_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );



    int opt = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );



    sockaddr_in addr{};

    addr.sin_family =
        AF_INET;

    addr.sin_port =
        htons(8080);

    addr.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);



    bind(
        server_fd,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    );


    listen(
        server_fd,
        10
    );



    std::cout
        << "waiting client..."
        << std::endl;



    int client_fd =
        accept(
            server_fd,
            nullptr,
            nullptr
        );



    std::cout
        << "client connected"
        << std::endl;




    /*
        设置客户端socket非阻塞
    */

   setNonBlocking(client_fd);



    char buffer[1024];



    while(true)
    {

        ssize_t n =
            recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0
            );



        if(n > 0)
        {

            std::cout
                << "recv: "
                << std::string(
                    buffer,
                    n
                )
                << std::endl;

        }


        else if(n == 0)
        {

            std::cout
                << "client close"
                << std::endl;

            break;

        }


        else
        {


            if(
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
            {

                std::cout
                    << "no data now"
                    << std::endl;


                sleep(1);

                continue;

            }



            std::cerr
                << "recv error:"
                << strerror(errno)
                << std::endl;

            break;

        }

    }


    close(client_fd);

    close(server_fd);


    return 0;
}