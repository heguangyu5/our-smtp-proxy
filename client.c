#include "client.h"
#include "log.h"
#include "rcpt.h"
#include "transport.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

extern dllist_t *transports;
extern pthread_dllist_t *clients;

cl_t *newCl(int fd)
{
    cl_t *cl = calloc(1, sizeof(cl_t));
    cl->fd = fd;
    if (!pthread_dllistAppend(clients, cl, NULL)) {
        free(cl);
        return NULL;
    }
    return cl;
}

static int abortCl(int idx, void *data, void *arg)
{
    cl_t *cl = (cl_t *)data;
    pthread_cancel(cl->tid);
    return 1;
}

void abortClients()
{
    pthread_dllistVisit(clients, abortCl, NULL);

    pthread_mutex_lock(&clients->mtx);
    while (clients->count > 0) {
        pthread_cond_wait(&clients->cond, &clients->mtx);
    }
    pthread_mutex_unlock(&clients->mtx);
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
        snprintf(err, errlen, "500 invalid msg format, DATA command not found\r\n");
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
        snprintf(err, errlen, "500 invalid msg format, recipients not found\r\n");
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

    tp = dllistVisit(transports, findTpByName, from);
    if (tp == NULL) {
        freeToList(toList);
        snprintf(res, reslen, "500 invalid transport, %s not found\r\n", from);
        return;
    }

    if (!tpSendMail(tp, toList, data, res, reslen)) {
        freeToList(toList);
        return;
    }

    freeToList(toList);
}

static void freeBuf(void *arg)
{
    free(arg);
}

static void freeCl(void *arg)
{
    cl_t *cl = (cl_t *)arg;
    close(cl->fd);
    pthread_dllistDelete(clients, cl);
    free(cl);
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

    while (1) {
        pthread_cleanup_push(freeCl, cl);
        pthread_cleanup_push(freeBuf, buf);

        r = poll(fds, 1, 3600 * 24 * 1000);
        if (r == 0) { // timeout
            pthread_exit(NULL);
        }
        if (r < 0) {
            char error[1024];
            strerror_r(errno, error, 1024);
            CLIENT_THREAD_LOG("clinet(socket %d) poll error: %s", cl->fd, error)
            pthread_exit(NULL);
        }

        pthread_cleanup_pop(0);
        pthread_cleanup_pop(0);

        if (fds[0].revents & POLLIN) {
            pthread_cleanup_push(freeCl, cl);
            pthread_cleanup_push(freeBuf, buf);

            n = read(cl->fd, ptr, ptrlen);
            if (n <= 0) {
                pthread_exit(NULL);
            }

            pthread_cleanup_pop(0);
            pthread_cleanup_pop(0);

            msglen += n;
            if (msglen > 5 && strcmp(ptr + n - 5, "\r\n.\r\n") == 0) {
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
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
                    CLIENT_THREAD_LOG("response client(socket %d) '%s' error: %s\n", cl->fd, res, error)
                    free(buf);
                    freeCl(cl);
                    pthread_exit(NULL);
                }
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

                free(buf);
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
        } else {
            free(buf);
            freeCl(cl);
            pthread_exit(NULL);
        }
    }
}
