/* @implements 0x1003DA10 glide BrOpt44C0
 * @cpp_kind method
 * @cpp_symbol ?BrOpt44C0@@YAHPAVGameObj@@@Z
 *
 * Same thiscall pair as 0x1003D4A0, then hoist the flag-obj pointer
 * before the two NULL stores so orig's `test eax` sits above them.
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

struct FlagObj {
    int pad[7];
    int f1C;
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];
typedef char chk_f1c[(unsigned)&((FlagObj *)0)->f1C == 0x1C ? 1 : -1];

Phase *g_cur;
Phase *g_2948;
Phase *g_294C;
Phase *g_29B8;
FlagObj *g_29D8;
int g_mode;
int g_A9D000;
int g_AA2898;

void FnBF60(void);
void FnC020(void);

int BrOpt44C0(GameObj *pGame)
{
    Phase *pObj;
    FlagObj *pFlag;

    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    pFlag = g_29D8;
    g_294C = 0;
    g_29B8 = 0;
    g_cur = g_2948;
    if (pFlag != 0)
        pFlag->f1C &= ~0x10;
    if ((g_mode == 0 || g_mode == 1) && g_A9D000 == 0) {
        FnBF60();
        g_AA2898 = 1;
        FnC020();
    }
    return 0;
}
