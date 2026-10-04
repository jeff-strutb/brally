/* WHAT IT DOES: set this item's caption from its own table by its own
 * selection index, then lay it out. */
/* @implements 0x10039350 glide BrUiText1003FE10
 * @cpp_kind free
 * @cpp_symbol ?BrUiText1003FE10@@YAHPAVObj39350@@@Z
 *
 * cdecl, one arg, `ret`, 100 B. Put the catalogue string the +0x5D7C
 * selector points at into the owner's +0x2B5C item label, relayout through
 * the +0x04 vcall and apply. Returns 1.
 *
 * Same 0x438 item record as 0x10041300. One of three identical siblings
 * (0x10039270 / 0x10039350 / 0x10039510) differing only in the selector
 * global and the index table.
 *
 * The port body in slice2_23.c reaches its globals through a BrUiGlobals*
 * second parameter; the original is cdecl with ONE argument and direct
 * global addresses. Same split as the 0x10038650 sibling.
 */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "br_ui.h"   /* BrUiCtl_, the canonical record */

class Item39350 {
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



class Obj39350 {
public:
    char       pad000[0x2B5C];
    Item39350 m2B5C;          /* +0x2B5C */
};



extern "C" {
/* 64-bit core: g_brSel5D7C is defined once, in br_globals.c */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrStrByIndex: prototype in br_funcs.h */
/* BrItemApply_10038380: prototype in br_funcs.h */
}

int BrUiText1003FE10(Obj39350 *pObj)
{
    strcpy((*(class Item39350 *)&((BrUiCtl_ *)(pObj))->aText[0]).szName, BrStrGet(g_brTblABBB0[g_brSel5D7C]));

    (BR_VFN(&((*(class Item39350 *)&((BrUiCtl_ *)(pObj))->aText[0])), 1, void (*)(void *)))(&((*(class Item39350 *)&((BrUiCtl_ *)(pObj))->aText[0])));
    Br85ItemApply((struct BrCtl85 *)(pObj), 0);

    return 1;
}
