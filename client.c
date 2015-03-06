#include "client.h"
#include "message.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern cl_t *clList;

cl_t *newCl(int fd)
{
    cl_t *newCl = calloc(1, sizeof(cl_t));
    cl_t *cl;

    newCl->fd = fd;

    if (clList == NULL) {
        clList = newCl;
    } else {
        cl = clList;
        while (cl->next) {
            cl = cl->next;
        }
        cl->next = newCl;
        newCl->prev = cl;
    }

    return newCl;
}

void freeCl(cl_t *cl)
{
    if (cl == clList) {
        clList = cl->next;
    } else {
        cl->prev->next = cl->next;
    }

    close(cl->fd);
    free(cl);
}

void abortClients()
{
    sp_msg(LOG_INFO, "abort clients\n");

    cl_t *cl = clList;
    while (cl) {
        pthread_cancel(cl->tid);
        cl = cl->next;
    }

    int i = 0;
    while (clList) {
        sleep(1);
        sp_msg(LOG_INFO, "aborting ... (%d)\n", ++i);
    }
}

void *handleClient(void *arg)
{
    cl_t *cl = (cl_t *)arg;
    int i;

    pthread_detach(pthread_self());
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    printf("client %d connected\n", cl->fd);
    for (i = 0; i < 1000; i++);
    sleep(20);
    printf("client %d end\n", cl->fd);

    freeCl(cl);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return NULL;
}