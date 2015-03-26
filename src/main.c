#include "transport.h"
#include "log.h"
#include "client.h"
#include "report.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BIND_IP         "127.0.0.1"
#define BIND_PORT       "9925"
#define MONITOR_PORT    "9926"
#define BACKLOG         5

#define MAX_CLIENTS 1000 // proxy自身允许的最大连接数
#define MAX_WAIT    10   // 超出最大连接数的连接最多等待几秒proxy就要给出响应

#define PID_FILE "our-smtp-proxy.pid"

int quit, reload;
int proxyfd, monitorfd;

dllist_t *transports;
pthread_mutex_t transportsMtx = PTHREAD_MUTEX_INITIALIZER;

pthread_dllist_t *clients;

FILE *logFile; // stdout OR LOG_FILE

static int tcpListen(const char *host, const char *port)
{
    int sockfd, r;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int optval;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((r = getaddrinfo(host, port, &hints, &result)) != 0) {
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
        fprintf(stderr, "cannot create passive socket on %s:%s\n", host, port);
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

static void *monitor(void *arg)
{
    int fd;
    char buf[1024];
    report_t report;

    while (1) {
        fd = accept(monitorfd, NULL, NULL);
        if (fd == -1) {
            switch(errno) {
            case EINTR:
            case EAGAIN:
                break;
            case ECONNABORTED:
                MAIN_THREAD_LOG("proxy accept: ECONNABORTED")
                break;
            default:
                strerror_r(errno, buf, 1024);
                MAIN_THREAD_LOG("proxy accept: %s\n", buf)
                close(monitorfd);
                pthread_exit(NULL);
            }
        }
        if (fd > -1) {
            collectReport(&report);
            write(fd, report.buf, report.len);
            close(fd);
        }
    }
}

static void mainLoop()
{
    int fd;
    char buf[1024];
    cl_t *cl;
    pthread_t monitorThread;
    timer_t todaySendResetTimer;

    // write pid file
    FILE *fp;
    fp = fopen(PID_FILE, "w");
    fprintf(fp, "%d", getpid());
    fclose(fp);

    clients = pthread_dllistInit(MAX_CLIENTS, MAX_WAIT);

    MAIN_THREAD_LOG("start monitor thread\n");
    pthread_create(&monitorThread, NULL, &monitor, NULL);
    MAIN_THREAD_LOG("started\n");

    MAIN_THREAD_LOG("init todaySend reset timer\n");
    initTodaySendResetTimer(&todaySendResetTimer);
    MAIN_THREAD_LOG("done\n");

    MAIN_THREAD_LOG("our-smtp-proxy started, accepting connections\n");

    while (!quit) {
        fd = accept(proxyfd, NULL, NULL);
        if (fd == -1) {
            switch(errno) {
            case EINTR:
            case EAGAIN:
                break;
            case ECONNABORTED:
                MAIN_THREAD_LOG("proxy accept: ECONNABORTED")
                break;
            default:
                strerror_r(errno, buf, 1024);
                MAIN_THREAD_LOG("proxy accept: %s\n", buf)
                quit = 1;
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

        if (fd > -1) {
            cl = newCl(fd);
            if (cl == NULL) {
                write(fd, "500 Too many connections\r\n", 28);
                close(fd);
            } else {
                pthread_create(&cl->tid, NULL, &handleClient, cl);
            }
        }
    }

    // cleanup
    MAIN_THREAD_LOG("QUIT\n");
    MAIN_THREAD_LOG("cancel monitor thread\n");
    pthread_cancel(monitorThread);
    pthread_join(monitorThread, NULL);
    MAIN_THREAD_LOG("done\n");
    timer_delete(todaySendResetTimer);
    close(proxyfd);
    close(monitorfd);
    cleanup();
    pthread_dllistDestroy(clients);
    free(transports);
    // unlink pid file
    unlink(PID_FILE);
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

    proxyfd   = tcpListen(BIND_IP, BIND_PORT);
    monitorfd = tcpListen(BIND_IP, MONITOR_PORT);
    if (!daemonize) {
        mainLoop();
        return 0;
    }

    if (daemon(1, 0) == 0) {
        mainLoop();
        return 0;
    }
    perror("daemon");
    exit(3);
}
