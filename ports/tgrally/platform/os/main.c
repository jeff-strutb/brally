/* main.c: the process.  What the N64 did from power-on: the cartridge's data
 * put in place (here: built into the executable, tools/assets.py, and lifted
 * into the arena), then the game's entry point, BrBoot, on the first game
 * thread; the host's own thread runs the window, input and presentation. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "../render/rdr.h"

TgrConfig g_tgr;

void BrBoot(void);                  /* src/startup/boot.c */
void tgr_script_load(const char *path);
void tgr_input_key(int vk, int down);
void tgr_input_release(void);
extern volatile int tgr_paused;

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
            "usage: tgrally [--headless] [--frames N] [--script FILE] [--trace FILE]\n"
            "               [--shots DIR --shot-at F1,F2,...]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) g_tgr.headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) g_tgr.frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) g_tgr.script = argv[++i];
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc) g_tgr.trace = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) g_tgr.shot_dir = argv[++i];
        else if (!strcmp(argv[i], "--shot-at") && i + 1 < argc) g_tgr.shot_at = argv[++i];
        else usage();
    }
    host_set_app_name("Top Gear Rally", "Top Gear Rally");
    host_init(argc, argv);
    if (g_tgr.script)
        tgr_script_load(g_tgr.script);
    tgr_pak_init();
    tgr_addr_init();
    tgr_lift();
    tgr_gfx_init();
    tgr_audio_init();
    if (!g_tgr.headless) {
        if (!host_window_open(640, 480, "Top Gear Rally")) {
            fprintf(stderr, "tgr: no window\n");
            return 1;
        }
        rdr_window();
    }
    tgr_os_start(BrBoot);
    if (g_tgr.headless) {
        tgr_os_wait();
    } else {
        while (!tgr_os_finished()) {
            host_event ev;
            if (host_poll_event(&ev, 10)) {
                if (ev.type == HOST_EV_CLOSE)
                    break;
                if (ev.type == HOST_EV_FOCUS) {   /* in the background: paused, nothing held */
                    tgr_os_lock();
                    tgr_input_release();
                    tgr_os_unlock();
                    tgr_paused = !ev.down;
                }
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
