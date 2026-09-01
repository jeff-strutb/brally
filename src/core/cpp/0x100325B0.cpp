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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
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
extern Phase  *g_brPhaseAA2904;   /* 0x10AC5C5C */
extern int     g_AC300;           /* 0x100ABAA0 */
extern funcptr DAT_10b7352c;
extern int     DAT_10226a48;
extern int     g_brCdEnabled;     /* 0x1007B074 */
extern funcptr DAT_118ed1e8;
extern funcptr DAT_106b7abc;

int BrRaceDriverReset();          /* 0x10019A10 */
int BrClearFlag_AB504();          /* 0x10013F00 */
int BrExt_10079550();             /* 0x10072840 */
int BrDiKeyboardShutdown();       /* 0x10071EB0 */
int FUN_100720a0();
int FUN_1006c6a0();
int FUN_10005f50(int);
int FUN_10035660();
int BrExt_1003BF60();             /* 0x100355F0 */
int FUN_10003030();
int BrPodNop();                   /* 0x10008D60 */
int FUN_1005a6a0();
int BrFadeRelease();              /* 0x10017F10 */
int BrStrResFree();               /* 0x1006D2A0 */
}

extern KeyCache g_AC0810;

extern "C" void BrExt_10038F30(int a)
{
    Phase *p = g_brPhaseAA2904;

    if (p != 0 && g_AC300 != 0) {
        p->f68 = 0;
        g_brPhaseAA2904->f18(0);
    }

    BrRaceDriverReset();
    BrClearFlag_AB504();

    if (DAT_10b7352c != 0) {
        (*DAT_10b7352c)();
    }

    BrExt_10079550();
    BrDiKeyboardShutdown();
    FUN_100720a0();
    FUN_1006c6a0();

    if (DAT_10226a48 != 0) {
        FUN_10005f50(1);
    }

    FUN_10035660();
    BrExt_1003BF60();

    if (g_brCdEnabled != 0) {
        FUN_10003030();
    }

    BrPodNop();

    if (DAT_118ed1e8 != 0) {
        (*DAT_118ed1e8)();
    }
    if (DAT_106b7abc != 0) {
        (*DAT_106b7abc)();
    }

    FUN_1005a6a0();
    g_AC0810.Reset();
    BrFadeRelease();
    BrStrResFree();
    CoUninitialize();

    exit(a);
}
