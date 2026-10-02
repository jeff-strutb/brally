#include "br_slots.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
/* WHAT IT DOES: leave this page if a session is live, and otherwise open the
 * session-browser page instead of doing nothing. */
/* @implements 0x1003CD60 glide BrOpt3810
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3810@@YAHPAVGameObj@@@Z
 *
 * Free cdecl, glide twin of the C draft at slice2_25.c BrOpt3810
 * (d3d 0x10043810). Leave/close ladder: the leave-host arm is the ELSE
 * (arm-at-the-end layout), the close/net/save arms are early-return
 * inline. Direct global rereads throughout (no pObj/pSub locals);
 * guarded slot-7 vcall on the current sub-phase (edx-form), GameSub
 * f68+slot-6 pair, the DPlay session-desc slot scan (indexed loop
 * strength-reduced to the pointer walk, sbb/neg from ?1:0), GlobalHandle
 * CSEd into esi across the two calls, and the BrOpt3760-style
 * static-nav thiscall in the save tail. pDesc packs into the dead
 * param home slot. KEY: the scan loop and the frees sit under TWO
 * SEQUENTIAL `if (pDesc != 0)` blocks: VC5 threads the first je past
 * both but keeps the second test after the loop; nesting the frees
 * inside the first block folds the test (-4 B, every jump shifts).
 */
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

class Phase8 {
public:
    virtual void v0();
    virtual void v1();
    virtual void v2();
    virtual void v3();
    virtual void v4();
    virtual void v5();
    virtual void v6(int);
    virtual void v7();
};

class Wnd {
public:
    char pad[0x1C];
    unsigned int f1C;
};

class Nav {
public:
    void m(void *);
};

class NetHost {
public:
    char pad[8];
    int f08;
};

struct BrSlot {
    int id;
    int a;
    int b;
};

struct DPSess {
    char pad[0x2C];
    unsigned int dwCurrentPlayers;
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrSub10046400: prototype in br_funcs.h */
/* BrExt8F30: prototype in br_funcs.h */
void CtlD620(int);
void Ctl3D930(int);
void Ctl3DC20(int);
/* FUN_10036740: prototype in br_funcs.h */
/* Fn355F0: prototype in br_funcs.h */
/* FUN_100356b0: prototype in br_funcs.h */
/* Fn7920: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */

/* 64-bit core: GlobalHandle is declared by the platform headers */
/* 64-bit core: GlobalUnlock is declared by the platform headers */
/* 64-bit core: GlobalFree is declared by the platform headers */
}

int BrOpt3810(GameObj *pGame)
{
    DPSess *pDesc;
    int i;

    if (DAT_10ac5bec != 0) {
        if (g_guardB != 0) {
            pGame->pSub->f68 = 0;
            pGame->pSub->s6(0);
            BrExt_10038F30(0);
        } else {
            BrSub10046400(pGame);
            if ((*(Phase8 * *)&g_brPAA2950) != 0) {
                (*(Phase8 * *)&g_brPAA2950)->v7();
                (*(Phase8 * *)&g_brPAA2950) = 0;
            }
            (*(Phase8 * *)&g_brPAA29B8) = (Phase8 *)((BrOptObj *)(g_2948));
            BrExt_1003BF60();
            DAT_10ac5bf0 = 1;
            if (DAT_10ac5bd4 == 0 || DAT_10ac5bd4 == 1)
                FUN_100356b0();
            if (DAT_10ac5bd4 == 2 || DAT_10ac5bd4 == 3) {
                if ((*(Wnd * *)&g_brPAA29D8) != 0)
                    (*(Wnd * *)&g_brPAA29D8)->f1C &= ~0x10;
            }
            DAT_10ac5bec = 0;
            return 0;
        }
    }

    if (DAT_10ac5be8 != 0) {
        BrSub10046400(pGame);
        if ((*(Phase8 * *)&g_brPAA2950) != 0) {
            (*(Phase8 * *)&g_brPAA2950)->v7();
            (*(Phase8 * *)&g_brPAA2950) = 0;
        }
        CtlD620(0);
        Ctl3D930(0);
        Ctl3DC20(0);
        DAT_10ac5be8 = 0;
        return 0;
    }

    if (g_host != 0) {
        pDesc = 0;
        if (g_brP277B40 != 0)
            FUN_10036740(g_brP277B40, (void **)(&pDesc));
        if (pDesc != 0) {
            for (i = 0; i < 8; ++i) {
                if (g_aBrAA2538[i].id == (*(NetHost * *)&g_brPA9D008)->f08) {
                    g_aBrAA2538[i].a = (pDesc->dwCurrentPlayers > 1) ? 1 : 0;
                    break;
                }
            }
        }
        if (pDesc != 0) {
            GlobalUnlock(GlobalHandle(pDesc));
            GlobalFree(GlobalHandle(pDesc));
        }
    }

    if (DAT_10ac5be4 != 0) {
        BrOptSave();
        (*(Nav *)&g_BrCtrlCfg).m(&g_navArg);
        pGame->pSub->f68 = 0;
        pGame->pSub->s6(0);
        g_5BB4 = 0;
        BrSub10072AF0(2, 0x200020);
        g_track = 2;
        return 0;
    }
    return 1;
}
