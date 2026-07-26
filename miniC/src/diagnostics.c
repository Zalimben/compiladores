/*
 * Módulo de diagnósticos del compiler driver.
 *
 * Centralizar aquí la presentación de errores permite que todos los módulos
 * de MiniC usen el mismo formato. Este archivo se ocupa de mostrar mensajes;
 * la decisión sobre qué código de salida devolver corresponde al driver.
 */
#include "diagnostics.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Muestra un error producido por MiniC.
 *
 * El parámetro format y los argumentos representados por "..." siguen las
 * mismas reglas que printf(). Por ejemplo:
 *
 *     diagnosticError("no se puede abrir '%s'", inputPath);
 *
 * Los diagnósticos se escriben en stderr para mantenerlos separados de la
 * salida normal del programa, que se escribe en stdout.
 */
void diagnosticError(const char *format, ...) {
    va_list arguments;

    fputs("minic: error: ", stderr);

    /*
     * En C, una función con cantidad variable de argumentos debe inicializar
     * un va_list con va_start(), consumirlo y finalizarlo con va_end().
     * vfprintf() es la variante de fprintf() que recibe ese va_list.
     */
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);

    fputc('\n', stderr);
}

/*
 * Muestra un error informado por el sistema operativo.
 *
 * Muchas funciones POSIX indican la causa de un fallo mediante errno. Se debe
 * copiar su valor antes de llamar a otras funciones, porque una llamada
 * posterior podría modificarlo. strerror() convierte el código guardado en
 * una descripción legible.
 */
void diagnosticSystemError(const char *context) {
    int savedErrno = errno;
    diagnosticError("%s: %s", context, strerror(savedErrno));
}
