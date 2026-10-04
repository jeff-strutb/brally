#include "br_ui.h"   /* BrUiCtl_, the canonical record */
/* WHAT IT DOES: highlight-sync for a page whose valid rows are 0 to 11 --
 * anything outside that range is treated as 'nothing selected' rather than
 * clamped. */
/* @implements 0x10038350 glide BrUiHook85_1003EE20
 * @cpp_kind method
 * @cpp_symbol ?BrUiHook85_1003EE20@@YAHPAVGameObj@@@Z
 *
 * Range-clamped index through a slot-8 vcall on the embedded object at
 * +0x3838 (`add ecx,0x3838`: embedded member, not a pointer load), the
 * result written back to the global when non-negative. No EH.
 */
#define _CRTIMP __declspec(dllimport)

class Sel {
public:
    virtual void t0();
    virtual void t1();
    virtual void t2();
    virtual void t3();
    virtual void t4();
    virtual void t5();
    virtual void t6();
    virtual void t7();
    virtual int t8(int);
};

class GameObj {
public:
    char pad[0x3838];
    Sel sel;
};


/* 64-bit core: declared once, in br_globals.h or its struct's header */

int BrUiHook85_1003EE20(GameObj *pGame)
{
    int v;
    int r;

    v = g_5D8C;
    if (v < 0 || v >= 12)
        v = -1;
    r = (*(class Sel *)&((BrUiCtl_ *)(pGame))->list).t8(v);
    if (r >= 0)
        g_5D8C = r;
    return 1;
}
