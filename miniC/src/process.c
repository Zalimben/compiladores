/*
 * Ejecución de procesos externos.
 *
 * Este módulo encapsula fork(), execvp() y waitpid() para que el driver no
 * dependa de los detalles de POSIX. No se invoca un intérprete de comandos:
 * los argumentos se entregan directamente al nuevo proceso y nunca son
 * interpretados por un shell.
 */

/*
 * Habilita las declaraciones POSIX antes de incluir las cabeceras del sistema.
 */
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include "diagnostics.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Ejecuta el programa indicado en arguments[0].
 *
 * arguments debe ser un vector terminado en NULL, igual que argv. El valor
 * retornado informa si el hijo pudo iniciarse y si terminó normalmente o por
 * una señal; interpretar ese resultado corresponde al driver.
 */
ProcessResult runProcess(char *const arguments[]) {
    ProcessResult result = {0, 0, 0, 0};
    pid_t child = fork();
    int status;

    /* fork() retorna un valor negativo cuando no pudo crear el proceso hijo. */
    if (child < 0) {
        diagnosticSystemError("no se pudo iniciar el preprocesador");
        return result;
    }

    /*
     * En el hijo, fork() retorna cero. execvp() reemplaza por completo el
     * proceso hijo con GCC y busca el ejecutable usando PATH.
     */
    if (child == 0) {
        execvp(arguments[0], arguments);

        /*
         * execvp() solo retorna cuando falla. Se usa _exit() para terminar sin
         * ejecutar rutinas de limpieza ni vaciar buffers heredados del padre.
         * El código 127 se utiliza convencionalmente para "no se pudo ejecutar".
         */
        diagnosticSystemError("no se pudo ejecutar 'gcc'");
        _exit(127);
    }

    /* A partir de aquí solo continúa el proceso padre. */
    result.started = 1;

    /*
     * waitpid() bloquea al padre hasta que termina el hijo. Una señal puede
     * interrumpir temporalmente la espera y producir EINTR; en ese caso se
     * intenta nuevamente en lugar de considerar que GCC falló.
     */
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            diagnosticSystemError("no se pudo esperar al preprocesador");
            result.started = 0;
            return result;
        }
    }

    /*
     * El entero status está codificado. Las macros WIF* determinan la clase de
     * terminación y las macros WEXITSTATUS/WTERMSIG extraen el dato relevante.
     */
    if (WIFEXITED(status)) {
        result.exited = 1;
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signalNumber = WTERMSIG(status);
    }

    return result;
}
