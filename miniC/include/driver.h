#ifndef MINIC_DRIVER_H
#define MINIC_DRIVER_H

#include "options.h"

typedef enum {
    DRIVER_SUCCESS = 0,
    DRIVER_USAGE_ERROR = 1,
    DRIVER_INPUT_ERROR = 2,
    DRIVER_PREPROCESS_ERROR = 3,
    DRIVER_COMPILER_ERROR = 4,
    DRIVER_OUTPUT_ERROR = 7,
    DRIVER_TEMPORARY_ERROR = 8,
    DRIVER_INTERNAL_ERROR = 9
} DriverExitCode;

int runDriver(const DriverOptions *options);

#endif
