/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003E9B0 glide Ctl3E9B0
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3E9B0@@QAEHXZ
 *
 * Shared-tail activate: new-path and existing-path both fall into
 * TailFn(); return 1. That keeps `mov eax,1` out of the flag stores
 * so they stay `c7` immediates (maxState=1, op-delete).
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

Phase *g_slot;
Phase *g_cur;

void EnterFn(Phase *);
void TailFn(void);

class Ctl3E9B0 {
public:
    int Activate();
};

int Ctl3E9B0::Activate()
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
    TailFn();
    return 1;
}
