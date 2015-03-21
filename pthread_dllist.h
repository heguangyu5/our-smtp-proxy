#ifndef __PTHREAD_DLLIST_H
#define __PTHREAD_DLLIST_H

#include <pthread.h>

typedef struct pthread_dllistNode {
    void *data;
    struct pthread_dllistNode *prev;
    struct pthread_dllistNode *next;
} pthread_dllistNode_t;

typedef struct pthread_dllistNodeData {
    pthread_dllistNode_t *node;
} pthread_dllistNodeData_t;

typedef struct pthread_dllist {
    pthread_dllistNode_t *head;
    pthread_dllistNode_t *tail;
    int maxNodes;
    int nodesCount;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int condTimeout;
} pthread_dllist_t;

pthread_dllist_t *pthread_dllistInit(int maxNodes, int condTimeout);
void pthread_dllistDestroy(pthread_dllist_t *dllist);
int  pthread_dllistAppend(pthread_dllist_t *dllist, void *data);
void pthread_dllistDelete(pthread_dllist_t *dllist, void *data);
void *pthread_dllistVisit(pthread_dllist_t *dllist, int (*nodeHandler)(int idx, void *data, void *arg), void *arg);
int  pthread_dllistCountNodes(pthread_dllist_t *dllist);

#endif
