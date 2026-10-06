/* br_keytable.c -- Key table: g_aBrKeyEnts and BrKeyTableFind, a backwards search of a small
 * biased key table returning the value pair filed under the key.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#undef BrOptSave
#undef BrOptAvailB

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x10037930
 * ========================================================================== */

/* WHAT IT DOES: looks a key up in a small table and hands back the pair of
 * values filed against it, reporting whether it found anything. The key is
 * shifted by the table's own offset first, and the search runs from the end
 * backwards, so where a key appears twice the later entry wins. */
BrKeyEnt g_aBrKeyEnts[BR_KEYTABLE_MAX];  /* 0x106EEF0C */
int32_t  g_brKeyCount;                   /* 0x10AC0808 */
uint32_t g_brKeyBias;                    /* 0x10AC080C */

/* DEAD 2026-09-09 at 84 B (all identical): a bias local; non-compound add;
 * i assigned after declaration; i via -=1; unsigned param; pA[0] stores;
 * for-loop; --i; a cast on the bias; every slot in the TU (12).  Corpus
 * MISS on the 5-insn opening.
 * @t4-pass 0x10030FD0 1 2026-09-09 probes 19 bytes 84 insns 29 regions 1 rows 0 census yes  (hand, fn.py variants; k-battery found the -1 B spelling, m-battery zero movement)
 * @t4-pass 0x10030FD0 2 2026-09-09 probes 12 bytes 84 insns 29 regions 1 rows 0 census yes  (position sweep) */
/* @t4-pass 0x10030FD0 3 2026-09-19 probes 43 bytes 81 insns 28 regions 1 rows 3 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10030FD0 4 2026-09-19 probes 43 bytes 81 insns 28 regions 1 rows 3 census yes  (tools/brally/crank.py) */
/* @implements 0x10037930 d3d BrKeyTableFind */
int BrKeyTableFind(uint32_t key, uint32_t *pA, uint32_t *pB)
{
    /* A plain for loop: its entry test is the original's unfused `dec eax /
     * test eax,eax / jl` (count == 0 skips the loop); a while loop over a
     * precomputed i fuses it into `dec / js`. */
    int32_t  i;

    key += g_brKeyBias;
    for (i = g_brKeyCount - 1; i >= 0; i--) {
        if (key == g_aBrKeyEnts[i].key) {
            *pA = g_aBrKeyEnts[i].a;
            *pB = g_aBrKeyEnts[i].b;
            return 1;
        }
    }
    return 0;
}
