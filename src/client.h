#ifndef __CLIENT_H
#define __CLIENT_H

#include <pthread.h>
#include "pthread_dllist.h"

typedef struct cl {
    pthread_dllistNode_t *node;
    int fd;
    pthread_t tid;
} cl_t;

cl_t *newCl(int fd);
void *handleClient(void *arg);
void abortClients();
void blockClients();
void unblockClients();

#endif
