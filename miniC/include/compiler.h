#ifndef MINIC_COMPILER_H
#define MINIC_COMPILER_H

/*
 * Resultado estable de una ejecución del compilador MiniC.
 *
 * El driver solo necesita saber si puede continuar y cuántos errores fueron
 * encontrados. En Entrega 2, compiler.c contiene una implementación simulada
 * con GCC; el lexer y las fases reales podrán reemplazarla sin cambiar esta
 * estructura ni compileFile().
 */
typedef struct {
    int success;
    int errorCount;
} CompilationResult;

typedef enum {
    COMPILER_MODE_EMIT_ASSEMBLY,
    COMPILER_MODE_LEX_ONLY,
    COMPILER_MODE_PARSE_ONLY,
    COMPILER_MODE_CODEGEN_ONLY
} CompilerMode;

void setCompilerVerbose(int verbose);
void setCompilerMode(CompilerMode mode);

CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
);

#endif
