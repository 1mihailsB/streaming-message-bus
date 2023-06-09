#include <iostream>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#define PORT "3490"
#define BACKLOG 1000
#define BUFS 4096

char buf[BUFS][BUFS];

int main()
{
    int listenSockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((listenSockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(listenSockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listenSockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listenSockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(listenSockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // epoll

    struct epoll_event ev, events[BACKLOG];
    int conn_sock, nfds, epollfd;

    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listenSockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenSockfd, &ev) == -1)
    {
        perror("epoll_ctl: listenSockfd");
        exit(EXIT_FAILURE);
    }

    int curSize = 0;

    for (;;)
    {
        nfds = epoll_wait(epollfd, events, BACKLOG, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n)
        {
            int curFd = events[n].data.fd;
            if (curFd == listenSockfd)
            {
                sockaddr con_address;
                socklen_t casize = sizeof con_address;
                conn_sock = accept(listenSockfd, &con_address, &casize);
                if (conn_sock == -1)
                {
                    perror("accept: conn_sock");
                    exit(EXIT_FAILURE);
                };

                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                              &ev) == -1)
                {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (curFd >= BUFS)
                {
                    printf("Skipped fd: %d. Out of buffer bounds.\n", curFd);
                    continue;
                }

                curSize = recv(curFd, buf[curFd], BUFS, 0);
                if (curSize == -1) {
                    perror("recv:");
                } else if (curSize == 0) {
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, curFd, &events[n]) == -1) {
                        perror("epoll_ctl EPOLL_CTL_DEL:");
                    }

                    close(curFd);
                }

                printf("received %d bytes from client %d: %s\n", curSize, curFd, buf[curFd]);
                curSize = 0;
                memset(&buf[curFd], 0, BUFS);
            }
        }
    }

    return 0;
}