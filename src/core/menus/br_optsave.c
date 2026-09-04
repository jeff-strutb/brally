/* br_optsave.c -- menus: BrOptSave, the option-snapshot shuffle at 0x1003E310
 * (BRGlide 0x10037920).
 *
 * Filed out of slice1_06.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice1_06.c -- BRD3D.dll 0x10037030-0x1005D440, a later pass. See slice1_06.h.
 *
 * Constants quoted below were read straight out of orig/BRD3D.dll rather than
 * guessed: 0x1008F62C is 0.0f, the table at 0x100AC660 is nine 8-byte
 * records, and 0x1008F788 is a vtable whose first slot is 0x1005CBF0.
 *
 * CORRECTED: this banner used to say "0x1007DFE0 is calloc(n,1) (it tail-calls
 * 0x1007D370 with a second argument of 1)". IT IS NOT. 0x1007DFE0 is
 * `operator new` == `_nh_malloc(size, 1)`, and the literal 1 is nhFlag, not
 * calloc's element count:
 *
 *   0x1007D370(size, nhFlag) reads nhFlag into edi and uses it for NOTHING
 *   but `test edi,edi / je fail` around `call 0x10082ED0` (_callnewh) and a
 *   retry loop. The allocation is `push esi / call 0x1007D3C0` -- ONE
 *   argument -- and 0x1007D3C0 ends in `HeapAlloc(heap, 0, size)` with flags
 *   ZERO, not HEAP_ZERO_MEMORY.
 *
 * Nothing on that path zeroes. CONVENTIONS.md has always said so ("0x1007DFE0
 * is operator new (_nh_malloc(size,1)) and does not zero"); this file
 * contradicted it, BrUiAssetPathsInit below called calloc on the strength of
 * it, and test_slice1_06.c asserted the resulting zero tail as if it were the
 * original's behaviour. Found while transcribing the Glide twin of the same
 * function (Glide 0x10056260 -> port/src/drawing/br_uiimg.c), where the
 * allocator is the MSVCRT import thunk 0x10074572 -> ??2@YAPAXI@Z and the
 * absence of zeroing is not in doubt.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 0x1003E310
 * ========================================================================== */

/* WHAT IT DOES: takes a snapshot of twelve of the game's option settings into
 * one block, so they can be put back later. The values come from two separate
 * places and are interleaved in a fixed order that is neither array's order --
 * the shuffle is the whole content of the function. */
/* @implements 0x1003E310 d3d BrOptSave */
#ifdef BR_MATCHING_BUILD
/* Twelve loose globals into the packed scratch array, in this exact source
 * order -- the three-ahead load/store interleave is the scheduler's. */
extern int32_t g_br0AC648, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC658,
               g_br0AC65C;                    /* slice2_25.c */
extern int32_t g_brAA2A00, g_brAA2A08, g_brAA2A0C, g_brAA2A18;
extern int32_t g_brAA2A10, g_brAA2A14;       /* slice6_70.c */
extern int32_t g_aBrB4E710[BR_OPT_SCRATCH_COUNT];   /* slice5_63.c */

void BrOptSave(void)
{
    g_aBrB4E710[0]  = g_br0AC648;
    g_aBrB4E710[1]  = g_brAA2A00;
    g_aBrB4E710[2]  = g_brAA2A08;
    g_aBrB4E710[3]  = g_br0AC64C;
    g_aBrB4E710[4]  = g_br0AC650;
    g_aBrB4E710[5]  = g_br0AC654;
    g_aBrB4E710[6]  = g_brAA2A0C;
    g_aBrB4E710[7]  = g_br0AC658;
    g_aBrB4E710[8]  = g_brAA2A10;
    g_aBrB4E710[9]  = g_brAA2A14;
    g_aBrB4E710[10] = g_br0AC65C;
    g_aBrB4E710[11] = g_brAA2A18;
}
#else
void BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc)
{
    pDst->a[0]  = pSrc->aCfg[0];   /* 0x100AC648 -> 0x10B4E710 */
    pDst->a[1]  = pSrc->aSel[0];   /* 0x10AA2A00 -> 0x10B4E714 */
    pDst->a[2]  = pSrc->aSel[2];   /* 0x10AA2A08 -> 0x10B4E718 */
    pDst->a[3]  = pSrc->aCfg[1];   /* 0x100AC64C -> 0x10B4E71C */
    pDst->a[4]  = pSrc->aCfg[2];   /* 0x100AC650 -> 0x10B4E720 */
    pDst->a[5]  = pSrc->aCfg[3];   /* 0x100AC654 -> 0x10B4E724 */
    pDst->a[6]  = pSrc->aSel[3];   /* 0x10AA2A0C -> 0x10B4E728 */
    pDst->a[7]  = pSrc->aCfg[4];   /* 0x100AC658 -> 0x10B4E72C */
    pDst->a[8]  = pSrc->aSel[4];   /* 0x10AA2A10 -> 0x10B4E730 */
    pDst->a[9]  = pSrc->aSel[5];   /* 0x10AA2A14 -> 0x10B4E734 */
    pDst->a[10] = pSrc->aCfg[5];   /* 0x100AC65C -> 0x10B4E738 */
    pDst->a[11] = pSrc->aSel[6];   /* 0x10AA2A18 -> 0x10B4E73C */
}
#endif
