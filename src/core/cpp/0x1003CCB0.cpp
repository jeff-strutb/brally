/* @implements 0x1003CCB0 glide BrOpt3760
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3760@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: member zero-store + slot-6 vcall (arg
 * push hoisted above the store), a guarded mode store, plain helper,
 * the static-object thiscall (`mov ecx,offset g_nav`), and a trailing
 * cdecl helper. No EH.
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
    char padA[0x64];
    int f68;
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
typedef char chk_f68[(unsigned)&((GameSub *)0)->f68 == 0x68 ? 1 : -1];

extern Nav g_nav;
extern int g_navArg;
int g_9360;
int g_CBE8;

void Fn7920(void);
void FnB0B0(void);

int BrOpt3760(GameObj *pGame)
{
    pGame->pSub->f68 = 0;
    pGame->pSub->s6(0);
    if (g_9360 == 0)
        g_CBE8 = 3;
    Fn7920();
    g_nav.m(&g_navArg);
    FnB0B0();
    return 0;
}
