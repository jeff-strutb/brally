/* host_null.c: a host with no window, no input and no audio device.
 *
 * The game runs exactly as it would on screen; nothing is shown and no
 * key is ever pressed except what a script injects. Used to bring the core
 * up, for tests, and for lockstep against the 32-bit lane.
 *
 * Directories (environment):
 *   BR_CDROOT   the CD's files            (default testdata/disc)
 *   BR_GAMEDIR  the install directory     (default: the CD root)
 *   BR_SAVEDIR  saves and settings        (default build/portable/save)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"

static char s_cd[1024], s_game[1024], s_save[1024];

void host_init(int argc, char **argv)
{
    const char *e;
    (void)argc;
    (void)argv;
    e = getenv("BR_CDROOT");
    snprintf(s_cd, sizeof s_cd, "%s", e ? e : "testdata/disc");
    e = getenv("BR_GAMEDIR");
    snprintf(s_game, sizeof s_game, "%s", e ? e : s_cd);
    e = getenv("BR_SAVEDIR");
    snprintf(s_save, sizeof s_save, "%s", e ? e : "build/portable/save");
    host_mkdir("build");
    host_mkdir("build/portable");
    host_mkdir(s_save);
}

void host_shutdown(void) {}

const char *host_game_dir(void) { return s_game; }
const char *host_cd_dir(void)   { return s_cd; }
const char *host_save_dir(void) { return s_save; }

int host_window_open(int width, int height, const char *title)
{
    fprintf(stderr, "host: window %dx%d \"%s\" (headless)\n", width, height, title ? title : "");
    return 1;
}

void host_window_close(void) {}

void host_present(const uint32_t *argb, int w, int h)
{
    (void)argb;
    (void)w;
    (void)h;
}

int host_poll_event(host_event *ev, uint32_t wait_ms)
{
    (void)ev;
    if (wait_ms && wait_ms != 0xFFFFFFFFu)
        host_sleep_ms(wait_ms);
    return 0;
}

void host_message_box(const char *text, const char *caption)
{
    fprintf(stderr, "host: message box [%s] %s\n", caption ? caption : "", text ? text : "");
}

int host_audio_open(int rate, host_audio_fn fn, void *user)
{
    (void)rate;
    (void)fn;
    (void)user;
    return 0;
}

void host_audio_close(void) {}
