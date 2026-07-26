# MiniC — compiler driver (Entrega 2)

Esta versión implementa las dos primeras entregas del driver de MiniC:

```text
archivo.c → preprocesamiento → archivo.i
archivo.c → preprocesamiento → compilador MiniC → archivo.s
```

GCC se utiliza como preprocesador y, temporalmente, como compilador simulado
para producir ensamblador. Todavía no se implementan el lexer, el parser ni el
generador de código propios de MiniC. El ensamblado, el enlace y la producción
de un ejecutable corresponden a la Entrega 3.

## Requisitos

- Linux u otro sistema POSIX;
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
./minic archivo.c
./minic -E archivo.c
./minic -E -P archivo.c
./minic -S archivo.c
./minic -S archivo.c -o salida.s
./minic -v -S archivo.c
./minic --keep-temp -S archivo.c
./minic --help
./minic --version
```

En esta entrega:

- sin una opción de etapa, se conserva el comportamiento de Entrega 1 y se
  produce un archivo `.i`;
- `-E` detiene el pipeline después del preprocesamiento;
- `-P` elimina los marcadores de línea del producto de `-E`;
- `-S` ejecuta el mock del compilador mediante GCC y produce un archivo `.s`;
- `-o` cambia el nombre del producto final;
- `-v` muestra las etapas y los comandos ejecutados;
- `--keep-temp` conserva el `.i` utilizado durante una compilación con `-S`.

Sin `-o`, `programa.c` produce `programa.i` con `-E` y `programa.s` con `-S`.

## Mock del compilador

Para probar verticalmente el pipeline se utiliza este programa mínimo:

```c
int main(void) {
    return 2;
}
```

En esta entrega, `compileFile()` invoca una operación equivalente a:

```bash
gcc -S -x cpp-output programa.i -o programa.s
```

Por tratarse de un mock, GCC puede aceptar construcciones que todavía no forman
parte de MiniC. Esto no define la gramática futura del lenguaje. Cuando estén
disponibles, el lexer, el parser y el generador propios reemplazarán el interior
de `compileFile()` sin modificar el driver.

## Ejemplo

```bash
./minic -S examples/return_2.c
```

El resultado `examples/return_2.s` contiene ensamblador semejante a:

```asm
    .text
    .globl main
main:
    movl $2, %eax
    ret
```

Para observar también el archivo preprocesado y las etapas:

```bash
./minic -v --keep-temp -S examples/return_2.c
```

## Códigos de salida utilizados

| Código | Significado |
|---:|---|
| 0 | operación exitosa |
| 1 | uso incorrecto |
| 2 | entrada inexistente o ilegible |
| 3 | fallo del preprocesador |
| 4 | fallo del compilador simulado |
| 7 | no se pudo publicar la salida |
| 8 | no se pudo administrar un temporal |
| 9 | error interno |

## Pruebas

```bash
make test
```

Las pruebas cubren preprocesamiento, generación de ensamblador, nombres de
salida, modo detallado, conservación y limpieza de temporales, errores del
preprocesador y errores informados por el mock. También se comprueba que el
archivo `.s` pueda ser ensamblado por GCC.

La separación de módulos y las decisiones principales están descritas en
[`docs/arquitectura.md`](docs/arquitectura.md).
