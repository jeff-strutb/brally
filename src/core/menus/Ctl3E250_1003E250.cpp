/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run, and
 * this one zeroes two counters before opening. */
/* @implements 0x1003E250 glide Ctl3E250
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3E250@@QAEHXZ
 *
 * p = g_slot; g_c20 = 0; g_c24 = 0; then shared-return activate.
 * Slot load BEFORE the two zero stores (cpp-family2-notes.md).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

extern "C" Phase *DAT_10ac5cbc;   /* the original global; was a per-file stand-in definition */
#define g_slot DAT_10ac5cbc
Phase *g_cur;
int g_c20;
int g_c24;

/* EnterFn was a stand-in; the original calls FUN_10052a60 (?FUN_10052a60@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
int FUN_10052a60(GameUi *);
#define EnterFn ((void (*)(Phase *))FUN_10052a60)

class Ctl3E250 {
public:
    int Activate();
};

int Ctl3E250::Activate()
{
    Phase *p;

    p = g_slot;
    g_c20 = 0;
    g_c24 = 0;
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
    return 1;
}
