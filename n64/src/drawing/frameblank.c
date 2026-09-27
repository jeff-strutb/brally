/* frameblank.c -- starting a frame, and flushing the screen to black between modes
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void func_80219470(unsigned int param_1);
void BrScreenClear(int r, int g, int b);
void BrFrameBeginLayout1(void);
int func_8021AA08();
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
/* -- end declarations -- */

/* WHAT IT DOES: Start building a new frame using the first of the two
 * screen-buffer layouts. */
/* @implements 0x80219A1C tgr BrFrameBeginLayout0 */
void BrFrameBeginLayout0(void)
{
  func_80219470(0);
}

/* WHAT IT DOES: Start building a new frame using the second of the two
 * screen-buffer layouts. */
/* @implements 0x80219A3C tgr BrFrameBeginLayout1 */
void BrFrameBeginLayout1(void)
{
  func_80219470(1);
}

/* WHAT IT DOES: Clear the whole screen to one colour (fill mode,
 * RGBA5551).
 * RESIDUE (4): both gDPSetCycleType commands store w1 before w0; the ROM
 * stores w0 first. */
/* @implements 0x80217FB8 tgr BrScreenClear */
void BrScreenClear(int r, int g, int b)
{
  unsigned short c;

  gDPPipeSync(D_8028A858++);
  gDPSetRenderMode(D_8028A858++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  c = GPACK_RGBA5551(r, g, b, 1);
  gDPSetFillColor(D_8028A858++, c | c << 16);
  gDPFillRectangle(D_8028A858++, 0, 0, (D_8028AAB0 << D_8028A850) - 1, (D_8028AAB4 << D_8028A850) - 1);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
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
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  func_8021AA08();
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
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  func_8021AA08();
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
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout1();
  BrScreenClear(0,0,0);
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout1();
  func_8021AA08();
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
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout0();
  BrScreenClear(0,0,0);
  func_8021AA08();
  osViBlack(1);
  BrFrameBeginLayout0();
  func_8021AA08();
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

