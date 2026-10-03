/* br_entityindex.c -- Entity bank/index: BrEntitySetIndex splits an object number into one of two
 * banks of sixteen and an index within it.
 *
 * Filed out of the address batch slice1_09.c; its preamble is carried verbatim.
 */

/* slice1_09.h declares these cdecl; the originals are thiscall with stack
 * args.  Hide those prototypes so the matching bodies can use __fastcall
 * plus a struct-typed second argument (never register-eligible, so forced
 * onto the stack).  Same split as thiscall; do not redefine BR_THISCALL. */
#define BrBitStreamReadBits  BrBitStreamReadBits_cdecl
#define BrBitStreamInit      BrBitStreamInit_cdecl
#define BrBitStreamSkipBytes BrBitStreamSkipBytes_cdecl
#define BrBitStreamWriteU8   BrBitStreamWriteU8_cdecl
#define BrBitStreamWriteU24  BrBitStreamWriteU24_cdecl
#define BrBitStreamWriteU32  BrBitStreamWriteU32_cdecl
#define BrEntitySetIndex     BrEntitySetIndex_cdecl
#define BrEntityBindAux      BrEntityBindAux_cdecl
#include "slice1_09.h"
#undef BrBitStreamReadBits
#undef BrBitStreamInit
#undef BrBitStreamSkipBytes
#undef BrBitStreamWriteU8
#undef BrBitStreamWriteU24
#undef BrBitStreamWriteU32
#undef BrEntitySetIndex
#undef BrEntityBindAux

#include <math.h>
#include <stddef.h>

/* ================================================================== */
/* Entity array offsets                                                */
/* ================================================================== */

/* 0x10076AE0  __thiscall, ret 4. `cmp eax,0x10 / jl` -- signed. */
/* WHAT IT DOES: records which of two banks of sixteen an object belongs to.
 * Anything numbered sixteen or above is stored as the second bank with its
 * number reduced by sixteen; anything below it is the first bank. */
/* @t4-pass 0x1006FD50 1 2026-09-09 probes 10 bytes 50 insns 10 regions 1 rows 2 census no  (hand, fn.py variants: the dossier dead list rerun -- minus/hex/inline/member/unsigned/neg-add spellings, store orders, guard flips, all inert or worse) */
/* @t4-pass 0x1006FD50 2 2026-09-09 probes 10 bytes 50 insns 10 regions 1 rows 2 census yes  (hand, fn.py variants: mechanism experiment on the three known keep-sub constructs -- pointer difference, loop-carried, narrow-typed -- all still emit add-negative; plus cond-expr/volatile/two-store forms, worse) */
/* @implements 0x10076AE0 d3d BrEntitySetIndex */
typedef struct { int n; } BrEntityIndexArg;
/* thiscall, one stack arg.  MSVC5 emits a straight-line constant subtraction
 * as `add reg,-K` when the result is a fresh value, but keeps `sub reg,K`
 * when a variable is decremented IN PLACE and stays live across a join.  So
 * the parameter itself is bumped and the index store follows the if/else;
 * VC5 tail-duplicates that store into both arms.  A copied local (`int i =
 * index.n`) is not the parameter and gets the add form. */
void __fastcall BrEntitySetIndex(void *pEntity, BrEntityIndexArg index)
{
    if (index.n >= 16) {
        index.n -= 16;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK) = 1;
    } else {
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK) = 0;
    }
    *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = index.n;
}
