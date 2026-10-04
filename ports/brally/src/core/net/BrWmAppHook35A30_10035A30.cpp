#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: handle the private window messages the multiplayer layer
 * posts to itself -- each case selects the affected slot and runs the
 * matching update. */
/* @implements 0x10035A30 glide BrWmAppHook35A30
 * @cpp_kind method
 * @cpp_symbol ?BrWmAppHook35A30@@YGHHHHH@Z
 *
 * Window-message hook (ret 0x10, hwnd unused). 0x501: slot-4 vcall on
 * the sel member at +0x3838 (vtbl load interleaved in the pushes: C++
 * member-call order), then three one-arg stdcall imports with the
 * first CSEd into edi (called twice). 0x113 (WM_TIMER): two local
 * calls gated on globals. Every path returns 0.
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

class GameObjS {
public:
    char pad[0x3838];
    Sel sel;
};


extern "C" {
/* 64-bit core: g_pGame is defined once, in br_globals.c */
/* 64-bit core: g_timerOn is defined once, in br_globals.c */
/* 64-bit core: g_timerArg is defined once, in br_globals.c */
/* 64-bit core: g_netHold is defined once, in br_globals.c */
/* 64-bit core: g_selArg is defined once, in br_globals.c */
/* BrTick36300: prototype in br_funcs.h */
/* BrTick36510: prototype in br_funcs.h */
}

/* The three imports (IAT 0x118F0454, 0x118F0450, 0x118F04C0) are
 * KERNEL32 GlobalHandle, GlobalUnlock and GlobalFree: lParam is a locked
 * GlobalAlloc block that this message hands over. */

int __stdcall BrWmAppHook35A30(char * hwnd, unsigned int msg, uintptr_t wp, intptr_t lp)
{
    GameObjS *p;

    switch (msg) {
    case 0x501:
        p = g_pGame;
        if (p != 0) {
            /* append the chat line: the list's slot 4 */
            BrTextList *pl = &((BrUiCtl_ *)(p))->list;
            pl->pVtbl->f10(pl, (const void *)lp, 0, 1, &g_selArg, 1);
        }
        GlobalUnlock(GlobalHandle((LPCVOID)lp));
        GlobalFree(GlobalHandle((LPCVOID)lp));
        break;
    case 0x113:
        if (g_brPAA29D4)
            BrNetEnumSessionsStart(g_brP277B40);
        if (g_host == 0)
            BrNetSessionApply();
        break;
    }
    return 0;
}
