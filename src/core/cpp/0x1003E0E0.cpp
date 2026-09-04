/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run, and
 * this one clears its buffer before opening. */
/* @implements 0x1003E0E0 glide Ctl3E0E0
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3E0E0@@QAEHXZ
 *
 * ResetBuf(&g_buf) then the shared-return activate (cpp-family2-notes.md).
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
char g_buf;

void EnterFn(Phase *);
void ResetBuf(void *);

class Ctl3E0E0 {
public:
    int Activate();
};

int Ctl3E0E0::Activate()
{
    Phase *p;

    ResetBuf(&g_buf);
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
