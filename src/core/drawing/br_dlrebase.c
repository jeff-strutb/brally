/* br_dlrebase.c -- drawing: making a loaded display list point at real memory.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_19.c, an address batch and not a module.  A display
 * list that comes off disk still holds the console's segmented addresses;
 * these walk it and turn each of them into somewhere that exists in this
 * process, and 0x1002DB0B is the per-model entry point that drives them.
 * The stub at 0x1002E70A is contiguous with them in the original.
 *
 * 0x10018A40 came from slice6_78.c: it sets the one flag the .rca loader
 * reads to decide whether to do its copying at all, which is the same
 * load-time fixup pass.
 *
 * slice2_19.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdarg.h>
#include "br_path.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_78.h"



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

/* WHAT IT DOES: return 0. */
/* @implements 0x1002E70A glide BrRet0_1002E70A */

int BrRet0_1002E70A(void)

{
  return 0;
}

/* WHAT IT DOES: corrects one address inside loaded data that still refers to
 * where the data used to live, shifting it to where it now sits -- and leaves
 * it alone if it points outside the block being moved. */
/* @implements 0x10035060 d3d BrDlRebaseWord */
/* @n64 0x8021D070 exact */
void BrDlRebaseWord(uint32_t *pWord, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (*pWord >= lo && *pWord < hi)
        *pWord = *pWord - lo + base;
}

/* WHAT IT DOES: walks a list of drawing commands just loaded from disk and
 * corrects the addresses inside it -- the ones naming where a model's corner
 * points and its textures live -- so that they point at where the data
 * actually is in memory. It stops at the command that ends the list. */
/* @implements 0x10035089 d3d BrDlRebase */
/* @n64 0x8021D098 located */
/* THREE things are load-bearing here, all visible only at /Od.
 *
 * (1) The null test is a WRAPPING if, not an early return: `if (p) { ... }`
 *     emits the single inverted `je end` the original has, while
 *     `if (!p) return;` emits `jne over / jmp end`.
 * (2) The step belongs in the for's THIRD clause.  `for (;; pDl += 2)` puts
 *     the increment at the TOP of the loop with a `jmp` over it on the first
 *     pass, which is 1002E744..1002E74C; writing `pDl += 2;` as the last
 *     statement of the body puts it at the bottom instead.
 * (3) It is a SWITCH on the EXPRESSION.  The original's compare chain runs
 *     4, 0xB8, 0xFD -- ASCENDING, with 4 and 0xFD sharing a target -- which
 *     an if/else chain cannot produce (it would test 4, 0xFD, 0xB8 in source
 *     order).  Switching on a NAMED local costs a second frame slot, because
 *     VC5 copies the value into its own switch temp; switching on the
 *     expression makes that temp the function's only local and restores the
 *     `push ecx` prologue. */
void BrDlRebase(uint32_t *pDl, uint32_t lo, uint32_t hi, uint32_t base)
{
    if (pDl != NULL) {
        for (;; pDl += 2) {
            switch ((pDl[0] >> 24) & 0xFFu) {
            case 0x04u:                          /* G_VTX     */
            case 0xFDu:                          /* G_SETTIMG */
                BrDlRebaseWord(&pDl[1], lo, hi, base);
                break;
            case 0xB8u:                          /* G_ENDDL   */
                return;
            }
        }
    }
}

/* WHAT IT DOES: prepares one loaded model for drawing: it sets a global flag
 * from the current game mode unless the model asks to be left alone, then
 * scans the model's drawing commands and marks the model if that scan reports
 * a hit. What the flag and the mark ultimately control was not established
 * here, so the purpose beyond "per-model preparation" is unclear. */
/* @implements 0x1003445A d3d BrDlOwnerFixup */
void BrDlOwnerFixup(BrDlOwner *pOwner)
{
    /* A TERNARY, not an if/else.  At /Od the ternary's value lands in a
     * compiler temp at [ebp-8] and is then copied into `want` at [ebp-4],
     * which is where the original's `sub esp, 8` -- two dwords for one named
     * local -- comes from.  An if/else writes `want` directly and needs only
     * four bytes of frame. */
    int32_t want;

    g_Br6C666C = 0;

    want = (g_Br0B380C == 2 || g_Br0B380C == 8) ? 0 : 1;

    if ((pOwner->flags & 4u) == 0)
        g_Br6C666C = want;

    /* Compound `|=`, not a read-modify-write through a widening cast: the
     * original reads the halfword straight into cx and ors the low byte
     * (`mov cx,[eax+0x4c]; or cl,8`).  Spelling it as
     * `flags = (uint16_t)(flags | 8u)` adds the `xor edx,edx` zero-extension
     * the original does not have. */
    if (BrSub100341B3(pOwner->pDl, g_BrDlTableA))
        pOwner->flags |= 8u;
}

/* 0x1002B9D0 */
/* WHAT IT DOES: sets a single global flag. Worth knowing: the same storage
 * is what the .rca loader reads to decide whether to do its copying at all,
 * so anything that clears this switches that copying off. */
/* @implements 0x10018A40 glide BrSegSetFlag */
void BrSegSetFlag(uint32_t v)
{
    g_br675540 = (int32_t)v;
}
