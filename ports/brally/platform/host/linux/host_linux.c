/* host_linux.c: the Linux host -- a window, the keyboard, mouse and game
 * controllers, sound out and decoded music, over SDL 3 (linked statically:
 * tools/toolchains/linux/setup.sh; X11, Wayland, PipeWire, PulseAudio and
 * ALSA are each loaded at run time when the machine has them). The
 * counterpart of host/macos and host/windows; host/posix under it supplies
 * time, threads, files, network and processes.
 *
 * The game keeps its own loop on the main thread, so this host never runs one
 * of its own: host_poll_event pumps SDL's queue. Frames arrive as ARGB pixels
 * through host_present (the software renderer), or are drawn by the Vulkan
 * renderer on a surface SDL makes for the window (host_linux_vk_surface), or
 * by the OpenGL one in a context SDL makes on it (host_linux_gl_open). Which
 * of the two the window is made for is the build's: -DHOST_LINUX_VULKAN or
 * -DHOST_LINUX_GL (link64.sh, from RENDER).
 *
 *   keys      SDL's scan codes (USB usages, the key's place, whatever the
 *             layout) to the DirectInput code and Win32 virtual key
 *   sound     SDL's audio stream: PipeWire, PulseAudio or ALSA, 32-bit float
 *   music     libFLAC (static), resampled to the mixer's rate by SDL
 *   pads      every game controller SDL knows, the Steam Deck's own among
 *             them, in the order they were connected (host/sdl/sdl_pads.c,
 *             as host/windows has them)
 *   Alt+Enter: full screen (as host/windows; F11 is Boss Rally's, a CD track key)
 *
 * Directories (environment, else the defaults):
 *   BR_CDROOT    the CD's files     (default reference/brally/data/disc, then disc/ beside the executable)
 *   BR_GAMEDIR   the install        (default: the CD root)
 *   BR_SAVEDIR   saves and settings (default $XDG_DATA_HOME/Boss Rally 64, else ~/.local/share/...)
 *   BR_MUSICDIR  the CD's tracks    (default music/cd beside the executable)
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <FLAC/stream_decoder.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "host.h"
#include "sdl_pads.h"

static char s_cd[1024], s_game[1024], s_save[1024], s_music[1024], s_here[1024];
static const char *s_app_dir = "Boss Rally 64", *s_app_title = "Boss Rally";

void host_set_app_name(const char *dir, const char *title)
{
    if (dir)
        s_app_dir = dir;
    if (title)
        s_app_title = title;
}

/* ---- process -------------------------------------------------------------------- */
static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* every folder of p made, as mkdir -p */
static void mkdirs(const char *p)
{
    char b[1024];
    char *s;
    snprintf(b, sizeof b, "%s", p);
    for (s = b + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(b, 0755);
            *s = '/';
        }
    mkdir(b, 0755);
}

static void exe_dir(char *out, size_t n)
{
    ssize_t k = readlink("/proc/self/exe", out, n - 1);
    char *s;
    if (k <= 0) {
        snprintf(out, n, ".");
        return;
    }
    out[k] = 0;
    if ((s = strrchr(out, '/')) != NULL)
        *s = 0;
}

void host_init(int argc, char **argv)
{
    const char *e;
    char id[128];
    size_t i;
    (void)argc;
    (void)argv;
    exe_dir(s_here, sizeof s_here);
    e = getenv("BR_CDROOT");
    if (e)
        snprintf(s_cd, sizeof s_cd, "%s", e);
    else if (is_dir("reference/brally/data/disc"))
        snprintf(s_cd, sizeof s_cd, "reference/brally/data/disc");
    else
        snprintf(s_cd, sizeof s_cd, "%s/disc", s_here);
    e = getenv("BR_GAMEDIR");
    snprintf(s_game, sizeof s_game, "%s", e ? e : s_cd);
    e = getenv("BR_SAVEDIR");
    if (e)
        snprintf(s_save, sizeof s_save, "%s", e);
    else if ((e = getenv("XDG_DATA_HOME")) && e[0] == '/')
        snprintf(s_save, sizeof s_save, "%s/%s", e, s_app_dir);
    else if ((e = getenv("HOME")) && e[0])
        snprintf(s_save, sizeof s_save, "%s/.local/share/%s", e, s_app_dir);
    else
        snprintf(s_save, sizeof s_save, "%s/save", s_here);
    mkdirs(s_save);
    e = getenv("BR_MUSICDIR");
    if (e)
        snprintf(s_music, sizeof s_music, "%s", e);
    else
        snprintf(s_music, sizeof s_music, "%s/music/cd", s_here);

    /* the app's name and id (the window's class, which a .desktop file's
       StartupWMClass names): "boss-rally-64" */
    for (i = 0; s_app_dir[i] && i < sizeof id - 1; i++)
        id[i] = s_app_dir[i] == ' ' ? '-' : (char)SDL_tolower((unsigned char)s_app_dir[i]);
    id[i] = 0;
    SDL_SetAppMetadata(s_app_title, NULL, id);
    /* text typed is the game's (its name entry); never Steam's on-screen keyboard */
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    if (!SDL_Init(SDL_INIT_VIDEO))
        fprintf(stderr, "host: SDL: %s\n", SDL_GetError());
    sdl_pads_init(0);
}

const char *host_music_dir(void) { return is_dir(s_music) ? s_music : NULL; }

const char *host_resource_dir(void)
{
    const char *e = getenv("BR_RESDIR");
    return e ? e : s_here;
}

void host_shutdown(void)
{
    host_audio_close();
    host_window_close();
    sdl_pads_quit();
    SDL_Quit();
}

const char *host_game_dir(void) { return s_game; }
const char *host_cd_dir(void)   { return s_cd; }
const char *host_save_dir(void) { return s_save; }

/* ---- keys: SDL scan code -> DirectInput scan code and Win32 virtual key ------------ */
static const struct { uint16_t sc; uint8_t dik; uint8_t vk; } k_keys[] = {
    { SDL_SCANCODE_ESCAPE, 0x01, 0x1B }, { SDL_SCANCODE_RETURN, 0x1C, 0x0D }, { SDL_SCANCODE_KP_ENTER, 0x9C, 0x0D },
    { SDL_SCANCODE_SPACE, 0x39, 0x20 }, { SDL_SCANCODE_TAB, 0x0F, 0x09 }, { SDL_SCANCODE_BACKSPACE, 0x0E, 0x08 },
    { SDL_SCANCODE_LEFT, 0xCB, 0x25 }, { SDL_SCANCODE_RIGHT, 0xCD, 0x27 }, { SDL_SCANCODE_UP, 0xC8, 0x26 },
    { SDL_SCANCODE_DOWN, 0xD0, 0x28 },
    { SDL_SCANCODE_LSHIFT, 0x2A, 0x10 }, { SDL_SCANCODE_RSHIFT, 0x36, 0x10 }, { SDL_SCANCODE_LCTRL, 0x1D, 0x11 },
    { SDL_SCANCODE_RCTRL, 0x9D, 0x11 }, { SDL_SCANCODE_LALT, 0x38, 0x12 }, { SDL_SCANCODE_RALT, 0xB8, 0x12 },
    { SDL_SCANCODE_A, 0x1E, 'A' }, { SDL_SCANCODE_B, 0x30, 'B' }, { SDL_SCANCODE_C, 0x2E, 'C' }, { SDL_SCANCODE_D, 0x20, 'D' },
    { SDL_SCANCODE_E, 0x12, 'E' }, { SDL_SCANCODE_F, 0x21, 'F' }, { SDL_SCANCODE_G, 0x22, 'G' }, { SDL_SCANCODE_H, 0x23, 'H' },
    { SDL_SCANCODE_I, 0x17, 'I' }, { SDL_SCANCODE_J, 0x24, 'J' }, { SDL_SCANCODE_K, 0x25, 'K' }, { SDL_SCANCODE_L, 0x26, 'L' },
    { SDL_SCANCODE_M, 0x32, 'M' }, { SDL_SCANCODE_N, 0x31, 'N' }, { SDL_SCANCODE_O, 0x18, 'O' }, { SDL_SCANCODE_P, 0x19, 'P' },
    { SDL_SCANCODE_Q, 0x10, 'Q' }, { SDL_SCANCODE_R, 0x13, 'R' }, { SDL_SCANCODE_S, 0x1F, 'S' }, { SDL_SCANCODE_T, 0x14, 'T' },
    { SDL_SCANCODE_U, 0x16, 'U' }, { SDL_SCANCODE_V, 0x2F, 'V' }, { SDL_SCANCODE_W, 0x11, 'W' }, { SDL_SCANCODE_X, 0x2D, 'X' },
    { SDL_SCANCODE_Y, 0x15, 'Y' }, { SDL_SCANCODE_Z, 0x2C, 'Z' },
    { SDL_SCANCODE_1, 0x02, '1' }, { SDL_SCANCODE_2, 0x03, '2' }, { SDL_SCANCODE_3, 0x04, '3' }, { SDL_SCANCODE_4, 0x05, '4' },
    { SDL_SCANCODE_5, 0x06, '5' }, { SDL_SCANCODE_6, 0x07, '6' }, { SDL_SCANCODE_7, 0x08, '7' }, { SDL_SCANCODE_8, 0x09, '8' },
    { SDL_SCANCODE_9, 0x0A, '9' }, { SDL_SCANCODE_0, 0x0B, '0' },
    { SDL_SCANCODE_F1, 0x3B, 0x70 }, { SDL_SCANCODE_F2, 0x3C, 0x71 }, { SDL_SCANCODE_F3, 0x3D, 0x72 },
    { SDL_SCANCODE_F4, 0x3E, 0x73 }, { SDL_SCANCODE_F5, 0x3F, 0x74 }, { SDL_SCANCODE_F6, 0x40, 0x75 },
    { SDL_SCANCODE_F7, 0x41, 0x76 }, { SDL_SCANCODE_F8, 0x42, 0x77 }, { SDL_SCANCODE_F9, 0x43, 0x78 },
    { SDL_SCANCODE_F10, 0x44, 0x79 }, { SDL_SCANCODE_F11, 0x57, 0x7A }, { SDL_SCANCODE_F12, 0x58, 0x7B },
    { SDL_SCANCODE_GRAVE, 0x29, 0xC0 }, { SDL_SCANCODE_MINUS, 0x0C, 0xBD }, { SDL_SCANCODE_EQUALS, 0x0D, 0xBB },
    { SDL_SCANCODE_LEFTBRACKET, 0x1A, 0xDB }, { SDL_SCANCODE_RIGHTBRACKET, 0x1B, 0xDD },
    { SDL_SCANCODE_BACKSLASH, 0x2B, 0xDC }, { SDL_SCANCODE_SEMICOLON, 0x27, 0xBA },
    { SDL_SCANCODE_APOSTROPHE, 0x28, 0xDE }, { SDL_SCANCODE_COMMA, 0x33, 0xBC }, { SDL_SCANCODE_PERIOD, 0x34, 0xBE },
    { SDL_SCANCODE_SLASH, 0x35, 0xBF },
    { SDL_SCANCODE_HOME, 0xC7, 0x24 }, { SDL_SCANCODE_PAGEUP, 0xC9, 0x21 }, { SDL_SCANCODE_END, 0xCF, 0x23 },
    { SDL_SCANCODE_PAGEDOWN, 0xD1, 0x22 }, { SDL_SCANCODE_INSERT, 0xD2, 0x2D }, { SDL_SCANCODE_DELETE, 0xD3, 0x2E },
    { SDL_SCANCODE_KP_0, 0x52, 0x60 }, { SDL_SCANCODE_KP_1, 0x4F, 0x61 }, { SDL_SCANCODE_KP_2, 0x50, 0x62 },
    { SDL_SCANCODE_KP_3, 0x51, 0x63 }, { SDL_SCANCODE_KP_4, 0x4B, 0x64 }, { SDL_SCANCODE_KP_5, 0x4C, 0x65 },
    { SDL_SCANCODE_KP_6, 0x4D, 0x66 }, { SDL_SCANCODE_KP_7, 0x47, 0x67 }, { SDL_SCANCODE_KP_8, 0x48, 0x68 },
    { SDL_SCANCODE_KP_9, 0x49, 0x69 }, { SDL_SCANCODE_KP_PERIOD, 0x53, 0x6E }, { SDL_SCANCODE_KP_MULTIPLY, 0x37, 0x6A },
    { SDL_SCANCODE_KP_PLUS, 0x4E, 0x6B }, { SDL_SCANCODE_KP_MINUS, 0x4A, 0x6D }, { SDL_SCANCODE_KP_DIVIDE, 0xB5, 0x6F },
};

/* ---- the event queue the host fills and host_poll_event drains ------------------- */
#define QMAX 256
static host_event s_q[QMAX];
static int s_qh, s_qt;
static uint8_t s_down[SDL_SCANCODE_COUNT];   /* held, so focus loss lets them go */

static void qpush(const host_event *e)
{
    int n = (s_qt + 1) % QMAX;
    if (n != s_qh) {
        s_q[s_qt] = *e;
        s_qt = n;
    }
}

static void key_event(SDL_Scancode sc, int down)
{
    size_t i;
    if ((unsigned)sc < SDL_SCANCODE_COUNT) {
        if (s_down[sc] == down)
            return;                   /* auto-repeat */
        s_down[sc] = (uint8_t)down;
    }
    for (i = 0; i < sizeof k_keys / sizeof k_keys[0]; i++)
        if (k_keys[i].sc == sc) {
            host_event e;
            memset(&e, 0, sizeof e);
            e.type = HOST_EV_KEY;
            e.vk = k_keys[i].vk;
            e.scan = k_keys[i].dik;
            e.down = down;
            qpush(&e);
            return;
        }
}

static void char_event(int ch)
{
    host_event e;
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_CHAR;
    e.ch = ch;
    qpush(&e);
}

/* ---- the window ----------------------------------------------------------------- */
static SDL_Window *s_win;
static int s_w = 640, s_h = 480;      /* the size the game asked for: the mouse's units */
static int s_buttons;                  /* the mouse buttons held: 1 the left */

int host_window_open(int width, int height, const char *title)
{
    SDL_WindowFlags f = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (s_win)
        return 1;
    s_w = width > 0 ? width : 640;
    s_h = height > 0 ? height : 480;
#if defined(HOST_LINUX_VULKAN)
    f |= SDL_WINDOW_VULKAN;
#elif defined(HOST_LINUX_GL)
    f |= SDL_WINDOW_OPENGL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif
    s_win = SDL_CreateWindow(title ? title : s_app_title, s_w * 2, s_h * 2, f);
    if (!s_win) {
        fprintf(stderr, "host: window: %s\n", SDL_GetError());
        return 0;
    }
    host_window_lock_aspect(1);
    {   /* the window's icon: icon.png beside the game, when the release
           builder put one there (Remastered's, or the disc's for Boss Rally) */
        char png[1100];
        SDL_Surface *ic;
        snprintf(png, sizeof png, "%s/icon.png", host_resource_dir());
        if ((ic = SDL_LoadPNG(png)) != NULL) {
            SDL_SetWindowIcon(s_win, ic);
            SDL_DestroySurface(ic);
        }
    }
    SDL_HideCursor();                  /* the game draws its own */
    SDL_StartTextInput(s_win);
    SDL_ShowWindow(s_win);
    SDL_RaiseWindow(s_win);
    return 1;
}

void host_window_close(void)
{
    if (s_win) {
        SDL_StopTextInput(s_win);
        SDL_DestroyWindow(s_win);
        s_win = NULL;
    }
}

void host_window_lock_aspect(int lock)
{
    float a = (float)s_w / (float)s_h;
    if (s_win)
        SDL_SetWindowAspectRatio(s_win, lock ? a : 0.0f, lock ? a : 0.0f);
}

int host_window_visible(void)
{
    return s_win && !(SDL_GetWindowFlags(s_win) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED));
}

/* the SDL_Window: what host_linux_vk_surface and host_linux_gl_open take */
void *host_window_handle(void) { return s_win; }

void host_window_pixels(int *w, int *h)
{
    *w = *h = 0;
    if (s_win && !SDL_GetWindowSizeInPixels(s_win, w, h))
        *w = *h = 0;
}

/* the instance extensions a Vulkan surface on the window needs; 0 without SDL's
 * Vulkan (no loader on the machine) */
int host_linux_vk_extensions(const char *const **names, uint32_t *n)
{
    static int loaded;
    Uint32 k = 0;
    const char *const *v;
    if (!loaded)                       /* (a Vulkan window has loaded it already) */
        loaded = SDL_Vulkan_LoadLibrary(NULL) ? 1 : -1;
    v = loaded > 0 ? SDL_Vulkan_GetInstanceExtensions(&k) : NULL;
    *names = v;
    *n = v ? k : 0;
    return v != NULL;
}

/* a VkSurfaceKHR for window (host_window_handle) made in instance (a VkInstance),
 * into *surface; 1 when made */
int host_linux_vk_surface(void *window, void *instance, void *surface)
{
    if (!window || !instance)
        return 0;
    if (!SDL_Vulkan_CreateSurface((SDL_Window *)window, (VkInstance)instance, NULL, (VkSurfaceKHR *)surface)) {
        fprintf(stderr, "host: vulkan surface: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

/* an OpenGL 3.3 core context on window (a hidden one of its own when NULL),
 * current on the calling thread; NULL when the machine has none */
void *host_linux_gl_open(void *window)
{
    SDL_Window *w = (SDL_Window *)window;
    SDL_GLContext c;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    if (!w && !(w = SDL_CreateWindow("", 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN)))
        return NULL;
    if (!(c = SDL_GL_CreateContext(w))) {
        fprintf(stderr, "host: opengl: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_GL_MakeCurrent(w, c);
    SDL_GL_SetSwapInterval(0);         /* the game paces its own frames (as host/windows' WGL) */
    return c;
}

void *host_linux_gl_proc(const char *name) { return (void *)SDL_GL_GetProcAddress(name); }
void host_linux_gl_swap(void *window)
{
    if (window)
        SDL_GL_SwapWindow((SDL_Window *)window);
}

/* a frame of ARGB pixels (0xAARRGGBB, top row first) onto the window, scaled
 * to it with nearest-neighbour filtering (the software renderer's) */
void host_present(const uint32_t *argb, int w, int h)
{
    SDL_Surface *dst, *src;
    if (!s_win || !argb || w <= 0 || h <= 0 || !(dst = SDL_GetWindowSurface(s_win)))
        return;
    src = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_XRGB8888, (void *)argb, w * 4);
    if (!src)
        return;
    SDL_BlitSurfaceScaled(src, NULL, dst, NULL, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(src);
    SDL_UpdateWindowSurface(s_win);
}

static void mouse_event(float x, float y)
{
    host_event e;
    int ww = 0, wh = 0;
    SDL_GetWindowSize(s_win, &ww, &wh);
    memset(&e, 0, sizeof e);
    e.type = HOST_EV_MOUSE;
    e.x = ww > 0 ? (int)(x * s_w / ww) : 0;
    e.y = wh > 0 ? (int)(y * s_h / wh) : 0;
    e.buttons = s_buttons;
    qpush(&e);
}

static void translate(const SDL_Event *ev)
{
    host_event e;
    size_t i;
    memset(&e, 0, sizeof e);
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat &&
            ev->key.scancode == SDL_SCANCODE_RETURN && (ev->key.mod & SDL_KMOD_ALT)) {
            SDL_SetWindowFullscreen(s_win, !(SDL_GetWindowFlags(s_win) & SDL_WINDOW_FULLSCREEN));
            return;                    /* full screen: not the game's key */
        }
        key_event(ev->key.scancode, ev->type == SDL_EVENT_KEY_DOWN);
        /* Enter, Backspace and Escape are characters too (text input gives none) */
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            if (ev->key.scancode == SDL_SCANCODE_RETURN || ev->key.scancode == SDL_SCANCODE_KP_ENTER)
                char_event(0x0D);
            else if (ev->key.scancode == SDL_SCANCODE_BACKSPACE)
                char_event(0x08);
            else if (ev->key.scancode == SDL_SCANCODE_ESCAPE)
                char_event(0x1B);
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        for (i = 0; ev->text.text[i]; i++) {
            unsigned char c = (unsigned char)ev->text.text[i];
            if (c >= 0x20 && c < 0x7F)    /* ASCII only, as the other hosts */
                char_event(c);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        mouse_event(ev->motion.x, ev->motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                s_buttons |= 1;
            else
                s_buttons &= ~1;
            mouse_event(ev->button.x, ev->button.y);
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        e.type = HOST_EV_FOCUS;
        e.down = 1;
        qpush(&e);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        /* keys held when focus leaves go up, or they stay down for good */
        for (i = 0; i < SDL_SCANCODE_COUNT; i++)
            if (s_down[i])
                key_event((SDL_Scancode)i, 0);
        e.type = HOST_EV_FOCUS;
        e.down = 0;
        qpush(&e);
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_QUIT:
        e.type = HOST_EV_CLOSE;        /* the game decides, through WM_CLOSE */
        qpush(&e);
        break;
    default:
        break;
    }
}

int host_poll_event(host_event *ev, uint32_t wait_ms)
{
    SDL_Event e;
    int first = 1;
    while (s_qh == s_qt) {
        int got;
        if (first && wait_ms)
            got = SDL_WaitEventTimeout(&e, wait_ms == 0xFFFFFFFFu ? -1 : (Sint32)wait_ms);
        else
            got = SDL_PollEvent(&e);
        first = 0;
        if (!got)
            break;
        translate(&e);
    }
    if (s_qh == s_qt)
        return 0;
    *ev = s_q[s_qh];
    s_qh = (s_qh + 1) % QMAX;
    return 1;
}

void host_message_box(const char *text, const char *caption)
{
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption ? caption : s_app_title,
                                  text ? text : "", s_win))
        fprintf(stderr, "%s: %s\n", caption ? caption : s_app_title, text ? text : "");
}

/* ---- audio out: an SDL audio stream pulling the platform's mixer ------------------- */
static SDL_AudioStream *s_out;
static host_audio_fn s_afn;
static void *s_auser;

static void SDLCALL pull(void *user, SDL_AudioStream *st, int want, int total)
{
    float buf[2 * 1024];
    (void)user;
    (void)total;
    while (want > 0) {
        int frames = want / 8;
        if (frames > 1024)
            frames = 1024;
        if (frames <= 0)
            frames = 1;
        if (s_afn)
            s_afn(buf, frames, s_auser);
        else
            memset(buf, 0, (size_t)frames * 8);
        SDL_PutAudioStreamData(st, buf, frames * 8);
        want -= frames * 8;
    }
}

int host_audio_open(int rate, host_audio_fn fn, void *user)
{
    SDL_AudioSpec spec;
    if (s_out)
        return 1;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "host: audio: %s\n", SDL_GetError());
        return 0;
    }
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = rate;
    s_afn = fn;
    s_auser = user;
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    s_out = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, pull, NULL);
    if (!s_out) {
        fprintf(stderr, "host: audio: %s\n", SDL_GetError());
        return 0;
    }
    SDL_ResumeAudioStreamDevice(s_out);
    return 1;
}

void host_audio_close(void)
{
    if (s_out) {
        SDL_DestroyAudioStream(s_out);
        s_out = NULL;
    }
}

/* ---- decoded audio files: FLAC (libFLAC), resampled by SDL ------------------------- */
struct host_stream {
    FLAC__StreamDecoder *d;
    SDL_AudioStream *cv;               /* the file's rate in, the mixer's out */
    int rate, channels, bits, eof, rate_out;
};

static FLAC__StreamDecoderWriteStatus flac_write(const FLAC__StreamDecoder *d, const FLAC__Frame *f,
                                                 const FLAC__int32 *const buf[], void *user)
{
    host_stream *s = (host_stream *)user;
    float lr[2 * 1024];
    float sc = 1.0f / (float)(1u << (f->header.bits_per_sample - 1));
    unsigned n = f->header.blocksize, i = 0, k;
    int r = (int)f->header.channels > 1 ? 1 : 0;   /* mono: both sides the one channel */
    (void)d;
    if (!s->cv)
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    while (i < n) {
        unsigned m = n - i > 1024 ? 1024 : n - i;
        for (k = 0; k < m; k++) {
            lr[2 * k] = (float)buf[0][i + k] * sc;
            lr[2 * k + 1] = (float)buf[r][i + k] * sc;
        }
        SDL_PutAudioStreamData(s->cv, lr, (int)(m * 8));
        i += m;
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_meta(const FLAC__StreamDecoder *d, const FLAC__StreamMetadata *m, void *user)
{
    host_stream *s = (host_stream *)user;
    (void)d;
    if (m->type == FLAC__METADATA_TYPE_STREAMINFO) {
        s->rate = (int)m->data.stream_info.sample_rate;
        s->channels = (int)m->data.stream_info.channels;
        s->bits = (int)m->data.stream_info.bits_per_sample;
    }
}

static void flac_error(const FLAC__StreamDecoder *d, FLAC__StreamDecoderErrorStatus st, void *user)
{
    (void)d;
    (void)user;
    fprintf(stderr, "host: flac: %s\n", FLAC__StreamDecoderErrorStatusString[st]);
}

host_stream *host_stream_open(const char *path, int rate)
{
    host_stream *s = (host_stream *)calloc(1, sizeof *s);
    SDL_AudioSpec in, out;
    if (!s || !(s->d = FLAC__stream_decoder_new())) {
        free(s);
        return NULL;
    }
    if (FLAC__stream_decoder_init_file(s->d, path, flac_write, flac_meta, flac_error, s) !=
            FLAC__STREAM_DECODER_INIT_STATUS_OK ||
        !FLAC__stream_decoder_process_until_end_of_metadata(s->d) || s->rate <= 0 || s->channels <= 0) {
        host_stream_close(s);
        return NULL;
    }
    in.format = SDL_AUDIO_F32;
    in.channels = 2;
    in.freq = s->rate;
    out = in;
    out.freq = rate;
    s->rate_out = rate;
    if (!(s->cv = SDL_CreateAudioStream(&in, &out))) {
        host_stream_close(s);
        return NULL;
    }
    return s;
}

int host_stream_read(host_stream *s, float *lr, int frames)
{
    int got;
    if (!s || frames <= 0)
        return 0;
    while (!s->eof && SDL_GetAudioStreamAvailable(s->cv) < frames * 8) {
        if (!FLAC__stream_decoder_process_single(s->d) ||
            FLAC__stream_decoder_get_state(s->d) == FLAC__STREAM_DECODER_END_OF_STREAM) {
            s->eof = 1;
            SDL_FlushAudioStream(s->cv);   /* the resampler's tail */
        }
    }
    got = SDL_GetAudioStreamData(s->cv, lr, frames * 8);
    return got > 0 ? got / 8 : 0;
}

void host_stream_close(host_stream *s)
{
    if (!s)
        return;
    if (s->d) {
        FLAC__stream_decoder_finish(s->d);
        FLAC__stream_decoder_delete(s->d);
    }
    if (s->cv)
        SDL_DestroyAudioStream(s->cv);
    free(s);
}

/* ---- game controllers: SDL's gamepads (host/sdl/sdl_pads.c) ------------------------ */
int host_pad_read(host_pad *o) { return sdl_pads_read(0, o); }
