/* @implements 0x1003D930 glide Ctl3D930
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3D930@@QAEHXZ
 *
 * Slot load, then three global stores, then shared-return activate
 * (BrOptOpen2950B). After both arms, write pfnHook at +8 of a third
 * Phase *. Do not name temps for the stores.
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
typedef char chk_hk[(unsigned)&((Phase *)0)->pfnHook == 8 ? 1 : -1];

Phase *g_slot;
Phase *g_cur;
Phase *g_hookOwner;
int g_host;
int g_kind;
int g_flag;

void EnterFn(Phase *);
void HookFn(Phase *);

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
