compiladores
============

**Compiladores FPUNA** 

Práctica de Programación Nro. 1

Autor: Julio Paciello

## Descripción

Implementación de un **Analizador Léxico** que reconoce componentes léxicos para un lenguaje con sintaxis tipo Pascal. El analizador tokeniza:

- **Números**: enteros, decimales y notación científica (ej: 123, 45.67, 1.5e-3)
- **Identificadores y palabras reservadas**: tokens alfanuméricos (ej: variable, BEGIN, END)
- **Literales y caracteres**: cadenas entre comillas simples (ej: 'hola', 'a')
- **Operadores relacionales**: <, >, <=, >=, <>, =
- **Operadores aritméticos**: +, -, *, /
- **Operadores de asignación**: :=
- **Signos de puntuación**: ( ) [ ] , ; . : !
- **Comentarios**: (* ... *) y { ... }

## Archivos

- `anlex.c` - Analizador léxico principal con máquina de estados para números
- `anlex.h` - Archivo de encabezado con definiciones de estructuras
- `tablaSimbolos.c` - Tabla de símbolos (gestión de identificadores y literales)
- `tablaSimbolos.h` - Encabezado de tabla de símbolos

## Compilación

```bash
gcc anlex.c tablaSimbolos.c -o anlex
```

## Uso

```bash
./anlex archivo_fuente.pas
```

Produce salida con línea, lexema y código de componente léxico:
```
Lin 1: BEGIN -> 270
Lin 1: var -> 1
```

## Características

- Gestión de números con autómata finito de 7 estados
- Soporte para comentarios anidados y multilinea
- Seguimiento de número de línea para reportes de error
- Tabla de símbolos para almacenar identificadores y literales
- Detección y reporte de errores léxicos
