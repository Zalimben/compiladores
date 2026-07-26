#ifndef MINIC_OPTIONS_H
#define MINIC_OPTIONS_H

#include "compiler.h"

typedef enum {
    ACTION_RUN_PIPELINE,
    ACTION_SHOW_HELP,
    ACTION_SHOW_VERSION
} DriverAction;

typedef enum {
    STAGE_PREPROCESS,
    STAGE_COMPILE,
    STAGE_ASSEMBLE,
    STAGE_LINK
} CompilationStage;

typedef struct {
    const char *inputPath;
    char *outputPath;
    DriverAction action;
    CompilationStage finalStage;
    CompilerMode compilerMode;
    int suppressLineMarkers;
    int verbose;
    int keepTemporaryFiles;
} DriverOptions;

void initializeOptions(DriverOptions *options);
int parseArguments(int argc, char *argv[], DriverOptions *options);
void destroyOptions(DriverOptions *options);
void printHelp(void);
void printVersion(void);

#endif
