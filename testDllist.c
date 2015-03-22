#include "dllist.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

typedef struct tp {
    dllistNode_t *node;
    int id;
} tp_t;

int printTp(int idx, void *data, void *arg)
{
    tp_t *tp  = (tp_t *)data;
    printf("%d:%d ", idx, tp->id);
    return 1;
}

int isCutHead;
int cutCount;

void cutHead(int signo)
{
    isCutHead = 1;
    cutCount++;
}

int main(void)
{
    dllist_t *dllist = dllistNew();
    dllist_t *dllistDeleted = dllistNew();
    int i = 0;
    tp_t *tp;
    dllistNode_t *head;

    signal(SIGINT, cutHead);

    printf("pid = %d\n", getpid());

    for (;;) {
        i++;
        tp = calloc(1, sizeof(tp_t));
        tp->id = i;
        dllistAppend(dllist, tp);
        printf("dllist: ");
        dllistVisit(dllist, printTp, NULL);
        printf("\n");
        printf("delete: ");
        dllistVisit(dllistDeleted, printTp, NULL);
        printf("\n");
        if (isCutHead) {
            head = dllist->head;
            if (cutCount % 2 == 0) {
                dllistMvNode(dllist, head, dllistDeleted);
            } else {
                dllistDelete(dllist, head->data);
                free(head->data);
            }
            isCutHead = 0;
        }
        sleep(2);
    }

    return 0;
}
