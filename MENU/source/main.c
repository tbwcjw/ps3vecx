#include "common.h"

#include <ctype.h>
#include <dirent.h>

#include <sysutil/osk.h>
#include <tiny3d.h>
#include <libfont2.h>

#include "alert.h"
#include "osk.h"
#include "font.h"


SYS_PROCESS_PARAM(MENU_PRIO, MENU_STACKSIZE);

#define WHITE 0xFFFFFFFF
#define BLACK 0x00000000
#define LIGHT_GREY 0x878787FF
#define DARK_GREY 0x5A5A5AFF
#define CIRCLE 0xFF6666FF
#define CROSS 0x7CB2E8FF
#define YELLOW 0xFFFF00FF

#define FONT_DEFAULT FONT_SMALL
#define FONT_SMALL SetFontSize(8, 13)
#define FONT_LARGE SetFontSize(12, 20)
#define FONT_XHUGE SetFontSize(20, 34)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum
{
    STATE_GAME_SECTION,
    STATE_OVERLAY_SECTION,
    STATE_OPTIONS_SECTION,
    STATE_LAUNCH_SECTION,
    STATE_ERROR_DIALOG
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

static int launchIndex = 3;

#define OSK_BUF_LEN 8
static char osk_option_buf[ARRAY_SIZE(option_options)][OSK_BUF_LEN];


Config config = {0};

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
    int BTN_R1;
    int BTN_L1;
} PadButtons;

static PadButtons lastPad = {0};
#define PRESSED_NOW(btn) (paddata.btn && !lastPad.btn)

static volatile int error_dialog_alert = 0; 
static volatile int callback_exit = 0;
static volatile int running = 1;
static float tick = 0.0f;

static volatile int emu_exit_code = 0;

char *emu_exit_code_string()
{
    switch (emu_exit_code)
    {
        case EMU_EXIT_SUCCESS:  return "Success";
        case EMU_CONFIG_ERROR:  return "Configuration file failed to parse";
        case EMU_ROM_NOT_FOUND: return "Vectrex ROM (BIOS) was not found";
        case EMU_AUDIO_INIT:    return "Failed to initialize audio thread";
        default:                return "An unknown error occurred";
    }
}

static volatile int menu_error_code = 0;
typedef enum
{
    MENU_SUCCESS = 0,
    MENU_CONFIG_PARSE_ERROR = -1,
    MENU_CONFIG_NOT_FOUND = -2,
    MENU_CART_READ_ERROR = -3,
    MENU_CONFIG_CREATE_ERROR = -4,
    MENU_OVERLAY_READ_ERROR = -5,
    MENU_EMU_BINARY_NOT_FOUND = -6,
    MENU_MENU_BINARY_NOT_FOUND = -7,
    MENU_ABOUT_CART_NOT_FOUND = -8,
    MENU_CONTROLS_CART_NOT_FOUND = -9
} MenuErrorCode;

char *menu_error_code_string()
{
    switch (menu_error_code)
    {
        case MENU_SUCCESS: return                   "Success";
        case MENU_CONFIG_PARSE_ERROR:  return       "Configuration file failed to parse";
        case MENU_CONFIG_NOT_FOUND: return          "Config file not found, creating it";
        case MENU_CONFIG_CREATE_ERROR: return       "Failed to create a config file";
        case MENU_CART_READ_ERROR: return           "Failed to read one or more carts";
        case MENU_OVERLAY_READ_ERROR: return        "Failed to read one or more overlays";
        case MENU_EMU_BINARY_NOT_FOUND: return      "Failed to find the VECXEMU.SELF";
        case MENU_MENU_BINARY_NOT_FOUND: return     "Failed to find the VECXMENU.SELF";
        case MENU_ABOUT_CART_NOT_FOUND: return      "Failed to find about.vec";
        case MENU_CONTROLS_CART_NOT_FOUND: return   "Failed to find controls.vec";
        default: return                             "An unknown error occurred";
    }
}

typedef enum
{
    ERROR_TYPE_EMULATOR,
    ERROR_TYPE_MENU
} ErrorType;

static ErrorType current_error_type;
static volatile bool error_recoverable = 1;

void throw_error(ErrorType type, int code, bool recoverable)
{
    current_error_type = type;
    error_dialog_alert = 0;

    if (type == ERROR_TYPE_EMULATOR)
        emu_exit_code = code;
    else if (type == ERROR_TYPE_MENU)
        menu_error_code = code;
    else
        return;

    currentState = STATE_ERROR_DIALOG;

    if(!recoverable)
        error_recoverable = 0;
}

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
        // down
        if (*state == STATE_LAUNCH_SECTION)
            *state = STATE_GAME_SECTION;
        else
            *state = (State)(*state + 1);
    }
    else
    {
        // up
        if (*state == STATE_GAME_SECTION)
            *state = STATE_LAUNCH_SECTION;
        else
            *state = (State)(*state - 1);
    }

    if (*state != STATE_LAUNCH_SECTION)
        launchIndex = 4;
}

// effects
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
    float z = 65000.0f;
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


void draw_scaled_string(int x, int y, int startW, int startH, int minW, int minH, int maxWidth, const char *format, ...)
{
    char text[256];

    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    int w = startW;
    int h = startH;
    SetFontSize(w, h);

    int textWidth = strlen(text) * GetFontWidth();

    if (textWidth > maxWidth)
    {
        float scale = (float)maxWidth / textWidth;

        w = (int)(startW * scale);
        h = (int)(startH * scale);

        if (w < minW)
        {
            w = minW;
            h = minH;
        }

        SetFontSize(w, h);
    }

    int dY = y - (h / 2);

    SetFontAutoCenter(1);
    DrawFormatString(x, dY, "%s", text);
    SetFontAutoCenter(0);
}

void draw_header()
{
    FONT_SMALL;
    DrawFormatString(0, 0, "v%s", VERSION);
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
    draw_scaled_string(355, 90, 20, 34, 8, 13, 720, "%s", cart_options[cartIndex].filename);

    
    SetFontColor(LIGHT_GREY, BLACK);
    draw_scaled_string(355, 120, 12, 20, 8, 13, 720, "%s", cart_options[cartIndex].header);

    draw_horizontal_line(174.0f, 2.0f, WHITE);
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
            : "no overlay";

    SetFontColor(WHITE, BLACK);
    draw_scaled_string(355, 255, 20, 34, 8, 13, 720, "%s", display_name);

    draw_horizontal_line(333.0f, 1.0f, WHITE);
}
void draw_rect(float x, float y, float w,  float h, u32 color, float z) {
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x + w, y, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x + w, y + h, z);
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(x, y + h, z);
    tiny3d_VertexColor(color);

    tiny3d_End();
}

void draw_error_dialog()
{
    error_dialog_alert = 1;

    SetFontAutoCenter(1);
    FONT_LARGE;

    draw_rect(207.0f, 192.0f, 434.0f, 124.0f, WHITE, 65000.0f);
    draw_rect(209.0f, 194.0f, 430.0f, 120.0f, DARK_GREY, 64900.0f);

    SetFontZ(64000.0f);
    DrawString(225.0f, 200.0f, "An error occurred");

    SetFontColor(WHITE, BLACK);

    if (current_error_type == ERROR_TYPE_EMULATOR)
        DrawString(225.0f, 240.0f, emu_exit_code_string());
    else
        DrawString(225.0f, 240.0f, menu_error_code_string());

    if(error_recoverable)
    {
        SetFontColor(CROSS, BLACK);
        DrawString(249.0f, 285.0f, "Press X to continue");
    }
    else
    {
        SetFontColor(CIRCLE, BLACK);
        DrawString(249.0f, 285.0f, "Press X to exit...");
    }
    

    SetFontColor(WHITE, BLACK);
    SetFontZ(65535.0f);
    FONT_DEFAULT;
    SetFontAutoCenter(0);
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
        DrawFormatString(355, 430, "%.3f", option_options[optionIndex].value);
    else
        DrawFormatString(355, 430, "%d", (int)option_options[optionIndex].value);

    SetFontAutoCenter(0);

    draw_horizontal_line(492.0f, 1.0f, WHITE);
}

void draw_footer()
{
    draw_horizontal_line(492.0f, 1.0f, WHITE);
    FONT_LARGE;

    if (currentState == STATE_OPTIONS_SECTION)
    {
        SetFontColor(CROSS, BLACK);
        DrawString(10, 512, "X: EDIT");
    }

    if (currentState == STATE_LAUNCH_SECTION)
    {
        const char *items[] = {
            "EXIT",
            "ABOUT",
            "CONTROLS",
            "REFRESH",
            "START"
        };

        int positions[] = {380, 455, 540, 660, 770};

        for (int i = 0; i < 5; i++)
        {
            uint32_t color = (launchIndex == i)
                ? sin_brightness(YELLOW, 5, 96.0f)
                : WHITE;

            SetFontColor(color, BLACK);
            DrawString(positions[i], 512, (char *)items[i]);
        }
    }
    else
    {
        SetFontColor(WHITE, BLACK);
        DrawString(770, 512, "START");
    }

    SetFontColor(WHITE, BLACK);
    FONT_DEFAULT;
}

static void edit_option(int index)
{
    if (index < 0 || index >= (int)ARRAY_SIZE(option_options))
        return;

    char *buf = osk_option_buf[index];
    char seed[48];

    if (index == 4)
        snprintf(seed, sizeof(seed), "%.3f", option_options[index].value);
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
    if (option_options[1].value > MAX_THICKNESS)   option_options[1].value = MAX_THICKNESS;

    if (option_options[2].value < -1)  option_options[2].value = -1;    //rotation
    if (option_options[2].value > 1)  option_options[2].value = 1;
    
    if (option_options[3].value != 0)  option_options[3].value = 1;    //vsync


    if (option_options[4].value < 0.2f)   option_options[4].value = 0.25f;    //gain
    if (option_options[4].value > 5.0f)   option_options[4].value = 5.0f;

    save_config();
}


static void free_carts(void)
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

static void free_overlays(void)
{
    for (int i = 0; i < overlayCount; i++)
        free(overlay_options[i].filename);

    free(overlay_options);

    overlay_options = NULL;
    overlayCount = 0;
}

int parse_config_file(Config* config)
{
    FILE* file = fopen(CONFIG, "r");
    if (file == NULL)
    {
        throw_error(ERROR_TYPE_MENU, MENU_CONFIG_NOT_FOUND, true);
        file = fopen(CONFIG, "w"); //create file
        if (file == NULL) {
            throw_error(ERROR_TYPE_MENU, MENU_CONFIG_CREATE_ERROR, false);
        }

        fprintf(file, "rom=rom.dat\n");
        fprintf(file, "cart=about.vec\n");
        fprintf(file, "overlay=\n");
        fprintf(file, "alpha=128\n");
        fprintf(file, "thickness=2\n"); 
        fprintf(file, "rotation=0\n");
        fprintf(file, "sync=1\n");
        fprintf(file, "gain=1.0\n");
        fclose(file);
    }

    char line[128+8];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';

        char* sep = strchr(line, '=');
        if (!sep) continue;

        *sep = '\0';
        char* key = line;
        char* value = sep + 1;
        while (*value == ' ') value++;


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

    }

    fclose(file);
    return 0;
}


static int read_cart_header(const char *path, char *out, size_t out_size)
{
    unsigned char buf[256];
    size_t n;
    size_t used = 0;
    int strings_seen = 0;
    FILE *f;

    if (out_size == 0)
        return -1;

    out[0] = '\0';

    f = fopen(path, "rb");
    if (!f)
        return -1;

    n = fread(buf, 1, sizeof buf, f);
    fclose(f);

    for (size_t i = 0; i < n; i++)
    {
        if (buf[i] != 0x80)
            continue;

        size_t s = i;
        while (s > 0 && isprint(buf[s - 1]))
            s--;

        if (i - s < 2)
            continue;

        strings_seen++;

        if (strings_seen == 1 && buf[s] == 'g' && buf[s + 1] == ' ')
            continue;

        if (used > 0 && used + 1 < out_size)
            out[used++] = ' ';

        for (size_t j = s; j < i && used + 1 < out_size; j++)
            out[used++] = (char)buf[j];

        if(used > 3)
            break;

    }
    out[used] = '\0';


    if (used > 32)
    {
        used = 32 - 3;
        memcpy(out + used, "...", 3);
        used += 3;
        out[used] = '\0';
    }


    return used > 0 ? 0 : -1;
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
        count++;

        CartOption *cart = &options[count - 1];

        if (set_dynamic_string(&cart->filename, entry->d_name) < 0)
            goto error;

        char fullpath[1064];
        char header[256];

        snprintf(fullpath, sizeof fullpath, "%s/%s", CARTS, entry->d_name);

        if (read_cart_header(fullpath, header, sizeof header) < 0)
        {
            //fallback, read the filename again
            size_t name_len = len - 4;

            if (name_len >= sizeof header)
                name_len = sizeof header - 1;

            for (size_t i = 0; i < name_len; i++)
                header[i] = entry->d_name[i] == '_' ? ' ' : toupper((unsigned char)entry->d_name[i]);

            header[name_len] = '\0';
        }

        if (set_dynamic_string(&cart->header, header) < 0)
            goto error;
    }

    closedir(dir);

    //no cartridges
    if (count == 0)
    {
        options = calloc(1, sizeof(CartOption));

        if (!options)
            return -1;

        set_dynamic_string(&options[0].filename, "no cartridge");
        set_dynamic_string(&options[0].header, "");
        count = 1;
    }

    free_carts();

    cart_options = options;
    cartCount = count;
    cartIndex = 0;

    netDebug("Loaded %d cartridges\n", cartCount);

    return 0;

error:

    closedir(dir);

    for (int i = 0; i < count; i++)
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

    //no overlay first
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

            OverlayOption *temp = realloc(options, sizeof(OverlayOption) * (count + 1));

            if (!temp)
                goto error;

            options = temp;

            options[count].filename = NULL;

            if (set_dynamic_string(&options[count].filename, entry->d_name) < 0)
                goto error;

            count++;
        }

        closedir(dir);
        dir = NULL;
    }

    free_overlays();

    overlay_options = options;
    overlayCount = count;

    overlayIndex = 0;

    netDebug("Loaded %d overlays", overlayCount);

    return 0;

error:

    if (dir)
        closedir(dir);

    for (int i = 0; i < count + 1; i++)
        free(options[i].filename);

    free(options);

    return -1;
}

static void set_from_config(const Config *config)
{
    // select configured cartridge
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

    // select configured overlay
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

//exit routines
void load_emulator()
{
    save_config();
    sysProcessExitSpawn2(EMUSELF, NULL, NULL, NULL, 0, EMU_PRIO, EMU_STACKSIZE); 
}

void reload_menu()
{
    save_config();
    sysProcessExitSpawn2(MENUSELF, NULL, NULL, NULL, 0, MENU_PRIO, MENU_STACKSIZE); 
}

void finalize()
{
    save_config();

    if(running)
        running = 0;

    if(!callback_exit)
    {
#ifdef PS3LOADX
    sysProcessExitSpawn2("/dev_hdd0/game/PSL145310/RELOAD.SELF", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
#endif
    }
    else
    {
        sysProcessExit(0);
    }

    
}

//unused
int sys_ring_buzzer(int beeps) {
    if (beeps < 1) beeps = 1;
    if (beeps > 3) beeps = 3;

    static const uint64_t args[3][3] = {
        {0x1004, 0x4, 0x6},     //single beep
        {0x1004, 0x7, 0x36},    //two beeps
        {0x1004, 0xa, 0x1b6}    //three beeps
    };

    lv2syscall3(392, args[beeps - 1][0], args[beeps - 1][1], args[beeps - 1][2]);
    return 0;
}

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; 
    (void)usrdata;

    if (status == SYSUTIL_EXIT_GAME)
    {
        callback_exit = 1;
        finalize();
    }
}

int cart_index(char* filename)
{
    for (int i = 0; i < cartCount; i++)
    {
        if (strcmp(cart_options[i].filename, filename) == 0)
        {
            return i;
        }
    }
    return -1;
}

int padAnyPressed(padInfo *info, padData *data)
{
    static u16 prevHeld[MAX_PORT_NUM];
    int any = 0;

    ioPadGetInfo(info);

    for (u32 port = 0; port < MAX_PORT_NUM; port++)
    {
        if (!info->status[port] || ioPadGetData(port, data) != 0) {
            prevHeld[port] = 0;
            continue;
        }

        u16 held = (data->button[PAD_BUTTON_OFFSET_DIGITAL1] & 0xFF) |
                   ((data->button[PAD_BUTTON_OFFSET_DIGITAL2] & 0xFF) << 8);

        if (held & ~prevHeld[port])
            any = 1;

        prevHeld[port] = held;
    }

    return any;
}

int main(int argc, char **argv)
{
    sysUtilRegisterCallback(0, sysutil_cb, NULL);

    tiny3d_Init(1024 * 1024);
    tiny3d_Project2D();
    
    netInitialize();
    netDebugInit();
    netDebug("MENU, %#x, %#x", MENU_PRIO, MENU_STACKSIZE);

    load_texture();
    SetCurrentFont(0);
    FONT_SMALL;
    SetFontColor(WHITE, BLACK);
    SetFontAutoCenter(0);
    SetFontZ(65535.0f);

    ioPadInit(7);
    
    padInfo padinfo;
    padData paddata;
    memset(&paddata, 0, sizeof(paddata));


    if (argc > 1 && argv[1] != NULL)
    {
        emu_exit_code = atoi(argv[1]);

        if (emu_exit_code != EMU_EXIT_SUCCESS)
            throw_error(ERROR_TYPE_EMULATOR, emu_exit_code, true);
    }


    unsigned char emuSelf[8192];
    char emuSelfPath[512];

    if(load_file(EMUSELF, emuSelf, sizeof emuSelf) == -1) {
        throw_error(ERROR_TYPE_MENU, MENU_EMU_BINARY_NOT_FOUND, false);
    } else {
        lv2syscall2(834,(u64)emuSelfPath,0170777ULL);
        netDebug("chmod emu self");
    }

    unsigned char menuSelf[8192];
    char menuSelfPath[512];

    if(load_file(EMUSELF, menuSelf, sizeof menuSelf) == -1) {
        throw_error(ERROR_TYPE_MENU, MENU_MENU_BINARY_NOT_FOUND, false);
    } else {
        lv2syscall2(834,(u64)menuSelfPath,0170777ULL);
        netDebug("chmod menu self");
    }


    int ret = load_carts();
    if(ret < 0)
    {
        throw_error(ERROR_TYPE_MENU, MENU_CART_READ_ERROR, true);
    }
    
    ret = load_overlays();
    if(ret < 0)
    {
        throw_error(ERROR_TYPE_MENU, MENU_OVERLAY_READ_ERROR, true);
        return 0;
    }

    //set defaults
    config.rom          = strdup("rom.dat");
    config.cart         = strdup("about.vec");
    config.overlay      = strdup("");
    config.alpha = 255;
    config.thickness = 2;
    config.rotation = 0;
    config.sync = 1;
    config.gain = 1.0f;

    //parse config file, if not found it will be created
    ret = parse_config_file(&config);
    if(ret < 0)
        throw_error(ERROR_TYPE_MENU, MENU_CONFIG_PARSE_ERROR, true);
    
    //set our config data
    set_from_config(&config);

    while (running)
    {
        ioPadGetInfo(&padinfo);
        
        if(padAnyPressed(&padinfo, &paddata))
            play_alert(SOUND_OK);

        if (padinfo.status[0] && ioPadGetData(0, &paddata) == 0)
        {
            if (currentState == STATE_ERROR_DIALOG)
            {
                if (PRESSED_NOW(BTN_CROSS) || PRESSED_NOW(BTN_CIRCLE))
                {
                    if(!error_recoverable)
                        finalize();

                    emu_exit_code = 0;
                    currentState = STATE_GAME_SECTION;
                }
            }
            else
            {
                if (PRESSED_NOW(BTN_DOWN))
                {
                    cycle_state(&currentState, 1);
                }
                else if (PRESSED_NOW(BTN_UP))
                {
                    cycle_state(&currentState, -1);
                }
                // change kvp in section
                else if (PRESSED_NOW(BTN_RIGHT) || PRESSED_NOW(BTN_LEFT) || PRESSED_NOW(BTN_R1) || PRESSED_NOW(BTN_L1))
                {
                    int dir = (PRESSED_NOW(BTN_RIGHT) || PRESSED_NOW(BTN_R1)) ? 1 : -1;

                    if(currentState == STATE_GAME_SECTION)
                        cycle_index(&cartIndex, cartCount, dir);
                    else if(currentState == STATE_OVERLAY_SECTION)
                        cycle_index(&overlayIndex, overlayCount, dir);
                    else if(currentState == STATE_OPTIONS_SECTION)
                        cycle_index(&optionIndex, ARRAY_SIZE(option_options), dir);
                    else if (currentState == STATE_LAUNCH_SECTION)
                        cycle_index(&launchIndex, 5, dir);
                }
                // open dialog to change option value
                else if (PRESSED_NOW(BTN_CROSS) && currentState == STATE_OPTIONS_SECTION)
                {
                    edit_option(optionIndex);
                }
                // launch emulator
                else if (PRESSED_NOW(BTN_CROSS) && currentState == STATE_LAUNCH_SECTION)
                {
                    if (launchIndex == 0) finalize();               //exit
                    else if (launchIndex == 1)                      //about
                    {
                        int ret = cart_index("about.vec");

                        if(ret > 0)
                        {
                            cartIndex = ret;
                            overlayIndex = 0;
                            save_config();
                            load_emulator();
                        } else
                        {
                            throw_error(ERROR_TYPE_MENU, MENU_ABOUT_CART_NOT_FOUND, true);
                        }
                    }
                    else if (launchIndex == 2)                      //controls
                    {
                        int ret = cart_index("controls.vec");

                        if(ret > 0)
                        {
                            cartIndex = ret;
                            overlayIndex = 0;
                            save_config();
                            load_emulator();
                        } else
                        {
                            throw_error(ERROR_TYPE_MENU, MENU_CONTROLS_CART_NOT_FOUND, true);
                        }
                    }
                    else if (launchIndex == 3) reload_menu();       //reload
                    else if (launchIndex == 4) load_emulator();     //start
                }
            }
        }


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
            .BTN_START = paddata.BTN_START,
            .BTN_R1 = paddata.BTN_R1,
            .BTN_L1 = paddata.BTN_L1};
        
        tick += 0.016f;
        tiny3d_Clear(BLACK, TINY3D_CLEAR_ALL);
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

        if(currentState == STATE_ERROR_DIALOG)
        {
            if(!error_dialog_alert) 
                play_alert(SOUND_ERROR);

            draw_error_dialog();
        }
        

        draw_header();
        draw_game_section();
        draw_overlay_section();
        draw_option_section();
        draw_footer();

        tiny3d_Flip();
    }

    sysProcessExitSpawn2("/dev_hdd0/game/PSL145310/RELOAD.SELF", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
    return 0;
}