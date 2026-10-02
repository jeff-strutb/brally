/* WHAT IT DOES: build the caption for the pick item -- a fixed string on the
 * root page, otherwise a name assembled from the current mode and selection. */
/* @t3 0x10038F40 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 567/567 insns 172/172 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: the eax/ecx/edx rotation of the three index-lookup arms (rows 0+0 after regnorm). Dossier in this header; dead list = the two ledger passes (23 cpp probes).
 * Do not reopen before the end-grind. */
/* @implements 0x10038F40 glide BrItemSetPickLabel_10038F40
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetPickLabel_10038F40@@YAHPAVObj38F40@@@Z
 *
 * cdecl, one arg, `ret`, 567 B. Put the name of the current selection
 * into the owner's +0x2B5C item label. One special case (root phase, spare
 * flag clear) uses a fixed catalogue entry; otherwise the entry is looked
 * up three different ways into a scratch buffer, and when the selection's
 * descriptor carries the 0x10 bit a "locked" caption is flashed through
 * the item first -- the owner's +0x40 geometry is parked, the item's
 * +0x414 forced to a constant, the caption laid out and applied, and
 * +0x414 restored from the parked value.
 *
 * Same 0x438 item record as 0x10041300. The parked geometry is FLOAT --
 * both the fields and the local. That single typing decision is worth
 * 415 diffs here. Typed int, the local lives in a callee-saved register,
 * so the prologue grows a `push ebp` and the frame SHRINKS by the 4 bytes
 * the original reserves for it. Typed float it has to be homed, which is
 * what the original does; the copies in and out are still plain integer
 * `mov`s (VC5 moves float memory with GP registers), and the constant is
 * a straight immediate store -- all of which read like int code and are
 * exactly why the int typing looked right at first.
 * TELL: recomp has one more callee-saved push than the original and a
 * frame that is 4 bytes SMALLER. That pairing means an int local where
 * the original had a float one.
 *
 * Two structural points worth keeping:
 *  - the special case and the ordinary path both end in
 *    `strcpy(label, <something>)`, and VC5 MERGES the two inline strcpy
 *    expansions: the special case jumps straight into the tail's copy with
 *    its source already in edi. Writing them as one shared tail is what
 *    produces that.
 *  - each arm's index expression appears TWICE in the source -- once
 *    inside the catalogue lookup and once for the descriptor -- because
 *    the original recomputes it after the copy (the globals are reloaded
 *    across the call). Caching it in a local before the strcpy does not
 *    match.
 *
 * PARKED at 10 diffs, T3a. Only the FIRST index arm differs, and only in
 * register naming (orig loads the global into ecx and lands the map byte
 * in eax; ours uses edx and ecx). The other two arms and everything else
 * are byte-identical, and the original's own two arms do not agree with
 * each other either, so this is the allocator.
 * DO NOT RE-PROBE -- unchanged by swapping the addition's operand order in
 * either arm, and swapping the two arms costs one more diff.
 *
 * @t4-pass 0x10038F40 1 2026-09-13 probes 11 bytes 567 insns 172 regions 1 rows 0 census no  (cpp harness: k at function scope (before/after buffer), index term order, map deref, descriptor local, save split, arm swaps, string local, ret local, table pointer)
 * @t4-pass 0x10038F40 2 2026-09-13 probes 12 bytes 567 insns 172 regions 1 rows 0 census yes  (cpp harness: slot census (save slot + two arg reads): /Op /Oy- /Os /Ot, shifted stride, unsigned k, signed map, guard swap, inline relayout helper, save at function scope, src local, int cast)
 */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* BrTextBox, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
#include <string.h>

class Item438L {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;
    float f414;                 /* +0x414 */
};




class Obj38F40 {
public:
    char     pad000[0x40];
    float    f040;              /* +0x040 */
    char     pad044[0x2B5C - 0x44];
    Item438L m2B5C;             /* +0x2B5C */
};



struct BrDesc38F40 {
    char pad00[4];
    int  f04;                   /* +0x04 -- bit 4 = locked */
};

extern "C" {
/* 64-bit core: g_brPhase5C5C is defined once, in br_globals.c */
/* 64-bit core: g_brRoot5CBC is defined once, in br_globals.c */
/* 64-bit core: g_brFlag5C40 is defined once, in br_globals.c */
/* 64-bit core: g_brFlag0A9360 is defined once, in br_globals.c */
/* 64-bit core: g_brFlag5C00 is defined once, in br_globals.c */
/* 64-bit core: g_brSel5C10 is defined once, in br_globals.c */
/* 64-bit core: g_brIdx5C04 is defined once, in br_globals.c */
/* 64-bit core: g_brIdx5BFC is defined once, in br_globals.c */
/* 64-bit core: g_brIdx0ABDE8 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrStrByIndex: prototype in br_funcs.h */
/* BrItemApply_10038380: prototype in br_funcs.h */
}

int BrItemSetPickLabel_10038F40(Obj38F40 *pObj)
{
    char szName[128];

    if (g_brPAA29B8 == DAT_10ac5cbc && DAT_10ac5c40 == 0) {
        strcpy((*(char (*)[1025])&((BrTextBox *)&(pObj->m2B5C))->sz[0]), BrStrGet(0x1B));
    } else {
        int k;

        if ((*(int *)&g_brRaceRules.mode) == 0) {
            if (DAT_10ac5c00 != 0) {
                strcpy(szName, BrStrGet(g_brTblABAA8[
                    (*(unsigned char (*)[])&g_aBr0B3820)[(g_brIdx5C04 + (*(char *)&DAT_10ac5c10) * 12) * 2]]));
                k = (*(unsigned char (*)[])&g_aBr0B3820)[(g_brIdx5C04 + (*(char *)&DAT_10ac5c10) * 12) * 2];
            } else {
                strcpy(szName, BrStrGet(g_brTblABAA8[
                    (*(unsigned char (*)[])&g_aBr0B3820)[(g_brIdx5BFC + (*(char *)&DAT_10ac5c10) * 12) * 2]]));
                k = (*(unsigned char (*)[])&g_aBr0B3820)[(g_brIdx5BFC + (*(char *)&DAT_10ac5c10) * 12) * 2];
            }
        } else {
            strcpy(szName, BrStrGet(g_brTblABAA8[g_brIdx0ABDE8]));
            k = g_brIdx0ABDE8;
        }

        if ((*(BrDesc38F40 * (*)[])&g_apBrRaceDiff)[k]->f04 & 0x10) {
            float save = pObj->f040;

            (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) = 130.0f;

            strcpy((*(char (*)[1025])&((BrTextBox *)&(pObj->m2B5C))->sz[0]), BrStrGet(0xB0));

            (BR_VFN(&(pObj->m2B5C), 1, void (*)(void *)))(&(pObj->m2B5C));
            Br85ItemApply((struct BrCtl85 *)(pObj), 0);

            (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) = save;
        }

        strcpy((*(char (*)[1025])&((BrTextBox *)&(pObj->m2B5C))->sz[0]), szName);
    }

    (BR_VFN(&(pObj->m2B5C), 1, void (*)(void *)))(&(pObj->m2B5C));
    Br85ItemApply((struct BrCtl85 *)(pObj), 0);

    return 1;
}
