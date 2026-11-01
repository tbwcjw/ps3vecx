#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>


#include <io/pad.h>
#include <sysutil/osk.h>
#include <sys/process.h>
#include <tiny3d.h>
#include <libfont2.h>
#include <string.h>

#include "osk.h"
#include "debug.h"
#include "font.h"



#define WHITE 0xFFFFFFFF
#define BLACK 0x00000000
#define LIGHT_GREY 0x878787FF
#define DARK_GREY 0x5A5A5AFF
#define TRIANGLE 0x40E2A0FF
#define CIRCLE 0xFF6666FF
#define CROSS 0x7CB2E8FF
#define SQUARE 0xFF69F8FF
#define YELLOW 0xFFFF00FF

#define FONT_DEFAULT FONT_SMALL
#define FONT_SMALL SetFontSize(8, 13)
#define FONT_LARGE SetFontSize(12, 20)
#define FONT_XLARGE SetFontSize(14, 23)
#define FONT_XXLARGE SetFontSize(16, 26)
#define FONT_HUGE SetFontSize(18, 30)
#define FONT_XHUGE SetFontSize(20, 34)
#define FONT_GIANT SetFontSize(24, 40)
#define FONT_TITLE SetFontSize(28, 46)
#define FONT_HEADER SetFontSize(32, 52)

#define USRDIR "/dev_hdd0/game/PS3VECX01/USRDIR/"
#define EMUSELF USRDIR "VECXEMU.SELF"
#define MENUSELF USRDIR "VECXMENU.SELF"
#define CARTS USRDIR "carts/"
#define DEFAULTCART CARTS "boot.vec"
#define ROM USRDIR "rom/"
#define OVERLAYS USRDIR "overlays/"
#define CONFIG USRDIR "config.txt"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum
{
    STATE_GAME_SECTION,
    STATE_OVERLAY_SECTION,
    STATE_OPTIONS_SECTION,
    STATE_LAUNCH_SECTION
} State;

static State currentState = STATE_GAME_SECTION;

typedef struct
{
    char *filename;
    char *header;
} CartOption;

typedef struct
{
    char *filename;
} OverlayOption;

typedef struct
{
    char *name;
    char *osk_helper;
    float value;
} OptionOption;

static int cartIndex = 0;
static int cartCount = 0;
static CartOption *cart_options = NULL;

static int overlayIndex = 0;
static int overlayCount = 0;
static OverlayOption *overlay_options = NULL;

static int optionIndex = 0;
static OptionOption option_options[] = {
    {"overlay alpha", "Overlay Alpha: Between 0 and 255",255},
    {"line thickness", "Line Thickness: Between 1 and 5",2},
    {"rotation", "Rotation: 0 = Normal, 1 = Clockwise, -1 = Counterclockwise", 0},
    {"vsync", "VSync: 0 or 1", 1},
    {"audio gain", "Audio Gain: Between 0.25 and 5.0", 1.0f}};

//osk
#define OSK_BUF_LEN 8
static char osk_option_buf[ARRAY_SIZE(option_options)][OSK_BUF_LEN];



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

Config config = {0};

static volatile int running = 1;
static float tick = 0.0f;

// pad
typedef struct
{
    int BTN_CROSS;
    int BTN_CIRCLE;
    int BTN_SQUARE;
    int BTN_TRIANGLE;
    int BTN_UP;
    int BTN_DOWN;
    int BTN_LEFT;
    int BTN_RIGHT;
    int BTN_SELECT;
    int BTN_START;
} PadButtons;

static PadButtons lastPad = {0};
#define PRESSED_NOW(btn) (paddata.btn && !lastPad.btn)
#define RELEASED_NOW(btn) (!paddata.btn && lastPad.btn)


void cycle_index(int *index, int count, int direction)
{
    if (count <= 0)
        return;

    *index += direction;

    if (*index >= count)
        *index = 0;

    if (*index < 0)
        *index = count - 1;
}

void cycle_state(State *state, int direction)
{
    if (direction > 0)
    {
        // DOWN
        if (*state == STATE_LAUNCH_SECTION)
            *state = STATE_GAME_SECTION;
        else
            *state = (State)(*state + 1);
    }
    else
    {
        // UP
        if (*state == STATE_GAME_SECTION)
            *state = STATE_LAUNCH_SECTION;
        else
            *state = (State)(*state - 1);
    }
}
// animation tick


uint32_t sin_brightness(uint32_t base_color, float speed_multiplier, float min_brightness)
{
    float pulse = (sinf(tick * speed_multiplier) + 1.0f) * 0.5f;

    float brightness = min_brightness + pulse * (255.0f - min_brightness);

    uint8_t r = ((base_color >> 24) & 0xFF) * brightness / 255.0f;
    uint8_t g = ((base_color >> 16) & 0xFF) * brightness / 255.0f;
    uint8_t b = ((base_color >> 8) & 0xFF) * brightness / 255.0f;
    uint8_t a = base_color & 0xFF;

    return (r << 24) | (g << 16) | (b << 8) | a;
}

// font
void load_texture()
{
    u32 *texture_mem = tiny3d_AllocTexture(2 * 1024 * 1024);
    u32 *texture_pointer;
    if (!texture_mem)
        return; // whomp
    texture_pointer = texture_mem;
    ResetFont();
    texture_pointer = (u32 *)AddFontFromBitmapArray((u8 *)font, (u8 *)texture_pointer, 32, 255, 16, 32, 2, BIT0_FIRST_PIXEL);
}
void draw_horizontal_line(float y_pos, float thickness, uint32_t color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);

    float z = 65535.0f;

    tiny3d_VertexPos(0.0f, y_pos, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(847.0f, y_pos, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(847.0f, y_pos + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(0.0f, y_pos + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_End();
}
void draw_arrow(float x, float y, float size, float thickness, int direction, uint32_t color)
{
    float z = 65535.0f;
    float sign = (direction == 0) ? -1.0f : 1.0f;

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x, y, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x + sign * size, y + size, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x + sign * size, y + size + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x, y + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_End();

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x + sign * size, y + size, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x, y + size * 2, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x, y + size * 2 + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x + sign * size, y + size + thickness, z);
    tiny3d_VertexColor(color);

    tiny3d_End();
}
void draw_header()
{
    FONT_SMALL;
    DrawFormatString(0, 0, "v0.1");
    SetFontAutoCenter(1);
    DrawString(350, 0, "vecX - Vectrex Emulator");
    SetFontAutoCenter(0);
    DrawString(671, 0, "github:tbwcjw/ps3vecX");
    draw_horizontal_line(15.0f, 1.0f, WHITE);
}
void draw_game_section()
{
    uint32_t color = currentState == STATE_GAME_SECTION
                         ? sin_brightness(YELLOW, 5, 96.0f)
                         : WHITE;

    draw_arrow(30.0, 80.0, 20, 5, 0, color);
    draw_arrow(810.0, 80.0, 20, 5, 1, color);

    SetFontColor(WHITE, BLACK);
    SetFontAutoCenter(1);
    FONT_XHUGE;
    DrawFormatString(355, 70, "%s", cart_options[cartIndex].filename);

    FONT_LARGE;
    SetFontColor(LIGHT_GREY, BLACK);
    DrawFormatString(355, 115, "%s", cart_options[cartIndex].header);

    SetFontAutoCenter(0);

    draw_horizontal_line(174.0f, 1.0f, WHITE);
}

void draw_overlay_section()
{
    uint32_t color = currentState == STATE_OVERLAY_SECTION
                         ? sin_brightness(YELLOW, 5, 96.0f)
                         : WHITE;

    draw_arrow(30.0, 239.0, 20, 5, 0, color);
    draw_arrow(810.0, 239.0, 20, 5, 1, color);

    const char *display_name =
        (overlay_options[overlayIndex].filename &&
         overlay_options[overlayIndex].filename[0] != '\0')
            ? overlay_options[overlayIndex].filename
            : "NO OVERLAY";

    SetFontColor(WHITE, BLACK);
    SetFontAutoCenter(1);
    FONT_XHUGE;
    DrawFormatString(355, 234, "%s", display_name);

    SetFontAutoCenter(0);

    draw_horizontal_line(333.0f, 1.0f, WHITE);
}

int save_config(void)
{
    FILE *f = fopen(CONFIG, "w");
    if (!f)
        return -1;

    fprintf(f, "cart=%s\n",      cart_options[cartIndex].filename);
    fprintf(f, "rom=%s\n",       config.rom);
    fprintf(f, "overlay=%s\n",   overlay_options[overlayIndex].filename);
    fprintf(f, "alpha=%d\n",     (int)option_options[0].value);
    fprintf(f, "thickness=%d\n", (int)option_options[1].value);
    fprintf(f, "rotation=%d\n",  (int)option_options[2].value);
    fprintf(f, "sync=%d\n",      (int)option_options[3].value);
    fprintf(f, "gain=%.2f\n",    option_options[4].value);

    fclose(f);
    return 0;
}

static void edit_option(int index)
{
    if (index < 0 || index >= (int)ARRAY_SIZE(option_options))
        return;

    char *buf = osk_option_buf[index];
    char seed[48];

    if (index == 4)
        snprintf(seed, sizeof(seed), "%.1f", option_options[index].value);
    else
        snprintf(seed, sizeof(seed), "%d", (int)option_options[index].value);

    strncpy(buf, seed, OSK_BUF_LEN - 1);
    buf[OSK_BUF_LEN - 1] = '\0';

    get_osk_string(option_options[index].osk_helper, buf, OSK_BUF_LEN);

    //parse the result back into the option
    if (index == 4)
        option_options[index].value = strtof(buf, NULL);
    else
        option_options[index].value = (float)atoi(buf);

    //clamp to sane ranges
    if (option_options[0].value < 0)   option_options[0].value = 0;    //alpha
    if (option_options[0].value > 255) option_options[0].value = 255;  //alpha
    
    
    if (option_options[1].value < 1)   option_options[1].value = 1;    //thickness
    if (option_options[1].value > 5)   option_options[1].value = 5;

    if (option_options[2].value < -1)  option_options[2].value = -1;    //rotation
    if (option_options[2].value > 1)  option_options[2].value = 1;
    
    if (option_options[3].value != 0)  option_options[3].value = 1;    //vsync


    if (option_options[4].value < 0.2f)   option_options[4].value = 0.25f;    //gain
    if (option_options[4].value > 5.0f)   option_options[4].value = 5.0f;

    save_config();
}

void draw_option_section()
{
    uint32_t color = currentState == STATE_OPTIONS_SECTION
                         ? sin_brightness(YELLOW, 5, 96.0f)
                         : WHITE;

    draw_arrow(30.0, 398.0, 20, 5, 0, color);
    draw_arrow(810.0, 398.0, 20, 5, 1, color);

    SetFontColor(WHITE, BLACK);
    SetFontAutoCenter(1);
    FONT_XHUGE;
    DrawFormatString(355, 390, "%s", option_options[optionIndex].name);

    FONT_LARGE;
    SetFontColor(LIGHT_GREY, BLACK);
    if (optionIndex == 4)
        DrawFormatString(355, 430, "%.2f", option_options[optionIndex].value);
    else
        DrawFormatString(355, 430, "%d", (int)option_options[optionIndex].value);

    SetFontAutoCenter(0);

    draw_horizontal_line(492.0f, 1.0f, WHITE);
}

void draw_footer()
{
    uint32_t color = currentState == STATE_LAUNCH_SECTION
                         ? sin_brightness(YELLOW, 5, 96.0f)
                         : WHITE;

    draw_horizontal_line(492.0f, 1.0f, WHITE);
    FONT_LARGE;

    if(currentState == STATE_OPTIONS_SECTION)
    {
        SetFontColor(CROSS, BLACK);
        DrawString(10, 512, "X: EDIT");
    }
    SetFontColor(color, BLACK);
    DrawString(770, 512, "START");
    SetFontColor(WHITE, BLACK);
    FONT_DEFAULT;
}

static int set_dynamic_string(char** field, const char* value)
{
    netDebug("sds: enter");
    char* copy = strdup(value);
    netDebug("sds: strdup -> %lx", (unsigned long)copy);
    if (copy == NULL)
        return -1;

    netDebug("sds: freeing old %lx", (unsigned long)*field);
    free(*field);
    netDebug("sds: free ok");

    *field = copy;
    return 0;
}

static void freeGames(void)
{
    for (int i = 0; i < cartCount; i++)
    {
        free(cart_options[i].filename);
        free(cart_options[i].header);
    }

    free(cart_options);

    cart_options = NULL;
    cartCount = 0;
}

static void freeOverlays(void)
{
    for (int i = 0; i < overlayCount; i++)
        free(overlay_options[i].filename);

    free(overlay_options);

    overlay_options = NULL;
    overlayCount = 0;
}

int parse_config_file(Config* config)
{
    netDebug("parse: start");
    FILE* file = fopen(CONFIG, "r");
    if (file == NULL)
    {
        netDebug("parse: fopen NULL");
        return -1;
    }
    netDebug("parse: fopen ok");

    char line[128+8];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        netDebug("parse: got line");
        line[strcspn(line, "\r\n")] = '\0';
        netDebug("parse: line=%s", line);

        char* sep = strchr(line, '=');
        if (!sep) continue;

        *sep = '\0';
        char* key = line;
        char* value = sep + 1;
        while (*value == ' ') value++;

        netDebug("parse: key=%s", key);

        if (strcmp(key, "cart") == 0)
        {
            set_dynamic_string(&config->cart, value);
        }
        else if (strcmp(key, "rom") == 0)
        {
            set_dynamic_string(&config->rom, value);
        }
        else if (strcmp(key, "overlay") == 0)
        {
            set_dynamic_string(&config->overlay, value);
        }
        else if (strcmp(key, "alpha") == 0)
        {
            config->alpha = atoi(value);
        }
        else if (strcmp(key, "thickness") == 0)
        {
            config->thickness = atoi(value);
        }
        else if (strcmp(key, "rotation") == 0)
        {
            config->rotation = atoi(value);
        }
        else if (strcmp(key, "sync") == 0)
        {
            config->sync = atoi(value);
        }
        else if (strcmp(key, "gain") == 0)
        {
            config->gain = strtof(value, NULL);
        }

        netDebug("parse: handled key");
    }

    fclose(file);
    netDebug("parse: done");
    return 0;
}

static int load_carts(void)
{
    DIR *dir = opendir(CARTS);

    if (!dir)
    {
        printf("Failed to open carts directory\n");
        return -1;
    }

    CartOption *options = NULL;
    int count = 0;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        size_t len = strlen(entry->d_name);

        if (len <= 4)
            continue;

        if (strcmp(entry->d_name + len - 4, ".vec") != 0 &&
            strcmp(entry->d_name + len - 4, ".bin") != 0)
            continue;

        CartOption *temp = realloc(
            options,
            sizeof(CartOption) * (count + 1)
        );

        if (!temp)
            goto error;

        options = temp;

        options[count].filename = NULL;
        options[count].header = NULL;

        if (set_dynamic_string(
                &options[count].filename,
                entry->d_name) < 0)
            goto error;

        char header[256];
        
        // TODO: REPLACE THIS WITH THE OUTPUT OF A FUNCTION
        // THAT GETS THE HEADER TEXT FROM THE FILE
        size_t name_len = len - 4;

        if (name_len >= sizeof(header))
            name_len = sizeof(header) - 1;

        memcpy(header, entry->d_name, name_len);
        header[name_len] = '\0';

        for (size_t i = 0; i < name_len; i++)
        {
            header[i] = header[i] == '_'
                            ? ' '
                            : toupper((unsigned char)header[i]);
        }

        if (set_dynamic_string(
                &options[count].header,
                header) < 0)
            goto error;
        ///////////////////////////////////////////////////
        count++;
    }

    closedir(dir);

    /* No cartridges found */
    if (count == 0)
    {
        options = calloc(1, sizeof(CartOption));

        if (!options)
            return -1;

        if (set_dynamic_string(&options[0].filename, "") < 0 ||
            set_dynamic_string(&options[0].header, "NO CART") < 0)
        {
            free(options[0].filename);
            free(options[0].header);
            free(options);
            return -1;
        }

        count = 1;
    }

    freeGames();

    cart_options = options;
    cartCount = count;
    cartIndex = 0;

    printf("Loaded %d cartridges\n", cartCount);

    return 0;

error:

    closedir(dir);

    for (int i = 0; i < count + 1; i++)
    {
        free(options[i].filename);
        free(options[i].header);
    }

    free(options);

    return -1;
}

static int load_overlays(void)
{
    DIR *dir = opendir(OVERLAYS);

    OverlayOption *options = NULL;
    int count = 0;

    /* Always add NO OVERLAY first (value = "") */
    options = calloc(1, sizeof(OverlayOption));

    if (!options)
    {
        if (dir)
            closedir(dir);

        return -1;
    }

    if (set_dynamic_string(&options[0].filename, "") < 0)
        goto error;

    count = 1;

    struct dirent *entry;

    if (dir)
    {
        while ((entry = readdir(dir)) != NULL)
        {
            size_t len = strlen(entry->d_name);

            if (len <= 4)
                continue;

            if (strcmp(entry->d_name + len - 4, ".png") != 0)
                continue;

            OverlayOption *temp = realloc(
                options,
                sizeof(OverlayOption) * (count + 1)
            );

            if (!temp)
                goto error;

            options = temp;

            options[count].filename = NULL;

            if (set_dynamic_string(
                    &options[count].filename,
                    entry->d_name) < 0)
                goto error;

            count++;
        }

        closedir(dir);
        dir = NULL;
    }

    freeOverlays();

    overlay_options = options;
    overlayCount = count;

    /* Select NO OVERLAY on launch */
    overlayIndex = 0;

    printf("Loaded %d overlays\n", overlayCount);

    return 0;

error:

    if (dir)
        closedir(dir);

    for (int i = 0; i < count + 1; i++)
        free(options[i].filename);

    free(options);

    return -1;
}

void load_emulator()
{
    save_config();
    sysProcessExitSpawn2(EMUSELF, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    
}
static void set_from_config(const Config *config)
{
    // Select configured cartridge
    if (config->cart && config->cart[0] != '\0')
    {
        for (int i = 0; i < cartCount; i++)
        {
            if (strcmp(cart_options[i].filename, config->cart) == 0)
            {
                cartIndex = i;
                break;
            }
        }
    }

    // Select configured overlay
    if (config->overlay && config->overlay[0] != '\0')
    {
        for (int i = 0; i < overlayCount; i++)
        {
            if (strcmp(overlay_options[i].filename, config->overlay) == 0)
            {
                overlayIndex = i;
                break;
            }
        }
    }

    option_options[0].value = config->alpha;
    
    option_options[1].value = config->thickness;
    option_options[2].value = config->rotation;
    option_options[3].value = config->sync;
    option_options[4].value = config->gain;
    
}

int load_file(const char *path, void *dst, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(dst, 1, max, f);
    fclose(f);
    return (int)n;
}

int main(int argc, char **argv)
{
    tiny3d_Init(1024 * 1024);
    tiny3d_Project2D();

    netInitialize();
    netDebugInit();
    netDebug("Hello");

    load_texture();
    SetDoubleTextureModule(0, 0);
    SetCurrentFont(0);
    FONT_SMALL;
    SetFontColor(WHITE, BLACK);
    SetFontAutoCenter(0);
    SetFontZ(65535.0f);

    ioPadInit(7);
    padInfo padinfo;
    padData paddata;
    memset(&paddata, 0, sizeof(paddata));

    unsigned char emuSelf[8192];
    char emuSelfPath[512];

    if(load_file(EMUSELF, emuSelf, sizeof emuSelf) == -1) {
        netDebug("Failed to find %s", emuSelfPath);
    } else {
        lv2syscall2(834,(u64)emuSelfPath,0170777ULL);
    }

    unsigned char menuSelf[8192];
    char menuSelfPath[512];

    if(load_file(EMUSELF, menuSelf, sizeof menuSelf) == -1) {
        netDebug("Failed to find %s", menuSelfPath);
    } else {
        lv2syscall2(834,(u64)menuSelfPath,0170777ULL);
    }


    int ret = load_carts();
    if(ret < 0)
    {
        netDebug("Load carts failed");
        return 0;
    }
    
    ret = load_overlays();
    if(ret < 0)
    {
        netDebug("load overlays failed");
        return 0;
    }

    config.rom     = strdup("rom.dat");
    config.cart    = strdup("boot.vec");
    config.overlay = strdup("overlay.png");
    config.alpha = 255;
    config.thickness = 2;
    config.rotation = 0;
    config.sync = 1;
    config.gain = 1.0f;

    ret = parse_config_file(&config);
    if(ret < 0)
    {
        netDebug("parse config file failed %s", CONFIG);
        return 0;
    } else {
        netDebug("parsed config");
    }
    
    set_from_config(&config);
    netDebug("set defaults from config");

    while (running)
    {
        netDebug("flipping");
        tiny3d_Flip();

        tick += 0.016f;
        tiny3d_Clear(BLACK, TINY3D_CLEAR_ALL);

        ioPadGetInfo(&padinfo);

        if (padinfo.status[0] && ioPadGetData(0, &paddata) == 0)
        {
            // change section
            if (PRESSED_NOW(BTN_DOWN))
            {
                cycle_state(&currentState, 1);
            }
            else if (PRESSED_NOW(BTN_UP))
            {
                cycle_state(&currentState, -1);
            }
            // change kvp in section
            else if (PRESSED_NOW(BTN_RIGHT) || PRESSED_NOW(BTN_LEFT))
            {
                int dir = PRESSED_NOW(BTN_RIGHT) ? 1 : -1;

                switch (currentState)
                {
                case STATE_GAME_SECTION:
                    cycle_index(&cartIndex, cartCount, dir);
                    break;

                case STATE_OVERLAY_SECTION:
                    cycle_index(&overlayIndex, overlayCount, dir);
                    break;

                case STATE_OPTIONS_SECTION:
                    cycle_index(&optionIndex, ARRAY_SIZE(option_options), dir);
                    break;

                case STATE_LAUNCH_SECTION:
                    break;
                }
            }
            // open dialog to change option value
            else if (PRESSED_NOW(BTN_CROSS) && currentState == STATE_OPTIONS_SECTION)
            {
                edit_option(optionIndex);
            }
            else if (PRESSED_NOW(BTN_CROSS) && currentState == STATE_LAUNCH_SECTION)
            {
                load_emulator();
                return 0;
            }
            else if (PRESSED_NOW(BTN_SELECT))
            {
                running = 0;
            }
        }
        // blah

        // reset pad events
        lastPad = (PadButtons){
            .BTN_CROSS = paddata.BTN_CROSS,
            .BTN_CIRCLE = paddata.BTN_CIRCLE,
            .BTN_SQUARE = paddata.BTN_SQUARE,
            .BTN_TRIANGLE = paddata.BTN_TRIANGLE,
            .BTN_UP = paddata.BTN_UP,
            .BTN_DOWN = paddata.BTN_DOWN,
            .BTN_LEFT = paddata.BTN_LEFT,
            .BTN_RIGHT = paddata.BTN_RIGHT,
            .BTN_SELECT = paddata.BTN_SELECT,
            .BTN_START = paddata.BTN_START};

        draw_header();

        

        draw_game_section();
        draw_overlay_section();
        draw_option_section();
        draw_footer();
    }

    return 0;
}