#ifndef __DLLIST_H
#define __DLLIST_H

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
    int nodesCount;
} dllist_t;

dllist_t *dllistNew();
void dllistAppend(dllist_t *dllist, void *data);
void dllistDelete(dllist_t *dllist, void *data);
void *dllistVisit(dllist_t *dllist, int (*nodeHandler)(int idx, void *data, void *arg), void *arg);

#endif
