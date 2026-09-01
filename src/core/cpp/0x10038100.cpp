/* @implements 0x10038100 glide BrUiHook85_1003EB10
 * @cpp_kind method
 * @cpp_symbol ?BrUiHook85_1003EB10@@YAHPAVGameObj@@@Z
 *
 * Two vcalls (slot 8 then slot 9) on the embedded +0x3838 object with the
 * vtbl CSE'd into edi across both — C++ frontend caching, plus the
 * clamp-through-global pattern: negative result reloads the global,
 * non-negative writes it back. 0x10038250 is a byte-identical twin. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Sel {
public:
    virtual void t0();
    virtual void t1();
    virtual void t2();
    virtual void t3();
    virtual void t4();
    virtual void t5();
    virtual void t6();
    virtual void t7();
    virtual int t8(int);
    virtual void t9(int);
};

class GameObj {
public:
    char pad[0x3838];
    Sel sel;
};

typedef char chk_sel[(unsigned)&((GameObj *)0)->sel == 0x3838 ? 1 : -1];

int g_AB94;
int g_5C30;

int BrUiHook85_1003EB10(GameObj *pGame)
{
    int r;

    r = pGame->sel.t8(g_AB94);
    if (r >= 0)
        g_AB94 = r;
    else
        r = g_AB94;
    if (g_5C30 != 0 && r >= 0)
        pGame->sel.t9(r);
    return 1;
}
