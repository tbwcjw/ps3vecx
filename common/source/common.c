#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int load_file(const char *path, void *dst, size_t max)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    size_t size = fread(dst, 1, max, file);
    fclose(file);
    return (int)size;
}

int set_dynamic_string(char **field, const char *value)
{
    char *copy = strdup(value);
    if (copy == NULL)
        return -1;

    free(*field);
    *field = copy;
    return 0;
}
