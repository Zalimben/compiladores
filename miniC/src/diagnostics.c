#include "diagnostics.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void diagnosticError(const char *format, ...) {
    va_list arguments;

    fputs("minic: error: ", stderr);
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

void diagnosticSystemError(const char *context) {
    int savedErrno = errno;
    diagnosticError("%s: %s", context, strerror(savedErrno));
}
