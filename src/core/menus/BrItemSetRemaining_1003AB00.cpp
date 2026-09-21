/* WHAT IT DOES: show how many are left -- the entry's allowance minus what
 * has been used -- in this item's label. */
/* @t3 0x1003AB00 2026-09-14 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 198/200 insns 74/75 rows 2+1 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is ONE flag-test row: the original's unfused
 * `sub eax,[used]; test eax,eax; jge` where our cl fuses to `sub; jns` --
 * unproven in all three source corpora and under VC4.2 (see the passes
 * above); everything after is byte-identical shifted 2.  Dossier and
 * dead list live in this header.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1003AB00 glide BrItemSetRemaining_1003AB00
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetRemaining_1003AB00@@YAHPAVObj3AB00@@@Z
 *
 * cdecl, one arg, `ret`, 200 B. Work out how many of something are left
 * -- a table entry chosen by a mode flag, minus a running total, clamped
 * at zero -- print it as decimal, and if the result is not the empty
 * string upper-case it into the owner\'s +0x2B5C item label and relayout
 * (+0x08) / repaint (+0x2C). Returns 1, or 0 when the number came out
 * empty (the original falls out of the strlen with eax already zero).
 *
 * Same 0x438 item record as 0x10041300, and the same label-pointer-in-a-
 * local null test before the repaint vcall as 0x10037EF0.
 *
 * The scratch buffer is memset to 0 first; VC5 hoists the count and fill
 * value above the register saves because edi is not free until after
 * `push edi`.
 *
 * PARKED at 130 diffs, and the cause is ONE 2-byte instruction: the
 * original re-tests the clamp value
 *     sub eax, [g_brUsed5C1C] / test eax,eax / jge +2 / xor eax,eax
 * where ours reuses the subtraction's own flags
 *     sub eax, [g_brUsed5C1C] / jns +2 / xor eax,eax
 * Everything from there to the end is byte-identical, just shifted 2.
 * A scan of 943 byte-exact functions found exactly ONE place our cl
 * emits sub/add-then-test of the same register (0x10038380, and there
 * the tested value is a POINTER formed by `add esi,0x2b65`, not an
 * arithmetic result), so the fused form looks like our cl's rule for
 * integers.
 * DO NOT RE-PROBE -- unchanged by: `v -= x` vs `v = v - x`, a separate
 * result local, the ternary form, `!(v >= 0)`, the CSE form
 * `if (v - x < 0) v = 0; else v = v - x;`, an empty else, and making v
 * unsigned with an `(int)` cast in the comparison. All 130.
 *
 * 2026-09-13 certification passes.  The sub/test/jge triple is UNPROVEN in
 * all three source corpora (--corpus ext 1588, ext2 1515, crt 690 -- zero
 * hits on the 3-insn window) and VC4.2 (vc42_probe) does not produce it
 * either; with the 943-function byte-exact scan already in this header,
 * the unfused form is outside every compiler configuration this project
 * can drive.  In pass 1, `if (v <= -1)` scored 128/130 -- that is a longer
 * `cmp eax,-1; jg` encoding shifting the positional count, NOT the
 * original `test eax,eax; jge` shape; rejected as a wrong-shape patch.
 * The slot census is IDENTICAL slot-for-slot on both sides (three lea
 * anchors, one arg reload; producers match), so the residue is exactly
 * the one flag-test row.
 * @t4-pass 0x1003AB00 1 2026-09-13 probes 10 bytes 198 insns 74 regions 1 rows 3 census no  (cpp_score: v=v-x, fused-assign-in-if, dup-subtract CSE, !(>=0), arm ternary, (int) cast, block-scoped result local, v<=-1 (wrong-shape 128, rejected), branchless &~(v>>31) (131), 0>v -- residue unmoved)
 * @t4-pass 0x1003AB00 2 2026-09-13 probes 10 bytes 198 insns 74 regions 1 rows 3 census yes  (cpp_score: ternary in the _itoa arg, dup-subtract ternary, compare-before-subtract v>=x?v-x:0 (133), empty-else both polarities, += -x, long temp, mask-compare (133), v-=v, v=v-v -- zero movement; slot census identical both sides)
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>
#include <string.h>
#endif

class Item438D {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 */
    virtual void  s2();         /* +0x08 relayout */
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();        /* +0x28 */
    virtual void  s11();        /* +0x2C repaint */

    int   f004;                 /* +0x004 */
    char  b008;                 /* +0x008 */
    char  szName[0x401];        /* +0x009 */
};

typedef char chk_nameD[(unsigned)&((Item438D *)0)->szName == 9 ? 1 : -1];

class Obj3AB00 {
public:
    char    pad000[0x2B5C];
    Item438D m2B5C;              /* +0x2B5C */
};

typedef char chk_itemD[(unsigned)&((Obj3AB00 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
struct BrRec24 { int f00; char pad04[20]; };

/* `extern` is load-bearing: a bare object declaration inside a C++
 * `extern "C" {}` block is a DEFINITION (this TU's own .bss), and the image
 * build then cannot bind the symbol to the real cell. */
extern int      g_brMode5BF4;          /* 0x10AC5BF4 */
extern BrRec24  g_brTbl0B3020[];       /* 0x100B3020 */
extern char     g_brSel5C10;           /* 0x10AC5C10 */
extern int      g_brUsed5C1C;          /* 0x10AC5C1C */
_CRTIMP char *_strupr(char *s);
}

int BrItemSetRemaining_1003AB00(Obj3AB00 *pObj)
{
    char  szNum[32];
    int   v;
    char *pLabel;

    memset(szNum, 0, sizeof(szNum));

    if (g_brMode5BF4 == 0)
        v = g_brTbl0B3020[0].f00;
    else
        v = g_brTbl0B3020[g_brSel5C10].f00;

    v -= g_brUsed5C1C;
    if (v < 0)
        v = 0;

    _itoa(v, szNum, 10);

    if (strlen(szNum) == 0)
        return 0;

    pLabel = pObj->m2B5C.szName;
    strcpy(pLabel, _strupr(szNum));

    pObj->m2B5C.s2();
    if (pLabel != 0)
        pObj->m2B5C.s11();

    return 1;
}
