#ifndef __SMTP_H
#define __SMTP_H

#include "rcpt.h"
#include "transport.h"
#include <stddef.h>

int tcpConnect(const char *host, const char *port);
int smtpEHLO(tpConn_t *conn, char *err, size_t errlen);
int smtpAuth(tpConn_t *conn, const char *auth, const char *username, const char *password, char *err, size_t errlen);
int smtpMAILFROM(tpConn_t *conn, const char *from, char *err, size_t errlen);
int smtpRCPTTO(tpConn_t *conn, rcpt_t *toList, char *err, size_t errlen);
int smtpDATA(tpConn_t *conn, const char *data, char *err, size_t errlen);
int smtpRSET(tpConn_t *conn, char *err, size_t errlen);
int smtpNOOP(tpConn_t *conn);
int smtpQUIT(tpConn_t *conn, char *err, size_t errlen);

#endif
