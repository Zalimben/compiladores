# Arquitectura de MiniC — Entrega 2

La segunda entrega ofrece dos puntos de detención:

```text
                         -E
archivo.c → GCC -E ─────────────→ archivo.i
             │
             │ -S
             └→ archivo.i → compileFile() → archivo.s
```

Para `-E`, la opción `-P` controla si GCC elimina los marcadores de línea. Para
`-S`, el driver activa `-P` internamente y entrega el `.i` limpio a
`compileFile()`.

## Módulos

| Módulo | Responsabilidad |
|---|---|
| `main.c` | Inicializa las opciones, ejecuta el driver y devuelve su código de salida. |
| `options.c` | Interpreta argumentos, valida opciones y determina la etapa y el producto final. |
| `driver.c` | Coordina el pipeline, los archivos temporales y la detención ante errores. |
| `process.c` | Ejecuta GCC mediante `fork()`, `execvp()` y `waitpid()`. |
| `compiler.c` | Implementa temporalmente `compileFile()` como un mock que delega la generación de ensamblador en GCC. |
| `diagnostics.c` | Uniforma mensajes del driver, información detallada y errores con ubicación. |

## Interfaz con el compilador

El driver solo conoce la interfaz declarada en `compiler.h`:

```c
typedef struct {
    int success;
    int errorCount;
} CompilationResult;

CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
);
```

Esta separación permite incorporar el lexer, un AST, análisis semántico y un
generador de código propio sin trasladar esas responsabilidades al driver.

## Compilador simulado

Entrega 2 no implementa todavía la gramática de MiniC. `compileFile()` ejecuta:

```bash
gcc -S -x cpp-output entrada.i -o salida.s
```

`-x cpp-output` informa a GCC que la entrada ya está preprocesada. Esto es
necesario porque el nombre temporal no siempre termina en `.i`.

El mock existe para verificar la coordinación completa
`programa.c → programa.i → programa.s`. El archivo `compiler.c` concentra esta
dependencia temporal. Cuando se implemente el compilador real, el cuerpo de
`compileFile()` se reemplazará conceptualmente por:

```text
lex → parse → validate → generateAssembly
```

El driver y la interfaz pública no necesitarán cambios.

## Archivos temporales

Los productos se escriben primero en archivos creados con `mkstemp()`. Solo se
publican mediante `rename()` después de que una etapa termina correctamente.
Así, un error no reemplaza una salida anterior por un archivo parcial.

Durante `-S`, el `.i` intermedio se elimina normalmente. Con `--keep-temp`, se
publica junto al archivo fuente y se conserva incluso cuando el compilador
encuentra un error posterior.

## Manejo de errores

Cada etapa detiene el pipeline cuando falla:

```text
fallo de GCC       → código 3 → no se ejecuta compileFile()
error del mock     → código 4 → no se publica el archivo .s
```

Durante esta entrega, los diagnósticos de sintaxis proceden de GCC. El módulo
de diagnósticos ya ofrece una función para asociar errores con archivo, línea y
columna cuando se incorpore el lexer/parser real.

El ensamblado y el enlace permanecen fuera del alcance de esta entrega.
