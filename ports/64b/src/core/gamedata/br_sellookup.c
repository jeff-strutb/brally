/* br_sellookup.c -- Selector table lookup: BrSelLookup reads a pair of values from a 12-column
 * byte table by two selector bytes, optionally rotating the first by six.
 *
 * Filed out of the address batch slice1_05.c; its preamble is carried verbatim.
 */

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
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup

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
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x1002F460 d3d BrSelLookup */
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

void BrSelLookup(void)
{
    BrSelInM *p = (*(BrSelInM * *)&g_aBrRaceCar[0].pEquip);
    int idx = p->f04 * 12 + p->f05;
    unsigned char t;
    int a;

    t = (*(unsigned char (*)[])&g_aBr0B3820)[idx * 2];
    a = t;
    (*(int *)&g_Br0B380C) = a;

    t = p->f00;
    if (t & 1) {
        if (a < 6)
            a += 6;
        else
            a -= 6;
        (*(int *)&g_Br0B380C) = a;
    }

    /* recomputed, so the fold above cannot leak into the second lookup */
    idx = p->f04 * 12 + p->f05;
    DAT_104b15e8 = (*(unsigned char (*)[])&g_aBr0B3820[1])[idx * 2];
}
