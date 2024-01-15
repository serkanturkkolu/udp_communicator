#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#define BUF_SIZE 1024

bool run = true;

void signal_handler(int)
{
    std::cout << "\nSIGINT Signal Caught" << std::endl;
    run = false;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "usage: [udp_comm] [IP] [LISTEN_PORT] [SEND_PORT]" << std::endl;
        return 3;
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        std::cerr << "Can't catch SIGINT" << std::endl;
    }
    const char *target_ip = argv[1];
    uint16_t recv_port = std::stoi(argv[2]);
    uint16_t send_port = std::stoi(argv[3]);
    
    sockaddr_in sin{};
    sin.sin_port = htons(recv_port);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;


    char buf[BUF_SIZE + 1]{};
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        std::cerr << "socket creation failed: " << strerror(errno) << std::endl;
        return 1;
    }
    if (bind(sock, (sockaddr *)&sin, sizeof(sin)) != 0)
    {
        std::cerr << "bind failed: " << strerror(errno) << std::endl;
        return 1;
    }
    epoll_event event{};
    epoll_event revent[2]{};
    int epfd = epoll_create(2);
    event.data.fd = sock;
    event.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event);
    event.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event);
    sockaddr_in sin_peer{};
    sin_peer.sin_family = AF_INET;
    sin_peer.sin_port = htons(send_port);
    if (inet_pton(AF_INET, target_ip, &sin_peer.sin_addr.s_addr) != 1)
    {
        std::cerr << "target ip convertion error: " << strerror(errno) << std::endl;
        return 7;
    }
    sockaddr_in sin_peer_recv{};
    sin_peer_recv.sin_family = AF_INET;

    std::cout << "target: " << target_ip << ":" << send_port << std::endl;
    std::cout << "listening: " << recv_port << std::endl;
    while (run)
    {
        int ret = -1;
        if (ret = epoll_wait(epfd, revent, 2, -1); ret == -1)
        {
            if (run == false)
            {
                break;
            }
            std::cerr << "epoll wait error: " << strerror(errno) << std::endl;
            return 2;
        }
        for (int i = 0; i < ret; ++i)
        {
            if (revent[i].data.fd == sock)
            {
                if(revent[i].events & EPOLLIN)
                {
                    socklen_t socklen = sizeof(sin_peer_recv);
                    ssize_t size = recvfrom(revent[i].data.fd, buf, BUF_SIZE, 0, (sockaddr *)&sin_peer_recv, &socklen);
                    if (size == -1)
                    {
                        std::cerr << "epoll recvfrom error: " << strerror(errno) << std::endl;
                        return 5;
                    }
                    buf[size] = '\0';
                    std::cout <<"[" << inet_ntoa(sin_peer_recv.sin_addr) << ":"<< ntohs(sin_peer_recv.sin_port) << "]:" << buf;
                    memset(buf,0,BUF_SIZE+1);
                }
            }
            else if (revent[i].data.fd == STDIN_FILENO)
            {
                if (revent[i].events & EPOLLIN) {
                    ssize_t size = read(revent[i].data.fd, buf, BUF_SIZE);
                    if (size == -1)
                    {
                        std::cerr << "epoll read error: " << strerror(errno) << std::endl;
                        return 4;
                    }
                    if(int sz = sendto(sock, buf, size, 0, (const sockaddr *)&sin_peer, sizeof(sockaddr_in)); sz<0){
                        std::cerr << "Sendto failed: [" << inet_ntoa(sin_peer.sin_addr) << ":"<< ntohs(sin_peer.sin_port) << "]" << strerror(errno) << std::endl;

                    }
                    memset(buf,0,BUF_SIZE+1);
                    break;
                }
            }
        }
    }
    close(epfd);
    close(sock);
    std::cout << "[Finished]" << std::endl;
}