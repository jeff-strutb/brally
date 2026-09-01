/* @implements 0x10040420 glide BrPhaseLeave_10046FD0
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeave_10046FD0@@YAHPAVGameObj@@@Z
 *
 * Phase-leave family, esi-form: three guarded no-arg slot-7 vcalls that
 * each release their pointer global, then the standard prefix + g_cur
 * swap. No EH.
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
GameSub *g_5C8C;
GameSub *g_5C90;
GameSub *g_5C94;
Phase *g_5C60;
int g_5CCC;

int BrPhaseLeave_10046FD0(GameObj *pGame)
{
    GameSub *pS;
    Phase *pObj;

    pS = g_5C8C;
    if (pS != 0) {
        pS->s7();
        g_5C8C = 0;
    }
    pS = g_5C90;
    if (pS != 0) {
        pS->s7();
        g_5C90 = 0;
    }
    pS = g_5C94;
    if (pS != 0) {
        pS->s7();
        g_5C94 = 0;
    }
    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5CCC = 0;
    g_cur = g_5C60;
    return 0;
}
