#include "dllist.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/**
 * 测试方法:
 *  一个thread一直往链表中追加node,
 *  另一个thread每秒打印一次链表
 *  main thread如果catch到INT信号,就删掉链表的head
 */

typedef struct client {
    dllistNode_t *node;
    int id;
} client_t;

void cutHead(int signo) {}

void *append(void *arg)
{
    dllist_t *dllist = (dllist_t *)arg;
    client_t *cl;
    int i = 0;

    for (;;) {
        i++;
        cl = calloc(1, sizeof(client_t));
        cl->id = i;
        if (!dllistAppend(dllist, cl)) {
            free(cl);
            printf("dllistAppend timedout\n");
        }
        sleep(1);
    }
}

int printNode(void *data, void *arg)
{
    client_t *cl = (client_t *)data;
    printf("%d ", cl->id);
    return 1;
}

void *print(void *arg)
{
    dllist_t *dllist = (dllist_t *)arg;
    for (;;) {
        dllistVisit(dllist, printNode, NULL);
        printf("\n");
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    int maxNodes, condTimeout;
    pthread_t t1, t2;
    dllist_t *dllist;
    client_t *cl;

    if (argc != 3) {
        fprintf(stderr, "usage: %s maxNodes condTimeout\n", argv[0]);
        exit(1);
    }

    maxNodes    = atoi(argv[1]);
    condTimeout = atoi(argv[2]);

    signal(SIGINT, cutHead);

    printf("pid = %d\n", getpid());

    dllist = dllistInit(maxNodes, condTimeout);
    t1 = pthread_create(&t1, NULL, append, (void *)dllist);
    t2 = pthread_create(&t2, NULL, print, (void *)dllist);

    for (;;) {
        pause();
        pthread_mutex_lock(&dllist->mtx);
        cl = NULL;
        if (dllist->head) {
            cl = (client_t *)dllist->head->data;
        }
        pthread_mutex_unlock(&dllist->mtx);
        if (cl) {
            dllistDelete(dllist, cl);
            free(cl);
        }
    }

    return 0;
}
