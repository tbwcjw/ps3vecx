#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct {
    char *rom;
    char *cart;
    char *overlay;
    int alpha;
    int thickness;
    int rotation;
    int sync;
    float gain;
    int pdecay;
} Config;

typedef enum {
    CONFIG_LOAD_ERROR = -1,
    CONFIG_CREATE_ERROR = -2,
    CONFIG_FILE_CREATED = 1
} ConfigLoadResult;

int parse_config_file(Config *config);
void set_config_defaults(Config *config);

#endif // CONFIG_H
