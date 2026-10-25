/* plat.h: the platform layer's internals (platform/os, gfx, audio).
 *
 * What the game asks of the machine (tgr_os.h) is done here on the host:
 * each game thread is a host thread, running alongside the others; the
 * clock is real time, with a retrace every 1/60 s; a load, a drawing task
 * or a controller read is done when asked and its message posted at once.
 * The game's display lists are drawn by platform/gfx, one task or present
 * at a time. */
#ifndef TGR_PLAT_H
#define TGR_PLAT_H
#include <stddef.h>
#include <stdint.h>
#include "tgr_os.h"
#include "tgr_addr.h"

/* ---- the run ---------------------------------------------------------------- */
typedef struct TgrConfig {
    int  headless;          /* no window, no pacing */
    int  frames;            /* stop after this many retraces (0: never) */
    const char *script;     /* scripted pad input (n64box's format) */
    const char *trace;      /* write the per-retrace trace (n64box's log) here */
    const char *shot_dir;   /* save frames as PNGs here: */
    const char *shot_at;    /* ... at these retraces (a comma list; named shots, never periodic) */
} TgrConfig;
extern TgrConfig g_tgr;

/* the cartridge's data, from ROM 0x70AB0 to its end (the initialised data and
 * the assets the game streams in), taken from the builder's ROM: assembled
 * into the executable at build time (tools/assets.py, ROMDATA=embed), or
 * written beside the game by the release builder and read at start-up
 * (ROMDATA=file, os/romdata.c). No ROM at run time either way */
#define TGR_ROMDATA_BASE 0x70AB0u
#define TGR_ROMDATA_SIZE 0x78F550u                  /* an 8 MB cartridge, less 0x70AB0 */
extern const uint8_t *tgr_romdata, *tgr_romdata_end;
int tgr_romdata_load(void);                         /* 1 when the data is in place */
/* ROM bytes [off, off + n) into dst; what lies outside the data reads as zero */
void tgr_rom_read(uint32_t off, void *dst, uint32_t n);

void tgr_addr_init(void);                    /* the native tables' pages (os/addr.c) */
void tgr_lift(void);
void tgr_log(const char *fmt, ...);

/* ---- the threads and the clock (os/thread.c) -------------------------------- */
uint64_t tgr_count(void);                   /* the clock: 46.875 MHz ticks since the start */
uint32_t tgr_frame(void);                   /* retraces so far */
void tgr_os_start(void (*boot)(void));      /* run BrBoot on the first game thread */
void tgr_pak_init(void);                    /* in play, the Controller Pak file (os/si.c) */
void tgr_os_wait(void);                     /* the caller (the process) waits for the end */
int  tgr_os_finished(void);                 /* the run is over (--frames reached) */
void tgr_os_lock(void);                     /* the queues and the input, from a host thread */
void tgr_os_unlock(void);
/* what the platform delivers when its work is done: an event, or a message to a queue */
void tgr_post_event(int event);
void tgr_post_mesg(TgrMsgQueue *mq, TgrMsg msg);
void tgr_event(int event);                  /* the same, by a caller that holds the lock */
void tgr_thread_idle(void);                 /* the idle thread's rest */
/* called at every retrace, from the clock thread */
void tgr_vi_retrace(void);

/* ---- the controllers (os/si.c) ---------------------------------------------- */
void tgr_input_frame(uint32_t frame);       /* the pads' state for this retrace */
int  tgr_pads(void);                        /* controllers plugged in */

/* ---- the RCP (gfx/) -------------------------------------------------------- */
void tgr_gfx_task(uint32_t dl);             /* run a display list (original address) */
void tgr_rcp_task(uint32_t dl);             /* draw it (gfx/rcp.c, onto render/rdr.h) */
void tgr_dump_state(const char *path, uint32_t dl);  /* game memory as the original holds it, to a file */
void tgr_gfx_swap(uint32_t fb);             /* the framebuffer osViSwapBuffer shows */
void tgr_gfx_present(void);                 /* the retrace: show it */
void tgr_gfx_init(void);
void tgr_gfx_lock(void);                    /* the renderer: one task or present at a time */
void tgr_gfx_unlock(void);

/* ---- audio out (audio/out.c) ------------------------------------------------ */
void tgr_audio_init(void);
/* the output's low-pass cut, Hz (0: none past the resampler's own; audio/out.c) */
void tgr_audio_lowpass(int hz);
void tgr_audio_buffer(const int16_t *lr, int frames, int rate);

/* ---- the trace (os/trace.c): what n64box logs per retrace ------------------- */
void tgr_trace(const char *kind, const char *fmt, ...);
#endif
