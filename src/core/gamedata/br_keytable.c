/* br_keytable.c -- Key table: g_aBrKeyEnts and BrKeyTableFind, a backwards search of a small
 * biased key table returning the value pair filed under the key.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
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
/* @t3 0x10030FD0 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 81/83 insns 28/29 rows 2+1 regions 1 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * 2026-09-19 respell (81 B, FITS the 83 B image slot): the volatile count
 * read keeps the original's bias-then-count order so both globals share
 * eax's 5-byte form; the dec's flags then feed js directly, eliding the
 * original's test+jl (the one `test R,R` singleton and the 28/29 insn gap).
 * A5 oracle EQUIVALENT on 64 seeds under the 0x10030FD0 profile in
 * tools/oracle_profiles.py (count pinned 0..3 -- a random count is a 2^31
 * runaway -- hit and miss paths both driven).  Dossier and dead list: the
 * comment blocks above.  Do not reopen before the end-grind (CLAUDE.md
 * rule 12). */
/* @t4-pass 0x10030FD0 3 2026-09-19 probes 43 bytes 81 insns 28 regions 1 rows 3 census yes  (tools/crank.py) */
/* @t4-pass 0x10030FD0 4 2026-09-19 probes 43 bytes 81 insns 28 regions 1 rows 3 census yes  (tools/crank.py) */
/* @implements 0x10037930 d3d BrKeyTableFind */
int BrKeyTableFind(uint32_t key, uint32_t *pA, uint32_t *pB)
{
    /* Declaring the count FIRST and biasing the key destructively keeps the
     * key out of eax (count claims it), which turns the old lea back into
     * the original's `add` and frees a register-form global load: 85->84 B,
     * REGNORM 1+1 -> 0+0 (2026-09-09).  RESIDUE (1 B, RAW 2+2): the
     * original loads the BIAS through eax too (5-byte a1 form, between the
     * key load and the add); here the count holds eax at that point so the
     * bias takes the 6-byte ecx form.  One byte, pure assignment. */
    int32_t  i;

    key += g_brKeyBias;
    i = *(volatile int32_t *)&g_brKeyCount - 1;

    /* `dec eax / test eax,eax / jl` -- count == 0 leaves i == -1 and the
     * whole loop is skipped. */
    while (i >= 0) {
        if (key == g_aBrKeyEnts[i].key) {
            *pA = g_aBrKeyEnts[i].a;
            *pB = g_aBrKeyEnts[i].b;
            return 1;
        }
        i--;
    }
    return 0;
}
