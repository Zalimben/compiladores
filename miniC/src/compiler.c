/*
 * Punto de integración con el futuro compilador MiniC.
 *
 * Todavía no existen lexer, parser, AST ni generador propios. Este módulo
 * ofrece mocks basados en GCC para validar el control de las etapas internas:
 *
 *     --lex      mock de análisis léxico
 *     --parse    mock de análisis léxico y sintáctico
 *     --codegen  mock de generación de ensamblador, sin publicar el resultado
 *
 * GCC no expone exactamente las fronteras internas de un compilador académico.
 * Por ello, --lex y --parse usan ambos "-fsyntax-only"; son aproximaciones
 * temporales y no definen el comportamiento del futuro lenguaje MiniC.
 */
#include "compiler.h"

#include "diagnostics.h"
#include "process.h"

#include <stdio.h>

static int verboseMode = 0;
static CompilerMode currentMode = COMPILER_MODE_EMIT_ASSEMBLY;

static void showCommand(char *const arguments[]) {
    size_t index;

    fputs("minic: comando:", stderr);
    for (index = 0; arguments[index] != NULL; ++index) {
        fprintf(stderr, " '%s'", arguments[index]);
    }
    fputc('\n', stderr);
}

/*
 * Configura la información mostrada por el compilador sin modificar la firma
 * estable de compileFile(). El futuro compilador podrá usar esta misma opción
 * para mostrar sus fases internas.
 */
void setCompilerVerbose(int verbose) {
    verboseMode = verbose;
}

/*
 * Selecciona cuánto debe avanzar compileFile(). Mantener la selección fuera de
 * su firma conserva la interfaz estable que ya utiliza el driver.
 */
void setCompilerMode(CompilerMode mode) {
    currentMode = mode;
}

static const char *modeDescription(void) {
    switch (currentMode) {
        case COMPILER_MODE_LEX_ONLY:
            return "mock del lexer";
        case COMPILER_MODE_PARSE_ONLY:
            return "mock del lexer y parser";
        case COMPILER_MODE_CODEGEN_ONLY:
            return "mock de generación de código";
        case COMPILER_MODE_EMIT_ASSEMBLY:
            return "compilador simulado con GCC";
        default:
            return "compilador simulado con GCC";
    }
}

CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
) {
    CompilationResult result = {0, 1};
    ProcessResult processResult;
    char *syntaxOnlyArguments[] = {
        "gcc",
        "-fsyntax-only",
        "-x",
        "cpp-output",
        (char *) preprocessedPath,
        NULL
    };
    char *codeGenerationArguments[] = {
        "gcc",
        "-S",
        "-x",
        "cpp-output",
        (char *) preprocessedPath,
        "-o",
        (char *) assemblyPath,
        NULL
    };
    char **arguments;

    /*
     * PUNTOS EXACTOS PARA INCORPORAR LAS FASES REALES:
     *
     * COMPILER_MODE_LEX_ONLY:
     *     Reemplazar GCC por lexerCreate() y lexerScan(). No construir AST.
     *
     * COMPILER_MODE_PARSE_ONLY:
     *     Ejecutar lexerScan() y parserParseProgram(). No generar ensamblador.
     *
     * COMPILER_MODE_CODEGEN_ONLY:
     *     Ejecutar lexer, parser, análisis semántico y construir la
     *     representación de ensamblador. No escribir assemblyPath.
     *
     * COMPILER_MODE_EMIT_ASSEMBLY:
     *     Ejecutar todas las fases anteriores y finalmente llamar a
     *     generateAssembly(program, assemblyPath).
     *
     * Estas implementaciones deberán residir en módulos separados. El driver
     * no debe contener lógica de tokens, gramática, AST ni generación.
     */
    if (
        currentMode == COMPILER_MODE_LEX_ONLY ||
        currentMode == COMPILER_MODE_PARSE_ONLY
    ) {
        arguments = syntaxOnlyArguments;
    } else {
        arguments = codeGenerationArguments;
    }

    if (verboseMode) {
        diagnosticInfo(
            "%s: %s",
            modeDescription(),
            preprocessedPath
        );
        showCommand(arguments);
    }

    processResult = runProcess(arguments);
    if (!processResult.started) {
        return result;
    }

    if (!processResult.exited) {
        diagnosticError(
            "%s terminó por la señal %d",
            modeDescription(),
            processResult.signalNumber
        );
        return result;
    }

    if (processResult.exitCode != 0) {
        diagnosticError(
            "%s terminó con código %d",
            modeDescription(),
            processResult.exitCode
        );
        return result;
    }

    result.success = 1;
    result.errorCount = 0;
    return result;
}
