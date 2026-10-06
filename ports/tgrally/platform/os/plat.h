/* plat.h: the platform layer's internals (platform/os, gfx, audio).
 *
 * libultra's API (ultra64.h) is implemented here over a model of the N64
 * that is the same as tools/tgrally/n64box.py's, so the original ROM run headless
 * and this port run alike: one CPU (exactly one game thread runs at a time,
 * switched only at OS calls), one virtual clock (osGetCount; a retrace every
 * 1/60 s, advanced only when every game thread is blocked), the same order
 * of events.  In a window the retraces are also paced to the wall clock. */
#ifndef TGR_PLAT_H
#define TGR_PLAT_H
#include <stddef.h>
#include <stdint.h>
#include "ultra64.h"
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

/* ---- the scheduler and the clock (os/thread.c) ------------------------------- */
#define TGR_TICKS_PER_FRAME 781250u         /* osGetCount at 46.875 MHz, 1/60 s */
uint64_t tgr_count(void);                   /* the virtual clock */
uint32_t tgr_frame(void);                   /* retraces so far */
void tgr_os_start(void (*boot)(void));      /* run BrBoot on the first game thread */
void tgr_pak_init(void);                    /* in play, the Controller Pak file (os/si.c) */
void tgr_os_wait(void);                     /* the caller (the process) waits for the end */
int  tgr_os_finished(void);                 /* the run is over (--frames reached) */
void tgr_os_lock(void);                     /* the game's state, from a host thread */
void tgr_os_unlock(void);
/* a delivery the hardware makes later: an event, or a message to a queue */
void tgr_post_event_at(uint64_t when, int event);
void tgr_post_mesg_at(uint64_t when, OSMesgQueue *mq, OSMesg msg);
void tgr_event(int event);                  /* now (the caller holds the game) */
/* called at every retrace, with the game's state locked */
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

/* ---- audio out (audio/out.c) ------------------------------------------------ */
void tgr_audio_init(void);
void tgr_audio_buffer(const int16_t *lr, int frames, int rate);

/* ---- the trace (os/trace.c): what n64box logs per retrace ------------------- */
void tgr_trace(const char *kind, const char *fmt, ...);
#endif
