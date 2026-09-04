/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003D3C0 glide Ctl3D3C0
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3D3C0@@QAEHXZ
 *
 * Family-1 installer plus a shared tail after both arms (slice2_25
 * BrOptOpen2948): if two guards are clear and the mode is 0 or 1, call
 * NetFn. return 1 AFTER that tail so the flag stores stay c7.
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
int g_guardA;
int g_guardB;
int g_mode;

void EnterFn(Phase *);
void NetFn(void);

class Ctl3D3C0 {
public:
    int Activate();
};

int Ctl3D3C0::Activate()
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
    if (g_guardA == 0 && g_guardB == 0 && (g_mode == 0 || g_mode == 1))
        NetFn();
    return 1;
}
