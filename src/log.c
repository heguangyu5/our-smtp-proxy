#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

extern FILE *logFile;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void mylog(const char *format, ...)
{
    va_list ap;
    time_t nowt;
    struct tm nowtm;
    char now[20];

    nowt = time(NULL);
    localtime_r(&nowt, &nowtm);
    strftime(now, 20, "%F %T", &nowtm);

    va_start(ap, format);
    pthread_mutex_lock(&mtx);
    fprintf(logFile, "[%s]", now);
    vfprintf(logFile, format, ap);
    pthread_mutex_unlock(&mtx);
    va_end(ap);

    fflush(logFile);
}
