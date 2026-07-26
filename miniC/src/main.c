#include "driver.h"
#include "options.h"

int main(int argc, char *argv[]) {
    DriverOptions options;
    int result;

    initializeOptions(&options);
    result = parseArguments(argc, argv, &options);
    if (result == DRIVER_SUCCESS) {
        result = runDriver(&options);
    }
    destroyOptions(&options);

    return result;
}
