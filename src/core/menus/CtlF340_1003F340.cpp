/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003F340 glide CtlF340
 * @cpp_kind method
 * @cpp_symbol ?Activate@CtlF340@@QAEHXZ
 *
 * Shared-return activate: `return 1` sits after the if/else so `mov eax,1`
 * stays out of the flag stores (they remain `c7` immediates). Ctor
 * DECLARED, no dtor — unwind is operator delete (maxState=1).
 *
 * 0xC8 Phase: +0 vtbl, +4 pfnEnter, +0xC f0C, +0x68 f68.
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

void EnterFn(Phase *);

class CtlF340 {
public:
    int Activate();
};

int CtlF340::Activate()
{
    Phase *p;

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
