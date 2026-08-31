/* @implements 0x1003D4A0 glide BrOpt3F50
 * @cpp_kind method
 * @cpp_symbol ?BrOpt3F50@@YAHPAVGameObj@@@Z
 *
 * Free cdecl. Slot+0x1C is a no-arg virtual thiscall
 * (`mov edx,[ecx]; call [edx+0x1C]`). Slot+0x00 with one stack arg is
 * `mov eax,[ecx]; push 1; call [eax]` — C __fastcall edx-slot colours
 * the vtbl into edx. No EH (no new).
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

int g_mode;
Phase *g_cur;
Phase *g_2948;
Phase *g_298C;

int BrOpt3F50(GameObj *pGame)
{
    Phase *pObj;

    g_mode = 2;
    pGame->pSub->s7();
    pObj = g_cur;
    if (pObj != 0)
        pObj->f00(1);
    g_298C = 0;
    g_cur = g_2948;
    return 0;
}
