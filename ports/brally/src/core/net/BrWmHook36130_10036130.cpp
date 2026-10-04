#include "slice1_02.h"   /* br_globals: its objects */
#include "br_ui.h"
#include "dplay.h"   /* DPNAME */   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: handle a DirectPlay callback for one player record, ignoring
 * the notifications flagged as uninteresting and otherwise updating that
 * player's slot. */
/* @implements 0x10036130 glide BrWmHook36130
 * @cpp_kind method
 * @cpp_symbol ?BrWmHook36130@@YGHHHPAURec36130@@IH@Z
 *
 * Sibling of 0x10035A30: same Sel slot-4 vcall through a Sel* temp
 * (different arg global), then a store into the 1080-byte slot array
 * indexed by the unsigned short at +0x1E164, and a local call.
 * Early-outs: 0 when the game object is null (eax reuse: return the
 * pointer itself), 1 when bit 9 of arg4 is set.
 */
#define _CRTIMP __declspec(dllimport)

class Sel {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4(int, int, int, void *, int);
};

struct Rec36130 {
    int f0;
    int f4;
    int f8;
};

struct Slot36130 {
    int f0;
    char pad[1076];
};

class GameObjS {
public:
    char pad[0x3838];
    Sel sel;
    char pad2[0x24];
    Slot36130 slots[100];
    char pad3[0x324];
    unsigned short wIdx;
};




extern "C" {
/* 64-bit core: g_pGame2 is defined once, in br_globals.c */
/* 64-bit core: g_selArg2 is defined once, in br_globals.c */
/* BrTick36080: prototype in br_funcs.h */
}

int __stdcall BrWmHook36130(int a1, int a2, const DPNAME *a3, unsigned int a4, void *a5)
{
    GameObjS *p;

    p = (GameObjS *)((*(GameObjS * *)&g_brPAA29E4));
    if (p == 0)
        return 0;
    if (a4 & 0x200)
        return 1;
    {
        /* append the player's short name: the list's slot 4 */
        BrTextList *pl = &((BrUiCtl_ *)(p))->list;
        pl->pVtbl->f10(pl, a3->lpszShortNameA, 0, 1, &g_selArg2, 1);
    }
    {
        /* the original stores into a 0x438-stride int walk that starts at
         * list+0x28: element 0 is list.f28, element c the f434 word of
         * text box c-1 */
        BrTextList *pl = &((BrUiCtl_ *)((*(GameObjS * *)&g_brPAA29E4)))->list;
        unsigned short c = (unsigned short)pl->count;
        if (c == 0)
            pl->f28 = (uint32_t)a1;
        else
            pl->aItems[c - 1].f434 = a1;
    }
    BrSlotMark(a1);
    return 1;
}
