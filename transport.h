#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#define TP_FREE 1
#define TP_BUSY 2

typedef struct tpConn {
    int sockfd;
    int status;
    int noopCount;
} tpConn_t;

typedef struct tp {
    char *name;

    char *host;
    char *port;
    char *ssl;
    char *auth;
    char *username;
    char *password;

    tpConn_t *conn;
    int connCount;
    int maxConn;

    int maxSendPerConn;
    int sleepSecondsPerSend;
    int maxNoop;

    struct tp *next;
} tp_t;

tp_t *findTpByName(const char *name);
int loadTpConfig();
void freeTpList();

#endif
