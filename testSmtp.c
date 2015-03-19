#include "log.h"
#include "smtp.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

FILE *logFile;

int main(int argc, char *argv[])
{
    char *host;
    char *port;
    char *username;
    char *password;
    int  sockfd;
    char err[1024];
    size_t errlen = 1024;

    if (argc != 5) {
        printf("Usage: ./test-smtp host port username password\n");
        exit(1);
    }

    host     = argv[1];
    port     = argv[2];
    username = argv[3];
    password = argv[4];

    logFile = stdout;
    // connect
    if ((sockfd = tcpConnect(host, port)) == -1) {
        exit(1);
    }
    // ehlo
    if (!smtpEHLO(sockfd, err, errlen)) {
        printf("smtpEHLO: %s", err);
        exit(1);
    }
    // auth
    if (!smtpAuth(sockfd, "LOGIN", username, password, err, errlen)) {
        printf("smtpAuth: %s", err);
        exit(1);
    }
    // send first message
    if (!smtpMAILFROM(sockfd, username, err, errlen)) {
        printf("smtpMAILFROM: %s", err);
        exit(1);
    }

    rcpt_t to;
    to.email = "employee003@126.com";
    to.next  = NULL;
    if (!smtpRCPTTO(sockfd, &to, err, errlen)) {
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
            username,
            to.email
        );
    if (!smtpDATA(sockfd, msg, err, errlen)) {
        printf("smtpDATA: %s", err);
        exit(1);
    }

    // rset
    if (!smtpRSET(sockfd, err, errlen)) {
        printf("smtpRSET: %s", err);
        exit(1);
    }

    // send second message
    if (!smtpMAILFROM(sockfd, username, err, errlen)) {
        printf("smtpMAILFROM: %s", err);
        exit(1);
    }

    rcpt_t to1, to2;
    to1.email = "employee004@126.com";
    to1.next  = &to2;
    to2.email = "626996842@qq.com";
    to2.next  = NULL;
    if (!smtpRCPTTO(sockfd, &to1, err, errlen)) {
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
            username,
            to1.email,
            to2.email
        );
    if (!smtpDATA(sockfd, msg, err, errlen)) {
        printf("smtpDATA: %s", err);
        exit(1);
    }

    // noop
    if (!smtpNOOP(sockfd)) {
        printf("smtpNOOP error\n");
    }
    // quit
    if (!smtpQUIT(sockfd)) {
        printf("smtpQUIT error\n");
    }

    close(sockfd);
    return 0;
}
