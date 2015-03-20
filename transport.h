#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include "rcpt.h"
#include <stddef.h>
#include <pthread.h>
#include "dllist.h"

#define TP_INI_FILE "transport.ini"

typedef struct tpConn {
    int sockfd;
    int sendCount;
    int noopCount;
    struct tpConn *next;
    struct tpConn *prev;
    pthread_mutex_t mtx;
    pthread_t tid;
    struct tp *tp;
} tpConn_t;

typedef struct tp {
    dllistNode_t *node;

    char *name;

    char *host;
    char *port;
    char *ssl;
    char *auth;
    char *username;
    char *password;

    pthread_mutex_t mtx;

    tpConn_t *conn;
    int connCount;
    int maxConn;

    int maxSendPerConn;
    int sleepSecondsPerSend;
    int maxNoop;
    int sleepSecondsPerNoop;
} tp_t;

int findTpByName(void *tp, void *name);
void loadTpConfig(int testTp);
void freeTpList();
int tpSendMail(tp_t *tp, rcpt_t *toList, char *data, char *err, size_t errlen);
void abortTpConns();

#endif
