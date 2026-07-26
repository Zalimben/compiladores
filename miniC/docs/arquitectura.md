# Arquitectura de MiniC — Entrega 3

## Pipeline

```text
Fuente .c
   │
   ├─ GCC -E -P
   ▼
Preprocesado .i
   │
   ├─ compileFile() — mock con GCC
   ▼
Ensamblador .s
   │
   ├─ GCC -c -x assembler
   ▼
Objeto .o
   │
   ├─ GCC
   ▼
Ejecutable
```

Las opciones `-E`, `-S` y `-c` detienen el recorrido después de la etapa
correspondiente. Sin una opción de detención se ejecutan las cuatro etapas.

## Separación de responsabilidades

| Módulo | Responsabilidad |
|---|---|
| `main.c` | Inicializa las opciones, ejecuta el driver y devuelve su código de salida. |
| `options.c` | Interpreta argumentos, selecciona la etapa final y determina el producto. |
| `driver.c` | Coordina el pipeline, los temporales, el ensamblado y el enlace. |
| `process.c` | Ejecuta procesos externos mediante `fork()`, `execvp()` y `waitpid()`. |
| `compiler.c` | Implementa `compileFile()`; actualmente contiene el mock basado en GCC. |
| `diagnostics.c` | Presenta errores e información del modo detallado. |

El driver conoce `compileFile()`, pero no conoce cómo se implementa la
compilación de MiniC. Esta dependencia apunta a una interfaz, no al lexer ni al
parser.

## Punto futuro para lexer y parser

`src/compiler.c` contiene un comentario marcado como:

```text
PUNTO EXACTO PARA INCORPORAR EL COMPILADOR REAL
```

El bloque que construye y ejecuta el comando `gcc -S` deberá reemplazarse allí
por llamadas semejantes a:

```text
Lexer           lexer = lexerCreate(preprocessedPath)
TokenStream     tokens = lexerScan(lexer)
Parser          parser = parserCreate(tokens)
AstProgram      program = parserParseProgram(parser)
                semanticValidate(program)
                generateAssembly(program, assemblyPath)
```

El lexer, parser, AST, análisis semántico y generador deberán residir en módulos
propios. No deben añadirse a `driver.c`, `options.c` ni `process.c`.

La firma pública se conserva:

```c
CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
);
```

Por ello, reemplazar el mock no afectará las etapas de preprocesamiento,
ensamblado, enlace ni la interfaz de línea de comandos.

## Gestión de productos

Cada etapa escribe inicialmente en un archivo creado con `mkstemp()`. El
producto se publica con `rename()` solo después de un resultado exitoso.

```text
Sin --keep-temp:
    temporales únicos → se eliminan al terminar

Con --keep-temp:
    temporal .i → programa.i
    temporal .s → programa.s
    temporal .o → programa.o
```

Si una etapa falla:

1. no se ejecutan etapas posteriores;
2. se conserva cualquier intermedio solicitado mediante `--keep-temp`;
3. se eliminan los demás temporales;
4. no se reemplaza el producto final anterior;
5. se devuelve el código correspondiente a la etapa.

## Ejecución segura

Los comandos se representan como vectores de argumentos terminados en `NULL`.
`execvp()` los entrega directamente a GCC sin pasar por un shell, por lo que
las rutas con espacios no requieren escape especial.

Como los temporales no siempre terminan con la extensión tradicional, el
driver indica explícitamente el lenguaje:

- `-x cpp-output` para el archivo preprocesado;
- `-x assembler` para el archivo ensamblador.

Esto evita depender del sufijo aleatorio del nombre temporal.
