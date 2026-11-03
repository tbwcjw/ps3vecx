#ifndef _COMMON_H_
#define _COMMON_H_

#define USRDIR "/dev_hdd0/game/PS3VECX01/USRDIR/"
#define EMUSELF USRDIR "VECXEMU.SELF"
#define MENUSELF USRDIR "VECXMENU.SELF"
#define CARTS USRDIR "carts/"
#define ROM USRDIR "rom/"
#define OVERLAYS USRDIR "overlays/"
#define CONFIG USRDIR "config.txt"

#define EMU_PRIO 512
#define EMU_STACKSIZE 0x100000

#define MENU_PRIO 1000
#define MENU_STACKSIZE 0x100000

#define MAX_THICKNESS 5 //max line thickness

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <netinet/in.h>
#include <net/net.h>
#include <sys/process.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/sysutil.h>

#include "assert.h"
#include "debug.h"
#include <io/pad.h>

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

typedef enum
{
    EMU_EXIT_SUCCESS = 0,
    EMU_CONFIG_ERROR = -1,
    EMU_ROM_NOT_FOUND = -2,
    EMU_AUDIO_INIT = -3
} EmuExitCode;


int load_file(const char *path, void *dst, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(dst, 1, max, f);
    fclose(f);
    return (int)n;
}

static int set_dynamic_string(char** field, const char* value)
{
    char* copy = strdup(value);
    if (copy == NULL)
        return -1;

    free(*field); 
    *field = copy;
    return 0;
}

#endif // _COMMON_H_