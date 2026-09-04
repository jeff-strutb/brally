/* WHAT IT DOES: leave this menu page: run its leave routine, destroy the
 * page object, and make its parent current again. One of a family that
 * differ only in which parent they return to and which state flags they
 * clear, clearing five selection indices on the way out. */
/* @implements 0x1003E030 glide BrPhaseLeave_10044AE0
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_10044AE0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family, esi-form (five zero stores through esi), with one
 * trailing cdecl helper call after the g_cur swap. No EH.
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
Phase *g_5C98;
int g_5CA0;
int g_5D10;
int g_5D30;
int g_5D2C;
int g_5BD8;

void Fn55F0(void);

int BrPhaseLeave_10044AE0(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5CA0 = 0;
    g_5D10 = 0;
    g_5D30 = 0;
    g_5D2C = 0;
    g_5BD8 = 0;
    g_cur = g_5C98;
    Fn55F0();
    return 0;
}
