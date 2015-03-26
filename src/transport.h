#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include "rcpt.h"
#include <stddef.h>
#include <pthread.h>
#include "dllist.h"

#define TP_INI_FILE "transport.ini"

#define TP_CONN_BUSY 1
#define TP_CONN_IDLE 2
#define TP_CONN_NOOP 3

#define TZDIFF (8*3600)  // @see initTodaySendResetTimer

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

    int totalSend;
    int todaySend;

    // 等着要发邮件的client数量
    // 当getAnIdleConn需要等待时,就加1,取得conn后,减1
    // noop线程取得lock后,如果发现有waiting的,就放弃lock
    // 这样来减少noop,加快发信速度
    int waiting;
} tp_t;

int findTpByName(int idx, void *data, void *name);
void loadTpConfig(int testTp);
int tpSendMail(tp_t *tp, rcpt_t *toList, char *data, char *err, size_t errlen);
void abortTransportsConns();
void freeTransports();
int reportTp(int idx, void *data, void *arg);
void initTodaySendResetTimer(timer_t *timer);

#endif
