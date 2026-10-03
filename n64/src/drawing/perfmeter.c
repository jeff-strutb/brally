/* perfmeter.c -- the performance meter: per-frame timing marks for three
 * bars, double-buffered so one frame is drawn while the next is recorded
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrPerfEntry {
  unsigned int colour;          /* fill colour of the segment ending here */
  unsigned int time;            /* CPU count since the frame began */
} BrPerfEntry;
unsigned int osGetCount(void);
extern int D_8028BDA0;
unsigned long long D_8028BDA8 = 0;
extern int D_803519B0[2][3];
extern BrPerfEntry D_8034E9B0[2][3][256];
extern Gfx *D_8028A858;
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028AAB4;                  /* and height */
extern int D_8028A850;                  /* the frame buffer's scale shift (1 in high resolution) */
extern int D_8028A85C;                  /* the frame buffer being drawn */
extern unsigned int D_8031AA28[];      /* the frame buffers */
extern unsigned int D_8028A890;         /* the render mode to restore */
extern unsigned int D_8028A894;
void BrScissorSet(int x, int y, int w, int h);
/* -- end declarations -- */

/* WHAT IT DOES: Record a timing mark on one of the performance meter's
 * three bars: the time since the frame began, and the RGBA5551 colour (in
 * both halves of the word, as a fill colour) of the segment it ends. */
/* @implements 0x8022D7E0 tgr BrPerfMark */
void BrPerfMark(int bar, int r, int g, int b, int a)
{
  BrPerfEntry *e;
  unsigned int colour;

  e = &D_8034E9B0[D_8028BDA0][bar][D_803519B0[D_8028BDA0][bar]++];
  colour = (r << 8) & 0xf800 | (g << 3) & 0x7c0 | (b >> 2) & 0x3e | (a >> 7) & 1;
  colour |= colour << 16;
  e[1].colour = colour;
  e->time = osGetCount() - D_8028BDA8;
}

/* WHAT IT DOES: Start a new frame on the performance meter: close each bar
 * with a magenta mark, swap to the other buffer, note the CPU count as the
 * frame's start, and empty the three bars. */
/* @implements 0x8022D8AC tgr BrPerfFrameStart */
void BrPerfFrameStart(void)
{
  int i;

  for (i = 0; i < 3; i++) {
    BrPerfMark(i, 0xff, 0, 0xff, 0x7f);
  }
  D_8028BDA0 ^= 1;
  D_8028BDA8 = osGetCount();
  for (i = 0; i < 3; i++) {
    D_803519B0[D_8028BDA0][i] = 0;
    D_8034E9B0[D_8028BDA0][i][0].time = 0;
  }
}

/* WHAT IT DOES: Draw the performance meter at the right edge of the screen
 * from the last frame's marks: find the longest bar, let the scale follow
 * it (growing at once, shrinking a frame-time step at a time after 30 quiet
 * frames, never under 2350000 counts), then a dark frame, a white tick every
 * frame time (781250 counts), and each bar's segments in their colours.
 * The scale state is function static (the ROM addresses it afresh at every
 * access); the local bounds and row pointers are what let IDO unroll both
 * inner loops by four as the ROM has them.
 * RESIDUE (717): the ROM keeps the first loop's counters and row pointer in
 * s4/s6/s7/fp (it saves two more registers, frame 0x40) where ours uses
 * temporaries, which renames everything after.
 * NEVER RUN IN THE RETAIL GAME: its five callers (BrRaceTick, BrIntroScreen,
 * BrMenu, BrCarSelect, 0x80243260) draw it only while D_8028AA98 >= 2.
 * That word is 0 in the ROM's .data, no instruction forms its address
 * (n64rom xref: reads only, at those five sites), no data word points at
 * it, and a write watch over all 64 box scripts saw no store to it. */
/* @t4-pass 0x8022D97C 1 2026-09-29 compiles 119 best 717 moved 5  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022D97C 2 2026-09-29 compiles 119 best 717 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022D97C tgr BrPerfMeterDraw */
void BrPerfMeterDraw(void)
{
  static int hold = 0;              /* 0x8028BDB0: frames before the scale may shrink */
  static unsigned int scale;        /* 0x803519C8: the meter's full scale, CPU counts */
  unsigned int max;
  unsigned int t;
  int h;
  int bar;
  int j;
  int n;
  BrPerfEntry *e;

  gDPPipeSync(D_8028A858++);
  BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetColorImage(D_8028A858++, G_IM_FMT_RGBA, G_IM_SIZ_16b, D_8028AAB0 << D_8028A850,
                   D_8031AA28[D_8028A85C] + 0x80000000);
  max = 0;
  for (bar = 0; bar < 3; bar++) {
    n = D_803519B0[D_8028BDA0 ^ 1][bar];
    e = D_8034E9B0[D_8028BDA0 ^ 1][bar];
    for (j = 1; j < n; j++) {
      if (max < e[j].time) {
        max = e[j].time;
      }
    }
  }
  if (max < scale + 7812) {
    if (hold != 0) {
      hold--;
      max = scale;
    } else if (scale > 1736110) {
      scale -= 781250;
      if (max >= scale + 7812) {
        hold = 30;
        scale = max;
      } else {
        max = scale;
        hold = 30;
      }
    }
  } else {
    scale = max;
    hold = 30;
  }
  if (max < 1700000) {
    max = 2350000;
  }
  h = ((D_8028AAB4 << D_8028A850) * 208) / 240;
  gDPSetFillColor(D_8028A858++, 0x00010001);
  gDPFillRectangle(D_8028A858++, ((D_8028AAB0 - 16) << D_8028A850) - 22, (16 << D_8028A850) - 2,
                   ((D_8028AAB0 - 16) << D_8028A850) - 3, (16 << D_8028A850) + h + 2);
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, 0xFFFFFFFF);
  for (t = 0; t < max; t += 781250) {
    gDPFillRectangle(D_8028A858++, ((D_8028AAB0 - 16) << D_8028A850) - 21, (16 << D_8028A850) + t * h / max,
                     ((D_8028AAB0 - 16) << D_8028A850) - 20, (16 << D_8028A850) + t * h / max);
  }
  gDPPipeSync(D_8028A858++);
  for (bar = 0; bar < 3; bar++) {
    e = D_8034E9B0[D_8028BDA0 ^ 1][bar];
    n = D_803519B0[D_8028BDA0 ^ 1][bar];
    for (j = 1; j < n; j++) {
      gDPSetFillColor(D_8028A858++, e[j].colour);
      gDPFillRectangle(D_8028A858++, ((D_8028AAB0 - 16) << D_8028A850) + bar * 4 - 18,
                       e[j - 1].time * h / max + (16 << D_8028A850),
                       ((D_8028AAB0 - 16) << D_8028A850) + bar * 4 - 16,
                       e[j].time * h / max + (16 << D_8028A850));
      gDPPipeSync(D_8028A858++);
    }
  }
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gDPSetRenderMode(D_8028A858++, D_8028A890, D_8028A894);
}
