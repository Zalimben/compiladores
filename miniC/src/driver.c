/*
 * Coordinación del pipeline de MiniC.
 *
 * En la Entrega 1 el pipeline termina después del preprocesamiento:
 *
 *     archivo.c -> GCC -E -> archivo.i
 *
 * El driver valida la entrada, prepara una salida temporal, ejecuta la
 * herramienta externa y publica el resultado solo cuando la etapa tiene éxito.
 */

/*
 * Solicita las interfaces POSIX utilizadas por este archivo, entre ellas
 * mkstemp(). Debe definirse antes de incluir las cabeceras del sistema.
 */
#define _POSIX_C_SOURCE 200809L

#include "driver.h"

#include "diagnostics.h"
#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Verifica que la entrada exista, sea un archivo regular y pueda leerse.
 *
 * stat() obtiene información del objeto indicado por la ruta; S_ISREG evita
 * aceptar directorios u otros objetos especiales. access() comprueba el
 * permiso efectivo de lectura del proceso actual.
 */
static int validateInput(const char *inputPath) {
    struct stat information;

    if (stat(inputPath, &information) != 0) {
        diagnosticError("no se puede abrir '%s': %s", inputPath, strerror(errno));
        return DRIVER_INPUT_ERROR;
    }

    if (!S_ISREG(information.st_mode)) {
        diagnosticError("la entrada '%s' no es un archivo regular", inputPath);
        return DRIVER_INPUT_ERROR;
    }

    if (access(inputPath, R_OK) != 0) {
        diagnosticError("no se puede leer '%s': %s", inputPath, strerror(errno));
        return DRIVER_INPUT_ERROR;
    }

    return DRIVER_SUCCESS;
}

/*
 * Reserva un nombre temporal en el mismo directorio que la salida final.
 *
 * mkstemp() sustituye las seis X por caracteres únicos y crea el archivo de
 * forma segura, evitando colisiones entre procesos. La función devuelve una
 * cadena dinámica que el llamador debe liberar, o NULL si ocurre un error.
 *
 * Crear el temporal junto a la salida permite que rename() publique luego el
 * resultado de manera atómica dentro del mismo sistema de archivos.
 */
static char *createTemporaryPath(const char *outputPath) {
    static const char suffix[] = ".tmp.XXXXXX";

    /*
     * sizeof(suffix) incluye el byte nulo final, por lo que la suma reserva
     * exactamente el espacio necesario para ambas cadenas.
     */
    size_t requiredLength = strlen(outputPath) + sizeof(suffix);
    char *temporaryPath = malloc(requiredLength);
    int descriptor;

    if (temporaryPath == NULL) {
        diagnosticError("no hay memoria suficiente para preparar la salida");
        return NULL;
    }

    snprintf(temporaryPath, requiredLength, "%s%s", outputPath, suffix);
    descriptor = mkstemp(temporaryPath);
    if (descriptor < 0) {
        diagnosticError(
            "no se pudo crear un archivo temporal para '%s': %s",
            outputPath,
            strerror(errno)
        );
        free(temporaryPath);
        return NULL;
    }

    /*
     * Solo se necesitaba que mkstemp() reservara el nombre con seguridad. GCC
     * abrirá nuevamente el archivo cuando escriba la salida preprocesada.
     */
    if (close(descriptor) != 0) {
        diagnosticError(
            "no se pudo cerrar el archivo temporal '%s': %s",
            temporaryPath,
            strerror(errno)
        );
        unlink(temporaryPath);
        free(temporaryPath);
        return NULL;
    }

    return temporaryPath;
}

/*
 * Ejecuta la etapa de preprocesamiento y publica su producto.
 *
 * Se escribe primero en un temporal. Si GCC falla, ese archivo se elimina y
 * una salida anterior permanece intacta. El vector arguments termina en NULL
 * porque ese es el formato exigido por execvp().
 */
static int preprocess(const DriverOptions *options) {
    char *temporaryPath = createTemporaryPath(options->outputPath);
    ProcessResult result;
    char *arguments[8];
    int argumentIndex = 0;

    if (temporaryPath == NULL) {
        return DRIVER_TEMPORARY_ERROR;
    }

    arguments[argumentIndex++] = "gcc";
    arguments[argumentIndex++] = "-E";

    /*
     * -P es opcional: con esta bandera GCC elimina los marcadores que indican
     * el archivo y número de línea de procedencia del texto preprocesado.
     */
    if (options->suppressLineMarkers) {
        arguments[argumentIndex++] = "-P";
    }
    arguments[argumentIndex++] = (char *) options->inputPath;
    arguments[argumentIndex++] = "-o";
    arguments[argumentIndex++] = temporaryPath;
    arguments[argumentIndex] = NULL;

    result = runProcess(arguments);

    /* Un fallo al crear o esperar el proceso es un error interno del driver. */
    if (!result.started) {
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_INTERNAL_ERROR;
    }

    /*
     * POSIX distingue entre terminación por señal y terminación normal. Solo
     * en el segundo caso existe un exitCode que pueda compararse con cero.
     */
    if (!result.exited) {
        diagnosticError(
            "el preprocesador terminó por la señal %d",
            result.signalNumber
        );
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_PREPROCESS_ERROR;
    }

    if (result.exitCode != 0) {
        diagnosticError(
            "el preprocesamiento terminó con código %d",
            result.exitCode
        );
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_PREPROCESS_ERROR;
    }

    /*
     * rename() reemplaza el nombre final únicamente después de confirmar el
     * éxito de GCC. Si falla, todavía se puede eliminar el temporal.
     */
    if (rename(temporaryPath, options->outputPath) != 0) {
        diagnosticError(
            "no se pudo crear la salida '%s': %s",
            options->outputPath,
            strerror(errno)
        );
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_OUTPUT_ERROR;
    }

    free(temporaryPath);
    return DRIVER_SUCCESS;
}

/*
 * Ejecuta la acción solicitada y retorna un DriverExitCode.
 *
 * Las acciones informativas no necesitan validar un archivo. Para una
 * compilación, en cambio, se detiene inmediatamente ante cualquier error:
 * ninguna etapa posterior debe ejecutarse si una anterior falla.
 */
int runDriver(const DriverOptions *options) {
    int validationResult;

    if (options->action == ACTION_SHOW_HELP) {
        printHelp();
        return DRIVER_SUCCESS;
    }

    if (options->action == ACTION_SHOW_VERSION) {
        printVersion();
        return DRIVER_SUCCESS;
    }

    validationResult = validateInput(options->inputPath);
    if (validationResult != DRIVER_SUCCESS) {
        return validationResult;
    }

    /* Evita que una opción -o sobrescriba directamente el archivo fuente. */
    if (strcmp(options->inputPath, options->outputPath) == 0) {
        diagnosticError("la entrada y la salida no pueden ser el mismo archivo");
        return DRIVER_USAGE_ERROR;
    }

    return preprocess(options);
}
