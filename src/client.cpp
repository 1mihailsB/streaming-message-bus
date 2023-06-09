#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 
#define CONS 5

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// void fillMessage(char* msg)
// {
//     return [];
// }

int main(int argc, char *argv[])
{
    int sockfd[5];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(int sn = 0; sn < CONS; sn++) {
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd[sn] = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(sockfd[sn], p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd[sn]);
                perror("client: connect");
                continue;
            }

            printf("socket %d connected\n", sockfd[sn]);

            break;
        }
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    int times = 0;
    while(1) {
        int sent = 0;
        times++;
        char text[17];
        int size = sprintf(text, "Hello%d", times);


        for(int sn = 0; sn < CONS; sn++) {
            // size + 1 to also send '\o'
            if ((sent = send(sockfd[sn], text, size, 0)) == -1) {
                perror("send");
                exit(1);
            }
        }

        usleep(500000);
    }

    // close(sockfd);

    return 0;
}