# Plan de desarrollo del *compiler driver* de MiniC

## 1. Propósito

El *compiler driver* será el programa encargado de coordinar todas las etapas necesarias para transformar un archivo fuente MiniC en un ejecutable nativo.

El desarrollo seguirá el enfoque incremental de Nora Sandler:

```text
Código fuente → preprocesamiento → compilación → ensamblado → enlace → ejecutable
```

El usuario deberá poder ejecutar:

```bash
./minic programa.c
```

y obtener, de forma predeterminada, un ejecutable denominado:

```text
programa
```

Para un archivo como:

```c
int main(void) {
    return 2;
}
```

el resultado deberá comprobarse mediante:

```bash
./programa
echo $?
```

Salida esperada:

```text
2
```

---

## 2. Decisión de arquitectura

El proyecto seguirá el flujo de compilación nativa utilizado por Sandler:

```mermaid
flowchart LR
    A["Fuente .c"] --> B["Preprocesador"]
    B --> C["Compilador MiniC"]
    C --> D["Ensamblador .s"]
    D --> E["Ensamblado y enlace"]
    E --> F["Ejecutable nativo"]
```

En consecuencia:

- el compilador MiniC generará código ensamblador;
- GCC se utilizará como preprocesador, ensamblador y enlazador;
- el resultado final será un ejecutable nativo;
- no se utilizarán bytecode ni una máquina virtual de pila;
- el alcance inicial estará orientado a Linux x86-64.

El flujo completo será:

```text
programa.c
    ↓ gcc -E -P
programa.i
    ↓ compilador MiniC
programa.s
    ↓ gcc -c
programa.o
    ↓ gcc
programa
```

---

## 3. Alcance general

El *compiler driver* será responsable de:

1. procesar los argumentos de línea de comandos;
2. validar el archivo fuente;
3. determinar la etapa final solicitada;
4. invocar el preprocesador del sistema;
5. entregar el archivo preprocesado al compilador MiniC;
6. recibir y guardar el código ensamblador;
7. invocar GCC para ensamblar el código;
8. invocar GCC para enlazar el archivo objeto;
9. producir el ejecutable final;
10. asignar nombres predeterminados a las salidas;
11. aceptar un nombre de salida mediante `-o`;
12. administrar los archivos temporales;
13. detener el proceso cuando una etapa falla;
14. propagar códigos de error;
15. presentar diagnósticos comprensibles;
16. mostrar las etapas ejecutadas mediante el modo detallado;
17. conservar los archivos intermedios cuando se solicite.

### Separación de responsabilidades

El driver no implementará directamente:

- el análisis léxico;
- el análisis sintáctico;
- el AST;
- el análisis semántico;
- la generación interna de código.

Su responsabilidad será coordinar estos componentes mediante interfaces estables. El compilador MiniC será responsable de transformar el archivo preprocesado en ensamblador.

---

## 4. Interfaz final de línea de comandos

La sintaxis general será:

```bash
minic [opciones] archivo.c
```

### Opciones obligatorias

| Opción | Comportamiento |
|---|---|
| `-E` | Ejecuta solamente el preprocesamiento |
| `-P` | Elimina los marcadores de línea del resultado preprocesado |
| `-S` | Genera código ensamblador y se detiene |
| `-c` | Genera un archivo objeto y se detiene |
| `-o archivo` | Define el nombre del producto final |
| `--keep-temp` | Conserva los archivos intermedios |
| `-v` | Muestra las etapas y comandos ejecutados |
| `--help` | Muestra la ayuda del programa |
| `--version` | Muestra la versión del compilador |

### Ejemplos

```bash
./minic -E programa.c
./minic -S programa.c
./minic -S programa.c -o salida.s
./minic -c programa.c
./minic programa.c
./minic programa.c -o aplicacion
./minic --keep-temp programa.c
./minic -v programa.c
./minic --help
./minic --version
```

### Etapas de detención

| Comando | Última etapa ejecutada | Producto |
|---|---|---|
| `minic -E programa.c` | Preprocesamiento | `programa.i` |
| `minic -S programa.c` | Compilación | `programa.s` |
| `minic -c programa.c` | Ensamblado | `programa.o` |
| `minic programa.c` | Enlace | `programa` |
| `minic programa.c -o app` | Enlace | `app` |

---

## 5. Programa mínimo admitido

Durante las tres primeras semanas, el compilador interno solamente necesita reconocer una función `main` que retorne una constante entera:

```c
int main(void) {
    return 2;
}
```

Para Linux x86-64, una salida mínima posible es:

```asm
    .globl main
main:
    movl $2, %eax
    ret
```

La finalidad de este programa mínimo es comprobar verticalmente el pipeline completo. La ampliación del lenguaje MiniC se realizará después, sin modificar la arquitectura esencial del driver.

---

## 6. Arquitectura del proyecto

```text
minic/
├── include/
│   ├── driver.h
│   ├── options.h
│   ├── process.h
│   ├── compiler.h
│   └── diagnostics.h
├── src/
│   ├── main.c
│   ├── driver.c
│   ├── options.c
│   ├── process.c
│   ├── compiler.c
│   └── diagnostics.c
├── tests/
│   ├── driver/
│   ├── valid/
│   └── invalid/
├── examples/
│   └── return_2.c
├── docs/
├── build/
├── Makefile
└── README.md
```

### Responsabilidades de los archivos

| Archivo | Responsabilidad |
|---|---|
| `main.c` | Punto de entrada del programa |
| `options.c` | Procesamiento y validación de argumentos |
| `driver.c` | Coordinación del pipeline |
| `process.c` | Ejecución segura de procesos externos |
| `compiler.c` | Coordinación de las fases internas del compilador |
| `diagnostics.c` | Presentación de errores, advertencias e información |

El archivo `main.c` deberá mantenerse pequeño:

```c
int main(int argc, char *argv[]) {
    DriverOptions options;

    if (!parseArguments(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    return runDriver(&options);
}
```

---

## 7. Interfaces sugeridas

### Etapa final

```c
typedef enum {
    STAGE_PREPROCESS,
    STAGE_COMPILE,
    STAGE_ASSEMBLE,
    STAGE_LINK
} CompilationStage;
```

### Opciones del driver

```c
typedef struct {
    const char *inputPath;
    const char *outputPath;
    CompilationStage finalStage;
    int verbose;
    int keepTemporaryFiles;
} DriverOptions;
```

### Procesamiento de argumentos y ejecución

```c
int parseArguments(
    int argc,
    char *argv[],
    DriverOptions *options
);

int runDriver(
    const DriverOptions *options
);
```

### Interfaz estable con el compilador

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

Esta interfaz deberá permanecer estable cuando posteriormente se incorporen el lexer, parser, AST, análisis semántico y un generador de código más completo.

---

# Entrega 1 — Driver básico y preprocesamiento

## 8. Semana 1

### Objetivo

Construir la interfaz básica del driver y completar:

```text
programa.c → preprocesador → programa.i
```

### Funcionalidades

El driver deberá:

1. recibir un único archivo `.c`;
2. comprobar que se haya proporcionado una entrada;
3. validar que el archivo exista;
4. validar la extensión del archivo;
5. procesar las opciones iniciales;
6. mostrar la ayuda y la versión;
7. ejecutar el preprocesador;
8. detectar errores del preprocesador;
9. producir el archivo `.i`;
10. devolver códigos de salida apropiados.

### Interfaz requerida

```bash
./minic programa.c
./minic -E programa.c
./minic -E -P programa.c
./minic --help
./minic --version
```

En esta entrega, las opciones `-E -P` deberán realizar una operación equivalente a:

```bash
gcc -E -P programa.c -o programa.i
```

Donde:

- `-E` solicita únicamente el preprocesamiento;
- `-P` elimina los marcadores de línea del resultado;
- `programa.i` es el archivo preprocesado.

### Ejecución segura de GCC

No se utilizará `system()`. En sistemas POSIX se recomienda `fork()` y `execvp()`:

```c
char *arguments[] = {
    "gcc",
    "-E",
    "-P",
    inputPath,
    "-o",
    preprocessedPath,
    NULL
};

execvp(arguments[0], arguments);
```

Esta técnica evita que caracteres especiales incluidos en una ruta sean interpretados por un shell.

### Criterios de aceptación

- `./minic -E programa.c` genera `programa.i`.
- Rechaza archivos inexistentes.
- Rechaza archivos con una extensión no permitida.
- Rechaza opciones desconocidas.
- Informa cuando falta el archivo de entrada.
- Informa cuando `-o` no tiene un argumento.
- Propaga el fallo del preprocesador.
- No utiliza `system()`.
- Compila con `-Wall -Wextra -Wpedantic`.
- Los mensajes de error son claros.

### Entregables

- código fuente del driver;
- archivos de cabecera;
- programa MiniC de prueba;
- pruebas positivas y negativas;
- `README.md`;
- descripción breve de la arquitectura.

---

# Entrega 2 — Compilación mínima y generación de ensamblador

## 9. Semana 2

### Objetivo

Integrar el driver con el compilador MiniC para completar:

```text
programa.c → programa.i → programa.s
```

### Flujo

```mermaid
flowchart LR
    A["programa.c"] --> B["programa.i"]
    B --> C["compileFile()"]
    C --> D["programa.s"]
```

### Funcionalidades

El driver deberá:

1. ejecutar el preprocesador;
2. generar el archivo preprocesado;
3. entregar el archivo `.i` a `compileFile()`;
4. reconocer el programa mínimo;
5. generar código ensamblador x86-64;
6. guardar el resultado en un archivo `.s`;
7. implementar la opción `-S`;
8. implementar `-o` para cambiar el nombre de la salida;
9. implementar el modo detallado `-v`;
10. detener el pipeline cuando el compilador informa un error.

### Interfaz requerida

```bash
./minic -E programa.c
./minic -S programa.c
./minic -S programa.c -o salida.s
./minic -v -S programa.c
./minic --keep-temp -S programa.c
```

### Resultado esperado

Para:

```c
int main(void) {
    return 2;
}
```

se deberá producir un archivo ensamblador semejante a:

```asm
    .globl main
main:
    movl $2, %eax
    ret
```

### Criterios de aceptación

- Preprocesa correctamente la entrada.
- Compila `return 2;`.
- Produce un archivo `.s` válido.
- `-S` impide el ensamblado y el enlace.
- `-o` cambia el archivo de salida.
- `-v` muestra las etapas ejecutadas.
- Un error del compilador interrumpe el pipeline.
- Los errores del driver se distinguen de los errores del compilador.
- El driver y `compileFile()` están desacoplados.
- Las funciones añadidas incluyen pruebas.

### Entregables

- driver actualizado;
- interfaz estable con `compileFile()`;
- compilador mínimo;
- generador mínimo de ensamblador;
- ejemplos válidos e inválidos;
- pruebas de integración;
- documentación actualizada.

---

# Entrega 3 — Ensamblado, enlace e integración final

## 10. Semana 3

### Objetivo

Completar el pipeline:

```text
programa.c → programa.i → programa.s → programa.o → ejecutable
```

### Funcionamiento esperado

```bash
./minic return_2.c
./return_2
echo $?
```

Resultado:

```text
2
```

### Funcionalidades

El driver deberá:

1. generar el archivo ensamblador;
2. invocar GCC para ensamblar;
3. generar un archivo objeto;
4. implementar la opción `-c`;
5. invocar GCC para enlazar;
6. producir el ejecutable final;
7. determinar automáticamente los nombres de salida;
8. respetar el nombre indicado mediante `-o`;
9. eliminar archivos temporales;
10. conservarlos mediante `--keep-temp`;
11. manejar fallos y terminaciones de los procesos externos;
12. contar con pruebas automatizadas del pipeline completo.

### Interfaz final requerida

```bash
./minic programa.c
./minic -E programa.c
./minic -S programa.c
./minic -c programa.c
./minic programa.c -o ejecutable
./minic --keep-temp programa.c
./minic -v programa.c
./minic --help
./minic --version
```

### Criterios de aceptación

- Produce un ejecutable nativo válido.
- El ejecutable retorna el valor esperado.
- `-E`, `-S` y `-c` detienen correctamente el pipeline.
- `-o` funciona en las etapas correspondientes.
- Los nombres predeterminados son correctos.
- Los archivos temporales se eliminan automáticamente.
- `--keep-temp` conserva los archivos intermedios.
- Ninguna etapa posterior se ejecuta después de un fallo.
- Los códigos de error son diferentes de cero ante un fallo.
- Las rutas con espacios se procesan correctamente.
- Las pruebas incluyen casos positivos y negativos.
- El driver y el compilador permanecen desacoplados.
- El proyecto compila sin advertencias importantes.

### Entregables

- versión final del código fuente;
- archivos de cabecera;
- `Makefile`;
- pruebas automatizadas;
- programas de ejemplo;
- documentación técnica;
- manual de uso;
- demostración del pipeline completo;
- informe breve de decisiones de diseño.

---

## 11. Cronograma resumido

| Semana | Entrega | Resultado verificable |
|---:|---|---|
| 1 | Driver y preprocesamiento | `minic -E programa.c` produce `programa.i` |
| 2 | Compilación mínima | `minic -S programa.c` produce `programa.s` |
| 3 | Ensamblado y enlace | `minic programa.c` produce un ejecutable |

Las entregas son acumulativas. Cada semana debe comenzar desde la versión funcional de la semana anterior.

---

## 12. Convención de nombres

| Entrada | Etapa | Salida predeterminada |
|---|---|---|
| `programa.c` | Preprocesamiento | `programa.i` |
| `programa.c` | Compilación | `programa.s` |
| `programa.c` | Ensamblado | `programa.o` |
| `programa.c` | Enlace | `programa` |

### Sin `--keep-temp`

Al finalizar una compilación completa deberán permanecer:

```text
programa.c
programa
```

### Con `--keep-temp`

Deberán conservarse:

```text
programa.c
programa.i
programa.s
programa.o
programa
```

---

## 13. Gestión de archivos temporales

El driver deberá:

- crear archivos intermedios con nombres seguros;
- evitar sobrescribir accidentalmente archivos del usuario;
- eliminar los temporales después de una compilación completa;
- conservarlos únicamente cuando se utilice `--keep-temp`;
- realizar la limpieza aunque falle una etapa intermedia;
- conservar el producto solicitado cuando se utilice una opción de detención.

Por ejemplo:

- con `-E`, el archivo `.i` es un producto final y no debe eliminarse;
- con `-S`, el archivo `.s` es un producto final;
- con `-c`, el archivo `.o` es un producto final;
- sin una opción de detención, `.i`, `.s` y `.o` son temporales.

---

## 14. Manejo de errores

El driver deberá distinguir entre los fallos de sus diferentes etapas.

| Código | Significado |
|---:|---|
| `0` | Operación exitosa |
| `1` | Uso incorrecto del driver |
| `2` | Archivo de entrada no encontrado |
| `3` | Fallo del preprocesador |
| `4` | Fallo del compilador MiniC |
| `5` | Fallo del ensamblador |
| `6` | Fallo del enlazador |
| `7` | No se pudo crear una salida |
| `8` | Error al administrar archivos temporales |
| `9` | Error interno del compilador |

### Ejemplos de diagnósticos

```text
minic: error: no se especificó un archivo de entrada
minic: error: no se puede abrir 'programa.c'
minic: error: opción desconocida '--abc'
minic: error: la opción '-o' requiere un nombre de archivo
minic: error: el preprocesamiento terminó con código 1
programa.c:4:12: error: se esperaba ';'
```

Cuando una etapa falla:

1. no se ejecutan las etapas posteriores;
2. se muestra un diagnóstico;
3. se eliminan los temporales correspondientes;
4. se devuelve un código diferente de cero.

---

## 15. Plan de pruebas

### Casos positivos

```bash
./minic -E return_2.c
./minic -S return_2.c
./minic -S return_2.c -o salida.s
./minic -c return_2.c
./minic return_2.c
./minic return_2.c -o app
./minic --keep-temp return_2.c
./minic -v return_2.c
./minic --help
./minic --version
```

### Casos negativos

```bash
./minic
./minic inexistente.c
./minic archivo.txt
./minic --opcion-desconocida programa.c
./minic -o
./minic -E -S programa.c
./minic programa1.c programa2.c
```

### Aspectos que deben verificarse

- ausencia del archivo de entrada;
- archivo inexistente;
- extensión no permitida;
- opción desconocida;
- opciones de detención incompatibles;
- ausencia del argumento de `-o`;
- más de un archivo fuente;
- fallo del preprocesador;
- error del compilador;
- fallo del ensamblador;
- fallo del enlazador;
- imposibilidad de crear la salida;
- limpieza después de un error;
- conservación de temporales;
- rutas con espacios;
- propagación de códigos de salida;
- valor retornado por el ejecutable.

---

## 16. Matriz de trazabilidad de entregables

| Capacidad | Entrega 1 | Entrega 2 | Entrega 3 |
|---|:---:|:---:|:---:|
| Lectura de argumentos | ✓ | ✓ | ✓ |
| Validación del archivo fuente | ✓ | ✓ | ✓ |
| `--help` y `--version` | ✓ | ✓ | ✓ |
| Preprocesamiento | ✓ | ✓ | ✓ |
| Opción `-E` | ✓ | ✓ | ✓ |
| Integración con `compileFile()` |  | ✓ | ✓ |
| Generación de ensamblador |  | ✓ | ✓ |
| Opción `-S` |  | ✓ | ✓ |
| Opción `-o` | Inicial | ✓ | ✓ |
| Modo `-v` |  | ✓ | ✓ |
| Generación de archivo objeto |  |  | ✓ |
| Opción `-c` |  |  | ✓ |
| Enlace |  |  | ✓ |
| Ejecutable nativo |  |  | ✓ |
| Limpieza completa de temporales | Inicial | Parcial | ✓ |
| `--keep-temp` |  | Inicial | ✓ |
| Pruebas del pipeline completo |  |  | ✓ |

---

## 17. Criterios generales de evaluación

### Corrección funcional

- Cada opción produce el resultado esperado.
- Las etapas se ejecutan en el orden correcto.
- Los modos de detención funcionan correctamente.
- El ejecutable final retorna el valor esperado.

### Diseño

- El driver no contiene la lógica interna del compilador.
- Los módulos tienen responsabilidades claramente separadas.
- La interfaz con `compileFile()` es estable.
- No existe duplicación innecesaria.

### Robustez

- Los errores detienen el pipeline.
- Se propagan correctamente los códigos de salida.
- Los temporales se limpian de manera segura.
- Las rutas se entregan a los procesos sin utilizar un shell.

### Calidad del código

- Compila con `-Wall -Wextra -Wpedantic`.
- Los nombres de funciones y estructuras son descriptivos.
- La gestión de memoria y recursos es correcta.
- El código mantiene un formato consistente.

### Pruebas y documentación

- Existen pruebas positivas y negativas.
- El `README.md` explica cómo compilar y utilizar el driver.
- Se documentan las decisiones principales.
- Los ejemplos pueden reproducirse.

---

## 18. Fuera de alcance

- compilación de varios archivos fuente en una sola ejecución;
- enlace con bibliotecas personalizadas;
- optimización;
- generación para múltiples arquitecturas;
- soporte simultáneo para Windows, macOS y Linux;
- selección de diferentes compiladores del sistema;
- compilación incremental;
- resolución automática de dependencias;
- modo interactivo;
- ejecución mediante una máquina virtual;
- generación de bytecode;
- implementación completa de MiniC;
- opciones internas como `--lex`, `--parse`, `--validate` o `--tacky`.

Las opciones de inspección interna podrán incorporarse posteriormente, cuando estén implementadas las respectivas fases del compilador.

---

## Lista de verificación final

### Entrega 1

- [ ] Recibe un archivo `.c`.
- [ ] Valida existencia y extensión.
- [ ] Implementa `--help`.
- [ ] Implementa `--version`.
- [ ] Ejecuta el preprocesador.
- [ ] Implementa `-E`.
- [ ] Propaga errores del preprocesador.
- [ ] Incluye pruebas positivas y negativas.
- [ ] Incluye `README.md`.

### Entrega 2

- [ ] Integra `compileFile()`.
- [ ] Reconoce `return 2;`.
- [ ] Genera ensamblador x86-64.
- [ ] Implementa `-S`.
- [ ] Implementa `-o` para el ensamblador.
- [ ] Implementa `-v`.
- [ ] Detiene el pipeline ante errores.
- [ ] Incluye pruebas de integración.
- [ ] Actualiza la documentación.

### Entrega 3

- [ ] Genera el archivo objeto.
- [ ] Implementa `-c`.
- [ ] Enlaza el ejecutable.
- [ ] Genera nombres predeterminados.
- [ ] Implementa completamente `-o`.
- [ ] Elimina archivos temporales.
- [ ] Implementa `--keep-temp`.
- [ ] Procesa rutas con espacios.
- [ ] Automatiza las pruebas completas.
- [ ] Demuestra que el ejecutable retorna `2`.
- [ ] Incluye el informe final de decisiones de diseño.
