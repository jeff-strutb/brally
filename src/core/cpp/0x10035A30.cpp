/* WHAT IT DOES: handle the private window messages the multiplayer layer
 * posts to itself -- each case selects the affected slot and runs the
 * matching update. */
/* @implements 0x10035A30 glide BrWmAppHook35A30
 * @cpp_kind method
 * @cpp_symbol ?BrWmAppHook35A30@@YGHHHHH@Z
 *
 * Window-message hook (ret 0x10, hwnd unused). 0x501: slot-4 vcall on
 * the sel member at +0x3838 (vtbl load interleaved in the pushes — C++
 * member-call order), then three one-arg stdcall imports with the
 * first CSEd into edi (called twice). 0x113 (WM_TIMER): two local
 * calls gated on globals. Every path returns 0.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Sel {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4(int, int, int, void *, int);
};

class GameObjS {
public:
    char pad[0x3838];
    Sel sel;
};

typedef char chk_sel[(unsigned)&((GameObjS *)0)->sel == 0x3838 ? 1 : -1];

extern "C" {
GameObjS *g_pGame;
int g_timerOn;
int g_timerArg;
int g_netHold;
int g_selArg;
void BrTick36300(int);
void BrTick36510(void);
}

__declspec(dllimport) int __stdcall ImpA(int);
__declspec(dllimport) int __stdcall ImpB(int);
__declspec(dllimport) int __stdcall ImpC(int);

int __stdcall BrWmAppHook35A30(int hwnd, unsigned int msg, int wp, int lp)
{
    GameObjS *p;

    switch (msg) {
    case 0x501:
        p = g_pGame;
        if (p != 0) {
            Sel *s = &p->sel;
            s->s4(lp, 0, 1, &g_selArg, 1);
        }
        ImpB(ImpA(lp));
        ImpC(ImpA(lp));
        break;
    case 0x113:
        if (g_timerOn)
            BrTick36300(g_timerArg);
        if (g_netHold == 0)
            BrTick36510();
        break;
    }
    return 0;
}
