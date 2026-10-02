/* BrCtlBindingToItem_10039620.cpp -- controls, one C++ TU: 0x10039580
 * BrCtlNameFind (the name-table reader) and, after it, 0x10039620
 * BrCtlBindingToItem (writes the bound key's name into a menu label). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
#include <string.h>

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



class Obj39620 {
public:
    char       pad000[0x2B5C];
    Item39620  m2B5C;          /* +0x2B5C */
};



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
/* 64-bit core: g_brCfgB71290 is defined once, in br_globals.c */
/* 64-bit core: g_brBindAAAD4 is defined once, in br_globals.c */
/* 64-bit core: g_brFlag5B9C is defined once, in br_globals.c */
/* 64-bit core: g_brKind5D64 is defined once, in br_globals.c */
/* 64-bit core: g_brSel5B98 is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrStrByIndex: prototype in br_funcs.h */
/* BrCtlNameFind: prototype in br_funcs.h */
/* BrItemApply_10038380: prototype in br_funcs.h */
}

/* ==========================================================================
 * 0x10039580 -- the reader (D3D 0x10040040, BrCfgLookupIndex in slice2_23.c)
 *
 * The original hardcodes the three table addresses and their end addresses
 * and carries FOUR copies of the same walk, one per `kind` arm; the compare
 * against the end address is signed (`jl`).  In BRGlide.dll kind 1 is the
 * MOUSE table (0x10B71B08) and kinds 2 and 3 are both the JOYSTICK table
 * (0x10B71C70) -- read straight off the jump table.
 *
 * Filed here, ahead of 0x10039620, because that is where it sits in the
 * original's TU: with no function before it, BrCtlBindingToItem cross-jumps
 * case 1's fallback test into the shared block (572 B); with this one
 * compiled first it is byte-exact.  It is byte-exact here too.
 * ========================================================================== */

/* BrCfgRec39580: br_coretypes.h */

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

/* WHAT IT DOES: finds the row whose key matches in the keyboard (0), mouse
 * (1) or joystick (2 and 3) name table and returns its index; 0 when the key
 * is not there or the kind is out of range. */
/* @implements 0x10039580 glide BrCtlNameFind
 * @cpp_kind free
 * @cpp_symbol _BrCtlNameFind */
extern "C" int BrCtlNameFind(int kind, int key)
{
    int i;
    const BrCfgRec39580 *p;

    switch (kind) {
    case 0:
        i = 0;
        for (p = g_aBrCtlNameKey; (uintptr_t)p < (uintptr_t)&g_aBrCtlNameKey[120]; p++, i++) {
            if (p->key == key)
                return i;
        }
        return 0;
    case 1:
        i = 0;
        for (p = g_aBrCtlNameMouse; (uintptr_t)p < (uintptr_t)&g_aBrCtlNameMouse[10]; p++, i++) {
            if (p->key == key)
                return i;
        }
        return 0;
    case 2:
        i = 0;
        for (p = g_aBrCtlNameJoy; (uintptr_t)p < (uintptr_t)&g_aBrCtlNameJoy[134]; p++, i++) {
            if (p->key == key)
                return i;
        }
        return 0;
    case 3:
        i = 0;
        for (p = g_aBrCtlNameJoy; (uintptr_t)p < (uintptr_t)&g_aBrCtlNameJoy[134]; p++, i++) {
            if (p->key == key)
                return i;
        }
        return 0;
    }
    return 0;
}

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
 * Byte-exact 2026-09-27.  The one block-layout residue this was parked on
 * (case 1's fallback test cross-jumped into the shared block, 572 B) was TU
 * state, not spelling: with any function compiled ahead of it in the TU the
 * original's layout comes out.  The original's neighbour there is
 * 0x10039580 BrCtlNameFind, now filed above it in this file.
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
    if ((*(int *)&BrGlNavOff5B9C) == 0) {
        switch (g_brKind5D64) {
        case 0:
            i = (unsigned char)(*(Cfg39620 *)&g_BrCtrlCfg).GetB(0, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key);
            i = BrCtlNameFind(0, i);
            strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            break;
        case 1:
            c = (*(Cfg39620 *)&g_BrCtrlCfg).GetB(1, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key);
            if ((*(Cfg39620 *)&g_BrCtrlCfg).GetA(1, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(1, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1C74[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrGet(0xB1));
            }
            break;
        case 2:
            c = (*(Cfg39620 *)&g_BrCtrlCfg).GetB(2, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key);
            if ((*(Cfg39620 *)&g_BrCtrlCfg).GetA(2, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(2, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1C74[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrGet(0xB1));
            }
            break;
        case 3:
            c = (*(Cfg39620 *)&g_BrCtrlCfg).GetB(3, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key);
            if ((*(Cfg39620 *)&g_BrCtrlCfg).GetA(3, (*(BrBind39620 (*)[21])&g_brBindAAAD4)[g_brSel5B98].key) != 0) {
                i = BrCtlNameFind(3, c);
                strcpy(pObj->m2B5C.szName, g_aBrDevName1B0C[i]);
            } else if (c != 0) {
                i = BrCtlNameFind(0, c);
                strcpy(pObj->m2B5C.szName, g_aBrKeyName3B44[i]);
            } else {
                strcpy(pObj->m2B5C.szName, BrStrGet(0xB1));
            }
            break;
        }
    } else {
        strcpy(pObj->m2B5C.szName, BrStrGet(0xB2));
    }
    (BR_VFN(&(pObj->m2B5C), 1, void (*)(void *)))(&(pObj->m2B5C));
    Br85ItemApply((struct BrCtl85 *)(pObj), 0);
    return 1;
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x10062C30: the original calls BrFn10069BC0 by address */
int Cfg39620::GetA(int a1, unsigned int a2)
{
    return (int)BrFn10069BC0((void *)this, (int32_t)a1, (uint32_t)a2);
}

/* 0x10062CA0: the original calls BrFn10069C30 by address */
char Cfg39620::GetB(int a1, unsigned int a2)
{
    return (char)BrFn10069C30((void *)this, (int32_t)a1, (uint32_t)a2);
}
