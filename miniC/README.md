# MiniC — compiler driver (Entrega 1)

Esta versión implementa la primera entrega del driver de MiniC: valida un
archivo fuente C y usa GCC como preprocesador para producir un archivo `.i`.
Todavía no compila MiniC a ensamblador ni genera ejecutables.

## Requisitos

- un sistema POSIX (Linux);
- un compilador compatible con C11;
- GCC disponible en `PATH`;
- `make`.

## Compilación

```bash
make
```

El comando crea el ejecutable `minic` en este directorio. El proyecto se
compila con `-Wall -Wextra -Wpedantic`.

## Uso

```bash
./minic archivo.c
./minic -E archivo.c
./minic -E -P archivo.c
./minic -E -P archivo.c -o salida.i
./minic --help
./minic --version
```

En esta entrega, ejecutar el driver con o sin `-E` termina después del
preprocesamiento. Si no se proporciona `-o`, `ruta/programa.c` genera
`ruta/programa.i`. De forma predeterminada, GCC conserva sus marcadores de
línea; la opción `-P` los elimina.

Ejemplo:

```bash
./minic -E -P examples/return_2.c
```

Esto produce `examples/return_2.i` mediante una operación equivalente a:

```bash
gcc -E -P examples/return_2.c -o examples/return_2.i
```

## Códigos de salida utilizados

| Código | Significado |
|---:|---|
| 0 | operación exitosa |
| 1 | uso incorrecto |
| 2 | entrada inexistente o ilegible |
| 3 | fallo del preprocesador |
| 7 | no se pudo publicar la salida |
| 8 | no se pudo administrar el temporal |
| 9 | error interno |

## Pruebas

```bash
make test
```

Las pruebas cubren los casos positivos de preprocesamiento y los errores
requeridos por Entrega 1. Los archivos de prueba se crean en un directorio
temporal y se eliminan al finalizar.

La separación de módulos y las decisiones principales están descritas en
[`docs/arquitectura.md`](docs/arquitectura.md).
