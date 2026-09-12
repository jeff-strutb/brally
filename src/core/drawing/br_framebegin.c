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

/* ==========================================================================
 * 0x1002CEE9 -- closing the frame.  The counterpart of the openers above, in
 * the same /Od range (br_framedrive.c names it BrFrameEnd).
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
/* The per-frame task record: 0x40 bytes, two of them, selected by the frame
 * parity at 0x106ED67C. */
typedef struct {
    int   f00;
    int   f04;
    int   f08;
    int   f0C;
    int  *f10;
    int   f14;
    int  *f18;
    int   f1C;
    int  *f20;
    int   f24;
    int   f28;
    int   f2C;
    int   f30;
    int   f34;
    int   pad38[2];
} BrFrameTask;

extern BrFrameTask   DAT_106e8618[];    /* the two task records              */
extern int  DAT_118ee268[];
extern int  DAT_118ee278[];
extern int  DAT_100a9eb8;
extern int  DAT_100a9ebc;
extern int  DAT_106ed6f8;
extern int  DAT_106e9d90[];             /* the 16-byte-aligned scratch block */
extern int  DAT_106ed6f0;               /* high-water marks                  */
extern int  DAT_106ed6e8;
extern int  DAT_106ed6ec;
extern int *DAT_1035f7d8;
extern int *DAT_102e16b0;
extern int  DAT_1035faec;
extern int  DAT_1035fba4;
extern int  DAT_106e8200;               /* this frame's list length          */
extern int  DAT_106ed6f4;               /* frames drawn                      */
extern void (*DAT_106e8a1c)(void);      /* one-shot callback                 */
extern int  DAT_106e8698;
extern int  DAT_106e729c;               /* frame timing                      */
extern int  DAT_106e7298;
extern int  DAT_106e86b0;
extern int  DAT_106ed678;
extern int  DAT_100ad7c8;
extern int  DAT_106ed684;
extern int  DAT_106ed688;
extern int  DAT_106ec774;
extern int  DAT_106ed628;
extern void (*DAT_10b73530)(int);       /* the list submit hook              */
extern int  DAT_106ea430[];
extern int  DAT_106ea410[];
extern char DAT_10ac7380[], DAT_10ac6ac0[], DAT_10ac70b0[], DAT_10ac67f0[];
extern char s_HUGE_GLIST_ERROR_100aa2d4[];
void FUN_100385e0(int *, int, int);
void FUN_1002f26b(char *);
void FUN_100192d0(void);
int  FUN_10059f00(void);

/* WHAT IT DOES: closes the frame's display list.  It appends the two
 * terminating commands, fills in this frame's task record (buffers, sizes,
 * the list's start and rounded length), keeps the high-water marks of the
 * three command pools and aborts on an oversized list.  On every frame but
 * the first it then runs the frame-end work: the two debug overlays, the
 * one-shot callback, the frame timer, the mode-switch countdown with its
 * screen selection, the input step, and the timing bookkeeping.  Finally it
 * hands the list to the submit hook and flips the frame parity. */
/* @implements 0x1002CEE9 glide BrFrameEnd */
void BrFrameEnd(void)
{
    /* /Od slot order is the locals' NAME-HASH order, not declaration order:
     * these four names land rec at [ebp-4], len at [ebp-8], q1 at [ebp-0xc]
     * and q2 at [ebp-0x10]; (t, n, p, q) / (task, count, cmd, cmd2) /
     * (rec, len, p1, p2) all permute them (16 sets measured). */
    BrFrameTask  *rec;
    int           len;
    BrDlCmd      *q1;
    BrDlCmd      *q2;

    q1 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 1;
    q1->op = 0xE9000000;
    q1->arg = 0;
    q2 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 1;
    q2->op = 0xB8000000;
    q2->arg = 0;

    rec = &DAT_106e8618[DAT_106ed67c];
    rec->f00 = 1;
    rec->f10 = DAT_118ee268;
    rec->f18 = DAT_118ee278;
    rec->f04 = 2;
    rec->f04 |= 4;
    rec->f28 = DAT_100a9eb8;
    rec->f2C = DAT_100a9ebc - DAT_106ed6f8 * 8;
    rec->f14 = 0x1000;
    rec->f1C = 0x800;
    rec->f20 = (int *)(((int)DAT_106e9d90 + 15) & ~15);
    rec->f24 = 0x400;
    rec->f30 = DAT_106e79d4 + DAT_106ed67c * 0x17700 + 0x200;
    rec->f34 = (((int)DAT_106e7710 - (DAT_106e79d4 + DAT_106ed67c * 0x17700 + 0x200)) >> 3) << 3;

    len = ((int)DAT_106e7710 - (DAT_106e79d4 + DAT_106ed67c * 0x17700 + 0x200)) >> 3;
    if (len > DAT_106ed6f0)
        DAT_106ed6f0 = len;
    len = (DAT_1035f7d8 - DAT_102e16b0) >> 1;
    if (len > DAT_106ed6e8)
        DAT_106ed6e8 = len;
    len = (DAT_1035faec - DAT_1035fba4) >> 5;
    if (len > DAT_106ed6ec)
        DAT_106ed6ec = len;

    DAT_106e8200 = ((int)DAT_106e7710 - (DAT_106e79d4 + DAT_106ed67c * 0x17700 + 0x200)) >> 3;
    if (DAT_106e8200 > 12000)
        FUN_1002f26b(s_HUGE_GLIST_ERROR_100aa2d4);
    FUN_10008d60();

    if (DAT_106ed6f4 != 0) {
        FUN_10008d60(0, 0, 0, 0, 0xFF);
        FUN_100385e0(DAT_106ea430, 0, 1);
        FUN_10008d60(0, 0xFF, 0xFF, 0, 0xFF);
        if (DAT_106e8a1c != 0) {
            DAT_106e8a1c();
            DAT_106e8a1c = 0;
        }
        FUN_10008d60(0, 0, 0, 0, 0xFF);
        FUN_100385e0(DAT_106ea410, 0, 1);
        if (DAT_106e8698 != 0) {
            DAT_106e8a1c();
            DAT_106e8a1c = 0;
        }
        FUN_10008d60();
        DAT_106e729c = FUN_10059f00();
        DAT_106e86b0 = DAT_106e729c - DAT_106e7298;
        if (DAT_106ed670 != 0) {
            DAT_106ed670 = DAT_106ed670 - 1;
            if (DAT_106ed670 == 0) {
                if (DAT_106ed674 != 0) {
                    if (DAT_100ad7c8 == 2)
                        FUN_10008d60(DAT_10ac7380);
                    else
                        FUN_10008d60(DAT_10ac6ac0);
                } else {
                    if (DAT_100ad7c8 == 2)
                        FUN_10008d60(DAT_10ac70b0);
                    else
                        FUN_10008d60(DAT_10ac67f0);
                }
                FUN_10008d60(1);
                DAT_106ed678 = DAT_106ed674;
            }
        }
        FUN_10008d60(1, 0x20, 0x20, 0x20, 0xFF);
        FUN_10008d60(2, 0x20, 0x20, 0x20, 0xFF);
        FUN_10008d60(0, 0x20, 0x20, 0x20, 0xFF);
        if (DAT_106ed684 == 0 && DAT_106ed688 == 0)
            FUN_10008d60(0);
        if (DAT_106ed688 != 0)
            DAT_106ed688 = DAT_106ed688 - 1;
        FUN_100192d0();
        DAT_106e7298 = FUN_10059f00();
        DAT_106e729c = FUN_10059f00() - DAT_106e729c;
    } else {
        DAT_106ed6f4 = DAT_106ed6f4 + 1;
    }

    FUN_10008d60(0, 200, 0, 200, 0xFF);
    DAT_106ec774 = DAT_106ed628;
    FUN_10008d60(2, 200, 0, 0, 0xFF);
    FUN_10008d60(1, 200, 100, 0, 0xFF);
    DAT_10b73530(rec->f30);
    DAT_106ed67c = DAT_106ed67c ^ 1;
}
#endif /* BR_MATCHING_BUILD */

/* ==========================================================================
 * 0x1002AF17 -- the fog for this frame, emitted into the list.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
typedef struct { char pad[0x30]; float f30; float f34; } BrFogSrc;
typedef struct { char pad[0x38]; float f38; } BrFogCam;

extern int  DAT_106ed6ac, DAT_106ed6b0, DAT_106ed6b4, DAT_106ed6a8;  /* the four modes */
extern unsigned char DAT_106e72f0, DAT_106e86a4, DAT_106e7290;       /* fog colour r/g/b */
extern unsigned char DAT_106b7c78;                                    /* fog alpha        */
extern int  DAT_106ea428;                                             /* fog near         */
extern int  DAT_106ed568;                                             /* fog far          */
extern int  DAT_100b3858;                                             /* entrant count    */
extern int  DAT_100a718c;
extern BrFogSrc *DAT_106ed520;
extern BrFogCam *DAT_106e9d88;
extern float DAT_104abb60[];
extern float DAT_106eed10, DAT_106eed14;
extern unsigned char DAT_106eed58, DAT_106eed59, DAT_106eed5a;        /* the track's fog colour */
extern int  DAT_100b3014;
extern int  DAT_106e9d84, DAT_106e86a8;                               /* fog multiplier / offset */
extern float DAT_100774b0, DAT_100774b4, DAT_100774b8, DAT_100774bc, DAT_100774c0,
             DAT_100774c4, DAT_100774c8, DAT_100774cc, DAT_100774d0;
float FUN_10034720(float *, float *);

/* WHAT IT DOES: picks this frame's fog -- colour, alpha and the near/far
 * range -- from which of four viewing modes is active, blending the colour
 * towards a computed tint in two of them (one from a distance ratio, one
 * from the track's own fog colour scaled by height above a threshold), and
 * writes the fog multiplier/offset pair and the fog colour into the display
 * list. */
/* @implements 0x1002AF17 glide BrFrameFogEmit */
void BrFrameFogEmit(void)
{
    /* Two /Od facts: the converted byte is the LEFT operand of the blend
     * (`(float)c * (K - f)`), so its `fild`/`fstp` temp is evaluated before
     * `fld K; fsub f`; and the five locals' slots follow the name-hash
     * order, which these names satisfy (f -4, i -8, z -0xc, q1 -0x10,
     * q2 -0x14; 38 name sets measured).  Matches under /Od /Op. */
    float    f;
    int      i;
    float    z;
    BrDlCmd *q1;
    BrDlCmd *q2;

    if (DAT_106ed6ac != 0) {
        DAT_106e72f0 = DAT_106e86a4 = DAT_106e7290 = 0;
        DAT_106b7c78 = 0x40;
        if (DAT_100b3858 == 2) {
            DAT_106ea428 = 0x3E0;
            DAT_106ed568 = 0x3FC;
        } else {
            DAT_106ea428 = 0x3C8;
            DAT_106ed568 = 0x3FC;
        }
    } else if (DAT_106ed6b0 != 0) {
        DAT_106e72f0 = 0xB8;
        DAT_106e86a4 = 0xB8;
        DAT_106e7290 = 0xD8;
        DAT_106b7c78 = 0x40;
        if (DAT_100b3858 == 2) {
            DAT_106ea428 = 0x3B6;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x320;
            DAT_106ed568 = 0x41A;
        }
    } else if (DAT_106ed6b4 != 0) {
        DAT_106e72f0 = 0x60;
        DAT_106e86a4 = 0x68;
        DAT_106e7290 = 0x70;
        DAT_106b7c78 = 0x40;
        if (DAT_100a718c > 0 && (DAT_100a718c & 1) != 0) {
            f = DAT_100774b0 / (FUN_10034720(&DAT_106ed520->f30, DAT_104abb60) + DAT_100774b0);
            DAT_106e72f0 = (unsigned char)(int)((float)DAT_106e72f0 * (DAT_100774b4 - f) + DAT_100774b8 * f);
            DAT_106e86a4 = (unsigned char)(int)((float)DAT_106e86a4 * (DAT_100774b4 - f) + DAT_100774bc * f);
            DAT_106e7290 = (unsigned char)(int)((float)DAT_106e7290 * (DAT_100774b4 - f) + DAT_100774c0 * f);
        }
        if (DAT_100b3858 == 2) {
            DAT_106ea428 = 0x3A2;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x352;
            DAT_106ed568 = 0x401;
        }
    } else if (DAT_106ed6a8 != 0) {
        i = (int)((DAT_106e9d88->f38 - DAT_106eed10) / (DAT_106eed14 - DAT_106eed10) * DAT_100774c0);
        if (i < 0)
            i = 0;
        else if (i > 0xFF)
            i = 0xFF;
        if (DAT_100b3014 == 0) {
            if (DAT_106ed520->f34 > DAT_100774c4)
                z = DAT_106ed520->f30 * DAT_100774c8 * ((DAT_106ed520->f34 - DAT_100774c4) * DAT_100774cc);
            else
                z = 0;
            DAT_106e72f0 = (unsigned char)(int)((float)DAT_106eed58 * (DAT_100774b4 - z) + DAT_100774d0 * z);
            DAT_106e86a4 = (unsigned char)(int)((float)DAT_106eed59 * (DAT_100774b4 - z) + DAT_100774d0 * z);
            DAT_106e7290 = (unsigned char)(int)((float)DAT_106eed5a * (DAT_100774b4 - z) + DAT_100774d0 * z);
        } else {
            DAT_106e72f0 = DAT_106eed58;
            DAT_106e86a4 = DAT_106eed59;
            DAT_106e7290 = DAT_106eed5a;
        }
        DAT_106b7c78 = (unsigned char)i;
        if (DAT_100b3858 == 2) {
            DAT_106ea428 = 0x3E3;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x3D4;
            DAT_106ed568 = 0x3E8;
        }
    } else {
        DAT_106e72f0 = 0;
        DAT_106e86a4 = 0;
        DAT_106e7290 = 0;
        DAT_106b7c78 = 0xFF;
        DAT_106ea428 = 0;
        DAT_106ed568 = 0x3E8;
    }

    DAT_106e9d84 = 0x1F400 / (DAT_106ed568 - DAT_106ea428);
    DAT_106e86a8 = ((500 - DAT_106ea428) * 0x100) / (DAT_106ed568 - DAT_106ea428);

    q1 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 1;
    q1->op  = 0xBC000008;
    q1->arg = ((0x1F400 / (DAT_106ed568 - DAT_106ea428)) & 0xFFFF) << 16
            | ((((500 - DAT_106ea428) * 0x100) / (DAT_106ed568 - DAT_106ea428)) & 0xFFFF);

    q2 = DAT_106e7710;
    DAT_106e7710 = DAT_106e7710 + 1;
    q2->op  = 0xF8000000;
    q2->arg = ((DAT_106e72f0 & 0xFF) << 24) | ((DAT_106e86a4 & 0xFF) << 16)
            | ((DAT_106e7290 & 0xFF) << 8) | 0xFF;
}
#endif /* BR_MATCHING_BUILD */
