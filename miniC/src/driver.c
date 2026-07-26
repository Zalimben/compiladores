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

static char *createTemporaryPath(const char *outputPath) {
    static const char suffix[] = ".tmp.XXXXXX";
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
    if (options->suppressLineMarkers) {
        arguments[argumentIndex++] = "-P";
    }
    arguments[argumentIndex++] = (char *) options->inputPath;
    arguments[argumentIndex++] = "-o";
    arguments[argumentIndex++] = temporaryPath;
    arguments[argumentIndex] = NULL;

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

    return preprocess(options);
}
