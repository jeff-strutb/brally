/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run, and it
 * opens ONLY if the CD check passes -- this is a page whose content lives on
 * the disc. */
/* @implements 0x1003ED90 glide Ctl3ED90
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3ED90@@QAEHXZ
 *
 * CD-gated installer (slice3_31 BrPhaseActivate_10045900). Polarity is
 * `if (CdCheck() != 0) { success; return 1; } fail; return 0` so the large
 * path is fall-through (orig `je` far to the fail block at the end).
 * `if (CdCheck() == 0) { fail; return 0; } success` inverts to a short jne.
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
int CdCheck(void);
char *GetStr(int);
void ResetBuf(void *);

class Ctl3ED90 {
public:
    int Activate();
};

int Ctl3ED90::Activate()
{
    Phase *p;

    if (CdCheck() != 0) {
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
    ResetBuf(GetStr(0xD));
    return 0;
}
