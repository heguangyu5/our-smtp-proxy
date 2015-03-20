#ifndef __DLLIST_H
#define __DLLIST_H

#include <pthread.h>

typedef struct dllistNode {
    void *data;
    struct dllistNode *prev;
    struct dllistNode *next;
} dllistNode_t;

typedef struct dllist {
    dllistNode_t *head;
    dllistNode_t *tail;
    int maxNodes;
    int nodesCount;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
} dllist_t;

dllist_t *dllistInit(int maxNodes);
void dllistDestroy(dllist_t *dllist);
void dllistAppend(dllist_t *dllist, dllistNode_t *node);
void dllistDelete(dllist_t *dllist, dllistNode_t *node);
void dllistVisit(dllist_t *dllist, void (*nodeHandler)(dllistNode_t *node));
int  dllistCountNodes(dllist_t *dllist);

#endif
