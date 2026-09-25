/* br_sellookup.c -- Selector table lookup: BrSelLookup reads a pair of values from a 12-column
 * byte table by two selector bytes, optionally rotating the first by six.
 *
 * Filed out of the address batch slice1_05.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
/* The originals of the vtx-cache cluster take no BrVtxCache parameter --
 * state is loose globals -- and BrVtxExpand/Insert/Resolve have different
 * arities. Hide the header's port prototypes behind renames so the
 * matching twins can define the real symbols with the original
 * signatures; other TUs keep calling with the port signatures (cdecl, so
 * the extra leading argument is harmless at run time). */
#define BrVtxExpand       BrVtxExpand_hdr
#define BrVtxCacheInsert  BrVtxCacheInsert_hdr
#define BrVtxCacheResolve BrVtxCacheResolve_hdr
#define BrSelLookup       BrSelLookup_hdr
#define BrPtrListAdd      BrPtrListAdd_hdr
#define BrF3DVtxFixup     BrF3DVtxFixup_hdr
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#else
#include "slice1_05.h"
#include "br_gamestep.h"   /* 0x10034C66/0x10034C73 == BRGlide 0x1002E317/0x1002E324 */
#endif

#include <stddef.h>

/* 0x1002F460 */
/* WHAT IT DOES: purpose unclear. Observably it reads a pair of numbers out of
 * a table using two selector bytes as a row and column, and if one flag bit is
 * set it rotates the first of the pair by half a turn of twelve -- six becomes
 * zero, zero becomes six -- which has the shape of a mirroring or opposite-
 * direction rule. The second number is looked up fresh so the rotation cannot
 * affect it. What the table describes is not established here. */
/* Residue: a whole-body eax<->ecx transposition (REGNORM 0+0, one byte net
 * on the two absolute-load encodings) -- the input pointer takes ecx here,
 * eax in the original.  DEAD 2026-09-09, all identical: idx operand order
 * (both sites); idx assigned after declaration or split into *12 then +=;
 * named uchar locals for the two selector bytes; explicit int casts;
 * declaring idx before p; folding the a=t copy; uchar a (+25 B); folding
 * the flag temp; every slot in the TU (9 of 17 compile).  Corpus MISS on
 * the 6-insn opening.
 * @t4-pass 0x1001C9D0 1 2026-09-09 probes 10 bytes 95 insns 30 regions 1 rows 0 census yes  (hand, fn.py variants + corpus)
 * @t4-pass 0x1001C9D0 2 2026-09-09 probes 12 bytes 95 insns 30 regions 1 rows 0 census yes  (position sweep + const p, explicit !=0, column-base alias) */
/* @t3 0x1001C9D0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 95/96 insns 30/30 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 1 masked region, 1 B short on encoding;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1002F460 d3d BrSelLookup */
#ifdef BR_MATCHING_BUILD
/* Original: no parameters. The input record comes through a pointer
 * global, the table is two interleaved pinned byte columns (0x100B3028 /
 * 0x100B3029), and the results are globals. A shared unsigned-char temp
 * carries first the table byte (edx, copied into the int a) and then the
 * flag byte (dl reloaded without re-zeroing). */
typedef struct BrSelInM {
    unsigned char f00;
    unsigned char pad[3];
    unsigned char f04;
    unsigned char f05;
} BrSelInM;

extern BrSelInM     *DAT_10af2094;
extern unsigned char DAT_100b3028[];
extern unsigned char DAT_100b3029[];
extern int           DAT_100b3014;
extern int           DAT_104b15e8;

void BrSelLookup(void)
{
    BrSelInM *p = DAT_10af2094;
    int idx = p->f04 * 12 + p->f05;
    unsigned char t;
    int a;

    t = DAT_100b3028[idx * 2];
    a = t;
    DAT_100b3014 = a;

    t = p->f00;
    if (t & 1) {
        if (a < 6)
            a += 6;
        else
            a -= 6;
        DAT_100b3014 = a;
    }

    /* recomputed, so the fold above cannot leak into the second lookup */
    idx = p->f04 * 12 + p->f05;
    DAT_104b15e8 = DAT_100b3029[idx * 2];
}
#else
void BrSelLookup(const BrSelInput *pIn, const unsigned char (*aTable)[2],
                 int *pOutA, int *pOutB)
{
    int idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    int a   = (int)aTable[idx][0];

    *pOutA = a;

    if (pIn->f00 & 1) {
        a = (a >= 6) ? (a - 6) : (a + 6);
        *pOutA = a;
    }

    /* Recomputed from f04/f05 in the original, so the fold above cannot
     * leak into the second lookup. */
    idx = (int)pIn->f04 * 12 + (int)pIn->f05;
    *pOutB = (int)aTable[idx][1];
}
#endif
