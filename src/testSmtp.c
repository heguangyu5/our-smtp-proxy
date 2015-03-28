#include "log.h"
#include "smtp.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

FILE *logFile;

int main(int argc, char *argv[])
{
    tp_t *tp;
    tpConn_t *conn;

    int  sockfd;
    char err[1024];
    size_t errlen = 1024;

    if (argc < 5) {
        printf("Usage: ./test-smtp host port username password [SSL/TLS]\n");
        exit(1);
    }

    tp = calloc(1, sizeof(tp_t));
    conn = calloc(1, sizeof(tpConn_t));

    tp->host     = argv[1];
    tp->port     = argv[2];
    tp->username = argv[3];
    tp->password = argv[4];
    if (argc == 6) {
        tp->ssl = argv[5];
    }

    // init SSL
    SSL_load_error_strings();
    SSL_library_init();

    conn->tp = tp;

    logFile = stdout;
    // connect
    if ((sockfd = tcpConnect(tp->host, tp->port)) == -1) {
        exit(1);
    }
    conn->sockfd = sockfd;
    // ehlo
    if (!smtpEHLO(conn, err, errlen)) {
        printf("smtpEHLO: %s", err);
        exit(1);
    }
    // auth
    if (!smtpAuth(conn, "LOGIN", tp->username, tp->password, err, errlen)) {
        printf("smtpAuth: %s", err);
        exit(1);
    }
    // send first message
    if (!smtpMAILFROM(conn, tp->username, err, errlen)) {
        printf("smtpMAILFROM: %s", err);
        exit(1);
    }

    rcpt_t to;
    to.email = "employee003@126.com";
    to.next  = NULL;
    if (!smtpRCPTTO(conn, &to, err, errlen)) {
        printf("smtpRCPTTO: %s", err);
        exit(1);
    }

    char msg[1024];
    snprintf(msg, 1024,
"From: %s\r\n"
"To: %s\r\n"
"Subject: test my smtp\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"RT. just test my smtp.\r\n"
".\r\n",
            tp->username,
            to.email
        );
    if (!smtpDATA(conn, msg, err, errlen)) {
        printf("smtpDATA: %s", err);
        exit(1);
    }

    // rset
    if (!smtpRSET(conn, err, errlen)) {
        printf("smtpRSET: %s", err);
        exit(1);
    }

    // send second message
    if (!smtpMAILFROM(conn, tp->username, err, errlen)) {
        printf("smtpMAILFROM: %s", err);
        exit(1);
    }

    rcpt_t to1, to2;
    to1.email = "employee004@126.com";
    to1.next  = &to2;
    to2.email = "626996842@qq.com";
    to2.next  = NULL;
    if (!smtpRCPTTO(conn, &to1, err, errlen)) {
        printf("smtpRCPTTO: %s", err);
        exit(1);
    }

    snprintf(msg, 1024,
"From: %s\r\n"
"To: %s,%s\r\n"
"Subject: test my smtp\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"RT. just test my smtp.\r\n"
".\r\n",
            tp->username,
            to1.email,
            to2.email
        );
    if (!smtpDATA(conn, msg, err, errlen)) {
        printf("smtpDATA: %s", err);
        exit(1);
    }

    // noop
    if (!smtpNOOP(conn)) {
        printf("smtpNOOP error\n");
    }
    // quit
    if (!smtpQUIT(conn, err, errlen)) {
        printf("smtpQUIT error\n");
    }

    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
        SSL_CTX_free(conn->ctx);
    }
    close(sockfd);
    return 0;
}
