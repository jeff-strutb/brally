/* @implements 0x100403B0 glide BrPhaseLeave_10046F60
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_10046F60@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family: after the prefix, a SECOND guarded slot-0 vcall on
 * g_5C84 (also released to 0), then the g_cur swap from g_5C60. No EH.
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
Phase *g_5C84;
Phase *g_5C60;
int g_5CCC;

int BrPhaseLeave_10046F60(GameObj *pGame)
{
    Phase *pObj;
    Phase *pOld;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    pOld = g_5C84;
    g_cur = 0;
    g_5CCC = 0;
    if (pOld != 0) {
        pOld->f00(1);
        g_5C84 = 0;
    }
    g_cur = g_5C60;
    return 0;
}
