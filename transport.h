#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include "rcpt.h"
#include <stddef.h>
#include <pthread.h>
#include "dllist.h"

#define TP_INI_FILE "transport.ini"

#define TP_CONN_BUSY 1
#define TP_CONN_IDLE 2

typedef struct tpConn {
    dllistNode_t *node;

    struct tp *tp;

    int sockfd;
    int status;
    int sendCount;
    int noopCount;

    pthread_cond_t cond;
    pthread_t tid;

    int endFlag;
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
    int maxConn;
    int maxSendPerConn;
    int sleepSecondsPerSend;
    int maxNoop;
    int sleepSecondsPerNoop;

    // connns分三类:
    // 1. busy - 正在发邮件
    // 2. idle - 空闲中,待使用
    // 3. noop - noop线程正在发送noop命令以保持连接不断
    dllist_t *busyConns;
    dllist_t *idleConns;
    dllist_t *noopConns;

    pthread_mutex_t mtx;
    pthread_cond_t idleCond;
    pthread_cond_t endCond;
} tp_t;

int findTpByName(int idx, void *data, void *name);
void loadTpConfig(int testTp);
int tpSendMail(tp_t *tp, rcpt_t *toList, char *data, char *err, size_t errlen);
void abortTransportsConns();
void freeTransports();

#endif
