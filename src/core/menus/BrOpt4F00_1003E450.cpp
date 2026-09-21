/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, setting a return mode the parent reads. */
/* @implements 0x1003E450 glide BrOpt4F00
 * @cpp_kind method
 * @cpp_symbol ?BrOpt4F00@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family member (see tools/gen_phaseleave.py), hand-filed:
 * its current-phase slot is g_5CC0, not the family's g_cur, and the
 * tail sets a mode global to 2 instead of a zero store.
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

Phase *g_5CC0;
Phase *g_5CB4;
Phase *g_cur;
int g_mode;

int BrOpt4F00(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_5CC0;
    if (pObj != 0)
        pObj->f00(1);
    g_5CC0 = 0;
    g_cur = g_5CB4;
    g_mode = 2;
    return 0;
}
