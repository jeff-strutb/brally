/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run, and it
 * also marks the session as hosted and picks the session kind before
 * opening. */
/* @implements 0x1003D930 glide Ctl3D930
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3D930@@QAEHXZ
 *
 * Slot load, then three global stores, then shared-return activate
 * (BrOptOpen2950B). After both arms, write pfnHook at +8 of a third
 * Phase *. Do not name temps for the stores.
 */
#define _CRTIMP __declspec(dllimport)

class Phase;

typedef void (*PhaseEnterFn)(Phase *);

class Phase {
public:
    void *vtbl;
    PhaseEnterFn pfnEnter;
    void *pfnHook;
    int f0C;
    char _pad[0x58];
    int f68;
    char _rest[0x5C];
    Phase();
};

typedef char chk_sz[sizeof(Phase) == 0xC8 ? 1 : -1];
typedef char chk_hk[(unsigned)&((Phase *)0)->pfnHook == 8 ? 1 : -1];

extern "C" Phase *DAT_10ac5ca8;   /* the original global; was a per-file stand-in definition */
#define g_slot DAT_10ac5ca8
Phase *g_cur;
Phase *g_hookOwner;
int g_host;
extern "C" int DAT_10226a48;   /* the original global; was a per-file stand-in definition */
#define g_kind DAT_10226a48
extern "C" int DAT_10ac5bf0;   /* the original global; was a per-file stand-in definition */
#define g_flag DAT_10ac5bf0

/* EnterFn was a stand-in; the original calls BrOptFn10057C10 (?BrOptFn10057C10@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
int BrOptFn10057C10(GameUi *);
#define EnterFn ((void (*)(Phase *))BrOptFn10057C10)
/* HookFn was a stand-in; the original calls Opt3DF80::Leave (?Leave@Opt3DF80@@YAHPAVGameObj3DF80@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameObj3DF80;
namespace Opt3DF80 { int Leave(GameObj3DF80 *); }
#define HookFn ((void (*)(Phase *))Opt3DF80::Leave)

class Ctl3D930 {
public:
    int Activate();
};

int Ctl3D930::Activate()
{
    Phase *p;

    p = g_slot;
    g_host = 1;
    g_kind = 2;
    g_flag = 0;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);
        g_cur->f0C = 1;
        g_cur->f68 = 1;
    } else {
        g_cur = p;
    }
    g_hookOwner->pfnHook = HookFn;
    return 1;
}
