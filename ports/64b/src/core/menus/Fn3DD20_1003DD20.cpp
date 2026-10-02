#include "br_race.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "slice2_25.h"   /* br_globals: its objects */
/* WHAT IT DOES: populate the session list -- asks the host for its
 * description if this machine is hosting, otherwise fills the list from what
 * the browser found. */
/* @implements 0x1003DD20 glide Fn3DD20
 * @cpp_kind method
 * @cpp_symbol ?Fn3DD20@@YAHXZ
 *
 * Free cdecl (no unused-this `push ecx`; extra `sub esp,8` so the
 * prologue is `mov eax, fs:[0]` first). One `new Phase` (maxState=1,
 * unwind operator delete). Ctor DECLARED, no dtor. Live 1 in esi.
 *
 * Obj hook: do not name a pointer for g_obj_10AC4098 (`a1` eax). Direct class*
 * member access puts the object in ecx; int f08 occupies eax.
 */
#define _CRTIMP __declspec(dllimport)

class Phase;

/* Phase: br_coretypes.h */

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    void *pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};

struct Item {
    int f00;
    int f04;
};

class Obj {
public:
    char pad[8];
    int f08;
};





/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_slot g_5CAC
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_flag g_guardA
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_mode (*(int *)&g_brRaceRules.mode)
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: g_obj_10AC4098 is defined once, in br_globals.c */

/* ResetSlots: prototype in br_funcs.h */
/* GetDesc: prototype in br_funcs.h */
/* ActivateD140: prototype in br_funcs.h */
/* ActivateD220: prototype in br_funcs.h */
/* ActivateD3C0: prototype in br_funcs.h */
/* ActivateD620: prototype in br_funcs.h */
/* ActivateD930: prototype in br_funcs.h */
/* ActivateD7D0: prototype in br_funcs.h */
/* EnterFn was a stand-in; the original calls FUN_10051600 (?FUN_10051600@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
/* FUN_10051600: prototype in br_funcs.h */
#define EnterFn ((void (*)(Phase *))FUN_10051600)
/* HostFirst: prototype in br_funcs.h */
/* HostAgain: prototype in br_funcs.h */
/* ObjHook: prototype in br_funcs.h */

typedef void (__stdcall *Host_f7C)(void *self, void *item, int z);

int Fn3DD20(void)
{
    Phase *p;
    Item *item;
    void *host;
    int one;
    int h;

    one = 1;
    g_flag = one;
    BrSub100586A0();

    if (g_host != 0) {
        item = 0;
        host = (*(void * *)&g_brP277B40);
        if (host != 0)
            FUN_10036740(host, (void **)(&item));
        if (item != 0) {
            item->f04 &= ~0x20;
            host = (*(void * *)&g_brP277B40);
            ((Host_f7C)(*(void ***)host)[0x1F])(host, item, 0);
        }
    }

    Ctl3D140_fn(0);
    CtlD220_fn(0);
    Ctl3D3C0_fn(0);
    if (g_host != 0) {
        CtlD620_fn(0);
        Ctl3D930_fn(0);
    } else {
        BrOptOpen2950A(0);
    }

    p = g_slot;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(p));
        if (p == 0)
            return 0;
        (*(PhaseEnterFn *)&((BrPhase_ *)(p))->pfnEnter) = EnterFn;
        g_slot->pfnEnter(g_slot);
        (*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f0C) = one;
        (*(int *)&((BrPhase_ *)((*(Phase * *)&g_brPAA29B8)))->f68) = one;
    } else {
        (*(Phase * *)&g_brPAA29B8) = (Phase *)((BrOptObj *)(p));
    }

    h = g_host;
    g_mode = 6;
    if (h != 0) {
        if (g_inited == 0) {
            BrSub1003C150();
            g_inited = one;
            goto after_host;
        }
    }
    if (h != 0)
        BrSub1003CDA0();
after_host:
    ;

    if ((*(Obj * *)&g_brPA9D008) != 0 && (*(Obj * *)&g_brPA9D008)->f08 != 0)
        BrExt_1003DB00((struct BrObjA9D008 *)((*(Obj * *)&g_brPA9D008)), (*(Obj * *)&g_brPA9D008)->f08);
    return one;
}
