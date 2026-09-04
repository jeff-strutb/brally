/* WHAT IT DOES: start the credits rolling: sets the mode that plays them and
 * closes the page that launched them. */
/* @implements 0x1003AED0 glide BrUiCreditsAction_1003AED0
 * @cpp_kind method
 * @cpp_symbol ?BrUiCreditsAction_1003AED0@@YAHPAVGameObj@@@Z
 *
 * Guarded-vcall family variant: cdecl helper on a global's address, mode
 * stores forked on a global test (zero materialized in edx and reused for
 * the stores and the vcall arg), then the member zero-store + slot-6
 * vcall. No EH.
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

extern char g_strA[];
int g_5D98;
int g_9360;
int g_C760;
int g_5BF4;

void FnAF30(char *);

int BrUiCreditsAction_1003AED0(GameObj *pGame)
{
    FnAF30(g_strA);
    g_9360 = 4;
    if (g_5D98 != 0) {
        g_C760 = 2;
        g_5BF4 = 0;
    } else {
        g_C760 = 1;
    }
    pGame->pSub->f68 = 0;
    pGame->pSub->s6(0);
    return 0;
}
