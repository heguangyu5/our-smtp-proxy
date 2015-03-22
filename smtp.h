#ifndef __SMTP_H
#define __SMTP_H

#include "rcpt.h"
#include <stddef.h>

int tcpConnect(const char *host, const char *port);
int smtpEHLO(int sockfd, char *err, size_t errlen);
int smtpAuth(int sockfd, const char *auth, const char *username, const char *password, char *err, size_t errlen);
int smtpMAILFROM(int sockfd, const char *from, char *err, size_t errlen);
int smtpRCPTTO(int sockfd, rcpt_t *toList, char *err, size_t errlen);
int smtpDATA(int sockfd, const char *data, char *err, size_t errlen);
int smtpRSET(int sockfd, char *err, size_t errlen);
int smtpNOOP(int sockfd);
int smtpQUIT(int sockfd, char *err, size_t errlen);

#endif
