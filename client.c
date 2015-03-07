#include "client.h"
#include "message.h"
#include "rcpt.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

extern cl_t *clList;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_CLLIST pthread_mutex_lock(&mtx);
#define UNLOCK_CLLIST pthread_mutex_unlock(&mtx);

cl_t *newCl(int fd)
{
    cl_t *newCl = calloc(1, sizeof(cl_t));
    cl_t *cl;

    newCl->fd = fd;

    LOCK_CLLIST

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

    UNLOCK_CLLIST

    return newCl;
}

void freeCl(cl_t *cl)
{
    LOCK_CLLIST

    if (cl == clList) { // head
        clList = cl->next;
    } else if (cl->next == NULL) { // tail
        cl->prev->next = NULL;
    } else {
        cl->prev->next = cl->next;
        cl->next->prev = cl->prev;
    }

    UNLOCK_CLLIST

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

    char *from     = NULL;
    rcpt_t *toList = NULL;
    char *data     = NULL;

    struct pollfd fds[1];
    fds[0].fd      = cl->fd;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    int r, n;
    char *buf, *ptr;
    size_t blksize, buflen, ptrlen;

    blksize = 2048;
    buf     = calloc(1, blksize);
    ptr     = buf;
    buflen  = blksize;
    ptrlen  = blksize;

    pthread_detach(pthread_self());
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (1) {
        r = poll(fds, 1, 3600 * 24 * 1000);
        if (r == 0) { // timeout
            break;
        }
        if (r < 0) {
            char buf[1024];
            strerror_r(errno, buf, 1024);
            sp_msg(LOG_ERR, "poll error: %s", buf);
            break;
        }
        if (fds[0].revents & POLLIN) {
            n = read(cl->fd, ptr, ptrlen);
            if (n <= 0) {
                break;
            }
            if (strcmp(ptr + n - 3, ".\r\n") == 0) {
                // send email
                printf("client %d-%s\n", cl->fd, buf);
                free(buf);
                buf    = calloc(1, blksize);
                ptr    = buf;
                buflen = blksize;
                ptrlen = blksize;
            } else if (buf[buflen - 1] != '\0') {
                // buf not large enough
                buf    = realloc(buf, buflen + blksize);
                ptr    = buf + buflen;
                buflen += blksize;
                ptrlen = blksize;
                memset(ptr, 0, ptrlen);
            } else {
                ptr += n;
                ptrlen -= n;
            }
        }
    }

    freeToList(toList);
    freeCl(cl);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return NULL;
}
