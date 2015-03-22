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
    int count;
} dllist_t;

dllist_t *dllistNew();
void dllistAppend(dllist_t *dllist, void *data);
void dllistDelete(dllist_t *dllist, void *data);
void dllistAppendNode(dllist_t *dllist, dllistNode_t *node);
void dllistDeleteNode(dllist_t *dllist, dllistNode_t *node);
void dllistMvNode(dllist_t *a, dllistNode_t *aNode, dllist_t *b);
void *dllistVisit(dllist_t *dllist, int (*nodeHandler)(int idx, void *data, void *arg), void *arg);

#endif
