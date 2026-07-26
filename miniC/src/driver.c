/*
 * Coordinación del pipeline de MiniC.
 *
 * En la Entrega 2 están disponibles dos recorridos:
 *
 *     -E: archivo.c -> GCC -E       -> archivo.i
 *     -S: archivo.c -> GCC -E -P    -> compileFile() -> archivo.s
 *
 * El driver administra argumentos, archivos y etapas. En esta entrega,
 * compiler.c implementa compileFile() como un mock basado en GCC.
 */

/*
 * Solicita las interfaces POSIX utilizadas por este archivo, entre ellas
 * mkstemp(). Debe definirse antes de incluir las cabeceras del sistema.
 */
#define _POSIX_C_SOURCE 200809L

#include "driver.h"

#include "compiler.h"
#include "diagnostics.h"
#include "process.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * Devuelve una nueva ruta con la extensión indicada. El archivo de entrada ya
 * fue validado como .c, por lo que se eliminan sus dos últimos caracteres.
 */
static char *replaceExtension(const char *path, const char *extension) {
    size_t pathLength = strlen(path);
    size_t extensionLength = strlen(extension);
    size_t resultLength = pathLength - 2 + extensionLength;
    char *result = malloc(resultLength + 1);

    if (result == NULL) {
        diagnosticError("no hay memoria suficiente para preparar una ruta");
        return NULL;
    }

    memcpy(result, path, pathLength - 2);
    memcpy(result + pathLength - 2, extension, extensionLength + 1);
    return result;
}

/*
 * Reserva un nombre temporal junto al archivo indicado.
 *
 * mkstemp() sustituye XXXXXX por caracteres únicos y crea el archivo de forma
 * segura. Al ubicar el temporal en el mismo directorio, rename() puede publicar
 * después el resultado de manera atómica.
 */
static char *createTemporaryPath(const char *nearPath) {
    static const char suffix[] = ".tmp.XXXXXX";
    size_t requiredLength = strlen(nearPath) + sizeof(suffix);
    char *temporaryPath = malloc(requiredLength);
    int descriptor;

    if (temporaryPath == NULL) {
        diagnosticError("no hay memoria suficiente para preparar un temporal");
        return NULL;
    }

    snprintf(temporaryPath, requiredLength, "%s%s", nearPath, suffix);
    descriptor = mkstemp(temporaryPath);
    if (descriptor < 0) {
        diagnosticError(
            "no se pudo crear un archivo temporal para '%s': %s",
            nearPath,
            strerror(errno)
        );
        free(temporaryPath);
        return NULL;
    }

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

static void showCommand(char *const arguments[]) {
    size_t index;

    fputs("minic: comando:", stderr);
    for (index = 0; arguments[index] != NULL; ++index) {
        fprintf(stderr, " '%s'", arguments[index]);
    }
    fputc('\n', stderr);
}

/*
 * Ejecuta GCC como preprocesador y publica la salida de manera atómica.
 *
 * La opción suppressLineMarkers determina si se pasa -P. La compilación a
 * ensamblador siempre la activa para entregar al mock la representación limpia
 * que recibirá posteriormente el compilador MiniC real.
 */
static int preprocess(
    const char *inputPath,
    const char *outputPath,
    int suppressLineMarkers,
    int verbose
) {
    char *temporaryPath = createTemporaryPath(outputPath);
    ProcessResult result;
    char *arguments[8];
    int argumentIndex = 0;

    if (temporaryPath == NULL) {
        return DRIVER_TEMPORARY_ERROR;
    }

    arguments[argumentIndex++] = "gcc";
    arguments[argumentIndex++] = "-E";
    if (suppressLineMarkers) {
        arguments[argumentIndex++] = "-P";
    }
    arguments[argumentIndex++] = (char *) inputPath;
    arguments[argumentIndex++] = "-o";
    arguments[argumentIndex++] = temporaryPath;
    arguments[argumentIndex] = NULL;

    if (verbose) {
        diagnosticInfo("preprocesamiento: %s -> %s", inputPath, outputPath);
        showCommand(arguments);
    }

    result = runProcess(arguments);
    if (!result.started) {
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_INTERNAL_ERROR;
    }

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

    if (rename(temporaryPath, outputPath) != 0) {
        diagnosticError(
            "no se pudo crear la salida '%s': %s",
            outputPath,
            strerror(errno)
        );
        unlink(temporaryPath);
        free(temporaryPath);
        return DRIVER_OUTPUT_ERROR;
    }

    free(temporaryPath);
    return DRIVER_SUCCESS;
}

static void removeTemporary(const char *path) {
    if (path != NULL && unlink(path) != 0 && errno != ENOENT) {
        diagnosticError(
            "no se pudo eliminar el temporal '%s': %s",
            path,
            strerror(errno)
        );
    }
}

/*
 * Completa el recorrido archivo.c -> archivo.i -> archivo.s.
 *
 * El .i usa un nombre único mientras es temporal. Con --keep-temp se publica
 * como producto.i antes de llamar al compilador; así también permanece
 * disponible cuando el compilador encuentra un error.
 */
static int runCompilation(const DriverOptions *options) {
    char *intermediateProduct = replaceExtension(options->inputPath, ".i");
    char *intermediateTemporary = NULL;
    char *assemblyTemporary = NULL;
    const char *compilationInput;
    CompilationResult compilationResult;
    int result;

    if (intermediateProduct == NULL) {
        return DRIVER_INTERNAL_ERROR;
    }

    if (
        options->keepTemporaryFiles &&
        strcmp(intermediateProduct, options->outputPath) == 0
    ) {
        diagnosticError(
            "la salida '%s' coincide con el archivo intermedio",
            options->outputPath
        );
        free(intermediateProduct);
        return DRIVER_USAGE_ERROR;
    }

    intermediateTemporary = createTemporaryPath(intermediateProduct);
    if (intermediateTemporary == NULL) {
        free(intermediateProduct);
        return DRIVER_TEMPORARY_ERROR;
    }

    /*
     * -P se usa internamente aunque el usuario no lo indique: los marcadores
     * son útiles para compiladores completos, pero no son necesarios para el
     * contrato actual de compileFile().
     */
    result = preprocess(
        options->inputPath,
        intermediateTemporary,
        1,
        options->verbose
    );
    if (result != DRIVER_SUCCESS) {
        removeTemporary(intermediateTemporary);
        free(intermediateTemporary);
        free(intermediateProduct);
        return result;
    }

    compilationInput = intermediateTemporary;
    if (options->keepTemporaryFiles) {
        if (rename(intermediateTemporary, intermediateProduct) != 0) {
            diagnosticError(
                "no se pudo conservar el intermedio '%s': %s",
                intermediateProduct,
                strerror(errno)
            );
            removeTemporary(intermediateTemporary);
            free(intermediateTemporary);
            free(intermediateProduct);
            return DRIVER_TEMPORARY_ERROR;
        }
        compilationInput = intermediateProduct;
    }

    assemblyTemporary = createTemporaryPath(options->outputPath);
    if (assemblyTemporary == NULL) {
        if (!options->keepTemporaryFiles) {
            removeTemporary(intermediateTemporary);
        }
        free(intermediateTemporary);
        free(intermediateProduct);
        return DRIVER_TEMPORARY_ERROR;
    }

    if (options->verbose) {
        diagnosticInfo(
            "compilación MiniC (mock): %s -> %s",
            compilationInput,
            options->outputPath
        );
    }

    setCompilerVerbose(options->verbose);
    compilationResult = compileFile(compilationInput, assemblyTemporary);
    if (!compilationResult.success) {
        removeTemporary(assemblyTemporary);
        if (!options->keepTemporaryFiles) {
            removeTemporary(intermediateTemporary);
        }
        free(assemblyTemporary);
        free(intermediateTemporary);
        free(intermediateProduct);
        return DRIVER_COMPILER_ERROR;
    }

    if (rename(assemblyTemporary, options->outputPath) != 0) {
        diagnosticError(
            "no se pudo crear la salida '%s': %s",
            options->outputPath,
            strerror(errno)
        );
        removeTemporary(assemblyTemporary);
        if (!options->keepTemporaryFiles) {
            removeTemporary(intermediateTemporary);
        }
        free(assemblyTemporary);
        free(intermediateTemporary);
        free(intermediateProduct);
        return DRIVER_OUTPUT_ERROR;
    }

    if (!options->keepTemporaryFiles) {
        removeTemporary(intermediateTemporary);
    }

    free(assemblyTemporary);
    free(intermediateTemporary);
    free(intermediateProduct);
    return DRIVER_SUCCESS;
}

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

    if (strcmp(options->inputPath, options->outputPath) == 0) {
        diagnosticError("la entrada y la salida no pueden ser el mismo archivo");
        return DRIVER_USAGE_ERROR;
    }

    if (options->finalStage == STAGE_PREPROCESS) {
        return preprocess(
            options->inputPath,
            options->outputPath,
            options->suppressLineMarkers,
            options->verbose
        );
    }

    if (options->finalStage == STAGE_COMPILE) {
        return runCompilation(options);
    }

    diagnosticError("la etapa solicitada todavía no está implementada");
    return DRIVER_INTERNAL_ERROR;
}
