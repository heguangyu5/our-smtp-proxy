#include "dllist.h"
#include <stdlib.h>

dllist_t *dllistNew()
{
    dllist_t *dllist = calloc(1, sizeof(dllist_t));
    return dllist;
}

void dllistAppend(dllist_t *dllist, void *data)
{
    dllistNode_t *node = calloc(1, sizeof(dllistNode_t));
    node->data = data;
    ((dllistNodeData_t *)data)->node = node;

    if (dllist->tail == NULL) {
        node->prev = NULL;
        node->next = NULL;
        dllist->head = dllist->tail = node;
    } else {
        node->prev = dllist->tail;
        node->next = NULL;
        dllist->tail->next = node;
        dllist->tail = node;
    }
    dllist->nodesCount++;
}

void dllistDelete(dllist_t *dllist, void *data)
{
    if (data == NULL) {
        return;
    }

    dllistNode_t *node = ((dllistNodeData_t *)data)->node;
    if (node == NULL) {
        return;
    }

    node->data = NULL;
    ((dllistNodeData_t *)data)->node = NULL;

    if (node == dllist->head) { // head
        if (dllist->head->next) {
            dllist->head = dllist->head->next;
            dllist->head->prev = NULL;
        } else {
            dllist->head = dllist->tail = NULL;
        }
        node->next = NULL;
    } else if (node == dllist->tail) { // tail
        dllist->tail = dllist->tail->prev;
        dllist->tail->next = NULL;
        node->prev = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node->prev = NULL;
        node->next = NULL;
    }
    dllist->nodesCount--;
    free(node);
}

void *dllistVisit(dllist_t *dllist, int (*nodeHandler)(int idx, void *data, void *arg), void *arg)
{
    dllistNode_t *node;
    int idx = 0;

    node = dllist->head;
    while (node) {
        if (!nodeHandler(idx++, node->data, arg)) {
            break;
        }
        node = node->next;
    }

    if (node) {
        return node->data;
    }
    return NULL;
}
