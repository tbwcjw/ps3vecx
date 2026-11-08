#ifndef _COMMON_H
#define _COMMON_H


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>

#include <netinet/in.h>
#include <net/net.h>
#include <sys/process.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>

#include "config.h"
#include "debug.h"

#define USRDIR "/dev_hdd0/game/PS3VECX01/USRDIR/"
#define CONFIG USRDIR "config.txt"

#define EMUSELF USRDIR "VECXEMU.SELF"
#define MENUSELF USRDIR "VECXMENU.SELF"
#define CARTS USRDIR "carts/"
#define ROM USRDIR "rom/"
#define OVERLAYS USRDIR "overlays/"

#define EMU_PRIO 512
#define EMU_STACKSIZE 0x100000

#define MENU_PRIO 1000
#define MENU_STACKSIZE 0x100000

#define MAX_THICKNESS 5 //max line thickness

typedef enum
{
    EMU_EXIT_SUCCESS = 0,
    EMU_CONFIG_ERROR = -1,
    EMU_ROM_NOT_FOUND = -2,
    EMU_AUDIO_INIT = -3,
    EMU_VECX_INIT = -4
} EmuExitCode;

int load_file(const char *path, void *dst, size_t max);
int set_dynamic_string(char **field, const char *value);
#endif // COMMON_H