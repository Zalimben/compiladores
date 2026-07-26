# Compiladores y Lenguajes de Bajo Nivel

Repositorio académico dedicado al estudio de compiladores, análisis de
lenguajes y generación de código. Contiene ejercicios y proyectos desarrollados
de manera incremental para la asignatura **Compiladores y Lenguajes de Bajo
Nivel** de la FP-UNA.

Inspirado en el libro [Writing a C Compiler](https://nostarch.com/writing-c-compiler)

El objetivo principal es experimentar con las etapas de un compilador:

```text
Código fuente
    → preprocesamiento
    → análisis léxico
    → análisis sintáctico
    → análisis semántico
    → generación de código
    → ensamblado y enlace
```

## Contenido del repositorio

| Directorio | Descripción |
|---|---|
| [`miniC/`](miniC/) | Compiler driver completo: preprocesa, genera ensamblador mediante un mock de GCC, ensambla, enlaza y produce ejecutables nativos. |
| [`miniPascal/`](miniPascal/) | Analizador léxico y tabla de símbolos para un lenguaje con sintaxis tipo Pascal. |
| [`practica/`](practica/) | Ejemplo mínimo que relaciona una función en C con el ensamblador x86-64 generado. |

Cada proyecto posee objetivos y niveles de avance diferentes. Para conocer sus
opciones, arquitectura y forma de ejecución, consulta su documentación
específica:

- [Documentación de MiniC](miniC/README.md)
- [Documentación de miniPascal](miniPascal/README.md)
- [Plan de desarrollo del driver de MiniC](miniC/Plan_Compiler_Driver_MiniC.md)

## Requisitos generales

Los ejercicios están orientados principalmente a Linux y requieren, según el
proyecto:

- GCC o un compilador compatible con C11;
- GNU Make;
- un entorno POSIX para las funciones `fork()`, `execvp()` y `waitpid()`;
- una terminal para compilar y ejecutar los ejemplos.

## Inicio rápido con MiniC

Desde la raíz del repositorio:

```bash
make -C miniC
./miniC/minic miniC/examples/return_2.c
./miniC/examples/return_2
```

El driver genera el ejecutable `miniC/examples/return_2`; el programa finaliza
con código `2`. Para ejecutar las pruebas:

```bash
make -C miniC test
```

Para eliminar los artefactos producidos por la compilación:

```bash
make -C miniC clean
```

## Estado académico

Este repositorio está en desarrollo y sus implementaciones reflejan el avance
de distintas prácticas y entregas. No pretende ofrecer compiladores listos para
producción: se priorizan la claridad de las etapas, la experimentación y la
comprensión de los conceptos.

El contenido puede cambiar a medida que se incorporen nuevas fases, pruebas y
correcciones. Al reutilizarlo en un contexto académico, deben respetarse las
reglas de autoría, colaboración y citación establecidas por la institución y
por cada curso.

## Temas abordados

- reconocimiento de tokens y autómatas finitos;
- administración de una tabla de símbolos;
- procesamiento de argumentos de línea de comandos;
- ejecución segura de herramientas externas;
- preprocesamiento de código C;
- generación y observación de ensamblador x86-64;
- manejo de diagnósticos y códigos de salida;
- pruebas positivas y negativas de componentes del compilador.
