/* @implements 0x1003F700 glide Ctl3F700
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3F700@@QAEHXZ
 *
 * Shared-return activate with a 1-register (esi) because five stores of 1
 * sit in the prologue. Flag stores are `mov r/m, esi` not `c7`. Extra
 * just-built tail (three calls + hook) lives inside the new arm, so each
 * arm returns 1. Ctor DECLARED, no dtor — unwind is operator delete
 * (maxState=1).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Phase;

typedef void (*PhaseEnterFn)(Phase *);
typedef int (*PhaseHookFn)(void *);

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    PhaseHookFn pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};

typedef char chk_sz[sizeof(Phase) == 0xC8 ? 1 : -1];
typedef char chk_ent[(unsigned)&((Phase *)0)->pfnEnter == 4 ? 1 : -1];
typedef char chk_c[(unsigned)&((Phase *)0)->f0C == 0xC ? 1 : -1];
typedef char chk_68[(unsigned)&((Phase *)0)->f68 == 0x68 ? 1 : -1];

Phase *g_slot;
Phase *g_cur;
int g_mode;
int g_AF2094;
int g_AF3CE4;
int g_0ABAA4;
char g_5BC8E0;
Phase *g_hookObj;

void EnterFn(Phase *);
int HookFn(void *);
void ResetBuf(void);
void Fn08D60(void);
void Fn37660(void);
void Fn37B20(void);

class Ctl3F700 {
public:
    int Activate();
};

int Ctl3F700::Activate()
{
    Phase *p;

    g_mode = 2;
    ResetBuf();
    p = g_slot;
    g_AF2094 = 0;
    g_AF3CE4 = 1;
    g_mode = 2;
    g_0ABAA4 = 1;
    g_5BC8E0 = (char)0xFF;
    if (p == 0) {
        g_0ABAA4 = 1;
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);
        g_cur->f0C = 1;
        g_cur->f68 = 1;
        Fn08D60();
        Fn37660();
        Fn37B20();
        g_hookObj->pfnHook = HookFn;
        return 1;
    } else {
        g_cur = p;
        return 1;
    }
}
