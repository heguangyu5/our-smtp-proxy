#include "transport.h"
#include "log.h"
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

#define MAX_CLIENTS 5   // proxy自身允许的最大连接数
#define MAX_WAIT    10  // 超出最大连接数的连接最多等待几秒proxy就要给出响应

dllist_t *transports;
pthread_dllist_t *clients;
int quit;
int reload;
FILE *logFile; // stdout OR LOG_FILE

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
        exit(2);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            continue;
        }
        optval = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            perror("setsockopt");
            exit(2);
        }
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "cannot create passive socket on %s:%s\n", BIND_IP, BIND_PORT);
        exit(2);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(2);
    }

    freeaddrinfo(result);

    return sockfd;
}

static void cleanup()
{
    MAIN_THREAD_LOG("abort clients\n");
    abortClients();
    MAIN_THREAD_LOG("done\n");
    MAIN_THREAD_LOG("abort transports conns\n")
    abortTransportsConns();
    MAIN_THREAD_LOG("done\n");
    MAIN_THREAD_LOG("free transports\n");
    freeTransports();
    MAIN_THREAD_LOG("done\n");
}

static void mainLoop(int sockfd)
{
    int fd;
    char buf[1024];
    cl_t *cl;

    clients = pthread_dllistInit(MAX_CLIENTS, MAX_WAIT);

    mylog("our-smtp-proxy started\n");
    mylog("accepting connections\n");

    while (!quit) {
        fd = accept(sockfd, NULL, NULL);
        if (fd == -1) {
            switch(errno) {
            case ECONNABORTED:
                MAIN_THREAD_LOG("accept: ECONNABORTED")
                break;
            case EMFILE:
                MAIN_THREAD_LOG("accept: EMFILE")
                break;
            case ENFILE:
                MAIN_THREAD_LOG("accept: ENFILE")
                break;
            case EINTR:
            case EAGAIN:
                break;
            default:
                strerror_r(errno, buf, 1024);
                MAIN_THREAD_LOG("accept: %s\n", buf)
                break;
            }
        }
        if (quit) {
            break;
        }
        if (reload) {
            MAIN_THREAD_LOG("=====RELOAD=====\n");
            cleanup();
            loadTpConfig(0);
            MAIN_THREAD_LOG("=====RELOAD DONE=====\n");
            reload = 0;
            continue;
        }

        cl = newCl(fd);
        if (cl == NULL) {
            write(fd, "500 Too many connections\r\n", 28);
            close(fd);
        } else {
            pthread_create(&cl->tid, NULL, &handleClient, cl);
        }
    }

    // cleanup
    MAIN_THREAD_LOG("QUIT\n");
    cleanup();
    pthread_dllistDestroy(clients);
    free(transports);
}

static void setReloadFlag(int signo)
{
    reload = 1;
}

static void onQuit(int signo)
{
    quit = 1;
}

/**
 * Usage: ./our-smtp-proxy [-t] [-D]
 *        -t 测试transport.ini配置是否正确.
 *        -D 前台运行,默认是后台运行(daemonize).
 */
int main(int argc, char *argv[])
{
    int daemonize = 1;
    int testTp = 0;
    int listenfd;
    char c;

    opterr = 0;
    while ((c = getopt(argc, argv, "tD")) != -1) {
        switch (c) {
        case 't':
            testTp = 1;
            break;
        case 'D':
            daemonize = 0;
            break;
        }
    }

    if (testTp || !daemonize) {
        logFile = stdout;
    } else {
        logFile = fopen(LOG_FILE, "a");
        if (logFile == NULL) {
            perror("fopen " LOG_FILE);
            exit(2);
        }
    }

    // exit 1 if failed
    transports = dllistNew();
    loadTpConfig(testTp);

    signal(SIGHUP, setReloadFlag);
    signal(SIGINT, onQuit);
    signal(SIGTERM, onQuit);

    siginterrupt(SIGHUP, 1);
    siginterrupt(SIGINT, 1);
    siginterrupt(SIGTERM, 1);

    listenfd = setup();
    if (!daemonize) {
        mainLoop(listenfd);
        return 0;
    }

    if (daemon(1, 0) == 0) {
        mainLoop(listenfd);
        return 0;
    }
    perror("daemon");
    exit(3);
}
