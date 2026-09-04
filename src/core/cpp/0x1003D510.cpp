/* WHAT IT DOES: leave this page and return to its parent, clearing two state
 * flags on the way out. */
/* @implements 0x1003D510 glide BrOpt3FC0
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3FC0@@YAHPAVGameObj@@@Z
 *
 * Twin of 0x1003D4A0: same slot+0x1C / slot+0x00 thiscall pair, then two
 * zero-stores and a pointer copy. No named temp on the copy (ecx form).
 * No EH.
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
Phase *g_2908;
int g_AA2958;
int g_AA29A8;

int BrOpt3FC0(GameObj *pGame)
{
    Phase *pObj;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_AA2958 = 0;
    g_AA29A8 = 0;
    g_cur = g_2908;
    return 0;
}
