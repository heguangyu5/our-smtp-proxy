#ifndef __CLIENT_H
#define __CLIENT_H

#include <pthread.h>

typedef struct cl {
    int fd;
    pthread_t tid;
    struct cl *prev;
    struct cl *next;
} cl_t;

cl_t *newCl(int fd);
void *handleClient(void *arg);
void abortClients();

#endif
