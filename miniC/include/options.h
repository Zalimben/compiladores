#ifndef MINIC_OPTIONS_H
#define MINIC_OPTIONS_H

typedef enum {
    ACTION_PREPROCESS,
    ACTION_SHOW_HELP,
    ACTION_SHOW_VERSION
} DriverAction;

typedef struct {
    const char *inputPath;
    char *outputPath;
    DriverAction action;
    int suppressLineMarkers;
} DriverOptions;

void initializeOptions(DriverOptions *options);
int parseArguments(int argc, char *argv[], DriverOptions *options);
void destroyOptions(DriverOptions *options);
void printHelp(void);
void printVersion(void);

#endif
