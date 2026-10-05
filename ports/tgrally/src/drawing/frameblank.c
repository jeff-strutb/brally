/* frameblank.c -- starting a frame, and flushing the screen to black between modes
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void BrFrameBegin(int hires);
void BrScissorSet(int x, int y, int w, int h);
void BrFrameStatsReset(void);
void BrScreenClear(int r, int g, int b);
void BrFrameBeginLayout1(void);
void BrFrameEnd(void);
extern int D_8028A884;
void BrFrameBeginLayout0(void);
extern int D_8028AA08;
extern int D_8028AA0C;
extern int D_8028AA10;
extern int D_8028AA2C;
extern int D_8028AA30;
extern int D_8028AA34;
extern int D_8028AA38;
extern int D_8028AA3C;
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028B740;
extern int D_8028B744;
extern int D_8028B748;
extern int D_8028B74C;
extern int D_8028AAB0;
extern int D_8028AAB4;
extern int D_8028A85C;                 /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];      /* the frame buffers */
#define D_00000400 0x400                 /* the Z-buffer: physical 0x400 (a link-time address) */
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrFramePoolsReset(void);
typedef struct BrViewRect { int x; int y; int w; int h; int x10; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AB0C;                  /* number of players */
extern int D_8028A84C;                  /* resolution changed */
extern Gfx (*D_8028A848)[6000];         /* the two display-list buffers */
extern int D_8028AA68;                  /* texture filtering on */
extern int D_8028A898;                  /* the texture filter mode */
extern int D_8028A8A0;                  /* the colour dither mode */
extern int D_8028A89C;                  /* the alpha dither mode */
extern int D_8028AA50;                  /* z-buffering on */
extern int D_8028AA4C;                  /* gamma off */
extern int D_8028A8A4;                  /* the viewport being used */
extern char D_8028A900[];               /* the viewports (0x28 bytes each) */
extern char D_801B5000[];               /* the low-res frame buffers */
extern char D_801DA800[];
#define D_0028A8C0 0x28A8C0        /* a ROM offset */
typedef struct BrTask {         /* an OSTask (0x40 bytes) */
  unsigned int type;
  unsigned int flags;
  TgrAddr ucode_boot;  /* void * */
  unsigned int ucode_boot_size;
  TgrAddr ucode;  /* void * */
  unsigned int ucode_size;
  TgrAddr ucode_data;  /* void * */
  unsigned int ucode_data_size;
  TgrAddr dram_stack;  /* void * */
  unsigned int dram_stack_size;
  TgrAddr output_buff;  /* void * */
  TgrAddr output_buff_size;  /* void * */
  TgrAddr data_ptr;  /* void * */
  unsigned int data_size;
  TgrAddr yield_data_ptr;  /* void * */
  unsigned int yield_data_size;
} BrTask;
extern BrTask D_8031A9A8[2];            /* one graphics task per display list */
extern char rspbootTextStart[];
extern char rspbootTextEnd[];
extern char gspF3DEX_fifoTextStart[];
extern char gspF3DEX_fifoDataStart[];
extern char D_8031A598[];               /* the RSP's DRAM stack */
extern long long *D_8028A860[2];        /* the RDP FIFO: start and end */
extern int D_8028AB84;                  /* FIFO words kept back */
extern int D_8028AB70;                  /* display-list commands this frame */
extern int D_8028AB7C;                  /* the most commands in one frame */
extern int *D_8028C75C;                 /* matrix pool: next and start */
extern int *D_8028C760;
extern int D_8028AB74;                  /* the most matrices in one frame */
typedef struct { char x[16]; } BrVtx16;
extern BrVtx16 *D_8028C764;             /* vertex pool: next and start */
extern BrVtx16 *D_8028C768;
extern int D_8028AB78;                  /* the most vertices in one frame */
extern char D_8031A320[];               /* RSP done queue */
extern char D_8031A358[];               /* RDP done queue */
extern char D_8031A390[];               /* retrace queue */
extern void (*D_8031B31C)(void);        /* run once the RSP has finished */
extern int D_8031B320;
extern int D_8028AA94;                  /* debug: copy the low-memory frame into the display */
extern int D_80000400[];
extern int D_8028AAE0;                  /* CPU time of the last frame */
extern int D_8028AAE4;
extern int D_8028AAE8;
extern int D_8028A854;                  /* resolution the VI is set to */
extern int osTvType;
extern OSViMode D_802A5D70[1];               /* VI modes: low res MPAL, NTSC; high res MPAL, NTSC */
extern OSViMode D_802A54B0[1];
extern OSViMode D_802A6040[1];
extern OSViMode D_802A5780[1];
extern int D_8028A888;                  /* frames the screen stays blank */
extern int D_8028AA20;
extern int D_8028AA24;
void BrFatal(char *msg);
void BrPerfFrameStart(void);
/* -- end declarations -- */

/* WHAT IT DOES: Clear the Z-buffer: point the colour image at it, fill it
 * with the far depth, and point the colour image back at the frame buffer
 * being drawn.  The first colour image and the fill colour are written as
 * raw words through block-scope pointers (the macros schedule their two
 * words the other way round). */
/* @implements 0x80217C94 tgr BrZBufferClear */
void BrZBufferClear(void)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  {
    unsigned int *p = (unsigned int *)D_8028A858++;
    tgr_wr32(&p[0], 0xff100000 | (((D_8028AAB0 << D_8028A850) - 1) & 0xfff));
    tgr_wr32(&p[1], D_00000400);
  }
  {
    unsigned int *p = (unsigned int *)D_8028A858++;
    tgr_wr32(&p[0], 0xf7000000);
    tgr_wr32(&p[1], GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0));
  }
  gDPFillRectangle(D_8028A858++, 0, 0, (D_8028AAB0 << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  gDPSetColorImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8028AAB0 << D_8028A850, D_8031AA28[D_8028A85C] + 0x80000000);
}

/* WHAT IT DOES: BrZBufferClear for a rectangle: the Z-buffer is filled with
 * the far depth over (x, y) .. (x + w - 1, ...), then the colour image goes
 * back to the frame buffer being drawn.  The bottom edge is x + h - 1, not
 * y + h - 1: the ROM computes it from x (the race tick passes each view's
 * x, y, w, h). */
/* @implements 0x80217E20 tgr BrZBufferClearRect */
void BrZBufferClearRect(int x, int y, int w, int h)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  {
    unsigned int *p = (unsigned int *)D_8028A858++;
    tgr_wr32(&p[0], 0xff100000 | (((D_8028AAB0 << D_8028A850) - 1) & 0xfff));
    tgr_wr32(&p[1], D_00000400);
  }
  {
    unsigned int *p = (unsigned int *)D_8028A858++;
    tgr_wr32(&p[0], 0xf7000000);
    tgr_wr32(&p[1], GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0));
  }
  gDPFillRectangle(D_8028A858++, x, y, x + w - 1, x + h - 1);
  gDPPipeSync(D_8028A858++);
  gDPSetColorImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8028AAB0 << D_8028A850, D_8031AA28[D_8028A85C] + 0x80000000);
}

/* WHAT IT DOES: Clear the whole screen to one colour (fill mode,
 * RGBA5551).  Both cycle-type commands are raw words; the last one through
 * a block-scope pointer with each store on its own line (the block gives
 * the ROM's dead pointer spill; a one-line block swaps the two stores). */
/* @implements 0x80217FB8 tgr BrScreenClear */
void BrScreenClear(int r, int g, int b)
{
  unsigned short c;
  unsigned int *p;

  gDPPipeSync(D_8028A858++);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  p = (unsigned int *)D_8028A858++;
  tgr_wr32(&p[0], 0xba001402);
  tgr_wr32(&p[1], G_CYC_FILL);
  c = GPACK_RGBA5551(r, g, b, 1);
  gDPSetFillColor(D_8028A858++, c | c << 16);
  gDPFillRectangle(D_8028A858++, 0, 0, (D_8028AAB0 << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  {
    unsigned int *q = (unsigned int *)D_8028A858++;
    tgr_wr32(&q[0], 0xba001402);
    tgr_wr32(&q[1], G_CYC_1CYCLE);
  }
}

/* WHAT IT DOES: Fill a w by h rectangle at (x, y) with one colour (fill
 * mode, RGBA5551); on a hi-res screen the rectangle's numbers are doubled
 * (and its far corner shifted by the resolution again, as the ROM does).
 * Cycle-type commands as raw words, as in BrScreenClear; the PC twin
 * (br_gfxfill.c) writes every command that way. */
/* @implements 0x80218104 tgr BrGfxFillRect */
void BrGfxFillRect(int x, int y, int w, int h, int r, int g, int b)
{
  unsigned short c;
  unsigned int *p;

  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    w <<= 1;
    h <<= 1;
  }
  c = GPACK_RGBA5551(r, g, b, 1);
  gDPPipeSync(D_8028A858++);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  p = (unsigned int *)D_8028A858++;
  tgr_wr32(&p[0], 0xba001402);
  tgr_wr32(&p[1], G_CYC_FILL);
  gDPSetFillColor(D_8028A858++, c | c << 16);
  gDPFillRectangle(D_8028A858++, x, y, ((x + w) << D_8028A850) - 1, ((y + h) << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  p = (unsigned int *)D_8028A858++;
  tgr_wr32(&p[0], 0xba001402);
  tgr_wr32(&p[1], G_CYC_1CYCLE);
}

/* WHAT IT DOES: Push three empty frames through the second screen layout
 * with the video output blanked (the last one not cleared), before a
 * front-end screen that needs every buffer reset. */
/* @implements 0x802429EC tgr BrScreenFlush3Layout1 */
void BrScreenFlush3Layout1(void)
{
  D_8028A884 = 1;
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout1();
  BrFrameEnd();
  D_8028A884 = 0;
}

/* WHAT IT DOES: Push three empty frames through the first screen layout
 * with the video output blanked (the last one not cleared), before a
 * front-end screen that needs every buffer reset. */
/* @implements 0x80242A7C tgr BrScreenFlush3Layout0 */
void BrScreenFlush3Layout0(void)
{
  D_8028A884 = 1;
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  BrFrameEnd();
  osViBlack(1);
  BrFrameBeginLayout0();
  BrFrameEnd();
  D_8028A884 = 0;
}

/* WHAT IT DOES: Zero the frame builder's per-frame counters at the start of
 * a frame. */
/* @implements 0x802173C8 tgr BrFrameStatsReset */
void BrFrameStatsReset(void)
{
  D_8028AA2C = D_8028AA30 = D_8028AA34 = 0;
  D_8028AA38 = D_8028AA3C = 0;
  D_8028AA10 = D_8028AA08 = D_8028AA0C = 0;
}

