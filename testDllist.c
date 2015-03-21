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

void cutHead(int signo)
{
    isCutHead = 1;
}

int main(void)
{
    dllist_t *dllist = dllistNew();
    int i = 0;
    tp_t *tp;

    signal(SIGINT, cutHead);

    printf("pid = %d\n", getpid());

    for (;;) {
        i++;
        tp = calloc(1, sizeof(tp_t));
        tp->id = i;
        dllistAppend(dllist, tp);
        dllistVisit(dllist, printTp, NULL);
        printf("\n");
        if (isCutHead) {
            if (dllist->nodesCount > 0) {
                tp = (tp_t *)dllist->head->data;
                dllistDelete(dllist, tp);
                free(tp);
            }
            isCutHead = 0;
        }
        sleep(2);
    }

    return 0;
}
