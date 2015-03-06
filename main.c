#include "transport.h"
#include "message.h"
#include "client.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BIND_IP   "127.0.0.1"
#define BIND_PORT "9925"
#define BACKLOG   5

char *tpIni;
int daemonized;

tp_t *tpList;
cl_t *clList;

int quit;

static int setup()
{
    int sockfd, r;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int optval;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((r = getaddrinfo(BIND_IP, BIND_PORT, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
        exit(1);
    }

    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            fprintf(stderr, "setsockopt SO_REUSEADDR error: %s\n", strerror(errno));
            exit(1);
        }
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "setup failed\n");
        exit(1);
    }

    sp_msg(LOG_INFO, "server socket created on %s:%s\n", BIND_IP, BIND_PORT);

    if (listen(sockfd, BACKLOG) == -1) {
        fprintf(stderr, "listen: %s", strerror(errno));
        exit(1);
    }

    freeaddrinfo(result);

    return sockfd;
}

static void mainLoop(int sockfd)
{
    int fd;
    char buf[1024];
    cl_t *cl;

    sp_msg(LOG_INFO, "accepting connections\n");

    while (!quit) {
        fd = accept(sockfd, NULL, NULL);
        if (fd == -1) {
            switch(errno) {
            case ECONNABORTED:
                sp_msg(LOG_INFO, "accept: ECONNABORTED");
                break;
            case EMFILE:
                sp_msg(LOG_ERR, "accept: EMFILE");
                break;
            case ENFILE:
                sp_msg(LOG_ERR, "accept: ENFILE");
                break;
            case EINTR:
            case EAGAIN:
                break;
            default:
                strerror_r(errno, buf, 1024);
                sp_msg(LOG_ERR, "accept: %s\n", buf);
                break;
            }
        }
        if (quit) {
            break;
        }

        cl = newCl(fd);
        pthread_create(&cl->tid, NULL, &handleClient, cl);
    }

    // cleanup
    sp_msg(LOG_INFO, "\ncleaning up\n");
    abortClients();
    freeTpList();
}

void onQuit(int signo)
{
    quit = 1;
}

int main(int argc, char *argv[])
{
    int d = 0;
    char c;
    opterr = 0;
    while ((c = getopt(argc, argv, "t:d")) != -1) {
        switch (c) {
        case 't':
            tpIni = optarg;
            break;
        case 'd':
            d = 1;
            break;
        }
    }

    if (tpIni == NULL) {
        fprintf(stderr, "Usage: %s -t transport.ini [-d]\n", argv[0]);
        exit(1);
    }

    if (!loadTpConfig()) {
        exit(1);
    }

    signal(SIGINT, onQuit);
    signal(SIGTERM, onQuit);

    siginterrupt(SIGINT, 1);
    siginterrupt(SIGTERM, 1);

    mainLoop(setup());

    return 0;
}
