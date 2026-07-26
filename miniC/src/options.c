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
#define MINIC_VERSION "0.3.0"

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
 * Construye el nombre predeterminado del producto solicitado.
 *
 * Como la extensión ".c" ya fue validada, se elimina antes de añadir la
 * extensión correspondiente. Para STAGE_LINK no se añade extensión.
 */
static char *deriveOutputPath(
    const char *inputPath,
    CompilationStage finalStage
) {
    const char *extension;
    size_t inputLength = strlen(inputPath);
    size_t baseLength = inputLength - 2;
    size_t extensionLength;
    char *outputPath;

    switch (finalStage) {
        case STAGE_PREPROCESS:
            extension = ".i";
            break;
        case STAGE_COMPILE:
            extension = ".s";
            break;
        case STAGE_ASSEMBLE:
            extension = ".o";
            break;
        case STAGE_LINK:
            extension = "";
            break;
        default:
            extension = "";
            break;
    }

    extensionLength = strlen(extension);
    outputPath = malloc(baseLength + extensionLength + 1);

    if (outputPath == NULL) {
        diagnosticError("no hay memoria suficiente para determinar la salida");
        return NULL;
    }

    memcpy(outputPath, inputPath, baseLength);
    memcpy(outputPath + baseLength, extension, extensionLength + 1);
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
    options->action = ACTION_RUN_PIPELINE;
    options->finalStage = STAGE_LINK;
    options->suppressLineMarkers = 0;
    options->verbose = 0;
    options->keepTemporaryFiles = 0;
}

/*
 * Analiza la línea de comandos de izquierda a derecha.
 *
 * Retorna un DriverExitCode: DRIVER_SUCCESS si las opciones son válidas o un
 * código distinto de cero después de mostrar un diagnóstico. En la Entrega 3
 * se admite un solo archivo fuente y las opciones -E, -S, -c, -P, -o, -v,
 * --keep-temp, --help y --version.
 */
int parseArguments(int argc, char *argv[], DriverOptions *options) {
    int index;
    const char *stageOption = NULL;
    int suppressMarkersOptionSeen = 0;
    int verboseOptionSeen = 0;
    int keepTemporaryOptionSeen = 0;
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

        if (
            strcmp(argument, "-E") == 0 ||
            strcmp(argument, "-S") == 0 ||
            strcmp(argument, "-c") == 0
        ) {
            if (stageOption != NULL) {
                if (strcmp(stageOption, argument) == 0) {
                    diagnosticError(
                        "la opción '%s' se especificó más de una vez",
                        argument
                    );
                } else {
                    diagnosticError(
                        "las opciones '%s' y '%s' son incompatibles",
                        stageOption,
                        argument
                    );
                }
                return DRIVER_USAGE_ERROR;
            }
            stageOption = argument;
            if (strcmp(argument, "-E") == 0) {
                options->finalStage = STAGE_PREPROCESS;
            } else if (strcmp(argument, "-S") == 0) {
                options->finalStage = STAGE_COMPILE;
            } else {
                options->finalStage = STAGE_ASSEMBLE;
            }
        } else if (strcmp(argument, "-P") == 0) {
            if (suppressMarkersOptionSeen) {
                diagnosticError("la opción '-P' se especificó más de una vez");
                return DRIVER_USAGE_ERROR;
            }
            suppressMarkersOptionSeen = 1;
            options->suppressLineMarkers = 1;
        } else if (strcmp(argument, "-v") == 0) {
            if (verboseOptionSeen) {
                diagnosticError("la opción '-v' se especificó más de una vez");
                return DRIVER_USAGE_ERROR;
            }
            verboseOptionSeen = 1;
            options->verbose = 1;
        } else if (strcmp(argument, "--keep-temp") == 0) {
            if (keepTemporaryOptionSeen) {
                diagnosticError(
                    "la opción '--keep-temp' se especificó más de una vez"
                );
                return DRIVER_USAGE_ERROR;
            }
            keepTemporaryOptionSeen = 1;
            options->keepTemporaryFiles = 1;
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
        options->outputPath = deriveOutputPath(
            options->inputPath,
            options->finalStage
        );
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

/* Muestra la interfaz completa de la Entrega 3. */
void printHelp(void) {
    puts(
        "Uso: minic [opciones] archivo.c\n"
        "\n"
        "Driver de MiniC — Entrega 3.\n"
        "\n"
        "Opciones:\n"
        "  -E              Ejecuta solamente el preprocesamiento\n"
        "  -S              Genera ensamblador y se detiene\n"
        "  -c              Genera un archivo objeto y se detiene\n"
        "  -P              Elimina los marcadores de línea del resultado\n"
        "  -o archivo      Escribe el resultado en archivo\n"
        "  -v              Muestra las etapas y comandos ejecutados\n"
        "  --keep-temp     Conserva los archivos intermedios\n"
        "  --help          Muestra esta ayuda\n"
        "  --version       Muestra la versión\n"
        "\n"
        "Sin opción de etapa, genera un ejecutable sin extensión."
    );
}

/* Muestra una versión breve, apropiada para scripts y reportes de errores. */
void printVersion(void) {
    puts("minic " MINIC_VERSION " (Entrega 3)");
}
