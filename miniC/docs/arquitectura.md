# Arquitectura de MiniC — Entrega 1

La primera entrega implementa únicamente el recorrido:

```text
archivo.c → GCC (`-E`, opcionalmente `-P`) → archivo.i
```

El programa está separado en cuatro módulos:

- `options.c` interpreta la línea de comandos y determina el nombre de salida;
- `driver.c` valida la entrada y coordina el preprocesamiento;
- `process.c` ejecuta GCC mediante `fork()`, `execvp()` y `waitpid()`;
- `diagnostics.c` da un formato uniforme a los errores.

`main.c` solo inicializa las opciones, delega el trabajo y libera los recursos.
El driver no usa un shell ni `system()`, por lo que las rutas se entregan a GCC
como argumentos independientes.

El driver añade `-P` a los argumentos de GCC solamente cuando el usuario
solicita esa opción. Sin ella, el resultado conserva los marcadores de línea
del preprocesador.

La salida se construye primero en un archivo temporal ubicado junto al producto
final. Solo se renombra después de que GCC termina correctamente. Así, una
ejecución fallida no deja un `.i` parcial ni destruye una salida previa válida.

Las fases internas del compilador (`compiler.c`), la generación de ensamblador,
el ensamblado y el enlace se incorporarán en las entregas posteriores.
