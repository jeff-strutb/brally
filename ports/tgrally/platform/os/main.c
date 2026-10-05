/* main.c: the process.  What the N64 did from power-on: the ROM's first
 * megabyte read in (here: the user's ROM file, its data lifted into the
 * arena), then the game's entry point, BrBoot, on the first game thread;
 * the host's own thread runs the window, input and presentation. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "host.h"
#include "plat.h"

TgrConfig g_tgr;
uint8_t *g_rom;
size_t g_romlen;

void BrBoot(void);                  /* src/startup/boot.c */
void tgr_script_load(const char *path);
void tgr_input_key(int vk, int down);

static int load_rom(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    size_t i;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_rom = (uint8_t *)malloc((size_t)n);
    if (fread(g_rom, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return 0;
    }
    fclose(f);
    g_romlen = (size_t)n;
    /* .z64 is the cartridge's own order; .v64 swaps bytes in pairs, .n64 in words */
    if (g_rom[0] == 0x37 && g_rom[1] == 0x80) {
        for (i = 0; i + 1 < g_romlen; i += 2) {
            uint8_t t = g_rom[i]; g_rom[i] = g_rom[i + 1]; g_rom[i + 1] = t;
        }
    } else if (g_rom[0] == 0x40 && g_rom[1] == 0x12) {
        for (i = 0; i + 3 < g_romlen; i += 4) {
            uint8_t t = g_rom[i]; g_rom[i] = g_rom[i + 3]; g_rom[i + 3] = t;
            t = g_rom[i + 1]; g_rom[i + 1] = g_rom[i + 2]; g_rom[i + 2] = t;
        }
    }
    if (g_rom[0] != 0x80 || g_rom[1] != 0x37) {
        fprintf(stderr, "tgr: %s is not an N64 ROM\n", path);
        return 0;
    }
    if (strncasecmp((const char *)g_rom + 0x20, "TOP GEAR RALLY", 14) != 0 || g_rom[0x3E] != 'E')
        fprintf(stderr, "tgr: %s is not Top Gear Rally (USA); continuing\n", path);
    return 1;
}

void tgr_log(const char *fmt, ...)
{
    va_list ap;
    if (!getenv("TGR_LOG"))
        return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void usage(void)
{
    fprintf(stderr,
            "usage: tgrally [--rom FILE] [--headless] [--frames N] [--script FILE] [--trace FILE]\n"
            "               [--shots DIR] [--shot-every N]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int i;
    g_tgr.rom_path = getenv("TGR_ROM");
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) g_tgr.rom_path = argv[++i];
        else if (!strcmp(argv[i], "--headless")) g_tgr.headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) g_tgr.frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) g_tgr.script = argv[++i];
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc) g_tgr.trace = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) g_tgr.shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--shot-every") && i + 1 < argc) g_tgr.shot_every = atoi(argv[++i]);
        else usage();
    }
    if (!g_tgr.rom_path)
        g_tgr.rom_path = "reference/tgrally/Top Gear Rally (USA).z64";
    host_init(argc, argv);
    if (!load_rom(g_tgr.rom_path)) {
        fprintf(stderr, "tgr: cannot read the ROM %s (--rom or TGR_ROM)\n", g_tgr.rom_path);
        return 1;
    }
    if (g_tgr.script)
        tgr_script_load(g_tgr.script);
    tgr_addr_init();
    tgr_lift(g_rom, g_romlen);
    tgr_gfx_init();
    tgr_audio_init();
    if (!g_tgr.headless && !host_window_open(640, 480, "Top Gear Rally")) {
        fprintf(stderr, "tgr: no window\n");
        return 1;
    }
    tgr_os_start(BrBoot);
    if (g_tgr.headless) {
        tgr_os_wait();
    } else {
        for (;;) {
            host_event ev;
            if (host_poll_event(&ev, 10)) {
                if (ev.type == HOST_EV_CLOSE)
                    break;
                if (ev.type == HOST_EV_KEY) {
                    tgr_os_lock();
                    tgr_input_key(ev.vk, ev.down);
                    tgr_os_unlock();
                }
            }
        }
    }
    fprintf(stderr, "tgr: %u retraces\n", tgr_frame());
    host_shutdown();
    return 0;
}
