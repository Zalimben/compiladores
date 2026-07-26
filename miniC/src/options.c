#include "options.h"

#include "diagnostics.h"
#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIC_VERSION "0.1.0"

static int hasCExtension(const char *path) {
    size_t length = strlen(path);
    return length >= 3 && strcmp(path + length - 2, ".c") == 0;
}

static char *deriveOutputPath(const char *inputPath) {
    size_t length = strlen(inputPath);
    char *outputPath = malloc(length + 1);

    if (outputPath == NULL) {
        diagnosticError("no hay memoria suficiente para determinar la salida");
        return NULL;
    }

    memcpy(outputPath, inputPath, length + 1);
    outputPath[length - 1] = 'i';
    return outputPath;
}

static char *copyString(const char *value) {
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, value, length);
    }
    return copy;
}

void initializeOptions(DriverOptions *options) {
    options->inputPath = NULL;
    options->outputPath = NULL;
    options->action = ACTION_PREPROCESS;
    options->suppressLineMarkers = 0;
}

int parseArguments(int argc, char *argv[], DriverOptions *options) {
    int index;
    int preprocessOptionSeen = 0;
    int suppressMarkersOptionSeen = 0;
    const char *requestedOutput = NULL;

    // Imprime ayuda sobre el compiler
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        options->action = ACTION_SHOW_HELP;
        return DRIVER_SUCCESS;
    }

    // Imprime la versión del compiler
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        options->action = ACTION_SHOW_VERSION;
        return DRIVER_SUCCESS;
    }

    // Verifica la cantidad de argumentos par el compilador
    for (index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "-E") == 0) {
            if (preprocessOptionSeen) {
                diagnosticError("la opción '-E' se especificó más de una vez");
                return DRIVER_USAGE_ERROR;
            }
            preprocessOptionSeen = 1;
        } else if (strcmp(argument, "-P") == 0) {
            if (suppressMarkersOptionSeen) {
                diagnosticError("la opción '-P' se especificó más de una vez");
                return DRIVER_USAGE_ERROR;
            }
            suppressMarkersOptionSeen = 1;
            options->suppressLineMarkers = 1;
        } else if (strcmp(argument, "-o") == 0) {
            if (requestedOutput != NULL) {
                diagnosticError("la opción '-o' se especificó más de una vez");
                return DRIVER_USAGE_ERROR;
            }
            if (index + 1 >= argc) {
                diagnosticError("la opción '-o' requiere un nombre de archivo");
                return DRIVER_USAGE_ERROR;
            }
            requestedOutput = argv[++index];
            if (requestedOutput[0] == '\0') {
                diagnosticError("la opción '-o' requiere un nombre de archivo");
                return DRIVER_USAGE_ERROR;
            }
        } else if (argument[0] == '-') {
            diagnosticError("opción desconocida '%s'", argument);
            return DRIVER_USAGE_ERROR;
        } else if (options->inputPath != NULL) {
            diagnosticError("se admite un único archivo de entrada");
            return DRIVER_USAGE_ERROR;
        } else {
            options->inputPath = argument;
        }
    }

    if (options->inputPath == NULL) {
        diagnosticError("no se especificó un archivo de entrada");
        return DRIVER_USAGE_ERROR;
    }

    if (!hasCExtension(options->inputPath)) {
        diagnosticError(
            "el archivo de entrada '%s' debe tener extensión .c",
            options->inputPath
        );
        return DRIVER_USAGE_ERROR;
    }

    if (requestedOutput != NULL) {
        options->outputPath = copyString(requestedOutput);
        if (options->outputPath == NULL) {
            diagnosticError("no hay memoria suficiente para guardar la salida");
            return DRIVER_INTERNAL_ERROR;
        }
    } else {
        options->outputPath = deriveOutputPath(options->inputPath);
        if (options->outputPath == NULL) {
            return DRIVER_INTERNAL_ERROR;
        }
    }

    return DRIVER_SUCCESS;
}

void destroyOptions(DriverOptions *options) {
    free(options->outputPath);
    options->outputPath = NULL;
}

void printHelp(void) {
    puts(
        "Uso: minic [opciones] archivo.c\n"
        "\n"
        "Driver de MiniC — Entrega 1 (preprocesamiento).\n"
        "\n"
        "Opciones:\n"
        "  -E              Ejecuta solamente el preprocesamiento\n"
        "  -P              Elimina los marcadores de línea del resultado\n"
        "  -o archivo      Escribe el resultado en archivo\n"
        "  --help          Muestra esta ayuda\n"
        "  --version       Muestra la versión\n"
        "\n"
        "Sin -o, la salida se guarda junto a la entrada con extensión .i."
    );
}

void printVersion(void) {
    puts("minic " MINIC_VERSION " (Entrega 1)");
}
