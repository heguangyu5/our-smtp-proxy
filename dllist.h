#ifndef __DLLIST_H
#define __DLLIST_H

#include <pthread.h>

typedef struct dllistNode {
    void *data;
    struct dllistNode *prev;
    struct dllistNode *next;
} dllistNode_t;

typedef struct dllistNodeData {
    dllistNode_t *node;
} dllistNodeData_t;

typedef struct dllist {
    dllistNode_t *head;
    dllistNode_t *tail;
    int maxNodes;
    int nodesCount;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int condTimeout;
} dllist_t;

dllist_t *dllistInit(int maxNodes, int condTimeout);
void dllistDestroy(dllist_t *dllist);
int  dllistAppend(dllist_t *dllist, void *data);
void dllistDelete(dllist_t *dllist, void *data);
void dllistVisit(dllist_t *dllist, void (*nodeHandler)(void *data));
int  dllistCountNodes(dllist_t *dllist);

#endif
