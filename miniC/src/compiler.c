/*
 * Punto de integración con el futuro compilador MiniC.
 *
 *
 * Cuando el compilador real esté disponible, se reemplazará únicamente el
 * cuerpo de este módulo por una secuencia semejante a:
 *
 *     tokens = lex(preprocessedPath);
 *     ast = parse(tokens);
 *     validate(ast);
 *     generateAssembly(ast, assemblyPath);
 *
 * El driver seguirá llamando a la misma interfaz compileFile().
 */
#include "compiler.h"

#include "diagnostics.h"
#include "process.h"

#include <stdio.h>

static int verboseMode = 0;

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
 * Mock de la compilación MiniC.
 *
 * -S solicita a GCC que produzca ensamblador y "-x cpp-output" indica que la
 * entrada ya fue preprocesada. Esta última opción es necesaria porque los
 * archivos temporales poseen un sufijo único y no siempre terminan en ".i".
 */
CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
) {
    /*
     * PUNTO EXACTO PARA INCORPORAR EL COMPILADOR REAL:
     *
     * Este bloque basado en GCC deberá reemplazarse por llamadas a módulos
     * propios. Una organización futura posible es:
     *
     *     Lexer lexer = lexerCreate(preprocessedPath);
     *     TokenStream tokens = lexerScan(&lexer);
     *     Parser parser = parserCreate(&tokens);
     *     AstProgram *program = parserParseProgram(&parser);
     *     semanticValidate(program);
     *     generateAssembly(program, assemblyPath);
     *
     * Esas fases deben permanecer en módulos separados. No deben añadirse a
     * driver.c: el driver solo coordina el pipeline y consulta este resultado.
     */
    CompilationResult result = {0, 1};
    ProcessResult processResult;

    char *arguments[] = {
        "gcc",
        "-S",
        "-x",
        "cpp-output",
        (char *) preprocessedPath,
        "-o",
        (char *) assemblyPath,
        NULL
    };

    if (verboseMode) {
        diagnosticInfo(
            "compilador simulado con GCC: %s -> %s",
            preprocessedPath,
            assemblyPath
        );
        showCommand(arguments);
    }

    processResult = runProcess(arguments);
    if (!processResult.started) {
        return result;
    }

    if (!processResult.exited) {
        diagnosticError(
            "el compilador simulado terminó por la señal %d",
            processResult.signalNumber
        );
        return result;
    }

    if (processResult.exitCode != 0) {
        diagnosticError(
            "el compilador simulado terminó con código %d",
            processResult.exitCode
        );
        return result;
    }

    result.success = 1;
    result.errorCount = 0;
    return result;
}
