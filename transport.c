#include "transport.h"
#include "ini.h"
#include "message.h"
#include <stdlib.h>
#include <string.h>

extern char *tpIni;
extern tp_t *tpList;

tp_t *findTpByName(const char *name)
{
    tp_t *tp = tpList;

    while (tp) {
        if (strcmp(tp->name, name) == 0) {
            return tp;
        }
        tp = tp->next;
    }

    return NULL;
}

static tp_t *newTp(const char *name)
{
    tp_t *newTp = calloc(1, sizeof(tp_t));
    tp_t *tp = tpList;

    newTp->name = strdup(name);

    if (tpList == NULL) {
        tpList = newTp;
    } else {
        while (tp->next) {
            tp = tp->next;
        }
        tp->next = newTp;
    }

    return newTp;
}

static int tpIniHandler(void *user, const char *section, const char *name, const char *value)
{
    // find tp by section
    tp_t *tp = findTpByName(section);
    if (tp == NULL) {
        tp = newTp(section);
    }

    #define HANDLE_STR_ITEM(n) if (strcmp(name, #n) == 0) { tp->n = strdup(value); }
    #define HANDLE_INT_ITEM(n) if (strcmp(name, #n) == 0) { tp->n = atoi(value); }
    HANDLE_STR_ITEM(host)
    else HANDLE_STR_ITEM(port)
    else HANDLE_STR_ITEM(ssl)
    else HANDLE_STR_ITEM(auth)
    else HANDLE_STR_ITEM(username)
    else HANDLE_STR_ITEM(password)
    else HANDLE_INT_ITEM(maxConn)
    else HANDLE_INT_ITEM(maxSendPerConn)
    else HANDLE_INT_ITEM(sleepSecondsPerSend)
    else HANDLE_INT_ITEM(maxNoop)
    else {
        sp_msg(LOG_INFO, "unknown item %s = %s\n", name, value);
        return 0;
    }

    return 1;
}

void freeTpList()
{
    tp_t *tp;

    sp_msg(LOG_INFO, "free tpList\n");
    #define SAFE_FREE(m) if (m) free(m)
    while (tpList) {
        tp = tpList;
        SAFE_FREE(tp->host);
        SAFE_FREE(tp->port);
        SAFE_FREE(tp->ssl);
        SAFE_FREE(tp->auth);
        SAFE_FREE(tp->username);
        SAFE_FREE(tp->password);
        tpList = tpList->next;
        free(tp);
    }
}

static int checkTpList()
{
    int tpCount = 0;
    tp_t *tp = tpList;

    #define REQUIRED(n) if (n == NULL) { sp_msg(LOG_ERR, #n " required\n"); return 0; }
    while (tp) {
        REQUIRED(tp->host)
        REQUIRED(tp->port)
        REQUIRED(tp->ssl)
        REQUIRED(tp->auth)
        REQUIRED(tp->username)
        REQUIRED(tp->password)
        if (strcmp(tp->ssl, "") != 0 && strcmp(tp->ssl, "SSL") != 0) {
            sp_msg(LOG_ERR, "unsupported tp->ssl: %s\n", tp->ssl);
            return 0;
        }
        if (strcmp(tp->auth, "LOGIN") != 0) {
            sp_msg(LOG_ERR, "unsupported tp->auth: %s\n", tp->auth);
            return 0;
        }
        if (tp->maxConn < 1) {
            sp_msg(LOG_ERR, "tp->maxConn at least 1\n");
            return 0;
        }
        tp = tp->next;
        tpCount++;
    }

    return tpCount;
}

static void printTpList()
{
    tp_t *tp = tpList;
    while (tp) {
        sp_msg(LOG_INFO,
"\n[%s]\n"
"host                = %s\n"
"port                = %s\n"
"ssl                 = %s\n"
"auth                = %s\n"
"username            = %s\n"
"password            = %s\n"
"maxConn             = %d\n"
"maxSendPerConn      = %d\n"
"sleepSecondsPerSend = %d\n"
"maxNoop             = %d\n"
"\n",
            tp->name,
            tp->host,
            tp->port,
            tp->ssl,
            tp->auth,
            tp->username,
            tp->password,
            tp->maxConn,
            tp->maxSendPerConn,
            tp->sleepSecondsPerSend,
            tp->maxNoop
);
        tp = tp->next;
    }
}

int loadTpConfig()
{
    sp_msg(LOG_INFO, "load transport config file: %s\n", tpIni);
    if (ini_parse(tpIni, tpIniHandler, NULL) < 0) {
        freeTpList();
        return 0;
    }

    int tpCount;
    if ((tpCount = checkTpList()) == 0) {
        freeTpList();
        return 0;
    }

    sp_msg(LOG_INFO, "%d transport loaded\n", tpCount);
    printTpList();

    return tpCount;
}