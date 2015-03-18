#include <stdarg.h>
#include <stdio.h>

extern FILE *logFile;

void mylog(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(logFile, format, ap);
    va_end(ap);
}
