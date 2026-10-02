/* WHAT IT DOES: leave the current game phase -- resets the driver state,
 * clears the flags, runs whatever teardown callback is installed, and
 * releases the input devices. Called on the way out of a race. */
/* @implements 0x100325B0 glide BrExt_10038F30
 * @cpp_kind method
 * @cpp_symbol BrExt_10038F30
 *
 * The shutdown sequence: free cdecl, one int arg handed to exit() at the
 * end. One vcall (`mov eax,[ecx]; call [eax+0x18]`, one pushed 0, callee
 * pops) on the current phase; the phase pointer is cached for the test and
 * the +0x68 clear, then RE-READ from the global for the call's receiver.
 * BrKeyCacheReset is thiscall on the g_AC0810 object (`mov ecx,imm`).
 * CoUninitialize and exit are /MD imports (FF 15). No EH (no new).
 */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include <windows.h>
#include <objbase.h>
#include <stdlib.h>

typedef int (*funcptr)();

class Phase {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void f18(void *);
    char pad[0x68 - 4];
    int f68;
};

class KeyCache {
public:
    void Reset();
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrRaceDriverReset: prototype in br_funcs.h */
/* BrClearFlag_AB504: prototype in br_funcs.h */
/* BrExt_10079550: prototype in br_funcs.h */
/* BrDiKeyboardShutdown: prototype in br_funcs.h */
/* FUN_100720a0: prototype in br_funcs.h */
/* FUN_1006c6a0: prototype in br_funcs.h */
/* FUN_10005f50: prototype in br_funcs.h */
/* FUN_10035660: prototype in br_funcs.h */
/* BrExt_1003BF60: prototype in br_funcs.h */
/* FUN_10003030: prototype in br_funcs.h */
/* BrPodNop: prototype in br_funcs.h */
/* FUN_1005a6a0: prototype in br_funcs.h */
/* BrFadeRelease: prototype in br_funcs.h */
/* BrStrResFree: prototype in br_funcs.h */
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */

extern "C" void BrExt_10038F30(int a)
{
    Phase *p = (*(Phase * *)&g_brPAA29B8);

    if (p != 0 && (*(int *)&g_AC300) != 0) {
        p->f68 = 0;
        (*(Phase * *)&g_brPAA29B8)->f18(0);
    }

    BrRaceDriverReset();
    BrClearFlag_AB504();

    if (DAT_10b7352c != 0) {
        (*DAT_10b7352c)();
    }

    BrExt_10079550();
    BrDiKeyboardShutdown();
    BrDInputShutdown();
    FUN_1006c6a0();

    if ((*(int *)&g_brRaceNet) != 0) {
        BrNetShutdown();
    }

    FUN_10035660();
    BrExt_1003BF60();

    if (DAT_1007b074 != 0) {
        BrCdStopRelease();
    }

    BrPodNop();

    if ((*(funcptr *)&g_18ED1E8) != 0) {
        (*(*(funcptr *)&g_18ED1E8))();
    }
    if ((*(funcptr *)&BrGlFlipHook2) != 0) {
        (*(*(funcptr *)&BrGlFlipHook2))();
    }

    FUN_1005a6a0();
    g_brModelMgr.Reset();
    BrFadeRelease();
    BrStrResFree();
    CoUninitialize();

    exit(a);
}
