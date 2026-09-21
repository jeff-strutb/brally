/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003F410 glide Ctl3F410
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3F410@@QAEHXZ
 *
 * Two sequential `new Phase` (maxState=2, both toState=-1, both unwind
 * operator delete). Second allocation is only on the just-built path of
 * the first and writes only its own slot + f0C (no g_cur, no f68).
 *
 * Live local `one` plus a *shared* `return one` after the if/else is what
 * hoists `mov esi,1` into the flags gap (`test eax,eax; mov esi,1; jne`).
 * `return one` inside both arms rematerializes the existing path as
 * `mov eax,1` and delays esi=1 until after the first enter (82 diffs).
 * Ctor DECLARED, no dtor.
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
Phase *g_slot2;

void EnterFn(Phase *);
void Enter2Fn(Phase *);

class Ctl3F410 {
public:
    int Activate();
};

int Ctl3F410::Activate()
{
    Phase *p;
    Phase *q;
    int one;

    p = g_slot;
    one = 1;
    if (p == 0) {
        p = new Phase;
        g_slot = p;
        g_cur = p;
        if (p == 0)
            return 0;
        p->pfnEnter = EnterFn;
        g_slot->pfnEnter(g_slot);
        g_cur->f0C = one;
        g_cur->f68 = one;
        q = new Phase;
        g_slot2 = q;
        if (q == 0)
            return 0;
        q->pfnEnter = Enter2Fn;
        g_slot2->pfnEnter(g_slot2);
        g_slot2->f0C = one;
    } else {
        g_cur = p;
    }
    return one;
}
