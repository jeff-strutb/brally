/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, and it refreshes the navigation bar afterwards. */
/* @implements 0x1003FB10 glide BrPhaseLeave_100466C0
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_100466C0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family: one zero store (C7-05 form), then the plain helper
 * and the static-object thiscall after the g_cur swap. No EH.
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

class Nav {
public:
    void m(void *);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

Phase *g_cur;
Phase *g_5C70;
int g_5CDC;

extern Nav g_nav;
extern int g_navArg;

void Fn7920(void);

int BrPhaseLeave_100466C0(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5CDC = 0;
    g_cur = g_5C70;
    Fn7920();
    g_nav.m(&g_navArg);
    return 0;
}
