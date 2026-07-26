#ifndef MINIC_PROCESS_H
#define MINIC_PROCESS_H

typedef struct {
    int started;
    int exited;
    int exitCode;
    int signalNumber;
} ProcessResult;

ProcessResult runProcess(char *const arguments[]);

#endif
