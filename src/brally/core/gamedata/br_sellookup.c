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

extern BrSelInM     *DAT_10af2094;
extern unsigned char DAT_100b3028[];
extern unsigned char DAT_100b3029[];
extern int           DAT_100b3014;
extern int           DAT_104b15e8;

int BrSelLookup(void)
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
    /* The original leaves the table index in eax; returning it is what puts
     * the record pointer in eax and the setting in ecx through the body
     * (void swaps them).  Callers go through a pointer and ignore it. */
    return idx;
}
