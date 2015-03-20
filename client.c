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

extern dllist_t *clients;

cl_t *newCl(int fd)
{
    cl_t *cl = calloc(1, sizeof(cl_t));
    cl->fd = fd;
    if (!dllistAppend(clients, cl)) {
        free(cl);
        return NULL;
    }
    return cl;
}

static void freeCl(void *arg)
{
    cl_t *cl = (cl_t *)arg;
    close(cl->fd);
    dllistDelete(clients, cl);
    free(cl);
}

static void abortCl(void *cl)
{
    pthread_cancel(((cl_t *)cl)->tid);
}

void abortClients()
{
    MAIN_THREAD_LOG("abort clients ...\n")
    dllistVisit(clients, abortCl);

    pthread_mutex_lock(&clients->mtx);
    while (clients->nodesCount > 0) {
        pthread_cond_wait(&clients->cond, &clients->mtx);
    }
    pthread_mutex_unlock(&clients->mtx);

    MAIN_THREAD_LOG("all clients aborted\n");
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

    tp = findTpByName(from);
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
            CLIENT_THREAD_LOG("clinet(socket %d) poll error: %s", cl->fd, error)
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
                    CLIENT_THREAD_LOG("response client(socket %d) '%s' error: %s\n", cl->fd, res, error)
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
