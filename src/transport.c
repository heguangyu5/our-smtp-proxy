#include "dllist.h"
#include "transport.h"
#include "ini.h"
#include "log.h"
#include "smtp.h"
#include "report.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

extern dllist_t *transports;
extern pthread_mutex_t transportsMtx;

int findTpByName(int idx, void *data, void *name)
{
    tp_t *tp = (tp_t *)data;
    return !(strcmp(tp->name, (char *)name) == 0);
}

static tp_t *newTp(const char *name)
{
    tp_t *tp = calloc(1, sizeof(tp_t));
    
    tp->name = strdup(name);
    tp->busyConns = dllistNew();
    tp->idleConns = dllistNew();
    tp->noopConns = dllistNew();

    pthread_mutex_init(&tp->mtx, NULL);

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&tp->idleCond, &attr);
    pthread_condattr_destroy(&attr);

    pthread_cond_init(&tp->endCond, NULL);

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

static int printTp(int idx, void *data, void *arg)
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

static int checkTp(int idx, void *data, void *arg)
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

static int doTestTp(int idx, void *data, void *arg)
{
    tp_t *tp  = (tp_t *)data;
    int total = *((int *)arg);
    int sockfd;
    char err[1024];
    size_t errlen = 1024;
    tpConn_t *conn;

    conn = calloc(1, sizeof(tpConn_t));
    conn->tp = tp;

    TP_CONFIG_LOG("testing transport %s...(%d/%d)\n", tp->name, (idx+1), total)

    sockfd = tcpConnect(tp->host, tp->port);
    if (sockfd == -1) {
        exit(1);
    }

    conn->sockfd = sockfd;
    if (smtpEHLO(conn, err, errlen)
        && smtpAuth(conn, tp->auth, tp->username, tp->password, err, errlen)
        && smtpQUIT(conn, err, errlen)
    ) {
        if (conn->ssl) {
            SSL_shutdown(conn->ssl);
            SSL_free(conn->ssl);
            SSL_CTX_free(conn->ctx);
        }
        close(sockfd);
        TP_CONFIG_LOG("ok\n")
        return 1;
    }

    TP_CONFIG_LOG("failed !!! %s", err);
    exit(1);
}

void loadTpConfig(int testTp)
{
    pthread_mutex_lock(&transportsMtx);

    TP_CONFIG_LOG("load transport config file: %s\n", TP_INI_FILE)
    // load
    int n;
    if ((n = ini_parse(TP_INI_FILE, tpIniHandler, NULL)) != 0) {
        TP_CONFIG_LOG("ini_parse error(%d)\n", n);
        exit(1);
    }
    // check
    dllistVisit(transports, checkTp, NULL);
    TP_CONFIG_LOG("load %d transport\n", transports->count)
    // print
    dllistVisit(transports, printTp, NULL);
    // test
    if (testTp) {
        // try connect to smtp server
        dllistVisit(transports, doTestTp, &transports->count);
        exit(0);
    }

    pthread_mutex_unlock(&transportsMtx);
}

static void endConn(tpConn_t *conn, int hasTpLock)
{
    tp_t *tp = conn->tp;

    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
        SSL_CTX_free(conn->ctx);
    }
    close(conn->sockfd);

    if (!hasTpLock) {
        pthread_mutex_lock(&tp->mtx);
    }
    dllistDelete(tp->noopConns, conn);
    pthread_mutex_unlock(&tp->mtx);

    pthread_cond_destroy(&conn->cond);
    free(conn);

    pthread_cond_signal(&tp->endCond);
    // 同时也要signal idleCond
    // 试想这种情况: 
    //    一个tp最多3个连接,在3个连接全都busy的情况下,后边的client就wait在idleCond上,
    //    但是这三个连接发完信后,发现发信量够了,或者同时都出错了,要end,结果就是没连接可用了,然后超时
    //    如果在wait的时候有新的连接进来,就会发现可以新建连接,然后它新建个连接,
    //    发完信,idle,如果这些在90s内发生,就会恢复正常,但显然我们不能寄希望于这个新连接
    //    所以,endConn时signal idleCond,后边做下检测,有idle就用idle,没idle就新建连接
    pthread_cond_signal(&tp->idleCond);
}

static void *connNOOP(void *arg)
{
    pthread_detach(pthread_self());

    tpConn_t *conn = (tpConn_t *)arg;
    tp_t *tp = conn->tp;

    while (1) {
        // idle -> noop
        pthread_mutex_lock(&tp->mtx);
        while (conn->status == TP_CONN_BUSY || (conn->status == TP_CONN_IDLE && tp->waiting > 0)) {
            pthread_cond_wait(&conn->cond, &tp->mtx);
        }
        if (conn->status == TP_CONN_IDLE) {
            conn->status = TP_CONN_NOOP;
            dllistMvNode(tp->idleConns, conn->node, tp->noopConns);
        }
        if (conn->endFlag) {
            endConn(conn, 1);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&tp->mtx);
        // noop
        DPRINTF("conn(sockfd %d) noop\n", conn->sockfd);
        conn->noopCount++;
        if (!smtpNOOP(conn) || conn->noopCount == conn->tp->maxNoop) {
            endConn(conn, 0);
            pthread_exit(NULL);
        }
        // noop -> idle
        pthread_mutex_lock(&tp->mtx);
        if (conn->endFlag) {
            endConn(conn, 1);
            pthread_exit(NULL);
        }
        conn->status = TP_CONN_IDLE;
        dllistMvNode(tp->noopConns, conn->node, tp->idleConns);
        pthread_mutex_unlock(&tp->mtx);
        pthread_cond_signal(&tp->idleCond);

        sleep(conn->tp->sleepSecondsPerNoop);
    }
}

static tpConn_t *newConn(tp_t *tp, char *err, size_t errlen)
{
    int sockfd;
    tpConn_t *conn;

    conn = calloc(1, sizeof(tpConn_t));
    conn->tp = tp;

    DPRINTF("tp %s new conn\n", tp->name)

    sockfd = tcpConnect(tp->host, tp->port);
    if (sockfd == -1) {
        TP_LOG("cannot connect to smtp server %s:%s\n", tp->host, tp->port)
        snprintf(err, errlen, "500 cannot connect to smtp server\r\n");
        free(conn);
        return NULL;
    }

    conn->sockfd = sockfd;

    if (smtpEHLO(conn, err, errlen)
        && smtpAuth(conn, tp->auth, tp->username, tp->password, err, errlen)
    ) {
        conn->status = TP_CONN_BUSY;
        pthread_cond_init(&conn->cond, NULL);
        dllistAppend(tp->busyConns, conn);
        pthread_create(&conn->tid, NULL, &connNOOP, conn);
        return conn;
    }

    free(conn);
    close(sockfd);
    return NULL;
}

static tpConn_t *getAnIdleConn(tp_t *tp, int *isNewConn, char *err, size_t errlen)
{
    dllistNode_t *node;
    tpConn_t *conn;
    struct timespec to;

    pthread_mutex_lock(&tp->mtx);
    // 如果有可用的conn,直接返回
    if (tp->idleConns->count) {
        node = tp->idleConns->head;
        conn = (tpConn_t *)node->data;
        conn->status = TP_CONN_BUSY;
        dllistMvNode(tp->idleConns, node, tp->busyConns);
        pthread_mutex_unlock(&tp->mtx);
        return conn;
    }
    // 如果连接数没超,就新建个连接
    if (tp->busyConns->count + tp->noopConns->count < tp->maxConn) {
        conn = newConn(tp, err, errlen);
        *isNewConn = 1;
        pthread_mutex_unlock(&tp->mtx);
        return conn;
    }
    // 没有连接可用的情况下,等着
    // wait两种情况: 1)真的有idleConn 2)有conn end了
    // 90秒超时
    tp->waiting++;
    clock_gettime(CLOCK_MONOTONIC, &to);
    to.tv_sec += 90;
    while (tp->idleConns->count == 0 && tp->busyConns->count + tp->noopConns->count == tp->maxConn) {
        if (pthread_cond_timedwait(&tp->idleCond, &tp->mtx, &to) == ETIMEDOUT) {
            tp->waiting--;
            pthread_mutex_unlock(&tp->mtx);
            TP_LOG("getAnIdleConn timedout\n")
            snprintf(err, errlen, "500 proxy too busy, try later\r\n");
            return NULL;
        }
    }
    tp->waiting--;
    // has idle conn
    if (tp->idleConns->count) {
        node = tp->idleConns->head;
        conn = (tpConn_t *)node->data;
        conn->status = TP_CONN_BUSY;
        dllistMvNode(tp->idleConns, node, tp->busyConns);
        pthread_mutex_unlock(&tp->mtx);
        return conn;
    }
    // can newConn
    conn = newConn(tp, err, errlen);
    *isNewConn = 1;
    pthread_mutex_unlock(&tp->mtx);
    return conn;
}

int tpSendMail(tp_t *tp, rcpt_t *toList, char *data, char *res, size_t reslen)
{
    int isNewConn = 0;
    int ret = 0;
    tpConn_t *conn = getAnIdleConn(tp, &isNewConn, res, reslen);
    if (conn == NULL) {
        return 0;
    }

    conn->noopCount = 0;

    if (!isNewConn) {
        if (!smtpRSET(conn, res, reslen)) {
            DPRINTF("conn(sockfd %d) RSET failed: %s\n", conn->sockfd, res)
            goto end;
        }
    }

    DPRINTF("conn(sockfd %d) send mail\n", conn->sockfd)

    if (smtpMAILFROM(conn, tp->name, res, reslen)
        && smtpRCPTTO(conn, toList, res, reslen)
        && smtpDATA(conn, data, res, reslen)
    ) {
        DPRINTF("conn(sockfd %d) send mail success\n", conn->sockfd)
        conn->sendCount++;
        if (conn->sendCount == tp->maxSendPerConn) {
            ret = 1;
            goto end;
        }
        if (tp->sleepSecondsPerSend) {
            sleep(tp->sleepSecondsPerSend);
        }
        // busy -> idle
        pthread_mutex_lock(&tp->mtx);
        tp->totalSend++;
        tp->todaySend++;
        conn->status = TP_CONN_IDLE;
        dllistMvNode(tp->busyConns, conn->node, tp->idleConns);
        pthread_mutex_unlock(&tp->mtx);
        pthread_cond_signal(&tp->idleCond);
        pthread_cond_signal(&conn->cond);
        return 1;
    }

    DPRINTF("conn(sockfd %d) send mail failed: %s\n", conn->sockfd, res)

end:
    pthread_mutex_lock(&tp->mtx);
    if (ret) {
        tp->totalSend++;
        tp->todaySend++;
    }
    conn->endFlag = 1;
    conn->status  = TP_CONN_NOOP;
    dllistMvNode(tp->busyConns, conn->node, tp->noopConns);
    pthread_mutex_unlock(&tp->mtx);
    pthread_cond_signal(&conn->cond);
    return ret;
}

static int setEndFlag(int idx, void *data, void *arg)
{
    ((tpConn_t *)data)->endFlag = 1;
    return 1;
}

static int abortTpConns(int idx, void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    pthread_mutex_lock(&tp->mtx);

    dllistVisit(tp->noopConns, setEndFlag, NULL);
    dllistVisit(tp->idleConns, setEndFlag, NULL);

    while (tp->noopConns->count + tp->idleConns->count > 0) {
        pthread_cond_wait(&tp->endCond, &tp->mtx);
    }

    pthread_mutex_unlock(&tp->mtx);
    return 1;
}

// abortTpConns之前会先调用abortClients
// 这样当所有的clients都断掉后,conns全都处于idle状态,分布在idleConns和noopConns
void abortTransportsConns()
{
    dllistVisit(transports, abortTpConns, NULL);
}

static int freeTp(int idx, void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    free(tp->name);
    free(tp->host);
    free(tp->port);
    free(tp->ssl);
    free(tp->auth);
    free(tp->username);
    free(tp->password);
    free(tp->busyConns);
    free(tp->idleConns);
    free(tp->noopConns);
    pthread_mutex_destroy(&tp->mtx);
    pthread_cond_destroy(&tp->idleCond);
    pthread_cond_destroy(&tp->endCond);
    free(tp);

    return 1;
}

void freeTransports()
{
    dllistNode_t *node;

    dllistVisit(transports, freeTp, NULL);

    while (transports->head) {
        node = transports->head;
        dllistDeleteNode(transports, node);
        free(node);
    }
}

int reportTp(int idx, void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;
    report_t *report = (report_t *)arg;
    char buf[1024];

    pthread_mutex_lock(&tp->mtx);

    snprintf(buf, 1024, "%30s: %3d busy, %3d idle, %3d noop, total send: %10d, today send = %6d",
             tp->name,
             tp->busyConns->count,
             tp->idleConns->count,
             tp->noopConns->count,
             tp->totalSend,
             tp->todaySend);
    addReportItem(report, buf);

    pthread_mutex_unlock(&tp->mtx);
    return 1;
}

static int resetTpTodaySendCounter(int idx, void *data, void *arg)
{
    tp_t *tp = (tp_t *)data;

    pthread_mutex_lock(&tp->mtx);
    tp->todaySend = 0;
    pthread_mutex_unlock(&tp->mtx);

    return 1;
}

static void resetTodaySendCounter(union sigval val)
{
    pthread_mutex_lock(&transportsMtx);
    dllistVisit(transports, resetTpTodaySendCounter, NULL);
    pthread_mutex_unlock(&transportsMtx);
}

void initTodaySendResetTimer(timer_t *timer)
{
    // 先计算出今天还剩下多少时间
    // 一天86400s,取余得到今天已经过了多少秒(相对于UTC+00:00)
    // 为了得到本机时区对应的时间,需要TZDIFF来调整
    time_t now;
    int todayLeft;
    struct sigevent ev;
    struct itimerspec its;

    memset(&ev, 0, sizeof(ev));
    ev.sigev_notify = SIGEV_THREAD;
    ev.sigev_notify_function = resetTodaySendCounter;

    now = time(NULL);
    todayLeft = 86400 - (now % 86400 + TZDIFF);

    memset(&its, 0, sizeof(its));
    its.it_interval.tv_sec = 86400;
    its.it_value.tv_sec = todayLeft;

    timer_create(CLOCK_REALTIME, &ev, timer);
    timer_settime(*timer, 0, &its, NULL);
}
