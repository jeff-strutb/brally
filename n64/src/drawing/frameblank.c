/* frameblank.c -- starting a frame, and flushing the screen to black between modes
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80219470(unsigned int param_1);
void func_80217FB8(unsigned int param_1,unsigned int param_2,int param_3);
void func_80219A3C(void);
int func_8021AA08();
void func_80260AB0(char param_1);
extern int D_8028A884;
void func_80219A1C(void);
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

/* WHAT IT DOES: Push two empty black frames through the second screen
 * layout with the video output blanked, so a mode change starts from a
 * clean screen. */
/* @implements 0x8021A4C8 tgr BrScreenFlush2Layout1 */
void BrScreenFlush2Layout1(void)
{
  D_8028A884 = 1;
  func_80260AB0(1);
  func_80219A3C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A3C();
  func_80217FB8(0,0,0);
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
  func_80260AB0(1);
  func_80219A1C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A1C();
  func_80217FB8(0,0,0);
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
  func_80260AB0(1);
  func_80219A3C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A3C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A3C();
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
  func_80260AB0(1);
  func_80219A1C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A1C();
  func_80217FB8(0,0,0);
  func_8021AA08();
  func_80260AB0(1);
  func_80219A1C();
  func_8021AA08();
  D_8028A884 = 0;
}
