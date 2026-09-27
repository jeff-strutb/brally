/* perfmeter.c -- the performance meter: per-frame timing marks for three
 * bars, double-buffered so one frame is drawn while the next is recorded
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrPerfEntry {
  unsigned int colour;          /* fill colour of the segment ending here */
  int time;                     /* CPU count since the frame began */
} BrPerfEntry;
unsigned int osGetCount(void);
extern int D_8028BDA0;
extern unsigned long long D_8028BDA8;
extern int D_803519B0[2][3];
extern BrPerfEntry D_8034E9B0[2][3][256];
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
