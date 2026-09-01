/* @implements 0x1003CC60 glide BrOpt3710
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3710@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: plain helper, then a non-virtual
 * thiscall on a STATIC object (`push offset g_arg; mov ecx,offset g_nav;
 * call m` — only a real C++ member call on a global object reaches the
 * ecx-immediate shape), the slot-6 vcall, and two cdecl helpers with
 * call-site argument reads. No EH.
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

class Nav {
public:
    void m(void *);
};

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];

extern Nav g_nav;
extern int g_navArg;
int g_4098;

void Fn7920(void);
void Fn9A40(int);
void Fn25B0(int);

int BrOpt3710(GameObj *pGame)
{
    Fn7920();
    g_nav.m(&g_navArg);
    pGame->pSub->s6(0);
    Fn9A40(g_4098);
    Fn25B0(0);
    return 1;
}
