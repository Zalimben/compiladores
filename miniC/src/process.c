#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include "diagnostics.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

ProcessResult runProcess(char *const arguments[]) {
    ProcessResult result = {0, 0, 0, 0};
    pid_t child = fork();
    int status;

    if (child < 0) {
        diagnosticSystemError("no se pudo iniciar el preprocesador");
        return result;
    }

    if (child == 0) {
        execvp(arguments[0], arguments);
        diagnosticSystemError("no se pudo ejecutar 'gcc'");
        _exit(127);
    }

    result.started = 1;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            diagnosticSystemError("no se pudo esperar al preprocesador");
            result.started = 0;
            return result;
        }
    }

    if (WIFEXITED(status)) {
        result.exited = 1;
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signalNumber = WTERMSIG(status);
    }

    return result;
}
