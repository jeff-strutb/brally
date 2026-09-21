/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run, and
 * this one seeds the page with a value from the caller before opening. */
/* @implements 0x1003F260 glide Ctl3F260
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3F260@@QAEHXZ
 *
 * Shared-return activate (see docs/cpp-family2-notes.md):
 *   g_dst = g_src;
 *   if ((p = g_slot) == 0) { new Phase; flags; } else { g_cur = p; }
 *   return 1;
 * `return 1` MUST sit after the if/else (duplicated epilogues, c7 stores).
 * Ctor DECLAREd, no dtor — unwind is operator delete (maxState=1).
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
typedef char chk_ent[(unsigned)&((Phase *)0)->pfnEnter == 4 ? 1 : -1];
typedef char chk_c[(unsigned)&((Phase *)0)->f0C == 0xC ? 1 : -1];
typedef char chk_68[(unsigned)&((Phase *)0)->f68 == 0x68 ? 1 : -1];

Phase *g_slot;
Phase *g_cur;
int g_src;
int g_dst;

void EnterFn(Phase *);

class Ctl3F260 {
public:
    int Activate();
};

int Ctl3F260::Activate()
{
    Phase *p;

    g_dst = g_src;
    p = g_slot;
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
