#include "dllist.h"
#include "transport.h"
#include "ini.h"
#include "log.h"
#include "smtp.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern dllist_t *transports;

int findTpByName(void *tp, void *name)
{
    return !(strcmp(((tp_t *)tp)->name, (char *)name) == 0);
}

static tp_t *newTp(const char *name)
{
    tp_t *tp = calloc(1, sizeof(tp_t));
    tp->name = strdup(name);
    pthread_mutex_init(&tp->mtx, NULL);
    dllistAppend(transports, tp);
    return tp;
}

static int tpIniHandler(void *user, const char *section, const char *name, const char *value)
{
    // find tp by section
    tp_t *tp = dllistVisit(transports, findTpByName, (void *)section);
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
    else HANDLE_INT_ITEM(sleepSecondsPerNoop)
    else {
        TP_CONFIG_LOG("unknown config item %s = %s\n", name, value)
        return 0;
    }

    return 1;
}

static int freeTp(void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    free(tp->host);
    free(tp->port);
    free(tp->ssl);
    free(tp->auth);
    free(tp->username);
    free(tp->password);
    pthread_mutex_destroy(&tp->mtx);

    return 1;
}

void freeTpList()
{
    TP_CONFIG_LOG("free tpList\n")
    dllistVisit(transports, freeTp, NULL);
    TP_CONFIG_LOG("all transports are freed\n");
}

static int printTp(void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    mylog(
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
        "sleepSecondsPerNoop = %d\n"
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
        tp->maxNoop,
        tp->sleepSecondsPerNoop
    );

    return 1;
}

static int checkTp(void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    #define REQUIRED(n) if (n == NULL) { TP_CONFIG_LOG(#n " required\n") exit(1); }
    REQUIRED(tp->host)
    REQUIRED(tp->port)
    REQUIRED(tp->ssl)
    REQUIRED(tp->auth)
    REQUIRED(tp->username)
    REQUIRED(tp->password)
    if (strcmp(tp->ssl, "") != 0 && strcmp(tp->ssl, "SSL") != 0) {
        TP_CONFIG_LOG("unsupported ssl value: %s\n", tp->ssl)
        exit(1);
    }
    if (strcmp(tp->auth, "LOGIN") != 0) {
        TP_CONFIG_LOG("unsupported auth method: %s\n", tp->auth)
        exit(1);
    }
    if (tp->maxConn < 1) {
        TP_CONFIG_LOG("maxConn at least 1\n")
        exit(1);
    }

    return 1;
}

void loadTpConfig(int testTp)
{
    TP_CONFIG_LOG("load transport config file: %s\n", TP_INI_FILE)
    // load
    int n;
    if ((n = ini_parse(TP_INI_FILE, tpIniHandler, NULL)) != 0) {
        TP_CONFIG_LOG("ini_parse error(%d)\n", n);
        exit(1);
    }
    // check
    dllistVisit(transports, checkTp, NULL);
    TP_CONFIG_LOG("load %d transport\n", dllistCountNodes(transports))
    // print
    dllistVisit(transports, printTp, NULL);
    // test
    if (testTp) {
        // try connect to smtp server
        int i;
        for (i = 1; i <= 2; i++) {
            TP_CONFIG_LOG("testing transport %s...(%d/%d)\n", "xxx", i, 2)
            TP_CONFIG_LOG("ok\n")
        }
        exit(0);
    }
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
        sleep(conn->tp->sleepSecondsPerNoop);
    }
}

static tpConn_t *newConn(tp_t *tp, char *err, size_t errlen)
{
    int sockfd;
    tpConn_t *newConn;
    tpConn_t *conn;

    sockfd = tcpConnect(tp->host, tp->port);
    if (sockfd == -1) {
        TP_LOG("cannot connect to smtp server %s:%s\n", tp->host, tp->port)
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
        if (tp->conn == NULL) {
            tp->conn = newConn;
        } else {
            conn = tp->conn;
            while (conn->next) {
                conn = conn->next;
            }
            conn->next    = newConn;
            newConn->prev = conn;
        }
        return newConn;
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

    TP_LOG("getAFreeConn timedout\n")
    snprintf(err, errlen, "500 server too busy, try later\r\n");

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

    if (!isNewConn) {
        if (!smtpRSET(conn->sockfd, res, reslen)) {
            pthread_cancel(conn->tid);
            endConn(conn);
            return 0;
        }
    }

    if (smtpMAILFROM(conn->sockfd, tp->name, res, reslen)
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
