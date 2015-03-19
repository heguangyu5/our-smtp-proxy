#include "smtp.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

static int smtpWrite(int sockfd, const char *data, char *err, int errlen)
{
    int n, len;

    len = strlen(data);
    DPRINTF("smtpWrite(socket %d, len %d): %s", sockfd, len, data)
    if ((n = send(sockfd, data, len, 0)) == -1) {
        TP_CONN_LOG("socket %d: send error\n", sockfd)
        snprintf(err, errlen, "500 send error\r\n");
        return 0;
    }
    if (n < len) {
        TP_CONN_LOG("socket %d: partial send\n", sockfd)
        snprintf(err, errlen, "500 partial send\r\n");
        return 0;
    }

    return 1;
}

static int smtpExpect(int sockfd, char *code, int timeout, char *err, int errlen)
{
    int n;
    struct pollfd fds[1];
    char buf[1024];
    char *responseStart;

    memset(buf, 0, 1024);

    fds[0].fd      = sockfd;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    if (poll(fds, 1, timeout * 1000) == -1) {
        TP_CONN_LOG("socket %d: poll error\n", sockfd)
        snprintf(err, errlen, "500 poll error\r\n");
        return 0;
    }

    if (!(fds[0].revents & POLLIN)) {
        TP_CONN_LOG("socket %d: poll error, unexpected revents\n", sockfd)
        snprintf(err, errlen, "500 poll error, unexpected revents\r\n");
        return 0;
    }

    if ((n = recv(sockfd, buf, 1023, 0)) == -1) {
        TP_CONN_LOG("socket %d: recv error\n", sockfd)
        snprintf(err, errlen, "500 recv error\r\n");
        return 0;
    }
    if (n < 4) {
        TP_CONN_LOG("socket %d: smtp server error, invalid response, expect at least 4 bytes\n", sockfd)
        snprintf(err, errlen, "500 smtp server error, invalid response\r\n");
        return 0;
    }

    if (buf[n-1] != '\n') {
        TP_CONN_LOG("socket %d: smtp server error, invalid response, expect end with \\n\n")
        snprintf(err, errlen, "500 smtp server error, invalid response\r\n");
        return 0;
    }

    DPRINTF("smtpExpect(sockfd %d, code %s): %s", sockfd, code, buf)

    buf[n-1] = '\0';
    if ((responseStart = strrchr(buf, '\n')) != NULL) {
        responseStart++;
    } else {
        responseStart = buf;
    }
    buf[n-1] = '\n';

    if (strncmp(code, responseStart, 3) != 0) {
        strncpy(err, buf, errlen);
        return 0;
    }

    return 1;
}

int tcpConnect(const char *host, const char *port)
{
    int sockfd, n;
    struct addrinfo hints, *res, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((n = getaddrinfo(host, port, &hints, &res)) != 0) {
        TP_CONN_LOG("getaddrinfo error for %s:%s, %s\n", host, port, gai_strerror(n))
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sockfd);
    }

    if (rp == NULL) {
        TP_CONN_LOG("cannot connect to %s:%s", host, port)
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

int smtpEHLO(int sockfd, char *err, size_t errlen)
{
    return (smtpExpect(sockfd, "220", 300, err, errlen)
            && smtpWrite(sockfd, "EHLO localhost\r\n", err, errlen)
            && smtpExpect(sockfd, "250", 300, err, errlen));
}

static const char enMap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz"
                            "0123456789"
                            "+/";
#define BAD -1
static const char deMap[] = {
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD, 62, BAD,BAD,BAD, 63,
     52, 53, 54, 55,  56, 57, 58, 59,  60, 61,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
     15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,BAD, BAD,BAD,BAD,BAD,
    BAD, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
     41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD
};

char *base64_encode(const unsigned char *in, int inlen)
{
    char *out, *ptr;
    int outlen;

    outlen = inlen / 3 * 4 + 3;
    if (inlen % 3 != 0) {
        outlen += 4;
    }

    out = malloc(outlen);
    ptr = out;
    for (; inlen >= 3; inlen -= 3) {
        *ptr++ = enMap[in[0] >> 2];
        *ptr++ = enMap[((in[0] << 4) & 0x30) | (in[1] >> 4)];
        *ptr++ = enMap[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
        *ptr++ = enMap[in[2] & 0x3f];
        in += 3;
    }
    if (inlen > 0) {
        *ptr++ = enMap[in[0] >> 2];
        if (inlen == 1) {
            *ptr++ = enMap[((in[0] << 4) & 0x30) | 0];
            *ptr++ = '=';
            *ptr++ = '=';
        } else {
            *ptr++ = enMap[((in[0] << 4) & 0x30) | (in[1] >> 4)];
            *ptr++ = enMap[((in[1] << 2) & 0x3c) | 0];
            *ptr++ = '=';
        }
    }
    *ptr++ = '\r';
    *ptr++ = '\n';
    *ptr = '\0';

    return out;
}

int smtpAuth(int sockfd, const char *auth, const char *username, const char *password, char *err, size_t errlen)
{
    if (strcmp(auth, "LOGIN") != 0) {
        snprintf(err, errlen, "500 unsupported smtp auth method\r\n");
        return 0;
    }

    if (!smtpWrite(sockfd, "AUTH LOGIN\r\n", err, errlen) || !smtpExpect(sockfd, "334", 60, err, errlen)) {
        return 0;
    }

    char *u64 = base64_encode((unsigned char *)username, strlen(username));
    if (!smtpWrite(sockfd, u64, err, errlen) || !smtpExpect(sockfd, "334", 60, err, errlen)) {
        free(u64);
        return 0;
    }
    free(u64);

    char *p64 = base64_encode((unsigned char *)password, strlen(password));
    if (!smtpWrite(sockfd, p64, err, errlen) || !smtpExpect(sockfd, "235", 60, err, errlen)) {
        free(p64);
        return 0;
    }
    free(p64);

    return 1;
}

int smtpMAILFROM(int sockfd, const char *from, char *err, size_t errlen)
{
    char buf[1024];
    snprintf(buf, 1024, "MAIL FROM:<%s>\r\n", from);
    return (smtpWrite(sockfd, buf, err, errlen) && smtpExpect(sockfd, "250", 300, err, errlen));
}

int smtpRCPTTO(int sockfd, rcpt_t *toList, char *err, size_t errlen)
{
    char buf[1024];
    rcpt_t *to = toList;
    while (to) {
        snprintf(buf, 1024, "RCPT TO:<%s>\r\n", to->email);
        if (!smtpWrite(sockfd, buf, err, errlen) || !smtpExpect(sockfd, "250", 300, err, errlen)) {
            if (strncmp("251", err, 3) != 0) {
                return 0;
            }
        }
        to = to->next;
    }
    return 1;
}

int smtpDATA(int sockfd, const char *data, char *err, size_t errlen)
{
    return (smtpWrite(sockfd, "DATA\r\n", err, errlen)
            && smtpExpect(sockfd, "354", 120, err, errlen)
            && smtpWrite(sockfd, data, err, errlen)
            && smtpExpect(sockfd, "250", 600, err, errlen));
}

int smtpRSET(int sockfd, char *err, size_t errlen)
{
    return (smtpWrite(sockfd, "RSET\r\n", err, errlen)
            && (smtpExpect(sockfd, "250", 60, err, errlen) || strncmp("220", err, 3) == 0));
}

int smtpNOOP(int sockfd)
{
    char err[1024];
    return (smtpWrite(sockfd, "NOOP\r\n", err, 1024) && smtpExpect(sockfd, "250", 300, err, 1024));
}

int smtpQUIT(int sockfd)
{
    char err[1024];
    return (smtpWrite(sockfd, "QUIT\r\n", err, 1024) && smtpExpect(sockfd, "221", 300, err, 1024));
}
