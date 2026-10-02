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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
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
    BrFrameBeginDl(pRec, 0);
}

/* 0x10032886 */
/* WHAT IT DOES: starts a frame at the high resolution instead, using the
 * game's own frame record. Switching resolution mid-run is noticed and
 * reloads the state that everything downstream keys off when it halves or
 * doubles a rectangle. */
/* @implements 0x10032886 d3d BrFrameBeginHiRes */
void BrFrameBeginHiRes(void)
{
    BrFrameBeginDl(BrG_6C1628, 1);
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10008d60: prototype in br_funcs.h */
/* FUN_1002bf50: prototype in br_funcs.h */
/* FUN_1001cf90: prototype in br_funcs.h */
/* FUN_1002a8d7: prototype in br_funcs.h */
/* FUN_100625f0: prototype in br_funcs.h */

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
/* 64-bit core: declared once, in br_globals.h or its struct's header */

#define BR_EMIT(c,a) { \
  BrDlCmd *p_ = (*(BrDlCmd * *)&g_BrGfxPtr)++; \
  p_->op = (c); \
  p_->arg = (a); }

void BrFrameBeginDl(int *param_1,int param_2)
{
  if (param_2 ^ (*(int *)((char *)&g_aBrEntRecs + 0x44))) {
    (*(int *)((char *)&g_aBrEntRecs + 0x40)) = 1;
    (*(int *)((char *)&g_aBrEntRecs + 0x44)) = param_2;
  }
  BrPodNop();
  switch (g_brMode0AA8B4) {
  case 1:
    *param_1 = 0;
    param_1[1] = 0;
    param_1[2] = BrGbiRectG_A7514;
    param_1[3] = BrGbiRectG_A7518;
    break;
  case 2:
    param_1[0x16] = 8;
    param_1[0x17] = ((*(int *)&g_brRaceCueBase) >> 1) + 1;
    param_1[0x18] = g_scrW4 + -0x60;
    param_1[0x19] = ((*(int *)&g_brRaceCueBase) >> 1) + -8;
    *param_1 = 8;
    param_1[1] = 8;
    param_1[2] = g_scrW4 + -0x60;
    param_1[3] = ((*(int *)&g_brRaceCueBase) >> 1) + -8;
    break;
  }
  BrRenderCountersReset();
  BrGfx69580();
  (*(BrDlCmd * *)&g_BrGfxPtr) = (BrDlCmd *)(DAT_106e79d4 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 96000 + 0x200);
  (*(int *)&g_BrEnvOthermode) = DAT_100aa020 ? 0x2000 : 0;
  (*(int *)&BrG_6C0688) = 0x40;
  DAT_106e79b0 = 0;
  BR_EMIT(0xbc000006, 0)
  BR_EMIT(0xe7000000, 0)
  BrSub_1003289F(0,0,g_scrW4,(*(int *)&g_brRaceCueBase));
  BrRdpSetCombineLERP((*(BrDlCmd * *)&g_BrGfxPtr)++,0,0,0,0x3eb,0,0,0,0x3eb,0,0,0,1000,0,0,0,1000);
  BR_EMIT(0xba001001, 0)
  BR_EMIT(0xba000e02, 0)
  BR_EMIT(0xba001102, 0)
  BR_EMIT(0xba001301, 0x80000)
  BR_EMIT(0xba000c02, (*(int *)&g_BrEnvOthermode))
  BR_EMIT(0xba000903, 0xc00)
  BR_EMIT(0xba000801, 0)
  BR_EMIT(0xb9000002, 1)
  BR_EMIT(0xb900031d, 0xf0a4000)
  BR_EMIT(0xba000602, (*(int *)&BrG_6C0688))
  BR_EMIT(0xba000602, DAT_106e79b0)
  BR_EMIT(0xba001402, 0)
  BR_EMIT(0xf9000000, 0)
  BR_EMIT(0x1020040, (int)&(*(char *)&DAT_100a9ec0))
  BR_EMIT(0xb6000000, 0x1f3204)
  BR_EMIT(0xb7000000, 0x2000)
  if (DAT_100aa014 != 0) {
    BR_EMIT(0xb7000000, 0x800000)
  }
  else {
    BR_EMIT(0xb6000000, 0x800000)
  }
  BR_EMIT(0x6000000, (int)(&BrG_0AA770 + (*(int *)((char *)&g_aBrEntRecs + 0x5C)) * 0x28))
  BR_EMIT(0xbb000000, 0)
  BrPodNop();
  BrPodNop();
  BrPodNop();
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* minimum X */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* maximum X */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* minimum Y */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* maximum Y */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* hi-res: double every coordinate */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the fixed-point scale */

void BrSub_1003289F(int param_1,int param_2,int param_3,int param_4)

{
  BrDlCmd *piVar1;

  if (param_1 < (*(int *)&g_brFadePos)) {
    param_3 = param_3 - ((*(int *)&g_brFadePos) - param_1);
    param_1 = (*(int *)&g_brFadePos);
  }
  if (param_1 + param_3 > (*(int *)&g_brFadePos2)) {
    param_3 = (*(int *)&g_brFadePos2) - param_1;
  }
  if (param_3 < 0) {
    param_3 = 0;
  }
  if (param_2 < (*(int *)&g_brFadeB4)) {
    param_4 = param_4 - ((*(int *)&g_brFadeB4) - param_2);
    param_2 = (*(int *)&g_brFadeB4);
  }
  if (param_2 + param_4 > (*(int *)&g_brFadeA4)) {
    param_4 = (*(int *)&g_brFadeA4) - param_2;
  }
  if (param_4 < 0) {
    param_4 = 0;
  }
  if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0) {
    param_1 = param_1 * 2;
    param_2 = param_2 * 2;
    param_3 = param_3 * 2;
    param_4 = param_4 * 2;
  }
  piVar1 = (*(BrDlCmd * *)&g_BrGfxPtr)++;
  piVar1->op = 0xe7000000;
  piVar1->arg = 0;
  {
    BrDlCmd *piVar2 = (*(BrDlCmd * *)&g_BrGfxPtr)++;
    piVar2->op = (((int)((float)param_1 * DAT_100774b4) & 0xfff) << 12)
               | 0xe2000000
               | ((int)((float)param_2 * DAT_100774b4) & 0xfff);
    piVar2->arg = (((int)((float)(param_1 + param_3) * DAT_100774b4) & 0xfff) << 12)
                | ((int)((float)(param_2 + param_4) * DAT_100774b4) & 0xfff);
  }
  return;
}


/* ==========================================================================
 * 0x1002CEE9 -- closing the frame.  The counterpart of the openers above, in
 * the same /Od range (br_framedrive.c names it BrFrameEnd).
 * ========================================================================== */
/* The per-frame task record: 0x40 bytes, two of them, selected by the frame
 * parity at 0x106ED67C. */
/* BrFrameTask: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the two task records              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* the 16-byte-aligned scratch block */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* high-water marks                  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* this frame's list length          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* frames drawn                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* one-shot callback                 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* frame timing                      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* the list submit hook              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100385e0: prototype in br_funcs.h */
/* FUN_1002f26b: prototype in br_funcs.h */
/* FUN_100192d0: prototype in br_funcs.h */
/* FUN_10059f00: prototype in br_funcs.h */

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

    q1 = (*(BrDlCmd * *)&g_BrGfxPtr);
    (*(BrDlCmd * *)&g_BrGfxPtr) = (*(BrDlCmd * *)&g_BrGfxPtr) + 1;
    q1->op = 0xE9000000;
    q1->arg = 0;
    q2 = (*(BrDlCmd * *)&g_BrGfxPtr);
    (*(BrDlCmd * *)&g_BrGfxPtr) = (*(BrDlCmd * *)&g_BrGfxPtr) + 1;
    q2->op = 0xB8000000;
    q2->arg = 0;

    rec = &DAT_106e8618[(*(int *)((char *)&g_aBrEntRecs + 0x4C))];
    rec->f00 = 1;
    rec->f10 = DAT_118ee268;
    rec->f18 = DAT_118ee278;
    rec->f04 = 2;
    rec->f04 |= 4;
    rec->f28 = DAT_100a9eb8;
    rec->f2C = DAT_100a9ebc - (*(int *)((char *)&g_aBrEntRecs + 0xC8)) * 8;
    rec->f14 = 0x1000;
    rec->f1C = 0x800;
    rec->f20 = (int *)(((int)DAT_106e9d90 + 15) & ~15);
    rec->f24 = 0x400;
    rec->f30 = DAT_106e79d4 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x17700 + 0x200;
    rec->f34 = (((int)(*(BrDlCmd * *)&g_BrGfxPtr) - (DAT_106e79d4 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x17700 + 0x200)) >> 3) << 3;

    len = ((int)(*(BrDlCmd * *)&g_BrGfxPtr) - (DAT_106e79d4 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x17700 + 0x200)) >> 3;
    if (len > (*(int *)((char *)&g_aBrEntRecs + 0xC0)))
        (*(int *)((char *)&g_aBrEntRecs + 0xC0)) = len;
    len = (DAT_1035f7d8 - DAT_102e16b0) >> 1;
    if (len > (*(int *)((char *)&g_aBrEntRecs + 0xB8)))
        (*(int *)((char *)&g_aBrEntRecs + 0xB8)) = len;
    len = ((char *)DAT_1035faec - (char *)DAT_1035fba4) >> 5;
    if (len > (*(int *)((char *)&g_aBrEntRecs + 0xBC)))
        (*(int *)((char *)&g_aBrEntRecs + 0xBC)) = len;

    DAT_106e8200 = ((int)(*(BrDlCmd * *)&g_BrGfxPtr) - (DAT_106e79d4 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x17700 + 0x200)) >> 3;
    if (DAT_106e8200 > 12000)
        BrLogSet(s_HUGE_GLIST_ERROR_100aa2d4);
    BrPodNop();

    if ((*(int *)((char *)&g_aBrEntRecs + 0xC4)) != 0) {
        BrPodNop();
        BrStubTrue();
        BrPodNop();
        if (DAT_106e8a1c != 0) {
            DAT_106e8a1c();
            DAT_106e8a1c = 0;
        }
        BrPodNop();
        BrStubTrue();
        if (DAT_106e8698 != 0) {
            DAT_106e8a1c();
            DAT_106e8a1c = 0;
        }
        BrPodNop();
        DAT_106e729c = BrTickAdd_10078C10();
        DAT_106e86b0 = DAT_106e729c - DAT_106e7298;
        if ((*(int *)((char *)&g_aBrEntRecs + 0x40)) != 0) {
            (*(int *)((char *)&g_aBrEntRecs + 0x40)) = (*(int *)((char *)&g_aBrEntRecs + 0x40)) - 1;
            if ((*(int *)((char *)&g_aBrEntRecs + 0x40)) == 0) {
                if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0) {
                    if (DAT_100ad7c8 == 2)
                        BrPodNop();
                    else
                        BrPodNop();
                } else {
                    if (DAT_100ad7c8 == 2)
                        BrPodNop();
                    else
                        BrPodNop();
                }
                BrPodNop();
                (*(int *)((char *)&g_aBrEntRecs + 0x48)) = (*(int *)((char *)&g_aBrEntRecs + 0x44));
            }
        }
        BrPodNop();
        BrPodNop();
        BrPodNop();
        if ((*(int *)((char *)&g_aBrEntRecs + 0x54)) == 0 && (*(int *)((char *)&g_aBrEntRecs + 0x58)) == 0)
            BrPodNop();
        if ((*(int *)((char *)&g_aBrEntRecs + 0x58)) != 0)
            (*(int *)((char *)&g_aBrEntRecs + 0x58)) = (*(int *)((char *)&g_aBrEntRecs + 0x58)) - 1;
        BrS17BankFlip();
        DAT_106e7298 = BrTickAdd_10078C10();
        DAT_106e729c = BrTickAdd_10078C10() - DAT_106e729c;
    } else {
        (*(int *)((char *)&g_aBrEntRecs + 0xC4)) = (*(int *)((char *)&g_aBrEntRecs + 0xC4)) + 1;
    }

    BrPodNop();
    DAT_106ec774 = DAT_106ed628;
    BrPodNop();
    BrPodNop();
    (*(void (**)(int))&DAT_10b73530)(rec->f30);
    (*(int *)((char *)&g_aBrEntRecs + 0x4C)) = (*(int *)((char *)&g_aBrEntRecs + 0x4C)) ^ 1;
}

/* ==========================================================================
 * 0x1002AF17 -- the fog for this frame, emitted into the list.
 * ========================================================================== */
typedef struct { char pad[0x30]; float f30; float f34; } BrFogSrc;
typedef struct { char pad[0x38]; float f38; } BrFogCam;

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* the four modes */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* fog colour r/g/b */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                    /* fog alpha        */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                             /* fog near         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                             /* fog far          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                                             /* entrant count    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* the track's fog colour */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                               /* fog multiplier / offset */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10034720: prototype in br_funcs.h */

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

    if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
        (*(unsigned char *)&BrG_6C0260) = (*(unsigned char *)&BrG_6C1614) = (*(unsigned char *)&BrG_6C0200) = 0;
        (*(unsigned char *)&g_BrDrawByte78) = 0x40;
        if ((*(int *)&g_brRaceNEntrant) == 2) {
            DAT_106ea428 = 0x3E0;
            DAT_106ed568 = 0x3FC;
        } else {
            DAT_106ea428 = 0x3C8;
            DAT_106ed568 = 0x3FC;
        }
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0) {
        (*(unsigned char *)&BrG_6C0260) = 0xB8;
        (*(unsigned char *)&BrG_6C1614) = 0xB8;
        (*(unsigned char *)&BrG_6C0200) = 0xD8;
        (*(unsigned char *)&g_BrDrawByte78) = 0x40;
        if ((*(int *)&g_brRaceNEntrant) == 2) {
            DAT_106ea428 = 0x3B6;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x320;
            DAT_106ed568 = 0x41A;
        }
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x84)) != 0) {
        (*(unsigned char *)&BrG_6C0260) = 0x60;
        (*(unsigned char *)&BrG_6C1614) = 0x68;
        (*(unsigned char *)&BrG_6C0200) = 0x70;
        (*(unsigned char *)&g_BrDrawByte78) = 0x40;
        if (DAT_100a718c > 0 && (DAT_100a718c & 1) != 0) {
            f = DAT_100774b0 / (BrVec3DistXY(&(*(BrFogSrc * *)&g_BrCamera)->f30, DAT_104abb60) + DAT_100774b0);
            (*(unsigned char *)&BrG_6C0260) = (unsigned char)(int)((float)(*(unsigned char *)&BrG_6C0260) * (DAT_100774b4 - f) + DAT_100774b8 * f);
            (*(unsigned char *)&BrG_6C1614) = (unsigned char)(int)((float)(*(unsigned char *)&BrG_6C1614) * (DAT_100774b4 - f) + DAT_100774bc * f);
            (*(unsigned char *)&BrG_6C0200) = (unsigned char)(int)((float)(*(unsigned char *)&BrG_6C0200) * (DAT_100774b4 - f) + DAT_100774c0 * f);
        }
        if ((*(int *)&g_brRaceNEntrant) == 2) {
            DAT_106ea428 = 0x3A2;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x352;
            DAT_106ed568 = 0x401;
        }
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x78)) != 0) {
        i = (int)(((*(BrFogCam * *)&g_pBr63Race)->f38 - DAT_106eed10) / (DAT_106eed14 - DAT_106eed10) * DAT_100774c0);
        if (i < 0)
            i = 0;
        else if (i > 0xFF)
            i = 0xFF;
        if ((*(int *)&g_Br0B380C) == 0) {
            if ((*(BrFogSrc * *)&g_BrCamera)->f34 > DAT_100774c4)
                z = (*(BrFogSrc * *)&g_BrCamera)->f30 * DAT_100774c8 * (((*(BrFogSrc * *)&g_BrCamera)->f34 - DAT_100774c4) * DAT_100774cc);
            else
                z = 0;
            (*(unsigned char *)&BrG_6C0260) = (unsigned char)(int)((float)DAT_106eed58 * (DAT_100774b4 - z) + DAT_100774d0 * z);
            (*(unsigned char *)&BrG_6C1614) = (unsigned char)(int)((float)DAT_106eed59 * (DAT_100774b4 - z) + DAT_100774d0 * z);
            (*(unsigned char *)&BrG_6C0200) = (unsigned char)(int)((float)DAT_106eed5a * (DAT_100774b4 - z) + DAT_100774d0 * z);
        } else {
            (*(unsigned char *)&BrG_6C0260) = DAT_106eed58;
            (*(unsigned char *)&BrG_6C1614) = DAT_106eed59;
            (*(unsigned char *)&BrG_6C0200) = DAT_106eed5a;
        }
        (*(unsigned char *)&g_BrDrawByte78) = (unsigned char)i;
        if ((*(int *)&g_brRaceNEntrant) == 2) {
            DAT_106ea428 = 0x3E3;
            DAT_106ed568 = 0x3E8;
        } else {
            DAT_106ea428 = 0x3D4;
            DAT_106ed568 = 0x3E8;
        }
    } else {
        (*(unsigned char *)&BrG_6C0260) = 0;
        (*(unsigned char *)&BrG_6C1614) = 0;
        (*(unsigned char *)&BrG_6C0200) = 0;
        (*(unsigned char *)&g_BrDrawByte78) = 0xFF;
        DAT_106ea428 = 0;
        DAT_106ed568 = 0x3E8;
    }

    DAT_106e9d84 = 0x1F400 / (DAT_106ed568 - DAT_106ea428);
    DAT_106e86a8 = ((500 - DAT_106ea428) * 0x100) / (DAT_106ed568 - DAT_106ea428);

    q1 = (*(BrDlCmd * *)&g_BrGfxPtr);
    (*(BrDlCmd * *)&g_BrGfxPtr) = (*(BrDlCmd * *)&g_BrGfxPtr) + 1;
    q1->op  = 0xBC000008;
    q1->arg = ((0x1F400 / (DAT_106ed568 - DAT_106ea428)) & 0xFFFF) << 16
            | ((((500 - DAT_106ea428) * 0x100) / (DAT_106ed568 - DAT_106ea428)) & 0xFFFF);

    q2 = (*(BrDlCmd * *)&g_BrGfxPtr);
    (*(BrDlCmd * *)&g_BrGfxPtr) = (*(BrDlCmd * *)&g_BrGfxPtr) + 1;
    q2->op  = 0xF8000000;
    q2->arg = (((*(unsigned char *)&BrG_6C0260) & 0xFF) << 24) | (((*(unsigned char *)&BrG_6C1614) & 0xFF) << 16)
            | (((*(unsigned char *)&BrG_6C0200) & 0xFF) << 8) | 0xFF;
}

/* ==========================================================================
 * 0x1002B480 -- the frame's tint bytes: sky, ground and the four-step ramp.
 * ========================================================================== */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* a BrVec3 handed to 0x100344D0 */
/* FUN_100344d0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* tint A r/g/b */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* tint B r/g/b */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the ramp, r/g/b */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                 /* packed tints    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                            /* packed ramp     */

/* WHAT IT DOES: sets this frame's two tint colours and the four-step colour
 * ramp from the viewing mode.  Two modes take fixed colours, one derives
 * both tints from the fog colour by simple shifts, and the default mode
 * blends fixed colours with the fog colour by the fog alpha; the ramp is
 * either fixed or quarter/half/three-quarter/full steps of the first tint.
 * All of it is then packed into the three RGB words the renderer reads. */
/* @implements 0x1002B480 glide BrFrameTintSetup */
void BrFrameTintSetup(void)
{
    int i;

    if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) == 0) {
        (*(float *)&g_BrVisLightDefault) = 15.0f;
        (*(float *)((char *)&g_BrVisLightDefault + 0x4)) = 10.0f;
        (*(float *)((char *)&g_BrVisLightDefault + 0x8)) = 20.0f;
        br_dl_normalise(&(*(float *)&g_BrVisLightDefault));
    }
    if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0 || (*(int *)((char *)&g_aBrEntRecs + 0x84)) != 0) {
        (*(unsigned char *)&BrG_6C1580) = (unsigned char)(((*(unsigned char *)&BrG_6C0260) + 0x2FD) >> 2);
        (*(unsigned char *)&BrG_6C335C) = (unsigned char)(((*(unsigned char *)&BrG_6C1614) + 0x2FD) >> 2);
        (*(unsigned char *)&BrG_6C0968) = (unsigned char)(((*(unsigned char *)&BrG_6C0200) + 0x264) >> 2);
        (*(unsigned char *)&g_BrDrawByte80) = (unsigned char)(((*(unsigned char *)&BrG_6C0260) * 5) / 8);
        (*(unsigned char *)&BrG_6C0960) = (unsigned char)(((*(unsigned char *)&BrG_6C1614) * 5) / 8);
        (*(unsigned char *)((char *)&g_aBrEntRecs + 0x1C)) = (unsigned char)(((*(unsigned char *)&BrG_6C0200) * 5) / 8);
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
        (*(unsigned char *)&BrG_6C1580) = 0xDD;
        (*(unsigned char *)&BrG_6C335C) = 0xEE;
        (*(unsigned char *)&BrG_6C0968) = 0xFF;
        (*(unsigned char *)&g_BrDrawByte80) = 0x3C;
        (*(unsigned char *)&BrG_6C0960) = 0x39;
        (*(unsigned char *)((char *)&g_aBrEntRecs + 0x1C)) = 0x36;
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x78)) != 0) {
        (*(unsigned char *)&BrG_6C1580) = (unsigned char)(((((*(unsigned char *)&BrG_6C0260) + 0xFF) >> 1) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0xFF) / 0xFF);
        (*(unsigned char *)&BrG_6C335C) = (unsigned char)(((((*(unsigned char *)&BrG_6C1614) + 0xFF) >> 1) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0xFF) / 0xFF);
        (*(unsigned char *)&BrG_6C0968) = (unsigned char)(((((*(unsigned char *)&BrG_6C0200) + 0xCC) >> 1) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0xCC) / 0xFF);
        (*(unsigned char *)&g_BrDrawByte80) = (unsigned char)(((((*(unsigned char *)&BrG_6C0260) << 2) / 5) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0x66) / 0xFF);
        (*(unsigned char *)&BrG_6C0960) = (unsigned char)(((((*(unsigned char *)&BrG_6C1614) << 2) / 5) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0x66) / 0xFF);
        (*(unsigned char *)((char *)&g_aBrEntRecs + 0x1C)) = (unsigned char)(((((*(unsigned char *)&BrG_6C0200) << 2) / 5) * (*(unsigned char *)&g_BrDrawByte78) + (0xFF - (*(unsigned char *)&g_BrDrawByte78)) * 0x77) / 0xFF);
    } else {
        (*(unsigned char *)&BrG_6C1580) = 0xFF;
        (*(unsigned char *)&BrG_6C335C) = 0xFF;
        (*(unsigned char *)&BrG_6C0968) = 0xCC;
        (*(unsigned char *)&g_BrDrawByte80) = 0x66;
        (*(unsigned char *)&BrG_6C0960) = 0x66;
        (*(unsigned char *)((char *)&g_aBrEntRecs + 0x1C)) = 0x77;
    }

    if ((*(int *)((char *)&g_aBrEntRecs + 0x7C)) != 0) {
        DAT_106b8088[0] = 0x22;
        DAT_106ed524[0] = 0x22;
        DAT_106ea3e8[0] = 0x22;
        DAT_106b8088[1] = 0x44;
        DAT_106ed524[1] = 0x44;
        DAT_106ea3e8[1] = 0x44;
        DAT_106b8088[2] = 0x66;
        DAT_106ed524[2] = 0x66;
        DAT_106ea3e8[2] = 0x66;
        DAT_106b8088[3] = 0xFF;
        DAT_106ed524[3] = 0xFF;
        DAT_106ea3e8[3] = 0xFF;
    } else if ((*(int *)((char *)&g_aBrEntRecs + 0x80)) != 0) {
        DAT_106b8088[0] = 0xD0;
        DAT_106ed524[0] = 0xD0;
        DAT_106ea3e8[0] = 0xF0;
        DAT_106b8088[1] = 0xE0;
        DAT_106ed524[1] = 0xE0;
        DAT_106ea3e8[1] = 0xFF;
        DAT_106b8088[2] = 0xF0;
        DAT_106ed524[2] = 0xF0;
        DAT_106ea3e8[2] = 0xFF;
        DAT_106b8088[3] = 0xFF;
        DAT_106ed524[3] = 0xFF;
        DAT_106ea3e8[3] = 0xFF;
    } else {
        DAT_106b8088[0] = (unsigned char)((*(unsigned char *)&BrG_6C1580) >> 2);
        DAT_106ed524[0] = (unsigned char)((*(unsigned char *)&BrG_6C335C) >> 2);
        DAT_106ea3e8[0] = (unsigned char)((*(unsigned char *)&BrG_6C0968) >> 2);
        DAT_106b8088[1] = (unsigned char)((*(unsigned char *)&BrG_6C1580) >> 1);
        DAT_106ed524[1] = (unsigned char)((*(unsigned char *)&BrG_6C335C) >> 1);
        DAT_106ea3e8[1] = (unsigned char)((*(unsigned char *)&BrG_6C0968) >> 1);
        DAT_106b8088[2] = (unsigned char)(((*(unsigned char *)&BrG_6C1580) >> 1) + ((*(unsigned char *)&BrG_6C1580) >> 2));
        DAT_106ed524[2] = (unsigned char)(((*(unsigned char *)&BrG_6C335C) >> 1) + ((*(unsigned char *)&BrG_6C335C) >> 2));
        DAT_106ea3e8[2] = (unsigned char)(((*(unsigned char *)&BrG_6C0968) >> 1) + ((*(unsigned char *)&BrG_6C0968) >> 2));
        DAT_106b8088[3] = (*(unsigned char *)&BrG_6C1580);
        DAT_106ed524[3] = (*(unsigned char *)&BrG_6C335C);
        DAT_106ea3e8[3] = (*(unsigned char *)&BrG_6C0968);
    }

    DAT_106e9a78 = (unsigned int)(*(unsigned char *)&g_BrDrawByte80) << 24 | (unsigned int)(*(unsigned char *)&BrG_6C0960) << 16 | (unsigned int)(*(unsigned char *)((char *)&g_aBrEntRecs + 0x1C)) << 8;
    DAT_106ecb40 = (unsigned int)(*(unsigned char *)&BrG_6C1580) << 24 | (unsigned int)(*(unsigned char *)&BrG_6C335C) << 16 | (unsigned int)(*(unsigned char *)&BrG_6C0968) << 8;
    for (i = 0; i < 4; i = i + 1) {
        DAT_106e79e0[i] = (unsigned int)DAT_106b8088[i] << 24 | (unsigned int)DAT_106ed524[i] << 16
                        | (unsigned int)DAT_106ea3e8[i] << 8;
    }
}
