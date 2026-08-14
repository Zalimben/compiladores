# Plan de desarrollo del *compilador* para MiniC

## General

1. Implemente una librería de tabla de símbolos adecuada para el lenguaje. Esto requerirá
una estructura de tabla que incorpore información de ámbito, ya sea como tablas separadas vinculadas
en conjunto o con un mecanismo de eliminación que funcione de manera basada en pilas (Louden, Cap. 6).

2. Implemente un analizador léxico ya sea a mano como un DFA o utilizando Lex (Louden, Cap. 2).

3. Diseñe una estructura de árbol sintáctico apropiada para la generación mediante un
analizador sintáctico.

4. Implemente un analizador sintáctico (utilice el analizador léxico previamente construído), ya sea
a mano utilizando descendentes recursivos o mediante el uso de Yacc (Louden, Cap 4 y 5).
El analizador sintáctico deberá generar un árbol sintáctico apropiado.

5. Implemente un analizador semántico. El requerimiento principal del analizador, además de obtener información de la tabla de símbolos, es realizar verificación de tipo en el uso de variables y funciones. Como no hay apuntadores o estructuras, y el único tipo básico es el entero, los tipos que necesitan ser tratados mediante el verificador de tipo son los tipos void, integer, array y function.

6. Implemente un generador de código, de acuerdo con el ambiente de ejecución descrito en la sección anterior.