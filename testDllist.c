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
    int id;
} client_t;

void cutHead(int signo) {}

void *append(void *arg)
{
    dllist_t *dllist = (dllist_t *)arg;
    dllistNode_t *node;
    client_t *cl;
    int i = 0;

    for (;;) {
        i++;
        node = calloc(1, sizeof(dllistNode_t));
        cl   = calloc(1, sizeof(client_t));
        cl->id = i;
        node->data = cl;
        dllistAppend(dllist, node);
        sleep(1);
    }
}

void printNode(dllistNode_t *node)
{
    client_t *cl = (client_t *)node->data;
    printf("%d ", cl->id);
}

void *print(void *arg)
{
    dllist_t *dllist = (dllist_t *)arg;
    for (;;) {
        dllistVisit(dllist, printNode);
        printf("\n");
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    int maxNodes;
    pthread_t t1, t2;
    dllist_t *dllist;
    dllistNode_t *node;

    if (argc != 2) {
        fprintf(stderr, "usage: %s maxNodes\n", argv[0]);
        exit(1);
    }

    maxNodes = atoi(argv[1]);

    signal(SIGINT, cutHead);

    printf("pid = %d\n", getpid());

    dllist = dllistInit(maxNodes);
    t1 = pthread_create(&t1, NULL, append, (void *)dllist);
    t2 = pthread_create(&t2, NULL, print, (void *)dllist);

    for (;;) {
        pause();
        pthread_mutex_lock(&dllist->mtx);
        node = dllist->head;
        pthread_mutex_unlock(&dllist->mtx);
        dllistDelete(dllist, node);
        free(node);
    }

    return 0;
}
