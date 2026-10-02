#include "br_race.h"   /* br_globals: its objects */
#include "br_phase.h"   /* BrPhase_, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */
#include "slice3_42.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this page and go back, setting the return mode first
 * so the parent knows where the player came from. */
/* @implements 0x1003CCB0 glide BrOpt3760
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3760@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: member zero-store + slot-6 vcall (arg
 * push hoisted above the store), a guarded mode store, plain helper,
 * the static-object thiscall (`mov ecx,offset g_nav`), and a trailing
 * cdecl helper. No EH.
 */
#define _CRTIMP __declspec(dllimport)

class GameSub {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6(int);
    virtual void s7();
    char padA[0x64];
    int f68;
};

class GameObj {
public:
    char pad[0x2AE8];
    GameSub *pSub;
};

class Nav {
public:
    void m(void *);
};



/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Fn7920: prototype in br_funcs.h */
/* FnB0B0: prototype in br_funcs.h */

int BrOpt3760(GameObj *pGame)
{
    (*(int *)&((BrPhase_ *)((*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)))->f68) = 0;
    (*(GameSub * *)&((BrUiCtl_ *)(pGame))->pOwner)->s6(0);
    if ((*(int *)&g_brRaceRules.mode) == 0)
        g_CBE8 = 3;
    BrOptSave();
    (*(Nav *)&g_BrCtrlCfg).m(&g_navArg);
    BrMenuAutoSaveName();
    return 0;
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x100634B0: the original calls BrGlCfgSave by address */
inline void Nav::m(void * a1)
{
    BrGlCfgSave((void *)this, (const char *)a1);
}
