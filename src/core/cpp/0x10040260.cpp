/* @implements 0x10040260 glide BrPhaseLeaveNamed_10046E10
 * @cpp_kind method
 * @cpp_symbol ?BrPhaseLeaveNamed_10046E10@@YAHPAVGameObj@@@Z
 *
 * Phase-leave prefix + double-strcpy tail, the 0x1003FBE0 shape with two
 * zero stores instead of four. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

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
Phase *g_5C74;
int g_5C7C;
int g_5C38;
int g_AB94;
char g_srcStr[64];
char g_bufA[64];
char g_bufB[64];

int BrPhaseLeaveNamed_10046E10(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_5C7C = 0;
    g_5C38 = 0;
    strcpy(g_bufA, g_srcStr);
    g_AB94 = -1;
    strcpy(g_bufB, g_srcStr);
    g_cur = g_5C74;
    return 0;
}
