#include "br_race.h"   /* br_globals: its objects */
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

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* Fn7920: prototype in br_funcs.h */
/* FnB0B0: prototype in br_funcs.h */

int BrOpt3760(GameObj *pGame)
{
    pGame->pSub->f68 = 0;
    pGame->pSub->s6(0);
    if (g_9360 == 0)
        g_CBE8 = 3;
    Fn7920();
    g_nav.m(&g_navArg);
    FnB0B0();
    return 0;
}
