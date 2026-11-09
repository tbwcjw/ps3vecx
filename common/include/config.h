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

int parse_config_file(Config *config);
void set_config_defaults(Config *config);

#endif // CONFIG_H
