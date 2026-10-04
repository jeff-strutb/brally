/* br_pad.c -- the per-frame controller pass.
 *
 * RESPONSIBILITY: reading what the player is doing -- opening and closing the
 * sampling window each frame and walking the pad blocks.
 *
 * Moved here out of src/core/slice2_19.c (an address batch, not a module).
 * The bodies are byte-for-byte the text that was matched there; the layouts,
 * globals and prototypes they need all come from slice2_19.h.
 */
#include "slice1_05.h"   /* br_globals: its objects */
#include "br_cartypes.h"   /* BrRaceCtl, the canonical record */
#include "slice2_19.h"

/* 0x10019A70 is the (unclaimed, 11 KB) race step.  The original passes its
 * address as an IMMEDIATE, so the matching build needs a function symbol,
 * not a pointer variable.  The port keeps the variable. */
/* BrRaceStep_10019A70: prototype in br_funcs.h */
#define BR_PAD_RACE_STEP ((const void *)BrRaceStep)

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
        uint8_t st = (*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->status;
        if (st != 0) {
            (*(int32_t *)&((BrRaceCtl *)(pPad))->_pad0026[2]) = (st == 8) ? 1 : 0;
            (*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickX = 0;
            (*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickY = 0;
            *(uint16_t *)(void *)&(*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->b0 = 0;
        } else {
            (*(int32_t *)&((BrRaceCtl *)(pPad))->_pad0026[2]) = 0;
        }
    }

    w = *(const uint16_t *)(const void *)&(*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->b0;
    (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) = 0;
    if (w & 0x0800u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl)  = BR_PAD_DUP;
    if (w & 0x0400u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_DDOWN;
    if (w & 0x0200u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_DLEFT;
    if (w & 0x0100u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_DRIGHT;
    if (w & 0x8000u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_A;
    if (w & 0x4000u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_B;
    if (w & 0x0020u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_L;
    if (w & 0x0010u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_R;
    if (w & 0x2000u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_Z;
    if (w & 0x1000u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_START;
    if (w & 0x0008u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CUP;
    if (w & 0x0001u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CRIGHT;
    if (w & 0x0004u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CDOWN;
    if (w & 0x0002u) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CLEFT;

    if (BrGameStepIs(BR_PAD_RACE_STEP)) {
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_L) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_L_ALT;
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_R) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_R_ALT;

        /* Both probes are 16-BIT masks, not byte masks. Spelled as
         * `g_BrPadModeBytes[1] & 0x80` the two 0x80s are one constant in
         * the source, and VC5 pools them into `mov cl,0x80` + two
         * `test byte [eax+n],cl`. Spelled as `& 0x8000` on the halfword,
         * the narrowing to `test byte [eax+n],0x80` happens per
         * instruction, late, and there is nothing left to pool. */
        if (!(*(const unsigned short *)(const void *)g_BrPadModeBytes & 0x8000u)
            && !(*(const unsigned short *)(const void *)(g_BrPadModeBytes + 6)
                 & 0x8000u)) {
            uint32_t a = (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl);
            if (a & BR_PAD_DLEFT) {
                if (!(a & BR_PAD_DRIGHT))
                    (*(int8_t *)&((BrRaceCtl *)(pPad))->b24) = (int8_t)0xB0;
                else
                    (*(int8_t *)&((BrRaceCtl *)(pPad))->b24) = 0;
            } else if (a & BR_PAD_DRIGHT) {
                (*(int8_t *)&((BrRaceCtl *)(pPad))->b24) = 0x50;
            } else {
                (*(int8_t *)&((BrRaceCtl *)(pPad))->b24) = 0;
            }
        } else {
            (*(int8_t *)&((BrRaceCtl *)(pPad))->b24) = (*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickX;
        }

        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_A) {
            if ((*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickY < (int8_t)0xC0)    /* signed, -64 */
                (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_A_BACK;
            (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_A_D;
        }
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_B) {
            if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_A_D)
                (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_B_A;
            else
                (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_B_ALT;
        }
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_CUP)   (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CUP2;
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_CDOWN) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CDOWN2;
        if ((*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) & BR_PAD_CLEFT) (*(uint32_t *)&((BrRaceCtl *)(pPad))->ctl) |= BR_PAD_CLEFT2;
    }

    {
        BrRaceCtl *c = (BrRaceCtl *)pPad;
        if (c->apRec[0] == 0 && c->apRec[1] == 0) {
            /* the original's dead load of pHdr: a volatile READ with no
             * assignment is exactly one mov, no store */
            (void)*(void *volatile *)&c->pHdr;
        } else {
            /* the original walks aLen with an int cursor and reaches apRec
             * at -2 and aCap at +2 */
            int i;
            for (i = 0; i < 2; i++) {
                if (c->apRec[i] != 0) {
                    if (c->aLen[i] < c->aCap[i] && g_BrX06909B4 == 0)
                        c->aLen[i] += 2;
                }
            }
        }
    }

    {
        float t;

        (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[20]) = (float)(*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickX * g_BrK08F548;
        t = (float)(*(int8_t *)&((BrRaceCtl *)(pPad))->b24) * g_BrK08F548;
        (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[24]) = (float)(*(BrPadRaw * *)&((BrRaceCtl *)(pPad))->f158)->stickY * g_BrK08F548;
        (*(float *)&((BrRaceCtl *)(pPad))->steer) = t;

        if ((*(float *)&((BrRaceCtl *)(pPad))->_pad0004[20]) > 1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[20]) = 1.0f;
        else if ((*(float *)&((BrRaceCtl *)(pPad))->_pad0004[20]) < -1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[20]) = -1.0f;

        if ((*(float *)&((BrRaceCtl *)(pPad))->_pad0004[24]) > 1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[24]) = 1.0f;
        else if ((*(float *)&((BrRaceCtl *)(pPad))->_pad0004[24]) < -1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->_pad0004[24]) = -1.0f;

        if (t > 1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->steer) = 1.0f;
        else if (t < -1.0f)
            (*(float *)&((BrRaceCtl *)(pPad))->steer) = -1.0f;
    }
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10059e70: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrPadFrameBegin: prototype in br_funcs.h */
/* BrStubTrue: prototype in br_funcs.h */

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
  BrStubTrue();
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
    BrPadTranslate((BrPad *)((char *)&(*(char *)&g_aBrEnts) + i*0x15c));
    BrBitEdgeSplit((BrBitPair *)((char *)&(*(char *)&g_aBrEnts) + i*0x15c));
  }
}

/* WHAT IT DOES: once per frame, open the pad sampling window: on the first call set the
 * in-progress flag, zero the u16 latch at 0x100B5598 and trace the 0x106B8090 block. */
/* @implements 0x1002CE31 glide BrPadFrameInit */

void BrPadFrameInit(void)

{
  if (DAT_106ec778 == 0) {
    DAT_106ec778 = 1;
    (*(unsigned short *)&DAT_100b51e4[948]) = 0;
    BrStubTrue();
  }
  return;
}

/* WHAT IT DOES: begin the pad frame: init, trace, run 0x10059E70 on the 0x106ED630 block,
 * mark the u16 latch live and clear the in-progress flag. */
/* @implements 0x1002CE5F glide BrPadFrameBegin */

void BrPadFrameBegin(void)

{
  BrPadFrameInit();
  BrStubTrue();
  BrPadPackButtons(&(*(int *)&g_aBrEntRecs));
  (*(unsigned short *)&DAT_100b51e4[948]) = 1;
  DAT_106ec778 = 0;
  return;
}

