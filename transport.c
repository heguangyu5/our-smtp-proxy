#include "transport.h"
#include "ini.h"
#include "message.h"
#include "smtp.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    pthread_mutex_init(&newTp->mtx, NULL);

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
        pthread_mutex_destroy(&tp->mtx);
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

#define LOCK_TP(tp) pthread_mutex_lock(&tp->mtx);
#define UNLOCK_TP(tp) pthread_mutex_unlock(&tp->mtx);

#define LOCK_TP_CONN(conn) pthread_mutex_lock(&conn->mtx);
#define UNLOCK_TP_CONN(conn) pthread_mutex_unlock(&conn->mtx);

static void endConn(tpConn_t *conn)
{
    tp_t *tp = conn->tp;
    // remove from tp conn list first
    LOCK_TP(conn->tp)
    if (conn == tp->conn) { // head
        tp->conn = conn->next;
    } else if (conn->next == NULL) { // tail
        conn->prev->next = NULL;
    } else {
        conn->prev->next = conn->next;
        conn->next->prev = conn->prev;
    }
    UNLOCK_TP(conn->tp)
    // end it
    UNLOCK_TP_CONN(conn)
    pthread_mutex_destroy(&conn->mtx);
    close(conn->sockfd);
    free(conn);
}

static void *connNOOP(void *arg)
{
    pthread_detach(pthread_self());
    tpConn_t *conn = (tpConn_t *)arg;

    while (1) {
        LOCK_TP_CONN(conn)
        conn->noopCount++;
        if (!smtpNOOP(conn->sockfd) || conn->noopCount == conn->tp->maxNoop) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            endConn(conn);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            pthread_exit(NULL);
        }
        UNLOCK_TP_CONN(conn)
        sleep(2);
    }
}

static tpConn_t *newConn(tp_t *tp, char *err, size_t errlen)
{
    int sockfd;
    tpConn_t *newConn;
    tpConn_t *conn;

    sockfd = tcpConnect(tp->host, tp->port);
    if (sockfd == -1){
        snprintf(err, errlen, "500 cannot connect to smtp server\r\n");
        return NULL;
    }

    if (smtpEHLO(sockfd, err, errlen)
        && smtpAuth(sockfd, tp->auth, tp->username, tp->password, err, errlen)
    ) {
        newConn = calloc(1, sizeof(tpConn_t));
        newConn->tp     = tp;
        newConn->sockfd = sockfd;
        pthread_mutex_init(&newConn->mtx, NULL);
        LOCK_TP_CONN(newConn)
        pthread_create(&newConn->tid, NULL, &connNOOP, newConn);
        conn = tp->conn;
        while (conn->next) {
            conn = conn->next;
        }
        conn->next    = newConn;
        newConn->prev = conn;
        return conn;
    }

    close(sockfd);
    return NULL;
}

static tpConn_t *getAFreeConn(tp_t *tp, int *isNewConn, char *err, size_t errlen)
{
    tpConn_t *conn;
    int maxLoop = 300; // timeout = 300 * 0.3s = 90s

    while (maxLoop--) {
        LOCK_TP(tp)
        conn = tp->conn;
        while (conn) {
            if (pthread_mutex_trylock(&conn->mtx) == 0) {
                UNLOCK_TP(tp)
                return conn;
            }
            conn = conn->next;
        }
        // free conn not found
        if (tp->connCount < tp->maxConn) {
            conn = newConn(tp, err, errlen);
            *isNewConn = 1;
            UNLOCK_TP(tp)
            return conn;
        }
        // later try again
        UNLOCK_TP(tp)
        usleep(300000); // 300ms = 0.3s
    }

    sp_msg(LOG_ERR, "cannot find an idle connection\n");
    snprintf(err, errlen, "500 cannot find an idle connection\r\n");

    return NULL;
}

int tpSendMail(tp_t *tp, rcpt_t *toList, char *data, char *res, size_t reslen)
{
    int isNewConn = 0;
    tpConn_t *conn = getAFreeConn(tp, &isNewConn, res, reslen);
    if (conn == NULL) {
        return 0;
    }

    conn->noopCount = 0;
    if ((!isNewConn && smtpRSET(conn->sockfd, res, reslen))
        && smtpMAILFROM(conn->sockfd, tp->name, res, reslen)
        && smtpRCPTTO(conn->sockfd, toList, res, reslen)
        && smtpDATA(conn->sockfd, data, res, reslen)
    ) {
        conn->sendCount++;
        if (conn->sendCount == tp->maxSendPerConn) {
            pthread_cancel(conn->tid);
            endConn(conn);
            return 1;
        }
        if (tp->sleepSecondsPerSend) {
            sleep(tp->sleepSecondsPerSend);
        }
        UNLOCK_TP_CONN(conn)
        return 1;
    }

    pthread_cancel(conn->tid);
    endConn(conn);
    return 0;
}

void abortTpConns()
{
    tp_t *tp = tpList;
    while (tp) {
        while (tp->conn) {
            endConn(tp->conn);
        }
        tp = tp->next;
    }
}
