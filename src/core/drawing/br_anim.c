/* br_anim.c -- drawing: how an animation set is played.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_19.c, an address batch and not a module.  One routine
 * sets the play mode on every animation in a set; the three above it are its
 * named entry points -- once, loop, ping-pong -- and differ only in the flag
 * they pass.  0x1002EC2C is contiguous with them in the original.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* WHAT IT DOES: always answers "yes"; the accepting counterpart of the
 * above. What it is installed as was not established. */
/* @implements 0x1003557B d3d BrRet1_1003557B */
int BrRet1_1003557B(void) { return 1; }

/* WHAT IT DOES: sets how every animation in a set behaves -- play once, loop,
 * or run back and forth -- by turning the relevant switches on and off across
 * all of them at once. The three wrappers just below are the three settings a
 * caller actually asks for. */
/* @implements 0x10035585 d3d BrAnimFlagsApply */
void BrAnimFlagsApply(BrAnimSet *pSet, uint16_t orBits, uint32_t clearBits)
{
    int32_t i, n;
    BrAnimTrack *pT;

    clearBits = ~clearBits;          /* the original's `not eax`, 32-bit,
                                      * in the arg slot */

    /* Nested if (single je-to-epilogue), compound |=/&= (word ops end to
     * end: `or ax, word [ebp+0xc]` / `and ax, word [ebp+0x10]` -- the
     * value-cast spellings widen through eax with masks). */
    if (pSet->pList != NULL) {
        n = pSet->pList->n;
        for (i = 0; i < n; i++) {
            pT = pSet->pList->a[i];
            pT->flags |= orBits;
            pT->flags &= (uint16_t)clearBits;
        }
    }
}

/* The three playback modes a caller actually asks for. Each is one call to
 * BrAnimFlagsApply above with a fixed (set, clear) pair over bits 0 and 1:
 * bit 0 = repeat, bit 1 = reverse on the way back. */

/* WHAT IT DOES: play every animation in the set through ONCE and stop at the
 * end. Clears both the repeat and the bounce-back bits. */
/* @implements 0x1002ECAC glide BrAnimSetOnce */
/* @n64 0x8021D7E0 exact */
void BrAnimSetOnce(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 0, 3); }

/* WHAT IT DOES: play every animation in the set on repeat, restarting from the
 * beginning each time round. Sets repeat, clears bounce-back. */
/* @implements 0x1002ECC1 glide BrAnimSetLoop */
/* @n64 0x8021D804 exact */
void BrAnimSetLoop(BrAnimSet *pSet)     { BrAnimFlagsApply(pSet, 1, 2); }

/* WHAT IT DOES: play every animation in the set back and forth for ever --
 * forwards to the end, then backwards to the start. Sets both bits. */
/* @implements 0x1002ECD6 glide BrAnimSetPingPong */
/* @n64 0x8021D828 exact */
void BrAnimSetPingPong(BrAnimSet *pSet) { BrAnimFlagsApply(pSet, 3, 0); }
