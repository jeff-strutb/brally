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
 * SEQUENTIAL `if (pDesc != 0)` blocks — VC5 threads the first je past
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
typedef char chk_f68[(unsigned)&((GameSub *)0)->f68 == 0x68 ? 1 : -1];

extern Nav g_nav;
extern int g_navArg;

extern "C" {
extern int DAT_10ac5bec;
extern int DAT_10ac4090;
extern int DAT_10ac5be8;
extern Phase8 *DAT_10ac5ca8;
extern Phase8 *g_cur;
extern Phase8 *g_2948;
extern int DAT_10ac5bf0;
extern int g_brAA287C;
extern Wnd *DAT_10ac5d30;
extern int g_brAA2884;
extern int g_brP277B40;
extern NetHost *g_brPA9D008;
extern BrSlot g_aBrAA2538[8];
extern int g_brAA288C;
extern int DAT_10ac5bb4;
extern int g_brAA2854;

void BrSub10046400(GameObj *);
void BrExt8F30(int);
void CtlD620(int);
void Ctl3D930(int);
void Ctl3DC20(int);
void FUN_10036740(int, DPSess **);
void Fn355F0(void);
void FUN_100356b0(void);
void Fn7920(void);
void BrSub10072AF0(int, int);

__declspec(dllimport) void *__stdcall GlobalHandle(const void *);
__declspec(dllimport) int __stdcall GlobalUnlock(void *);
__declspec(dllimport) void *__stdcall GlobalFree(void *);
}

int BrOpt3810(GameObj *pGame)
{
    DPSess *pDesc;
    int i;

    if (DAT_10ac5bec != 0) {
        if (DAT_10ac4090 != 0) {
            pGame->pSub->f68 = 0;
            pGame->pSub->s6(0);
            BrExt8F30(0);
        } else {
            BrSub10046400(pGame);
            if (DAT_10ac5ca8 != 0) {
                DAT_10ac5ca8->v7();
                DAT_10ac5ca8 = 0;
            }
            g_cur = g_2948;
            Fn355F0();
            DAT_10ac5bf0 = 1;
            if (g_brAA287C == 0 || g_brAA287C == 1)
                FUN_100356b0();
            if (g_brAA287C == 2 || g_brAA287C == 3) {
                if (DAT_10ac5d30 != 0)
                    DAT_10ac5d30->f1C &= ~0x10;
            }
            DAT_10ac5bec = 0;
            return 0;
        }
    }

    if (DAT_10ac5be8 != 0) {
        BrSub10046400(pGame);
        if (DAT_10ac5ca8 != 0) {
            DAT_10ac5ca8->v7();
            DAT_10ac5ca8 = 0;
        }
        CtlD620(0);
        Ctl3D930(0);
        Ctl3DC20(0);
        DAT_10ac5be8 = 0;
        return 0;
    }

    if (g_brAA2884 != 0) {
        pDesc = 0;
        if (g_brP277B40 != 0)
            FUN_10036740(g_brP277B40, &pDesc);
        if (pDesc != 0) {
            for (i = 0; i < 8; ++i) {
                if (g_aBrAA2538[i].id == g_brPA9D008->f08) {
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

    if (g_brAA288C != 0) {
        Fn7920();
        g_nav.m(&g_navArg);
        pGame->pSub->f68 = 0;
        pGame->pSub->s6(0);
        DAT_10ac5bb4 = 0;
        BrSub10072AF0(2, 0x200020);
        g_brAA2854 = 2;
        return 0;
    }
    return 1;
}
