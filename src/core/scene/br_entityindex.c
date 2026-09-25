/* br_entityindex.c -- Entity bank/index: BrEntitySetIndex splits an object number into one of two
 * banks of sixteen and an index within it.
 *
 * Filed out of the address batch slice1_09.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
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
#endif
#include "slice1_09.h"
#ifdef BR_MATCHING_BUILD
#undef BrBitStreamReadBits
#undef BrBitStreamInit
#undef BrBitStreamSkipBytes
#undef BrBitStreamWriteU8
#undef BrBitStreamWriteU24
#undef BrBitStreamWriteU32
#undef BrEntitySetIndex
#undef BrEntityBindAux
#endif

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
/* @t3 0x1006FD50 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 50/50 insns 10/10 rows 1+1 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one instruction-selection fork: original `sub eax,0x10`, VC5
 * `add eax,-0x10` -- MSVC5 canonicalises straight-line constant subtraction
 * to add-negative (paired by t3.py canon, a326268).  The dossier below has
 * the full dead list including VC4.2 cross-evidence; the two ledger lines
 * add the keep-sub mechanism experiments (none fire here).
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10076AE0 d3d BrEntitySetIndex */
#ifdef BR_MATCHING_BUILD
/* thiscall, one stack arg.  Size-exact (50) but encoding-walled:
 * original `sub eax, 0x10`, VC5 `add eax, -0x10`.  `i - 16`, `i -= 16`,
 * unsigned subtract, and inline-in-store all emit the add form under
 * every VC5 flag probed (/O1 /O2 /Os /Ox /G3 /G5, C and C++ front ends).
 * VC4.2 DOES emit `sub eax,0x10` -- but schedules it AFTER the bank
 * store for every probed source order, where the original subs first.
 * Neither compiler reproduces both; parked. */
typedef struct { int n; } BrEntityIndexArg;
/* RESIDUE 2 bytes, FIRSTDIV +0xa, and the whole of it is one instruction:
 * the original has `sub eax,0x10` where we emit `add eax,-0x10`.  That is NOT
 * a spelling choice -- MSVC5 canonicalises every straight-line constant
 * subtraction to add-negative.  Probed and DEAD, do not re-run: `i = i - 16`,
 * `i -= 0x10`, the subtraction inlined into the store, `i = index.n - 16`, a
 * const-propagated `base` local, an in-place bump on the parameter member,
 * and the compile variants /O2 /Op, /O2 /Oy-, /O1 and /Ox.  An isolated
 * one-line probe confirms the rule for int, long, unsigned, short and both
 * pointer spellings.  See the `sub reg, imm` entry in docs/VC5-IDIOMS.md: the
 * three MSVC5 constructs known to keep a real `sub` are a loop-carried
 * decrement, a 16-bit-typed subtraction whose result stays live narrow, and a
 * pointer difference feeding further arithmetic -- this function fits none of
 * them, so the answer is still open.  It is NOT the operator. */
void __fastcall BrEntitySetIndex(void *pEntity, BrEntityIndexArg index)
{
    int i = index.n;
    if (i >= 16) {
        i -= 16;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 1;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    } else {
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_BANK)  = 0;
        *(int *)((unsigned char *)pEntity + BR_ENTITY_OFF_INDEX) = i;
    }
}
#else
void BrEntitySetIndex(void *pEntity, int index)
{
    unsigned char *p = (unsigned char *)pEntity;
    int *pIndex = (int *)(void *)(p + BR_ENTITY_OFF_INDEX);
    int *pBank  = (int *)(void *)(p + BR_ENTITY_OFF_BANK);

    if (index >= 16) {
        *pBank  = 1;
        *pIndex = index - 16;
    } else {
        *pBank  = 0;
        *pIndex = index;
    }
}
#endif
