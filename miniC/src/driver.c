/*
 * Coordinación del pipeline completo de MiniC.
 *
 *     .c -> GCC -E -> .i -> compileFile() -> .s
 *        -> GCC -c -> .o -> GCC (enlace) -> ejecutable
 *
 * compileFile() continúa siendo un mock basado en GCC. El driver desconoce esa
 * implementación: cuando se incorporen lexer y parser, este archivo no deberá
 * modificarse.
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
 * Reemplaza ".c" por la extensión solicitada. La cadena retornada pertenece al
 * llamador y debe liberarse con free().
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
 * mkstemp() crea el archivo y sustituye XXXXXX por caracteres únicos. Los
 * temporales se ubican junto a su producto lógico para que rename() pueda
 * publicarlos atómicamente.
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
 * Interpreta el estado POSIX de una herramienta externa y lo convierte en el
 * código de error propio de la etapa. De este modo nunca continúa una etapa
 * posterior después de un fallo.
 */
static int runExternalStage(
    char *const arguments[],
    const char *stageName,
    int failureCode
) {
    ProcessResult processResult = runProcess(arguments);

    if (!processResult.started) {
        return DRIVER_INTERNAL_ERROR;
    }

    if (!processResult.exited) {
        diagnosticError(
            "el %s terminó por la señal %d",
            stageName,
            processResult.signalNumber
        );
        return failureCode;
    }

    if (processResult.exitCode != 0) {
        diagnosticError(
            "el %s terminó con código %d",
            stageName,
            processResult.exitCode
        );
        return failureCode;
    }

    return DRIVER_SUCCESS;
}

/*
 * Preprocesa hacia una salida temporal y la publica solo después del éxito de
 * GCC. Esta función se usa tanto para el producto de -E como para el .i que
 * alimenta a compileFile().
 */
static int preprocess(
    const char *inputPath,
    const char *outputPath,
    int suppressLineMarkers,
    int verbose
) {
    char *temporaryPath = createTemporaryPath(outputPath);
    char *arguments[8];
    int argumentIndex = 0;
    int result;

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

    result = runExternalStage(
        arguments,
        "preprocesador",
        DRIVER_PREPROCESS_ERROR
    );
    if (result != DRIVER_SUCCESS) {
        unlink(temporaryPath);
        free(temporaryPath);
        return result;
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

static int assembleFile(
    const char *assemblyPath,
    const char *objectPath,
    int verbose
) {
    char *arguments[] = {
        "gcc",
        "-c",
        "-x",
        "assembler",
        (char *) assemblyPath,
        "-o",
        (char *) objectPath,
        NULL
    };

    if (verbose) {
        diagnosticInfo("ensamblado: %s -> %s", assemblyPath, objectPath);
        showCommand(arguments);
    }

    return runExternalStage(
        arguments,
        "ensamblador",
        DRIVER_ASSEMBLER_ERROR
    );
}

static int linkFile(
    const char *objectPath,
    const char *executablePath,
    int verbose
) {
    char *arguments[] = {
        "gcc",
        (char *) objectPath,
        "-o",
        (char *) executablePath,
        NULL
    };

    if (verbose) {
        diagnosticInfo("enlace: %s -> %s", objectPath, executablePath);
        showCommand(arguments);
    }

    return runExternalStage(
        arguments,
        "enlazador",
        DRIVER_LINKER_ERROR
    );
}

/*
 * Publica un temporal como producto final o como intermedio conservado. El
 * puntero se libera y queda en NULL después de un rename() exitoso.
 */
static int publishTemporary(
    char **temporaryPath,
    const char *productPath,
    int failureCode
) {
    if (rename(*temporaryPath, productPath) != 0) {
        diagnosticError(
            failureCode == DRIVER_OUTPUT_ERROR
                ? "no se pudo crear la salida '%s': %s"
                : "no se pudo conservar el intermedio '%s': %s",
            productPath,
            strerror(errno)
        );
        return failureCode;
    }

    free(*temporaryPath);
    *temporaryPath = NULL;
    return DRIVER_SUCCESS;
}

/*
 * Elimina y libera un temporal aún no publicado. Retorna cero si la limpieza
 * falla, para que una ejecución funcionalmente exitosa pueda informar el
 * código DRIVER_TEMPORARY_ERROR.
 */
static int discardTemporary(char **temporaryPath) {
    int success = 1;

    if (*temporaryPath == NULL) {
        return success;
    }

    if (unlink(*temporaryPath) != 0 && errno != ENOENT) {
        diagnosticError(
            "no se pudo eliminar el temporal '%s': %s",
            *temporaryPath,
            strerror(errno)
        );
        success = 0;
    }

    free(*temporaryPath);
    *temporaryPath = NULL;
    return success;
}

static int conflictsWithKeptIntermediate(
    const DriverOptions *options,
    const char *preprocessedProduct,
    const char *assemblyProduct,
    const char *objectProduct
) {
    if (!options->keepTemporaryFiles) {
        return 0;
    }

    if (strcmp(options->outputPath, preprocessedProduct) == 0) {
        return 1;
    }
    if (
        assemblyProduct != NULL &&
        strcmp(options->outputPath, assemblyProduct) == 0
    ) {
        return 1;
    }
    if (
        objectProduct != NULL &&
        strcmp(options->outputPath, objectProduct) == 0
    ) {
        return 1;
    }

    return 0;
}

/*
 * Ejecuta uno de los puntos de detención internos solicitados con --lex,
 * --parse o --codegen. El resultado que GCC usa para simular la fase se guarda
 * siempre en un temporal y se descarta: estos modos validan una fase, pero no
 * publican ensamblador, objeto ni ejecutable.
 */
static int runCompilerInspection(const DriverOptions *options) {
    char *preprocessedProduct = NULL;
    char *assemblyProduct = NULL;
    char *preprocessedTemporary = NULL;
    char *compilerTemporary = NULL;
    const char *compilerInput;
    CompilationResult compilationResult;
    int result = DRIVER_INTERNAL_ERROR;
    int cleanupSucceeded = 1;

    preprocessedProduct = replaceExtension(options->inputPath, ".i");
    assemblyProduct = replaceExtension(options->inputPath, ".s");
    if (preprocessedProduct == NULL || assemblyProduct == NULL) {
        goto cleanup;
    }

    preprocessedTemporary = createTemporaryPath(preprocessedProduct);
    if (preprocessedTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    result = preprocess(
        options->inputPath,
        preprocessedTemporary,
        1,
        options->verbose
    );
    if (result != DRIVER_SUCCESS) {
        goto cleanup;
    }

    compilerInput = preprocessedTemporary;
    if (options->keepTemporaryFiles) {
        result = publishTemporary(
            &preprocessedTemporary,
            preprocessedProduct,
            DRIVER_TEMPORARY_ERROR
        );
        if (result != DRIVER_SUCCESS) {
            goto cleanup;
        }
        compilerInput = preprocessedProduct;
    }

    compilerTemporary = createTemporaryPath(assemblyProduct);
    if (compilerTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    setCompilerVerbose(options->verbose);
    setCompilerMode(options->compilerMode);
    compilationResult = compileFile(compilerInput, compilerTemporary);
    result = compilationResult.success
        ? DRIVER_SUCCESS
        : DRIVER_COMPILER_ERROR;

cleanup:
    if (!discardTemporary(&compilerTemporary)) {
        cleanupSucceeded = 0;
    }
    if (!discardTemporary(&preprocessedTemporary)) {
        cleanupSucceeded = 0;
    }

    free(assemblyProduct);
    free(preprocessedProduct);

    if (result == DRIVER_SUCCESS && !cleanupSucceeded) {
        return DRIVER_TEMPORARY_ERROR;
    }
    return result;
}

/*
 * Ejecuta desde preprocesamiento hasta la etapa indicada por finalStage.
 *
 * Los punteros *Temporary identifican exclusivamente archivos que deben
 * eliminarse al salir. Cuando --keep-temp publica uno de ellos, el puntero se
 * vuelve NULL y el producto con nombre estable queda fuera de la limpieza.
 */
static int runNativePipeline(const DriverOptions *options) {
    char *preprocessedProduct = NULL;
    char *assemblyProduct = NULL;
    char *objectProduct = NULL;
    char *preprocessedTemporary = NULL;
    char *assemblyTemporary = NULL;
    char *objectTemporary = NULL;
    char *executableTemporary = NULL;
    const char *compilationInput;
    const char *assemblyInput;
    const char *objectInput;
    CompilationResult compilationResult;
    int result = DRIVER_INTERNAL_ERROR;
    int cleanupSucceeded = 1;

    preprocessedProduct = replaceExtension(options->inputPath, ".i");
    if (preprocessedProduct == NULL) {
        goto cleanup;
    }

    if (options->finalStage >= STAGE_ASSEMBLE) {
        assemblyProduct = replaceExtension(options->inputPath, ".s");
        if (assemblyProduct == NULL) {
            goto cleanup;
        }
    }

    if (options->finalStage >= STAGE_LINK) {
        objectProduct = replaceExtension(options->inputPath, ".o");
        if (objectProduct == NULL) {
            goto cleanup;
        }
    }

    if (
        conflictsWithKeptIntermediate(
            options,
            preprocessedProduct,
            assemblyProduct,
            objectProduct
        )
    ) {
        diagnosticError(
            "la salida '%s' coincide con un archivo intermedio",
            options->outputPath
        );
        result = DRIVER_USAGE_ERROR;
        goto cleanup;
    }

    preprocessedTemporary = createTemporaryPath(preprocessedProduct);
    if (preprocessedTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    /*
     * El pipeline interno siempre elimina marcadores. El futuro lexer recibirá
     * aquí el contenido preprocesado mediante compileFile(), no el .c original.
     */
    result = preprocess(
        options->inputPath,
        preprocessedTemporary,
        1,
        options->verbose
    );
    if (result != DRIVER_SUCCESS) {
        goto cleanup;
    }

    compilationInput = preprocessedTemporary;
    if (options->keepTemporaryFiles) {
        result = publishTemporary(
            &preprocessedTemporary,
            preprocessedProduct,
            DRIVER_TEMPORARY_ERROR
        );
        if (result != DRIVER_SUCCESS) {
            goto cleanup;
        }
        compilationInput = preprocessedProduct;
    }

    assemblyTemporary = createTemporaryPath(
        options->finalStage == STAGE_COMPILE
            ? options->outputPath
            : assemblyProduct
    );
    if (assemblyTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    /*
     * PUNTO DE SUSTITUCIÓN DEL COMPILADOR:
     * compileFile() usa GCC como mock en esta entrega. Más adelante, dentro de
     * compiler.c, esta llamada ejecutará lexer -> parser -> generación de
     * ensamblador. El resto del pipeline permanecerá sin cambios.
     */
    setCompilerVerbose(options->verbose);
    setCompilerMode(COMPILER_MODE_EMIT_ASSEMBLY);
    compilationResult = compileFile(compilationInput, assemblyTemporary);
    if (!compilationResult.success) {
        result = DRIVER_COMPILER_ERROR;
        goto cleanup;
    }

    if (options->finalStage == STAGE_COMPILE) {
        result = publishTemporary(
            &assemblyTemporary,
            options->outputPath,
            DRIVER_OUTPUT_ERROR
        );
        goto cleanup;
    }

    assemblyInput = assemblyTemporary;
    if (options->keepTemporaryFiles) {
        result = publishTemporary(
            &assemblyTemporary,
            assemblyProduct,
            DRIVER_TEMPORARY_ERROR
        );
        if (result != DRIVER_SUCCESS) {
            goto cleanup;
        }
        assemblyInput = assemblyProduct;
    }

    objectTemporary = createTemporaryPath(
        options->finalStage == STAGE_ASSEMBLE
            ? options->outputPath
            : objectProduct
    );
    if (objectTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    result = assembleFile(
        assemblyInput,
        objectTemporary,
        options->verbose
    );
    if (result != DRIVER_SUCCESS) {
        goto cleanup;
    }

    if (options->finalStage == STAGE_ASSEMBLE) {
        result = publishTemporary(
            &objectTemporary,
            options->outputPath,
            DRIVER_OUTPUT_ERROR
        );
        goto cleanup;
    }

    objectInput = objectTemporary;
    if (options->keepTemporaryFiles) {
        result = publishTemporary(
            &objectTemporary,
            objectProduct,
            DRIVER_TEMPORARY_ERROR
        );
        if (result != DRIVER_SUCCESS) {
            goto cleanup;
        }
        objectInput = objectProduct;
    }

    executableTemporary = createTemporaryPath(options->outputPath);
    if (executableTemporary == NULL) {
        result = DRIVER_TEMPORARY_ERROR;
        goto cleanup;
    }

    result = linkFile(
        objectInput,
        executableTemporary,
        options->verbose
    );
    if (result != DRIVER_SUCCESS) {
        goto cleanup;
    }

    result = publishTemporary(
        &executableTemporary,
        options->outputPath,
        DRIVER_OUTPUT_ERROR
    );

cleanup:
    if (!discardTemporary(&executableTemporary)) {
        cleanupSucceeded = 0;
    }
    if (!discardTemporary(&objectTemporary)) {
        cleanupSucceeded = 0;
    }
    if (!discardTemporary(&assemblyTemporary)) {
        cleanupSucceeded = 0;
    }
    if (!discardTemporary(&preprocessedTemporary)) {
        cleanupSucceeded = 0;
    }

    free(objectProduct);
    free(assemblyProduct);
    free(preprocessedProduct);

    if (result == DRIVER_SUCCESS && !cleanupSucceeded) {
        return DRIVER_TEMPORARY_ERROR;
    }
    return result;
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

    if (options->compilerMode != COMPILER_MODE_EMIT_ASSEMBLY) {
        return runCompilerInspection(options);
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

    return runNativePipeline(options);
}
