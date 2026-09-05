/* WHAT IT DOES: put the name of the key or button currently bound to the
 * selected control action into the owner's item label, then relayout and
 * apply.  With the "no binding" flag set the label reads catalogue string
 * 0xB2; otherwise the control kind picks the table -- keyboard names for
 * kind 0, the two device name tables for kinds 1-3 -- and a device kind whose
 * lookup fails falls back to the keyboard name of the raw code, or to string
 * 0xB1 when there is no code at all.  An out-of-range kind writes no label.
 * Returns 1. */
/* @implements 0x10039620 glide BrCtlBindingToItem
 * @cpp_kind free
 * @cpp_symbol ?BrCtlBindingToItem@@YAHPAVObj39620@@@Z
 *
 * cdecl, one arg, `ret`, 563 B.  The two lookups are thiscall members of the
 * global input object at 0x10B71290 called with a CONSTANT kind
 * (`push k / mov ecx,offset / call`), which is why this is a C++ TU: from C
 * the __fastcall-plus-wrapper spelling materialises the constant in a
 * register first (VC5-IDIOMS "thiscall with 3+ arguments", boundary).
 * Same item record and the same relayout-then-apply tail as 0x10038E10.
 *
 * Shape notes from the bytes:
 *  - the binding key is RE-READ from the global table for the second lookup
 *    (two `mov eax,[g_brSel5B98]`), as 0x10039870 warns -- do not cache it;
 *  - the byte answer of the first lookup lives across the second call, so
 *    VC5 homes it in a slot and reloads it with `and edx,0xff` for the
 *    name lookup -- one `unsigned char` local;
 *  - cases 1-3 carry the same fallback arm; VC5 cross-jumps the copies
 *    (case 1 keeps its own `test bl,bl`, cases 2/3 jump to a shared one);
 *  - the kind switch is a 4-entry jump table with `ja` past the strcpy for
 *    anything above 3.
 *
 * ‼ PARKED 2026-09-05 at 572/563 B, ONE block-layout residue.  Everything
 * else is instruction-exact: prologue, all four case arms, the shared
 * fallback block, the two merged BrStrByIndex pushes, the strcpy body and
 * the tail.  The residue: the original keeps case 1's OWN fallback test
 * inline (`test bl,bl / je <push 0xB1> / jmp <shared Find(0,c) arm>`, 13 B)
 * and only cases 2 and 3 share the block after case 3; ours cross-jumps all
 * three into that block (case 1's lookup test `je`s straight to it).  This
 * is VC5's single-pass cross-jump order, so the original's case 1 must
 * differ in IR from cases 2/3 while compiling to the same instructions.
 *
 * DEAD, do not re-run (each scored with tools/cpp_score.py):
 *   - case 0 spellings: nested `Find(0, (uchar)GetB(..))` gives the
 *     original's `and eax,0xff / push eax` but FLIPS THE WHOLE FUNCTION TO
 *     AN EBP FRAME (+13 B, 287 diffs); a byte local (shared or its own) goes
 *     through the slot (`mov [esp+14h],al / mov edx,[esp+14h] / and`);
 *     an INT local assigned the widened byte (`i = (uchar)GetB(); i =
 *     Find(0, i)`) is the one that is exact and frameless.
 *   - `if (flag != 0) STR else SWITCH` lays the string arm inline (`je`);
 *     the original's `jne` to a deferred string arm needs `if (flag == 0)
 *     SWITCH else STR` (lone if/else is failure-first).
 *   - one strcpy after the if/else via a `p` variable: 640 B, the label
 *     address is not set per arm.  strcpy in every arm is right.
 *   - fallback polarity `c == 0` first in case 1 only (592), in cases 2/3
 *     only (624); case 1's lookup test reversed (`== 0` then-arm, 624);
 *     Ghidra's literal `goto` from cases 2/3 INTO case 1's else-arm (544 --
 *     VC5 lays the single block once); `goto` from case 2 into a label in
 *     case 3's else-arm (identical to the plain copy); block-scoped `c`
 *     per case (640, three slots); `char c` with `(uchar)` casts;
 *     `default: break;`; `c = 0` initialiser.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item39620 {
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

typedef char chk_name39620[(unsigned)&((Item39620 *)0)->szName == 9 ? 1 : -1];

class Obj39620 {
public:
    char       pad000[0x2B5C];
    Item39620  m2B5C;          /* +0x2B5C */
};

typedef char chk_item39620[(unsigned)&((Obj39620 *)0)->m2B5C == 0x2B5C ? 1 : -1];

struct BrBind39620 {
    unsigned int key;           /* +0x00 */
    int          f04;           /* +0x04 */
};

class Cfg39620 {
public:
    int  GetA(int kind, unsigned int key);      /* 0x10062C30 */
    char GetB(int kind, unsigned int key);      /* 0x10062CA0 */
};

extern "C" {
Cfg39620    g_brCfgB71290;          /* 0x10B71290 */
BrBind39620 g_brBindAAAD4[21];      /* 0x100AAAD4 */
int         g_brFlag5B9C;           /* 0x10AC5B9C  "unbound" */
int         g_brKind5D64;           /* 0x10AC5D64  control kind 0..3 */
int         g_brSel5B98;            /* 0x10AC5B98  selected action */
char        g_aBrKeyName3B44[][0x24];   /* 0x100B3B44  "ESCAPE", ... */
char        g_aBrDevName1C74[][0x24];   /* 0x10B71C74 */
char        g_aBrDevName1B0C[][0x24];   /* 0x10B71B0C */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
int   BrCtlNameFind(int kind, int key);         /* 0x10039580 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int BrCtlBindingToItem(Obj39620 *pObj)
{
    unsigned char c;
    int           i;

    /* `if (flag == 0) SWITCH else STR`: the lone if/else is laid
     * failure-first, so the string arm is the one deferred past the switch
     * (`jne` to the end), where VC5 cross-jumps its BrStrByIndex call with
     * the 0xB1 arm's.  Each arm carries its own strcpy: the original sets
     * the label address and the source in EVERY arm and shares only the
     * intrinsic's body. */
    if (g_brFlag5B9C == 0) {
        switch (g_brKind5D64) {
        case 0:
            i = (unsigned char)g_brCfgB71290.GetB(0, g_brBindAAAD4[g_brSel5B98].key);
            i = BrCtlNameFind(0, i);
            strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            break;
        case 1:
            c = g_brCfgB71290.GetB(1, g_brBindAAAD4[g_brSel5B98].key);
            if (g_brCfgB71290.GetA(1, g_brBindAAAD4[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(1, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1C74[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrByIndex(0xB1));
            }
            break;
        case 2:
            c = g_brCfgB71290.GetB(2, g_brBindAAAD4[g_brSel5B98].key);
            if (g_brCfgB71290.GetA(2, g_brBindAAAD4[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(2, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1C74[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrByIndex(0xB1));
            }
            break;
        case 3:
            c = g_brCfgB71290.GetB(3, g_brBindAAAD4[g_brSel5B98].key);
            if (g_brCfgB71290.GetA(3, g_brBindAAAD4[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(3, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1B0C[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrByIndex(0xB1));
            }
            break;
        }
    } else {
        strcpy(pObj->m2B5C.szName, BrStrByIndex(0xB2));
    }
    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);
    return 1;
}
