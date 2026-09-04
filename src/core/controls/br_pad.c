/* br_pad.c -- the per-frame controller pass.
 *
 * RESPONSIBILITY: reading what the player is doing -- opening and closing the
 * sampling window each frame and walking the pad blocks.
 *
 * Moved here out of src/core/slice2_19.c (an address batch, not a module).
 * The bodies are byte-for-byte the text that was matched there; the layouts,
 * globals and prototypes they need all come from slice2_19.h.
 */
#include "slice2_19.h"

/* 0x10019A70 is the (unclaimed, 11 KB) race step.  The original passes its
 * address as an IMMEDIATE, so the matching build needs a function symbol,
 * not a pointer variable.  The port keeps the variable. */
#ifdef BR_MATCHING_BUILD
extern void BrRaceStep_10019A70(void);
#define BR_PAD_RACE_STEP ((const void *)BrRaceStep_10019A70)
#else
#define BR_PAD_RACE_STEP g_BrPadHookFn
#endif

/* 0x1002F380  __thiscall (one arg in ecx -- BR_THISCALL1 is exact) */
/* WHAT IT DOES: turns one frame of raw controller readings into what the game
 * understands -- which buttons are pressed, how far the stick is pushed, and
 * how much the player is steering, with the stick scaled and limited to a
 * full-left-to-full-right range. A disconnected controller reads as nothing
 * pressed and centred. While the driving screen is the one in charge it also
 * derives the extra combinations the car controls need, and lets a player
 * steer with the direction pad instead of the stick when the stick is not in
 * use.
 *
 * Shape notes, all read off the bytes: members are re-derefed per statement
 * (docs/VC5-IDIOMS.md); the button word is ONE u16 load tested by sub-
 * register; the ramp pair is an inline two-lap pointer loop, not a helper;
 * the x/y clamps compare the RELOADED member while steer's compares the
 * unrounded register (hence the local for steer only).
 *
 * The two mode-byte probes are 16-bit masks; see the comment on them. */
/* @implements 0x1002F380 glide BrPadTranslate */
void BR_THISCALL1 BrPadTranslate(BrPad *pPad)
{
    uint32_t w;

    {
        uint8_t st = pPad->pRaw->status;
        if (st != 0) {
            pPad->f28 = (st == 8) ? 1 : 0;
            pPad->pRaw->stickX = 0;
            pPad->pRaw->stickY = 0;
            *(uint16_t *)(void *)&pPad->pRaw->b0 = 0;
        } else {
            pPad->f28 = 0;
        }
    }

    w = *(const uint16_t *)(const void *)&pPad->pRaw->b0;
    pPad->buttons = 0;
    if (w & 0x0800u) pPad->buttons  = BR_PAD_DUP;
    if (w & 0x0400u) pPad->buttons |= BR_PAD_DDOWN;
    if (w & 0x0200u) pPad->buttons |= BR_PAD_DLEFT;
    if (w & 0x0100u) pPad->buttons |= BR_PAD_DRIGHT;
    if (w & 0x8000u) pPad->buttons |= BR_PAD_A;
    if (w & 0x4000u) pPad->buttons |= BR_PAD_B;
    if (w & 0x0020u) pPad->buttons |= BR_PAD_L;
    if (w & 0x0010u) pPad->buttons |= BR_PAD_R;
    if (w & 0x2000u) pPad->buttons |= BR_PAD_Z;
    if (w & 0x1000u) pPad->buttons |= BR_PAD_START;
    if (w & 0x0008u) pPad->buttons |= BR_PAD_CUP;
    if (w & 0x0001u) pPad->buttons |= BR_PAD_CRIGHT;
    if (w & 0x0004u) pPad->buttons |= BR_PAD_CDOWN;
    if (w & 0x0002u) pPad->buttons |= BR_PAD_CLEFT;

    if (BrHookIsCurrent(BR_PAD_RACE_STEP)) {
        if (pPad->buttons & BR_PAD_L) pPad->buttons |= BR_PAD_L_ALT;
        if (pPad->buttons & BR_PAD_R) pPad->buttons |= BR_PAD_R_ALT;

        /* Both probes are 16-BIT masks, not byte masks. Spelled as
         * `g_BrPadModeBytes[1] & 0x80` the two 0x80s are one constant in
         * the source, and VC5 pools them into `mov cl,0x80` + two
         * `test byte [eax+n],cl`. Spelled as `& 0x8000` on the halfword,
         * the narrowing to `test byte [eax+n],0x80` happens per
         * instruction, late, and there is nothing left to pool. */
        if (!(*(const unsigned short *)(const void *)g_BrPadModeBytes & 0x8000u)
            && !(*(const unsigned short *)(const void *)(g_BrPadModeBytes + 6)
                 & 0x8000u)) {
            uint32_t a = pPad->buttons;
            if (a & BR_PAD_DLEFT) {
                if (!(a & BR_PAD_DRIGHT))
                    pPad->steer = (int8_t)0xB0;
                else
                    pPad->steer = 0;
            } else if (a & BR_PAD_DRIGHT) {
                pPad->steer = 0x50;
            } else {
                pPad->steer = 0;
            }
        } else {
            pPad->steer = pPad->pRaw->stickX;
        }

        if (pPad->buttons & BR_PAD_A) {
            if (pPad->pRaw->stickY < (int8_t)0xC0)    /* signed, -64 */
                pPad->buttons |= BR_PAD_A_BACK;
            pPad->buttons |= BR_PAD_A_D;
        }
        if (pPad->buttons & BR_PAD_B) {
            if (pPad->buttons & BR_PAD_A_D)
                pPad->buttons |= BR_PAD_B_A;
            else
                pPad->buttons |= BR_PAD_B_ALT;
        }
        if (pPad->buttons & BR_PAD_CUP)   pPad->buttons |= BR_PAD_CUP2;
        if (pPad->buttons & BR_PAD_CDOWN) pPad->buttons |= BR_PAD_CDOWN2;
        if (pPad->buttons & BR_PAD_CLEFT) pPad->buttons |= BR_PAD_CLEFT2;
    }

    if (pPad->f2C == 0 && pPad->f30 == 0) {
        /* the original's dead load of f44: a volatile READ with no
         * assignment is exactly one mov, no store */
        (void)*(volatile int32_t *)&pPad->f44;
    } else {
        int32_t *p = &pPad->f34;
        int      i;
        for (i = 2; i > 0; --i, ++p) {
            if (*(p - 2) != 0) {
                if (*p < *(p + 2) && g_br5CCB5C == 0)
                    *p += 2;
            }
        }
    }

    {
        float t;

        pPad->axisX = (float)pPad->pRaw->stickX * g_BrK08F548;
        t = (float)pPad->steer * g_BrK08F548;
        pPad->axisY = (float)pPad->pRaw->stickY * g_BrK08F548;
        pPad->axisSteer = t;

        if (pPad->axisX > 1.0f)
            pPad->axisX = 1.0f;
        else if (pPad->axisX < -1.0f)
            pPad->axisX = -1.0f;

        if (pPad->axisY > 1.0f)
            pPad->axisY = 1.0f;
        else if (pPad->axisY < -1.0f)
            pPad->axisY = -1.0f;

        if (t > 1.0f)
            pPad->axisSteer = 1.0f;
        else if (t < -1.0f)
            pPad->axisSteer = -1.0f;
    }
}

#ifdef BR_MATCHING_BUILD

extern int DAT_106ed5d0;
extern int DAT_106b8090;
extern int DAT_106ec778;
extern int DAT_106ed630;
extern unsigned short _DAT_100b5598;
int FUN_10059e70();
extern char DAT_106ed708;
void BrPadFrameBegin(void);
int BrStubTrue();

/* WHAT IT DOES: return 1. */
/* @implements 0x1002F238 glide BrRet1_1002F238 */

int BrRet1_1002F238(void)

{
  return 1;
}

/* WHAT IT DOES: call BrStubTrue on the block at 0x106ED5D0 with (0,1). */
/* @implements 0x1002F242 glide BrSub_1002F242 */

void BrSub_1002F242(void)

{
  BrStubTrue(&DAT_106ed5d0,0,1);
  return;
}

/* WHAT IT DOES: run the per-pad input step: 0x1002CE5F once, then for each pad block
 * (base 0x106ED708, stride 0x15C, count 1 in this build) translate the raw pad state and
 * split the bit edges. thiscall callees via BR_THISCALL1. */
/* @implements 0x1002CE9A glide BrPadTranslateAll */

void BrPadTranslateAll(void)

{
  int i;
  BrPadFrameBegin();
  for (i = 0; i < 1; i++) {
    BrPadTranslate((BrPad *)((char *)&DAT_106ed708 + i*0x15c));
    BrBitEdgeSplit((BrBitPair *)((char *)&DAT_106ed708 + i*0x15c));
  }
}

/* WHAT IT DOES: once per frame, open the pad sampling window: on the first call set the
 * in-progress flag, zero the u16 latch at 0x100B5598 and trace the 0x106B8090 block. */
/* @implements 0x1002CE31 glide BrPadFrameInit */

void BrPadFrameInit(void)

{
  if (DAT_106ec778 == 0) {
    DAT_106ec778 = 1;
    _DAT_100b5598 = 0;
    BrStubTrue(&DAT_106b8090);
  }
  return;
}

/* WHAT IT DOES: begin the pad frame: init, trace, run 0x10059E70 on the 0x106ED630 block,
 * mark the u16 latch live and clear the in-progress flag. */
/* @implements 0x1002CE5F glide BrPadFrameBegin */

void BrPadFrameBegin(void)

{
  BrPadFrameInit();
  BrStubTrue(&DAT_106b8090,0,1);
  FUN_10059e70(&DAT_106ed630);
  _DAT_100b5598 = 1;
  DAT_106ec778 = 0;
  return;
}

#endif /* BR_MATCHING_BUILD */
