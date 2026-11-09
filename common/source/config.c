#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "config.h"

static const char cfg_defaults[] =
    "rom=rom.dat\n"
    "cart=about.vec\n"
    "overlay=\n"
    "alpha=128\n"
    "thickness=2\n"
    "rotation=0\n"
    "sync=1\n"
    "gain=1.0\n"
    "pdecay=30\n";

void set_config_defaults(Config *config)
{
    config->rom = strdup("rom.dat");
    config->cart = strdup("about.vec");
    config->overlay = strdup("");
    config->alpha = 128;
    config->thickness = 2;
    config->rotation = 0;
    config->sync = 1;
    config->gain = 1.0f;
    config->pdecay = 30;
}


int parse_config_file(Config *config)
{
    int result = 0;
    FILE *file = fopen(CONFIG, "r");
    if (file == NULL)
    {
        file = fopen(CONFIG, "w");
        if (file == NULL)
            return -1; 

        int ret = fputs(cfg_defaults, file) == EOF;
        if (fclose(file) != 0 || ret < 0)
            return -1;

        result = -2;
        file = fopen(CONFIG, "r");
        if (file == NULL)
            return -2;
    }

    char line[128 + 8];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char *sep = strchr(line, '=');
        if (!sep)
            continue;

        *sep = '\0';
        char *key = line;
        char *value = sep + 1;
        while (*value == ' ')
            value++;

        if (strcmp(key, "cart") == 0)
            set_dynamic_string(&config->cart, value);
        else if (strcmp(key, "rom") == 0)
            set_dynamic_string(&config->rom, value);
        else if (strcmp(key, "overlay") == 0)
            set_dynamic_string(&config->overlay, value);
        else if (strcmp(key, "alpha") == 0)
            config->alpha = atoi(value);
        else if (strcmp(key, "thickness") == 0)
            config->thickness = atoi(value);
        else if (strcmp(key, "rotation") == 0)
            config->rotation = atoi(value);
        else if (strcmp(key, "sync") == 0)
            config->sync = atoi(value);
        else if (strcmp(key, "gain") == 0)
            config->gain = strtof(value, NULL);
        else if (strcmp(key, "pdecay") == 0)
            config->pdecay = atoi(value);
    }

    fclose(file);
    return result;
}
