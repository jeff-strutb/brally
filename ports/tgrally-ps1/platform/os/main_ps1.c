/* main_ps1.c: the console from power-on.  What the N64 did: the cartridge's
 * initialised data lifted into place, then the game's entry point, BrBoot,
 * on the first game thread.
 *
 * A run is set up by run.cfg beside it (PCDRV), `key=value` lines:
 *   script=FILE    the pads from a script (n64box's format)
 *   frames=N       stop after N retraces
 *   trace=FILE     the per-retrace trace (gfx digests, swaps, audio buffers)
 *   headless=1     virtual time only: no pacing to the console's retraces
 * and any other key is an environment switch (TGR_*). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "ps1.h"

TgrConfig g_tgr;

void BrBoot(void);
void tgr_script_load(const char *path);

void tgr_log(const char *fmt, ...)
{
    va_list ap;
    if (!getenv("TGR_LOG"))
        return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static char s_cfg[1024];

static void config(void)
{
    int fd = pcdrv_open("run.cfg", 0), n;
    char *line, *save;
    if (fd < 0)
        return;
    n = pcdrv_read(fd, s_cfg, sizeof s_cfg - 1);
    pcdrv_close(fd);
    s_cfg[n > 0 ? n : 0] = 0;
    for (line = strtok_r(s_cfg, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#')
            continue;
        *eq++ = 0;
        if (!strcmp(line, "script")) g_tgr.script = eq;
        else if (!strcmp(line, "frames")) g_tgr.frames = atoi(eq);
        else if (!strcmp(line, "trace")) g_tgr.trace = eq;
        else if (!strcmp(line, "headless")) g_tgr.headless = atoi(eq);
        else ps1_setenv(line, eq);
    }
}

static void finish(void)
{
    int fd;
    fflush(stderr);
    if ((fd = pcdrv_creat("done")) >= 0)
        pcdrv_close(fd);
}

extern void tgr_trace_close(void);

int main(void)
{
    ps1_hw_init();
    pcdrv_init();
    config();
    tgr_log("tgr: config\n");
    {
        extern int ps1_hostlog;
        ps1_hostlog = getenv("TGR_HOSTLOG") != NULL;
    }
    if (getenv("TGR_PCSAMPLE"))
        ps1_pc_sample((uint32_t)atoi(getenv("TGR_PCSAMPLE")));
    if (g_tgr.script)
        tgr_script_load(g_tgr.script);
    tgr_log("tgr: script\n");
    if (!tgr_romdata_load())
        ps1_fatal("no romdata.bin");
    tgr_pak_init();
    tgr_addr_init();
    tgr_lift();
    tgr_log("tgr: lifted\n");
    tgr_gfx_init();
    tgr_audio_init();
    tgr_os_start(BrBoot);
    tgr_log("tgr: started\n");
    tgr_os_wait();
    fprintf(stderr, "tgr: %u retraces\n", tgr_frame());
    tgr_trace_close();
    finish();
    for (;;)
        ;
}
