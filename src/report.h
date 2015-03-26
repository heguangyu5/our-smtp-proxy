#ifndef __REPORT_H
#define __REPORT_H

#include <stddef.h>

typedef struct report {
    char buf[4096];
    size_t maxlen;
    size_t len;
} report_t;

void collectReport(report_t *report);
void addReportItem(report_t *report, const char *item);

#endif
