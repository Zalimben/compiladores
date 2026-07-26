/*
 * Procesamiento de las opciones de línea de comandos.
 *
 * Este módulo transforma argc/argv en una estructura DriverOptions. Aquí se
 * valida la forma del comando; la existencia y los permisos del archivo se
 * comprueban posteriormente en driver.c.
 */
#include "options.h"

#include "diagnostics.h"
#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Versión mostrada por la opción --version. */
#define MINIC_VERSION "0.1.0"

/*
 * Comprueba únicamente que la ruta termine en ".c".
 *
 * length >= 3 exige que exista al menos un carácter antes de la extensión.
 * Esta función no accede al sistema de archivos.
 */
static int hasCExtension(const char *path) {
    size_t length = strlen(path);
    return length >= 3 && strcmp(path + length - 2, ".c") == 0;
}

/*
 * Construye el nombre predeterminado del archivo preprocesado.
 *
 * Como la extensión ".c" ya fue validada, basta sustituir su último carácter:
 * "programa.c" se convierte en "programa.i". La memoria pertenece al llamador
 * y debe liberarse con free().
 */
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

/*
 * Crea una copia dinámica de una cadena.
 *
 * DriverOptions conserva outputPath después de que parseArguments() termina;
 * por eso no debe guardar una referencia temporal y necesita su propia copia.
 */
static char *copyString(const char *value) {
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, value, length);
    }
    return copy;
}

/*
 * Establece un estado inicial conocido antes de analizar los argumentos.
 *
 * Inicializar los punteros con NULL también permite liberar la estructura de
 * forma segura si ocurre un error antes de asignar memoria.
 */
void initializeOptions(DriverOptions *options) {
    options->inputPath = NULL;
    options->outputPath = NULL;
    options->action = ACTION_PREPROCESS;
    options->suppressLineMarkers = 0;
}

/*
 * Analiza la línea de comandos de izquierda a derecha.
 *
 * Retorna un DriverExitCode: DRIVER_SUCCESS si las opciones son válidas o un
 * código distinto de cero después de mostrar un diagnóstico. En la Entrega 1
 * se admite un solo archivo fuente y las opciones -E, -P, -o, --help y
 * --version.
 */
int parseArguments(int argc, char *argv[], DriverOptions *options) {
    int index;
    int preprocessOptionSeen = 0;
    int suppressMarkersOptionSeen = 0;
    const char *requestedOutput = NULL;

    /*
     * Ayuda y versión son acciones completas: no requieren archivo de entrada
     * cuando aparecen como único argumento.
     */
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        options->action = ACTION_SHOW_HELP;
        return DRIVER_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        options->action = ACTION_SHOW_VERSION;
        return DRIVER_SUCCESS;
    }

    /*
     * Cada argumento se clasifica como opción o como archivo de entrada. Los
     * indicadores *Seen permiten detectar repeticiones y producir un mensaje
     * más claro que el que daría simplemente ignorarlas.
     */
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
            /*
             * -o consume también el argumento siguiente. Incrementar index
             * aquí evita que ese nombre se interprete luego como otra entrada.
             */
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

    /* Las validaciones globales se realizan después de recorrer argv. */
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

    /*
     * outputPath siempre se almacena en memoria dinámica, tanto para -o como
     * para el nombre predeterminado. Así destroyOptions() tiene una única
     * regla de propiedad y no necesita distinguir ambos casos.
     */
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

/* Libera los recursos cuya propiedad pertenece a DriverOptions. */
void destroyOptions(DriverOptions *options) {
    free(options->outputPath);
    options->outputPath = NULL;
}

/* Muestra la interfaz disponible en la Entrega 1. */
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

/* Muestra una versión breve, apropiada para scripts y reportes de errores. */
void printVersion(void) {
    puts("minic " MINIC_VERSION " (Entrega 1)");
}
