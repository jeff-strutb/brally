/* br_framebegin.c -- drawing: opening a frame.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_18.c, an address batch and not a module.  This is what
 * runs at the top of every frame: reset the write pointer into the frame's
 * command buffer, lay down the fixed preamble of display-list commands, and
 * set the clipping rectangle that keeps split-screen halves apart.
 *
 * 0x1002BF4B and 0x1002BF50 are ADJACENT in the original -- the five-byte
 * empty function ends exactly where the scissor setter begins -- which is
 * what pins them to one translation unit.
 *
 * slice2_18.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "slice2_18.h"

/* 0x10032873 */
/* WHAT IT DOES: starts a frame at normal resolution: it resets the drawing-
 * command cursor to this frame's buffer and lays down the fixed block of
 * commands every frame opens with -- scissor, blend and combine setup,
 * geometry switches, the identity matrix and the viewport. */
/* @implements 0x10032873 d3d BrFrameBeginRec */
void BrFrameBeginRec(int32_t *pRec)
{
    BrFrameBegin(pRec, 0);
}

/* 0x10032886 */
/* WHAT IT DOES: starts a frame at the high resolution instead, using the
 * game's own frame record. Switching resolution mid-run is noticed and
 * reloads the state that everything downstream keys off when it halves or
 * doubles a rectangle. */
/* @implements 0x10032886 d3d BrFrameBeginHiRes */
void BrFrameBeginHiRes(void)
{
    BrFrameBegin(BrG_6C1628, 1);
}

#ifdef BR_MATCHING_BUILD
extern int DAT_106ed674;
extern int DAT_106ed670;
extern int DAT_100aa044;
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_106e9a2c;
extern int DAT_106e7714;
extern int DAT_106e79d4;
extern int DAT_106ed67c;
extern int DAT_106e72e8;
extern int DAT_100aa020;
extern int DAT_106e7718;
extern int DAT_106e79b0;
extern char DAT_100a9ec0;
extern char DAT_100a9f00;
extern int DAT_106ed68c;
extern int DAT_100aa014;
extern int DAT_106ed694;
int FUN_10008d60();
int FUN_1002bf50();
int FUN_1001cf90();
int FUN_1002a8d7();
int FUN_100625f0();

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002BF4B glide BrNop_1002BF4B */

void BrNop_1002BF4B(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002C509 glide BrNop_1002C509 */

void BrNop_1002C509(void)

{
  return;
}

/* WHAT IT DOES: build the frame-opening display list: reset the write pointer into this
 * frame's 96000-byte command buffer, then emit the fixed F3D-style preamble (segment,
 * sync, viewport via 0x1001CF90, othermode/geometry-mode settings, fog, the 0x28-stride
 * palette DL at 0x100A9F00) and the three mode pokes through 0x10008D60. The command
 * stream is a struct {op,arg} and every emit POST-INCREMENTS the global pointer -- that
 * is what puts each emit's temp in its own /Od stack slot and the two compiler temps
 * (switch selector, post-inc copy) at the frame bottom. */
/* @implements 0x1002B997 glide BrFrameBeginDl */

typedef struct BrDlCmd { int op; int arg; } BrDlCmd;
extern BrDlCmd *DAT_106e7710;

#define BR_EMIT(c,a) { \
  BrDlCmd *p_ = DAT_106e7710++; \
  p_->op = (c); \
  p_->arg = (a); }

void BrFrameBeginDl(int *param_1,int param_2)
{
  if (param_2 ^ DAT_106ed674) {
    DAT_106ed670 = 1;
    DAT_106ed674 = param_2;
  }
  FUN_10008d60(0,0,0x82,0,0xff);
  switch (DAT_100aa044) {
  case 1:
    *param_1 = 0;
    param_1[1] = 0;
    param_1[2] = DAT_100a7514;
    param_1[3] = DAT_100a7518;
    break;
  case 2:
    param_1[0x16] = 8;
    param_1[0x17] = (DAT_106e9a2c >> 1) + 1;
    param_1[0x18] = DAT_106e7714 + -0x60;
    param_1[0x19] = (DAT_106e9a2c >> 1) + -8;
    *param_1 = 8;
    param_1[1] = 8;
    param_1[2] = DAT_106e7714 + -0x60;
    param_1[3] = (DAT_106e9a2c >> 1) + -8;
    break;
  }
  FUN_1002a8d7();
  FUN_100625f0();
  DAT_106e7710 = (BrDlCmd *)(DAT_106e79d4 + DAT_106ed67c * 96000 + 0x200);
  DAT_106e72e8 = DAT_100aa020 ? 0x2000 : 0;
  DAT_106e7718 = 0x40;
  DAT_106e79b0 = 0;
  BR_EMIT(0xbc000006, 0)
  BR_EMIT(0xe7000000, 0)
  FUN_1002bf50(0,0,DAT_106e7714,DAT_106e9a2c);
  FUN_1001cf90(DAT_106e7710++,0,0,0,0x3eb,0,0,0,0x3eb,0,0,0,1000,0,0,0,1000);
  BR_EMIT(0xba001001, 0)
  BR_EMIT(0xba000e02, 0)
  BR_EMIT(0xba001102, 0)
  BR_EMIT(0xba001301, 0x80000)
  BR_EMIT(0xba000c02, DAT_106e72e8)
  BR_EMIT(0xba000903, 0xc00)
  BR_EMIT(0xba000801, 0)
  BR_EMIT(0xb9000002, 1)
  BR_EMIT(0xb900031d, 0xf0a4000)
  BR_EMIT(0xba000602, DAT_106e7718)
  BR_EMIT(0xba000602, DAT_106e79b0)
  BR_EMIT(0xba001402, 0)
  BR_EMIT(0xf9000000, 0)
  BR_EMIT(0x1020040, (int)&DAT_100a9ec0)
  BR_EMIT(0xb6000000, 0x1f3204)
  BR_EMIT(0xb7000000, 0x2000)
  if (DAT_100aa014 != 0) {
    BR_EMIT(0xb7000000, 0x800000)
  }
  else {
    BR_EMIT(0xb6000000, 0x800000)
  }
  BR_EMIT(0x6000000, (int)(&DAT_100a9f00 + DAT_106ed68c * 0x28))
  BR_EMIT(0xbb000000, 0)
  FUN_10008d60(0x40);
  FUN_10008d60(0x10);
  FUN_10008d60(DAT_106ed694 ? 1 : 2);
  return;
}

/* WHAT IT DOES: sets the clipping rectangle for everything drawn after it --
 * how split-screen halves and mirror insets are kept from spilling over each
 * other. The rectangle is trimmed to the screen bounds first (only the SIZE is
 * trimmed at the far edges, so a fully off-screen rectangle still emits a
 * zero-size one rather than being dropped), then doubled if the hi-res flag is
 * set, which can push it back outside.
 *
 * 0x1002BF50 -- /Od, and it belongs to THIS translation unit: 0x1002BF4B
 * (BrNop_1002BF4B, 5 bytes) ends exactly at 0x1002BF50. It was transcribed in
 * slice5_62.c, an /O2 file, where the unoptimised frame could never match; the
 * body is the same, only the home and the reload-everything spelling differ.
 *
 * The four float round-trips are the original's own: `fild [arg]` into a
 * float32 temp, `fld` it back, `fmul` the 0x100774B4 scale, then __ftol. That
 * is an explicit `(float)` cast in the source, and it is lossy above 2^24, so
 * it is kept rather than folded into a direct fild-and-scale. */
/* @implements 0x1002BF50 glide BrSub_1003289F */

extern int   DAT_104b16b0;   /* minimum X */
extern int   DAT_104b16a8;   /* maximum X */
extern int   DAT_104b16b4;   /* minimum Y */
extern int   DAT_104b16a4;   /* maximum Y */
extern int   DAT_106ed674;   /* hi-res: double every coordinate */
extern float DAT_100774b4;   /* the fixed-point scale */

void BrSub_1003289F(int param_1,int param_2,int param_3,int param_4)

{
  BrDlCmd *piVar1;

  if (param_1 < DAT_104b16b0) {
    param_3 = param_3 - (DAT_104b16b0 - param_1);
    param_1 = DAT_104b16b0;
  }
  if (param_1 + param_3 > DAT_104b16a8) {
    param_3 = DAT_104b16a8 - param_1;
  }
  if (param_3 < 0) {
    param_3 = 0;
  }
  if (param_2 < DAT_104b16b4) {
    param_4 = param_4 - (DAT_104b16b4 - param_2);
    param_2 = DAT_104b16b4;
  }
  if (param_2 + param_4 > DAT_104b16a4) {
    param_4 = DAT_104b16a4 - param_2;
  }
  if (param_4 < 0) {
    param_4 = 0;
  }
  if (DAT_106ed674 != 0) {
    param_1 = param_1 * 2;
    param_2 = param_2 * 2;
    param_3 = param_3 * 2;
    param_4 = param_4 * 2;
  }
  piVar1 = DAT_106e7710++;
  piVar1->op = 0xe7000000;
  piVar1->arg = 0;
  {
    BrDlCmd *piVar2 = DAT_106e7710++;
    piVar2->op = (((int)((float)param_1 * DAT_100774b4) & 0xfff) << 12)
               | 0xe2000000
               | ((int)((float)param_2 * DAT_100774b4) & 0xfff);
    piVar2->arg = (((int)((float)(param_1 + param_3) * DAT_100774b4) & 0xfff) << 12)
                | ((int)((float)(param_2 + param_4) * DAT_100774b4) & 0xfff);
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
