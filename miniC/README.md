# MiniC — compiler driver (Entrega 3)

Esta versión completa el compiler driver y coordina el pipeline nativo:

```text
archivo.c → archivo.i → archivo.s → archivo.o → ejecutable
```

GCC se utiliza para preprocesar, ensamblar y enlazar. También se utiliza
temporalmente como mock de `compileFile()` para transformar `.i` en `.s`.
Todavía no se implementan el lexer, el parser ni el generador de código propios
de MiniC.

## Requisitos

- Linux x86-64 u otro sistema POSIX compatible;
- un compilador compatible con C11;
- GCC disponible en `PATH`;
- GNU Make.

## Compilación

```bash
make
```

El comando crea `minic` en este directorio. El código se compila con
`-Wall -Wextra -Wpedantic`.

## Uso

```bash
./minic programa.c
./minic -E programa.c
./minic -E -P programa.c
./minic -S programa.c
./minic -c programa.c
./minic --lex programa.c
./minic --parse programa.c
./minic --codegen programa.c
./minic programa.c -o aplicacion
./minic --keep-temp programa.c
./minic -v programa.c
./minic --help
./minic --version
```

### Etapas

| Opción | Última etapa | Producto predeterminado |
|---|---|---|
| `-E` | Preprocesamiento | `programa.i` |
| `-S` | Compilación simulada | `programa.s` |
| `-c` | Ensamblado | `programa.o` |
| Sin opción | Enlace | `programa` |

`-o archivo` cambia el nombre del producto final de cualquiera de estas
etapas. `-P` elimina los marcadores de línea del producto solicitado con `-E`;
las compilaciones posteriores los eliminan internamente.

### Modos internos

Los siguientes modos preparan la integración del futuro compilador:

| Opción | Punto de detención futuro | Mock actual |
|---|---|---|
| `--lex` | Después del lexer, antes del parser | `gcc -fsyntax-only` |
| `--parse` | Después del lexer y parser | `gcc -fsyntax-only` |
| `--codegen` | Después de generar la representación de ensamblador, antes de emitirla | `gcc -S` hacia un temporal descartado |

GCC no permite detenerse exactamente en las mismas fronteras internas. Por
eso, `--lex` y `--parse` tienen el mismo comportamiento temporal y pueden
detectar errores de fases posteriores a la que representan.

Estos modos no generan un producto visible y no aceptan `-o`. Con
`--keep-temp` se conserva únicamente el `.i` entregado al mock:

```bash
./minic --keep-temp --parse programa.c
```

## Ejemplo completo

```bash
./minic examples/return_2.c
./examples/return_2
echo $?
```

Resultado esperado:

```text
2
```

Para ver los cuatro comandos ejecutados:

```bash
./minic -v examples/return_2.c
```

## Archivos temporales

Sin `--keep-temp`, una compilación completa conserva únicamente:

```text
return_2.c
return_2
```

Con:

```bash
./minic --keep-temp examples/return_2.c
```

se conservan:

```text
return_2.c
return_2.i
return_2.s
return_2.o
return_2
```

Los intermedios se crean con nombres únicos y los productos finales se publican
mediante `rename()`. Si una etapa falla, las etapas posteriores no se ejecutan,
los temporales no solicitados se eliminan y una salida anterior no se reemplaza
por un archivo parcial.

## Mock del compilador MiniC

La interfaz estable es:

```c
CompilationResult compileFile(
    const char *preprocessedPath,
    const char *assemblyPath
);
```

En esta entrega ejecuta una operación equivalente a:

```bash
gcc -S -x cpp-output programa.i -o programa.s
```

El punto exacto de sustitución está documentado en `src/compiler.c`. Cuando se
implemente el compilador real, el interior de `compileFile()` deberá ejecutar:

```text
lexer → parser → análisis semántico → generación de ensamblador
```

Estas fases no deben incorporarse al driver. Por tratarse de un mock, GCC puede
aceptar construcciones que todavía no definen la futura gramática de MiniC.

## Códigos de salida

| Código | Significado |
|---:|---|
| 0 | operación exitosa |
| 1 | uso incorrecto |
| 2 | entrada inexistente o ilegible |
| 3 | fallo del preprocesador |
| 4 | fallo del compilador simulado |
| 5 | fallo del ensamblador |
| 6 | fallo del enlazador |
| 7 | no se pudo publicar la salida |
| 8 | error al administrar temporales |
| 9 | error interno |

## Pruebas

```bash
make test
```

La suite cubre las cuatro etapas, los tres modos internos simulados, productos
predeterminados y personalizados, limpieza y conservación de intermedios,
rutas con espacios, errores de uso, fallos por etapa, diagnósticos del mock y
el valor retornado por el ejecutable.

Las decisiones técnicas están descritas en
[`docs/arquitectura.md`](docs/arquitectura.md).
