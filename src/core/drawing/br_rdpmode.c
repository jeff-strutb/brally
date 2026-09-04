/* br_rdpmode.c -- drawing: putting the rasteriser into a drawing mode.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  What is here is
 * the run of display-list commands a pass emits before it draws anything --
 * pipeline sync, render mode, blend colour, the colour combiner -- and the
 * combiner-word builders those commands are made of.
 *
 * ‼ br_gamestep.h IS LOAD-BEARING and must not be dropped as an unused
 * include.  It declares nothing this file calls, yet without it 0x1001CF90
 * loses its match outright -- same 448 bytes, 142 of them different, i.e. a
 * wholesale re-colouring.  slice1_05.c, where these functions came from,
 * included it; 0x10019210 in br_vtxcache.c behaves the same way.  Measured
 * both directions on a one-file sweep, twice.  The mechanism is not
 * established: the header has no includes, no pragmas and no macros beyond
 * one enum constant, so the only thing it changes is the set of names in the
 * TU's symbol table.
 */
#include "slice1_05.h"     /* BrGfxWords */
#include "br_gamestep.h"

#include <stddef.h>

/* ================================================================== */
/* 3. RDP colour combiner                                             */
/* ================================================================== */

/* 0x1002FAF0 */
/* WHAT IT DOES: translates one named colour ingredient -- the texture, the
 * shading, a flat colour, and so on -- into the number the graphics hardware
 * uses for it. Plain zero and plain one are special-cased; everything else is
 * just an offset. */
/* @implements 0x1002FAF0 d3d BrRdpCCMux */
int BrRdpCCMux(int token)
{
    /* orig 0x1002FAF0: `sub eax, 0` / `dec eax` is MSVC's consecutive
     * switch on cases 0 and 1, not a pair of ifs. */
    switch (token) {
    case 0:
        return 31;              /* G_CCMUX_0 */
    case 1:
        return 6;               /* G_CCMUX_1 */
    default:
        return token - 1000;
    }
}

/* 0x1002FAC0 */
/* WHAT IT DOES: the same translation as its neighbour above, but for the
 * transparency channel, where the hardware numbers the ingredients differently
 * and one of them is not available at all. */
/* @implements 0x1002FAC0 d3d BrRdpACMux */
int BrRdpACMux(int token)
{
    /* orig 0x1001D150: `sub eax,0; je; dec; je; sub eax,0x3f4; je` is the
     * consecutive-case switch 0 / 1 / 1013, same shape as BrRdpCCMux. */
    switch (token) {
    case 0:
        return 7;               /* G_ACMUX_0 */
    case 1:
        return 6;               /* G_ACMUX_1 */
    case 1013:
        return 0;               /* LOD_FRACTION: 13 in colour, 0 in alpha */
    default:
        return token - 1000;
    }
}

/* 0x1002F900 */
/* WHAT IT DOES: builds the single instruction that tells the graphics hardware
 * how to mix its ingredients together to get a pixel's colour -- which of the
 * texture, the lighting, the flat colours and so on go into each of the two
 * mixing stages, for colour and for transparency alike. Sixteen choices are
 * squeezed into two words, and the instruction's own identifying byte falls out
 * of the packing rather than being written in. */
/* @implements 0x1002F900 d3d BrRdpSetCombineLERP */
void BrRdpSetCombineLERP(BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    /* Orig converts every argument first (a0..d0 live in ebx/esi/edi/ebp
     * across the remaining calls; the rest overwrite their own stack slots)
     * and only then packs. Interleaved convert-and-shift is 40 bytes short
     * and never pushes ebp. */
    a0  = BrRdpCCMux(a0);
    b0  = BrRdpCCMux(b0);
    c0  = BrRdpCCMux(c0);
    d0  = BrRdpCCMux(d0);
    Aa0 = BrRdpACMux(Aa0);
    Ab0 = BrRdpACMux(Ab0);
    Ac0 = BrRdpACMux(Ac0);
    Ad0 = BrRdpACMux(Ad0);
    a1  = BrRdpCCMux(a1);
    b1  = BrRdpCCMux(b1);
    c1  = BrRdpCCMux(c1);
    d1  = BrRdpCCMux(d1);
    Aa1 = BrRdpACMux(Aa1);
    Ab1 = BrRdpACMux(Ab1);
    Ac1 = BrRdpACMux(Ac1);
    Ad1 = BrRdpACMux(Ad1);

    /* The 0xFC command byte comes out of this ones-fill after the shifts
     * total exactly 20 bits; it is never OR'd in explicitly. b0 is the one
     * field not masked; the 28-bit total shift discards the excess. */
    pOut->w0 = ((((((a0 & 0x0F) | 0xFFFFFFC0) << 5 | (c0 & 0x1F)) << 3
                  | (Aa0 & 7)) << 3 | (Ac0 & 7)) << 4 | (a1 & 0x0F)) << 5
               | (c1 & 0x1F);
    pOut->w1 = ((((((((b0 << 4 | (b1 & 0x0F)) << 3 | (Aa1 & 7)) << 3
                     | (Ac1 & 7)) << 3 | (d0 & 7)) << 3 | (Ab0 & 7)) << 3
                   | (Ad0 & 7)) << 3 | (d1 & 7)) << 3 | (Ab1 & 7)) << 3
               | (Ad1 & 7);
}

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
