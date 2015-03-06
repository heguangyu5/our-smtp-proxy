#include "message.h"
#include <stdarg.h>
#include <stdio.h>

extern int daemonized;

void sp_msg(int level, char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    if (daemonized) {
    } else {
        FILE *fp;
        if (level == LOG_INFO) {
            fp = stdout;
            fprintf(fp, "info: ");
        } else {
            fp = stderr;
            fprintf(fp, "err : ");
        }
        vfprintf(fp, format, ap);
    }
    va_end(ap);
}
