/* @implements 0x1003E370 glide Ctl3E370
 * @cpp_kind method
 * @cpp_symbol ?Activate@Ctl3E370@@QAEHXZ
 *
 * Same shared-return activate as the 201 B family, plus two global copies
 * before the slot test (orig 223 B). Do not name temps for the copies —
 * `int t0 = src` keeps eax/ecx live and flips the post-call flag stores
 * from `mov edx,[g_cur]` / c7 to the 201 B eax/ecx coloring (18 diffs).
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
int g_src1;
int g_src2;
int g_dst1;
int g_dst2;

void EnterFn(Phase *);

class Ctl3E370 {
public:
    int Activate();
};

int Ctl3E370::Activate()
{
    Phase *p;

    g_dst1 = g_src1;
    p = g_slot;
    g_dst2 = g_src2;
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
