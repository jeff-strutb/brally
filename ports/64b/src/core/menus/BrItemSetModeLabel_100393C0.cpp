/* WHAT IT DOES: set the mode item's caption, and on the root page also nudge
 * it up the screen -- the root layout has one line less above it, so the
 * item moves rather than the page being laid out twice. */
/* @t4-pass 0x100393C0 1 2026-09-24 probes 16 bytes 328 insns 101 regions 1 rows 0 census no  (hand, after the shared index->string call reached 328/330: the index local as short/uchar/long/unsigned/pointer/value, split index temps, the 0x5C00 arm through k, else-if chain; the k-in-eax choice never moved) */
/* @t4-pass 0x100393C0 2 2026-09-24 probes 10 bytes 328 insns 101 regions 1 rows 0 census yes  (hand, corpus query at +0xD1: the 12-instruction residue window is not in the corpus past 3 instructions; plus ten index-expression and declaration spellings, all identical) */
/* @t3 0x100393C0 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 328/330 insns 101/101 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one register choice: the shared index for the 0x5BFC and 0x5D58
 * arms lands in eax where the original has ecx (2 bytes, same instructions).
 * Do not reopen before the end-grind. */
/* @implements 0x100393C0 glide BrItemSetModeLabel_100393C0
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetModeLabel_100393C0@@YAHPAVObj393C0@@@Z
 *
 * cdecl, one arg, `ret`, 330 B. Put a catalogue string into the owner's
 * +0x2B5C item label and relayout. Which string depends on the state: in
 * the one special case (the current phase is the root one and the extra
 * flag is clear) it is a fixed entry and the item's +0x414 geometry field
 * is nudged twice, once before the text is set and once after the apply
 * hook; otherwise the entry comes from an index table reached three
 * different ways.
 *
 * Same 0x438 item record as 0x10041300 -- here its +0x414 field is read
 * as a FLOAT (fld/fsub/fstp), which is why this TU types it that way.
 *
 * The special-case arm and everything up to the index selection is
 * byte-exact; the arm ORDER matters and is settled: testing
 * `DAT_100a9360 == 0` with the odd-one-out arm as the ELSE is what puts
 * that block last, the way the original does (writing it as the first
 * `if` arm costs 26 diffs).
 *
 * PARKED at 94 diffs. What is left is how deep VC5 tail-merges the three
 * index arms, and it is allocator-driven, not source-driven. The original
 * merges TWO of the three -- its 5BFC and 5D58 arms both leave the index
 * in ecx and share the final `ABB50[k]` lookup, while the 5C04 arm leaves
 * it in eax and keeps its own copy (330 B). Measured alternatives:
 *   this shape, three full calls               352 B, 94 diffs
 *   one shared `k` and one shared lookup       320 B, 99 diffs (merges
 *                                              all three -- 10 B SHORT)
 *   shared `k` in two arms, inline in the 5C04 336 B, 101 diffs
 * Since the clean single-lookup source comes out SHORTER than the
 * original, the likelier reading is that the original's source is that
 * clean form and its allocator failed to merge the third arm. Either way
 * no source shape lands on 330. DO NOT RE-PROBE the three above.
 */
#define _CRTIMP __declspec(dllimport)
#include "br_race.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* BrTextBox, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
#include <string.h>

class Item438K {
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

    int   f004;                 /* +0x004 */
    char  b008;                 /* +0x008 */
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;                 /* +0x410 */
    float f414;                 /* +0x414 */
};




class Obj393C0 {
public:
    char      pad000[0x2B5C];
    Item438K  m2B5C;            /* +0x2B5C */
};



extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrStrByIndex: prototype in br_funcs.h */
/* BrItemApply_10038380: prototype in br_funcs.h */
}

int BrItemSetModeLabel_100393C0(Obj393C0 *pObj)
{
    char *s;

    if (g_brPAA29B8 == DAT_10ac5cbc && DAT_10ac5c40 == 0) {
        (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) = (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) - DAT_10077628;

        strcpy((*(char (*)[1025])&((BrTextBox *)&(pObj->m2B5C))->sz[0]), BrStrGet(0x1C));

        pObj->m2B5C.s1();
        Br85ItemApply((struct BrCtl85 *)(pObj), 0);

        (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) = (*(float *)&((BrTextBox *)&(pObj->m2B5C))->y) - DAT_1007762c;
        return 1;
    }

    /* 2026-09-24: the 0x10AC5C00 arm makes its own call; the other two
     * arms only pick an index and share one BrStrByIndex(tbl[k]) call.
     * RESIDUE 2 bytes: k lands in eax where the original has ecx. */
    {
        int k;
        if ((*(int *)((char *)&g_brRaceRules + 0xC)) /* BR_LP64_BYTE_VIEW */ == 0) {
            if (DAT_10ac5c00 != 0) {
                s = BrStrGet((*(int (*)[])&g_aBrAC3B0)[
                        (*(unsigned char (*)[])&g_aBr0B3820[1])[(g_brIdx5C04 + (*(char *)&DAT_10ac5c10) * 12) * 2]]);
                goto have;
            }
            k = (*(unsigned char (*)[])&g_aBr0B3820[1])[(g_brIdx5BFC + (*(char *)&DAT_10ac5c10) * 12) * 2];
        } else {
            k = DAT_10ac5d58;
        }
        s = BrStrGet((*(int (*)[])&g_aBrAC3B0)[k]);
    }
have:
    strcpy((*(char (*)[1025])&((BrTextBox *)&(pObj->m2B5C))->sz[0]), s);

    pObj->m2B5C.s1();
    Br85ItemApply((struct BrCtl85 *)(pObj), 0);

    return 1;
}
