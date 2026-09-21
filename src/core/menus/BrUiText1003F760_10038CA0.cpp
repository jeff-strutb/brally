/* WHAT IT DOES: choose this item's caption -- a fixed string on the root
 * page, otherwise one looked up from a table by the current selection, with
 * a wrap at 16 entries. */
/* @t3 0x10038CA0 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 136/136 insns 48/48 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: one register choice -- the catalogue-table load feeds `push` through ecx where the original uses eax (rows 0+0 after regnorm). Dossier and dead list in this header (23 cpp probes across two passes).
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10038CA0 glide BrUiText1003F760
 * @cpp_kind free
 * @cpp_symbol ?BrUiText1003F760@@YAHPAVObj38CA0@@@Z
 *
 * cdecl, one arg, `ret`, 100 B. Put the catalogue string the +0x5D80
 * selector points at into the owner's +0x2B5C item label, relayout through
 * the +0x04 vcall and apply. Returns 1.
 *
 * Same 0x438 item record as 0x10041300. One of three identical siblings
 * (0x10038CA0 / 0x10039350 / 0x10039510) differing only in the selector
 * global and the index table.
 *
 * The port body in slice2_23.c reaches its globals through a BrUiGlobals*
 * second parameter; the original is cdecl with ONE argument and direct
 * global addresses. Same split as the 0x10038650 sibling.
 *
 * @t4-pass 0x10038CA0 1 2026-09-13 probes 12 bytes 136 insns 48 regions 1 rows 0 census no  (cpp harness: table load/push register: index deref, table pointer, unsigned/long/register k, wrap compare, const s, per-arm strcpy, dst/item locals, negated guard, sized table)
 * @t4-pass 0x10038CA0 2 2026-09-13 probes 12 bytes 136 insns 48 regions 1 rows 0 census yes  (cpp harness: slot census (one arg read): /Op /Oy- /Os /Ot, !flag, arm swap, ret local, object local, s = 0, &szName[0], decimal index)
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item38CA0 {
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
};

typedef char chk_name38CA0[(unsigned)&((Item38CA0 *)0)->szName == 9 ? 1 : -1];

class Obj38CA0 {
public:
    char       pad000[0x2B5C];
    Item38CA0 m2B5C;          /* +0x2B5C */
};

typedef char chk_item38CA0[(unsigned)&((Obj38CA0 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int  g_brPhase5C5C;             /* 0x10AC5C5C */
int  g_brRoot5CBC;              /* 0x10AC5CBC */
int  g_brFlag5C40;              /* 0x10AC5C40 */
int  g_brSel0ABDF4;             /* 0x100ABDF4 */
int  g_brTblABB08[];            /* 0x100ABB08 */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

/* DEAD 2026-09-13 (11 cpp probes): the table value carried in `k` itself,
 * an `idx` local declared before or after `s`, `k` at function scope, the
 * wrap as a ternary (inline, in a local, in the index), and a single
 * `idx` feeding one shared call.  The single-call form puts the table
 * load in eax as the original does but turns `push 0x14` into
 * `mov eax,0x14` (5 diffs); every two-call form keeps `push 0x14` and
 * loads the table into ecx (2 diffs).  The original has both. */
int BrUiText1003F760(Obj38CA0 *pObj)
{
    char *s;

    if (g_brPhase5C5C == g_brRoot5CBC && g_brFlag5C40 == 0) {
        s = BrStrByIndex(0x14);
    } else {
        int k = g_brSel0ABDF4;

        if (k > 0xF)
            k -= 0x10;

        s = BrStrByIndex(g_brTblABB08[k]);
    }

    strcpy(pObj->m2B5C.szName, s);

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
