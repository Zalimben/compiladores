#ifndef MINIC_DIAGNOSTICS_H
#define MINIC_DIAGNOSTICS_H

#include <stddef.h>

void diagnosticError(const char *format, ...);
void diagnosticInfo(const char *format, ...);
void diagnosticSourceError(
    const char *path,
    size_t line,
    size_t column,
    const char *format,
    ...
);
void diagnosticSystemError(const char *context);

#endif
