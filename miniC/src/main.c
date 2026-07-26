/*
 * Punto de entrada del compiler driver de MiniC.
 *
 * main() se mantiene deliberadamente pequeño: interpreta las opciones,
 * delega la ejecución del pipeline al driver y libera los recursos. Esta
 * separación permite probar la lógica del compilador sin depender de main().
 */
#include "driver.h"
#include "options.h"

/*
 * argc contiene la cantidad de argumentos recibidos y argv contiene sus
 * textos. El valor retornado por main() se convierte en el código de salida
 * que la terminal puede consultar, por ejemplo, mediante "echo $?".
 */
int main(int argc, char *argv[]) {
    DriverOptions options;
    int result;

    initializeOptions(&options);
    result = parseArguments(argc, argv, &options);

    /*
     * El pipeline solo debe comenzar si la línea de comandos es válida.
     * parseArguments() ya muestra el diagnóstico cuando encuentra un error.
     */
    if (result == DRIVER_SUCCESS) {
        result = runDriver(&options);
    }

    /*
     * La liberación se realiza tanto en caso de éxito como de error.
     * free(NULL) es válido en C, por lo que también es seguro llamar a esta
     * función cuando el análisis de argumentos terminó anticipadamente.
     */
    destroyOptions(&options);

    return result;
}
