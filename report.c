#include "report.h"
#include "transport.h"
#include "pthread_dllist.h"
#include <string.h>
#include <stdio.h>

extern pthread_dllist_t *clients;
extern dllist_t *transports;
extern pthread_mutex_t transportsMtx;

void collectReport(report_t *report)
{
    char buf[1024];

    memset(report, 0, sizeof(report_t));
    report->maxlen = 4094;

    sprintf(buf, "[clients: %d]", pthread_dllistCountNodes(clients));
    addReportItem(report, buf);

    pthread_mutex_lock(&transportsMtx);
    sprintf(buf, "[transports: %d]", transports->count);
    addReportItem(report, buf);
    dllistVisit(transports, reportTp, report);
    pthread_mutex_unlock(&transportsMtx);
}

void addReportItem(report_t *report, const char *item)
{
    size_t len = strlen(item);
    if (report->len + len > report->maxlen) {
        return;
    }

    strcat(report->buf, item);
    report->len += len;
    report->buf[report->len] = '\n';
    report->len++;
}
