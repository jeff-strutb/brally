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
void osViBlack(char param_1);
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
extern char D_00000400[];              /* the Z-buffer (a link-time address) */
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrFramePoolsReset(void);
void osViSetSpecialFeatures(unsigned int func);
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
extern char D_0028A8C0[];               /* the identity matrix, as a physical address */
typedef struct BrTask {         /* an OSTask (0x40 bytes) */
  unsigned int type;
  unsigned int flags;
  void *ucode_boot;
  unsigned int ucode_boot_size;
  void *ucode;
  unsigned int ucode_size;
  void *ucode_data;
  unsigned int ucode_data_size;
  void *dram_stack;
  unsigned int dram_stack_size;
  void *output_buff;
  void *output_buff_size;
  void *data_ptr;
  unsigned int data_size;
  void *yield_data_ptr;
  unsigned int yield_data_size;
} BrTask;
extern BrTask D_8031A9A8[2];            /* one graphics task per display list */
extern char D_8026EA00[];               /* rspboot */
extern char D_8026EAD0[];               /* the F3DEX (FIFO) microcode text */
extern char D_802ABC00[];               /* its data */
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
extern char D_802A5D70[];               /* VI modes: low res MPAL, NTSC; high res MPAL, NTSC */
extern char D_802A54B0[];
extern char D_802A6040[];
extern char D_802A5780[];
extern int D_8028A888;                  /* frames the screen stays blank */
extern int D_8028AA20;
extern int D_8028AA24;
void BrFatal(char *msg);
void osWritebackDCacheAll(void);
void osRecvMesg(void *mq, void *msg, int flag);
void osViSetMode(void *mode);
void osViSwapBuffer(void *fb);
void *osViGetCurrentFramebuffer(void);
void BrPerfFrameStart(void);
unsigned int osGetCount(void);
void osSpTaskLoad(BrTask *t);
void osSpTaskStartGo(BrTask *t);
/* -- end declarations -- */

/* WHAT IT DOES: Start a frame at the given resolution (0 low, 1 high):
 * lay out the players' views, reset the frame counters and pools, choose
 * the frame buffers when the resolution has changed, and open this frame's
 * display list with the fixed RDP and RSP state: pipe sync, full-screen
 * scissor, the render and combine modes, texture filtering and dither,
 * geometry flags, the viewport, texturing off, the colour image and the
 * depth image; then set the VI's dither and gamma features.  The high-res
 * frame buffers sit below the low-res pair, computed from the two link
 * symbols as integers.
 * RESIDUE (198): register naming only -- the instruction sequence is the
 * ROM's with registers ignored; every temporary is one number later from
 * the first block on, and the view table base lands in v1, not v0.  200
 * permuter compiles do not move it. */
/* @implements 0x80219470 tgr BrFrameBegin */
void BrFrameBegin(int hires)
{
  int x;
  int w;
  int h;

  if (hires != D_8028A850) {
    D_8028A84C = 1;
    D_8028A850 = hires;
  }
  BrPerfMark(0, 0, 0x82, 0, 0xff);
  switch (D_8028AB0C) {
  case 1:
    x = 8;
    D_8031B2C8[0].x = x;
    D_8031B2C8[0].y = x;
    D_8031B2C8[0].w = D_8028AAB0 - 16;
    D_8031B2C8[0].h = D_8028AAB4 - 16;
    break;
  case 2:
    x = 8;
    D_8031B2C8[1].x = x;
    D_8031B2C8[1].y = (D_8028AAB4 >> 1) + 1;
    w = D_8028AAB0;
    h = (D_8028AAB4 >> 1) - 8;
    D_8031B2C8[1].h = h;
    w -= 0x60;
    D_8031B2C8[1].w = w;
    D_8031B2C8[0].x = x;
    D_8031B2C8[0].y = x;
    D_8031B2C8[0].w = w;
    D_8031B2C8[0].h = h;
    break;
  }
  BrFrameStatsReset();
  BrFramePoolsReset();
  if (D_8028A84C != 0) {
    if (!(D_8028A84C - 1)) {
      if (D_8028A850 != 0) {
        D_8031AA28[0] = (unsigned int)D_801B5000 - ((unsigned int)D_801DA800 - (unsigned int)D_801B5000) * 6;
        D_8031AA28[1] = (unsigned int)D_801B5000 - ((unsigned int)D_801DA800 - (unsigned int)D_801B5000) * 2;
      } else {
        D_8031AA28[0] = (unsigned int)D_801B5000;
        D_8031AA28[1] = (unsigned int)D_801DA800;
      }
    }
  }
  D_8028A858 = &D_8028A848[D_8028A85C][0x40];
  if (D_8028AA68 != 0) {
    D_8028A898 = 0x2000;
  } else {
    D_8028A898 = 0;
  }
  D_8028A8A0 = 0x40;
  D_8028A89C = 0;
  gRaw(D_8028A858++, 0xbc000006, 0);
  gRaw(D_8028A858++, 0xe7000000, 0);
  BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);
  gRaw(D_8028A858++, 0xfcffffff, 0xfffdf638);
  gRaw(D_8028A858++, 0xba001001, 0);
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001102, 0);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
  gRaw(D_8028A858++, 0xba000c02, D_8028A898);
  gRaw(D_8028A858++, 0xba000903, 0xc00);
  gRaw(D_8028A858++, 0xba000801, 0);
  gRaw(D_8028A858++, 0xb9000002, 1);
  gRaw(D_8028A858++, 0xb900031d, 0xf0a4000);
  gRaw(D_8028A858++, 0xba000602, D_8028A8A0);
  gRaw(D_8028A858++, 0xba000602, D_8028A89C);
  gRaw(D_8028A858++, 0xba001402, 0);
  gRaw(D_8028A858++, 0xf9000000, 0);
  gRaw(D_8028A858++, 0x1020040, D_0028A8C0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xb6000000;
    _g->words.w1 = (unsigned int)(0x1f3204);
  }
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xb7000000;
    _g->words.w1 = (unsigned int)(0x2000);
  }
  if (D_8028AA50 != 0) {
    gRaw(D_8028A858++, 0xb7000000, 0x800000);
  } else {
    gRaw(D_8028A858++, 0xb6000000, 0x800000);
  }
  gRaw(D_8028A858++, 0x6000000, D_8028A900 + D_8028A8A4 * 0x28);
  gRaw(D_8028A858++, 0xbb000000, 0);
  gDPSetColorImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8028AAB0 << D_8028A850, D_8031AA28[D_8028A85C] + 0x80000000);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = 0xfe000000;
    _g->words.w1 = (unsigned int)(D_00000400);
  }
  osViSetSpecialFeatures(0x40);
  osViSetSpecialFeatures(0x10);
  osViSetSpecialFeatures(D_8028AA4C != 0 ? 1 : 2);
}

/* WHAT IT DOES: Finish the frame and hand it to the RSP: close the display
 * list, fill in this buffer's graphics task (F3DEX FIFO microcode, the
 * list, the RDP FIFO), record the frame's list, matrix and vertex counts
 * (a list over 6000 commands is fatal); unless this is the first frame,
 * wait for the previous task's RSP and RDP to finish (running the pending
 * callback), optionally copy the debug frame, set the VI mode once after a
 * resolution change, swap to the finished frame buffer and wait for it,
 * lift the screen blanking; then start the task and flip buffers.
 * RESIDUE: same size class, not aligned -- the ROM builds the list size as
 * (list - base) + -(frame * 48000) - 0x200 (a negu), CSEs 48000 into a
 * register, and computes the microcode address twice; the task pointer
 * lives on the stack as here.  Not yet matched. */
/* @implements 0x8021AA08 tgr BrFrameEnd */
void BrFrameEnd(void)
{
  static int started = 0;         /* 0x8028AB80: a frame has been started before */
  BrTask *t;
  int n;
  int m;
  int v;
  int mul;
  int *src;
  int *dst;
  int i;
  void *fb;

  gRaw(D_8028A858++, 0xe9000000, 0);
  gRaw(D_8028A858++, 0xb8000000, 0);
  t = &D_8031A9A8[D_8028A85C];
  t->type = 1;
  t->ucode = D_8026EAD0;
  t->ucode_data = D_802ABC00;
  t->flags = 6;
  t->output_buff = D_8028A860[0];
  t->ucode_size = 0x1000;
  t->output_buff_size = D_8028A860[1] - D_8028AB84;
  t->ucode_data_size = 0x800;
  t->dram_stack = (void *)(((unsigned int)D_8031A598 + 0xf) & ~0xf);
  t->dram_stack_size = 0x400;
  t->ucode_boot = D_8026EA00;
  t->ucode_boot_size = (unsigned int)D_8026EAD0 - (unsigned int)D_8026EA00;
  t->data_ptr = &D_8028A848[D_8028A85C][0x40];
  t->data_size = (D_8028A858 - &D_8028A848[D_8028A85C][0x40]) * sizeof(Gfx);
  n = D_8028A858 - &D_8028A848[D_8028A85C][0x40];
  if (D_8028AB7C < n) {
    D_8028AB7C = n;
  }
  m = (D_8028C75C - D_8028C760) >> 1;
  if (D_8028AB74 < m) {
    D_8028AB74 = m;
  }
  v = D_8028C764 - D_8028C768;
  if (D_8028AB78 < v) {
    D_8028AB78 = v;
  }
  D_8028AB70 = n;
  if (n > 6000) {
    BrFatal("HUGE GLIST ERROR");
  }
  osWritebackDCacheAll();
  if (started != 0) {
    BrPerfMark(0, 0, 0, 0, 0xff);
    osRecvMesg(D_8031A320, 0, 1);
    BrPerfMark(0, 0xff, 0xff, 0, 0xff);
    if (D_8031B31C != 0) {
      D_8031B31C();
      D_8031B31C = 0;
    }
    BrPerfMark(0, 0, 0, 0, 0xff);
    osRecvMesg(D_8031A358, 0, 1);
    if (D_8028AA94 != 0) {
      mul = 1;
      if (D_8028A850 != 0) {
        mul = 4;
      }
      src = D_80000400;
      dst = (int *)D_8031AA28[D_8028A85C ^ 1];
      n = mul * D_8028AAB0 * D_8028AAB4 >> 1;
      for (i = 0; i < n; i++) {
        dst[i] = src[i];
      }
    }
    if (D_8031B320 != 0) {
      D_8031B31C();
      D_8031B31C = 0;
    }
    osWritebackDCacheAll();
    D_8028AAE8 = osGetCount();
    D_8028AAE0 = D_8028AAE8 - D_8028AAE4;
    if (D_8028A84C != 0) {
      if (--D_8028A84C == 0) {
        if (D_8028A850 != 0) {
          if (osTvType == 2) {
            osViSetMode(D_802A6040);
          } else {
            osViSetMode(D_802A5780);
          }
        } else {
          if (osTvType == 2) {
            osViSetMode(D_802A5D70);
          } else {
            osViSetMode(D_802A54B0);
          }
        }
        osViBlack(1);
        D_8028A854 = D_8028A850;
      }
    }
    BrPerfMark(1, 0x20, 0x20, 0x20, 0xff);
    BrPerfMark(2, 0x20, 0x20, 0x20, 0xff);
    BrPerfMark(0, 0x20, 0x20, 0x20, 0xff);
    fb = (void *)D_8031AA28[D_8028A85C ^ 1];
    osRecvMesg(D_8031A390, 0, 1);
    osViSwapBuffer(fb);
    osRecvMesg(D_8031A390, 0, 1);
    if (osViGetCurrentFramebuffer() != fb) {
      osRecvMesg(D_8031A390, 0, 1);
    }
    if (D_8028A884 == 0 && D_8028A888 == 0) {
      osViBlack(0);
    }
    if (D_8028A888 != 0) {
      D_8028A888--;
    }
    BrPerfFrameStart();
    D_8028AAE4 = osGetCount();
    D_8028AAE8 = osGetCount() - D_8028AAE8;
  } else {
    started++;
  }
  BrPerfMark(0, 200, 0, 200, 0xff);
  D_8028AA24 = D_8028AA20;
  BrPerfMark(2, 200, 0, 0, 0xff);
  BrPerfMark(1, 200, 100, 0, 0xff);
  osSpTaskLoad(t);
  osSpTaskStartGo(t);
  D_8028A85C ^= 1;
}

/* WHAT IT DOES: Start building a new frame using the first of the two
 * screen-buffer layouts. */
/* @implements 0x80219A1C tgr BrFrameBeginLayout0 */
void BrFrameBeginLayout0(void)
{
  BrFrameBegin(0);
}

/* WHAT IT DOES: Start building a new frame using the second of the two
 * screen-buffer layouts. */
/* @implements 0x80219A3C tgr BrFrameBeginLayout1 */
void BrFrameBeginLayout1(void)
{
  BrFrameBegin(1);
}

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
    p[0] = 0xff100000 | (((D_8028AAB0 << D_8028A850) - 1) & 0xfff);
    p[1] = (unsigned int)D_00000400;
  }
  {
    unsigned int *p = (unsigned int *)D_8028A858++;
    p[0] = 0xf7000000;
    p[1] = GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0);
  }
  gDPFillRectangle(D_8028A858++, 0, 0, (D_8028AAB0 << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
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
  p[0] = 0xba001402;
  p[1] = G_CYC_FILL;
  c = GPACK_RGBA5551(r, g, b, 1);
  gDPSetFillColor(D_8028A858++, c | c << 16);
  gDPFillRectangle(D_8028A858++, 0, 0, (D_8028AAB0 << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  {
    unsigned int *q = (unsigned int *)D_8028A858++;
    q[0] = 0xba001402;
    q[1] = G_CYC_1CYCLE;
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
  p[0] = 0xba001402;
  p[1] = G_CYC_FILL;
  gDPSetFillColor(D_8028A858++, c | c << 16);
  gDPFillRectangle(D_8028A858++, x, y, ((x + w) << D_8028A850) - 1, ((y + h) << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  p = (unsigned int *)D_8028A858++;
  p[0] = 0xba001402;
  p[1] = G_CYC_1CYCLE;
}

/* WHAT IT DOES: Does nothing: a variadic debug hook compiled empty (it
 * still spills its register arguments).  The PC twin is BrStub10008B80. */
/* @implements 0x80219A5C tgr BrStub80219A5C */
void BrStub80219A5C(int a0, ...)
{
}

/* WHAT IT DOES: Set the RDP scissor to a w by h box at (x, y), clipped to
 * the current clip rectangle, in 320-wide coordinates doubled on a hi-res
 * screen. */
/* @implements 0x80219A78 tgr BrScissorSet */
void BrScissorSet(int x, int y, int w, int h)
{
  if (x < D_8028B740) {
    w = w - D_8028B740 + x;
    x = D_8028B740;
  }
  if (x + w > D_8028B744) {
    w = D_8028B744 - x;
  }
  if (w < 0) {
    w = 0;
  }
  if (y < D_8028B748) {
    h = h - D_8028B748 + y;
    y = D_8028B748;
  }
  if (y + h > D_8028B74C) {
    h = D_8028B74C - y;
  }
  if (h < 0) {
    h = 0;
  }
  if (D_8028A850 != 0) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  gDPPipeSync(D_8028A858++);
  gDPSetScissor(D_8028A858++, G_SC_NON_INTERLACE, x, y, x + w, y + h);
}

/* WHAT IT DOES: Push two empty black frames through the second screen
 * layout with the video output blanked, so a mode change starts from a
 * clean screen. */
/* @implements 0x8021A4C8 tgr BrScreenFlush2Layout1 */
void BrScreenFlush2Layout1(void)
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
  D_8028A884 = 0;
}

/* WHAT IT DOES: Push two empty black frames through the first screen layout
 * with the video output blanked, so a mode change starts from a clean
 * screen. */
/* @implements 0x8021A540 tgr BrScreenFlush2Layout0 */
void BrScreenFlush2Layout0(void)
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
  D_8028A884 = 0;
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

