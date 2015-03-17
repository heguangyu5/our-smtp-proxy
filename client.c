#include "client.h"
#include "message.h"
#include "rcpt.h"
#include "transport.h"
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

void freeCl(void *arg)
{
    cl_t *cl = (cl_t *)arg;

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

static int prepareSendMail(char *msg, char **from, rcpt_t **toList, char **data, char *err, size_t errlen)
{
    // msg format:
    // from\r\n
    // to1\r\n
    // to2\r\n
    // DATA\r\n
    // msg body\r\n
    // .\r\n

    // data
    *data = strstr(msg, "\r\nDATA\r\n");
    if (*data == NULL) {
        snprintf(err, errlen, "500 syntax error - invalid msg format\r\n");
        return 0;
    }
    memset(*data, 0, 8);
    *data += 8;
    // from
    *from = msg;
    // toList
    char *to;
    to = strstr(msg, "\r\n");
    if (to == NULL) {
        snprintf(err, errlen, "500 syntax error - invalid msg format\r\n");
        return 0;
    }
    do {
        memset(to, 0, 2);
        to += 2;
        newTo(toList, to);
    } while ((to = strstr(to, "\r\n")) != NULL);

    return 1;
}

static void sendMail(char *msg, char *res, size_t reslen)
{
    char *from     = NULL;
    rcpt_t *toList = NULL;
    char *data     = NULL;
    tp_t *tp;

    if (!prepareSendMail(msg, &from, &toList, &data, res, reslen)) {
        return;
    }

    tp = findTpByName(from);
    if (tp == NULL) {
        freeToList(toList);
        snprintf(res, reslen, "500 transport not found - invalid transport %s\r\n", from);
        return;
    }

    if (!tpSendMail(tp, toList, data, res, reslen)) {
        freeToList(toList);
        return;
    }

    freeToList(toList);
}

void *handleClient(void *arg)
{
    cl_t *cl = (cl_t *)arg;

    struct pollfd fds[1];
    fds[0].fd      = cl->fd;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    int r, n;
    char *buf, *ptr;
    size_t blksize, buflen, ptrlen, msglen;

    blksize = 2048;
    buf     = calloc(1, blksize);
    ptr     = buf;
    buflen  = blksize;
    ptrlen  = blksize;
    msglen  = 0;

    char res[1024];
    size_t reslen = 1024;

    pthread_detach(pthread_self());
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (1) {
        r = poll(fds, 1, 3600 * 24 * 1000);
        if (r == 0) { // timeout
            free(buf);
            break;
        }
        if (r < 0) {
            char error[1024];
            strerror_r(errno, error, 1024);
            sp_msg(LOG_ERR, "poll error: %s", error);
            free(buf);
            break;
        }
        if (fds[0].revents & POLLIN) {
            n = read(cl->fd, ptr, ptrlen);
            if (n <= 0) {
                free(buf);
                break;
            }
            msglen += n;
            if (msglen > 5 && strcmp(ptr + n - 5, "\r\n.\r\n") == 0) {
                if (buflen == msglen) {
                    buf = realloc(buf, buflen + 1);
                    buf[buflen] = '\0';
                }
                // send email
                memset(res, 0, reslen);
                sendMail(buf, res, reslen);
                if (write(cl->fd, res, strlen(res)) == -1) {
                    char error[1024];
                    strerror_r(errno, error, 1024);
                    sp_msg(LOG_ERR, "write err msg '%s' to client error: %s\n", res, error);
                    free(buf);
                    break;
                }
                free(buf);

                pthread_cleanup_push(freeCl, cl);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                pthread_testcancel();
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                pthread_cleanup_pop(0);

                buf    = calloc(1, blksize);
                ptr    = buf;
                buflen = blksize;
                ptrlen = blksize;
                msglen = 0;
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

    freeCl(cl);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return NULL;
}
