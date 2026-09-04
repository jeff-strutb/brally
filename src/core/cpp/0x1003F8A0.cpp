/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, clearing four selection flags. */
/* @implements 0x1003F8A0 glide BrSub10046400
 * @cpp_kind method
 * @cpp_symbol ?BrSub10046400@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family, esi-form: four zero stores make VC5 materialize the
 * zero in esi (`xor esi,esi; cmp ecx,esi` doubles as the g_cur null test,
 * `push esi` prologue, `mov [g],esi` stores). Same source shape as the
 * C7-05 form gen_phaseleave stamps. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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
};

class GameObj {
public:
    char pad[0x2AE8];
    GameSub *pSub;
};

class Phase {
public:
    virtual void *f00(int);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

Phase *g_cur;
Phase *g_5CA8;
int g_5CAC;
int g_5D3C;
int g_5D38;
int g_5BB4;

int BrSub10046400(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5CAC = 0;
    g_5D3C = 0;
    g_5D38 = 0;
    g_5BB4 = 0;
    g_cur = g_5CA8;
    return 0;
}
