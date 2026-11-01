#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct {
    char *rom;
    char *cart;
    char *overlay;
    int alpha;
    int thickness;
    int rotation;
    int sync;
    float gain;
} Config;

#endif //_CONFIG_H_