/* @implements 0x1003CD20 glide BrOpt37D0
 * @cpp_kind method
 * @cpp_symbol ?BrOpt37D0@@YAHPAVGameObj@@@Z
 *
 * Double-guarded slot-6 vcall: two global gates, a member zero-store on
 * the embedded sub-object, the one-arg virtual thiscall
 * (`mov ecx,[eax+2AE8]; mov edx,[ecx]; call [edx+0x18]` with the arg
 * push hoisted above the store), then a one-arg cdecl helper. No EH.
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

typedef char chk_sub[(unsigned)&((GameObj *)0)->pSub == 0x2AE8 ? 1 : -1];
typedef char chk_f68[(unsigned)&((GameSub *)0)->f68 == 0x68 ? 1 : -1];

int g_5BEC;
int g_4090;

void Fn25B0(int);

int BrOpt37D0(GameObj *pGame)
{
    if (g_5BEC != 0 && g_4090 != 0) {
        pGame->pSub->f68 = 0;
        pGame->pSub->s6(0);
        Fn25B0(0);
    }
    return 1;
}
