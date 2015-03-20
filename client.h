#ifndef __CLIENT_H
#define __CLIENT_H

#include <pthread.h>
#include "dllist.h"

typedef struct cl {
    dllistNode_t *node;
    int fd;
    pthread_t tid;
} cl_t;

cl_t *newCl(int fd);
void *handleClient(void *arg);
void abortClients();

#endif
