/* br_rdpmode.c -- drawing: putting the rasteriser into a drawing mode.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  What is here is
 * the run of display-list commands a pass emits before it draws anything --
 * pipeline sync, render mode, blend colour, the colour combiner -- and the
 * combiner-word builders those commands are made of.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int *DAT_106e7710;
extern int DAT_106e72e8;
extern int DAT_106e7718;
extern int DAT_106e79b0;
extern int DAT_106ed6b0;
extern int DAT_1184c478;
extern int DAT_100ba2d0;
extern int DAT_10396efc;
int FUN_10011300(int, int, int, int, int);
int FUN_10010fb0(int);

/* WHAT IT DOES: emit the fixed run of display-list commands that puts the
 * renderer into the standard drawing mode for a pass -- pipeline sync,
 * render mode, blend colour and the colour-combiner setup, in that order. */
/* @implements 0x100119C0 glide FUN_100119c0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_100119c0(int param_1, int param_2)
{
  int *p_;

  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001402; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xbb000001; p_[1] = 0xffffffff; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000c02; p_[1] = DAT_106e72e8; }
  p_ = DAT_106e7710;
  DAT_106e7710 = DAT_106e7710 + 2;
  BrRdpSetCombineLERP(p_, 0, 0, 0, 0x3eb, 0x3e9, 0, 0x3eb, 0, 0, 0, 0, 0x3eb, 0x3e9, 0, 0x3eb, 0);
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = DAT_1184c478 & 0xffffff | 0xdc000000; p_[1] = 1; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xfd900000; p_[1] = (int)&DAT_100ba2d0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf5900000; p_[1] = 0x7018060; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe6000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf3000000; p_[1] = 0x77ff100; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf5881000; p_[1] = 0x18060; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xf2000000; p_[1] = 0xfc0fc; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000e02; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001301; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000201; p_[1] = 4; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000602; p_[1] = 0xc0; }
  if (DAT_106ed6b0 != 0) {
    { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000402; p_[1] = 0x80; }
    { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb900031d; p_[1] = 0x504b50; }
    FUN_10011300(param_1, DAT_10396efc & 0xffff, 0xe0, 0xe0, 0xff);
  }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xe7000000; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba001301; p_[1] = 0x80000; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000201; p_[1] = 0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000602; p_[1] = DAT_106e7718; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xba000402; p_[1] = DAT_106e79b0; }
  { p_ = DAT_106e7710; DAT_106e7710 = DAT_106e7710 + 2; *p_ = 0xb9000002; p_[1] = 1; }
  FUN_10010fb0(param_2);
}

#endif /* BR_MATCHING_BUILD */
