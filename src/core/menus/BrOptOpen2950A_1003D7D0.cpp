/* BrOptOpen2950A_1003D7D0.cpp -- menus, one C++ TU: 0x1003D620 CtlD620 (a
 * page opener) and, after it, 0x1003D7D0 BrOptOpen2950A (join a network game
 * and open its lobby). */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

/* WHAT IT DOES: open this menu page: create its object the first time it is
 * asked for, run its enter routine and make it the current page. One of a
 * family of near-identical page openers -- each owns its own page slot, and
 * the page object is created ONCE and reused for the rest of the run. */
/* @implements 0x1003D620 glide CtlD620
 * @cpp_kind method
 * @cpp_symbol ?Activate@CtlD620@@QAEHXZ
 *
 * Shared-return activate: `return 1` sits after the if/else so `mov eax,1`
 * stays out of the flag stores (they remain `c7` immediates). Ctor
 * DECLARED, no dtor — unwind is operator delete (maxState=1).
 *
 * 0xC8 Phase: +0 vtbl, +4 pfnEnter, +0xC f0C, +0x68 f68.
 */
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

extern "C" Phase *DAT_10ac5ca4;   /* the original global; was a per-file stand-in definition */
#define g_slot DAT_10ac5ca4
Phase *g_cur;

/* EnterFn was a stand-in; the original calls BrOptFn100575F0 (?BrOptFn100575F0@@YAHPAVGameUi@@@Z).  Declared under
 * its real symbol so the relocation resolves by name. */
class GameUi;
int BrOptFn100575F0(GameUi *);
#define EnterFn ((void (*)(Phase *))BrOptFn100575F0)

class CtlD620 {
public:
    int Activate();
};

int CtlD620::Activate()
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


class NameList55 {
public:
    int  hdr;
    char asz[100][0x104];
    NameList55();
};

class OptObj41B60 {
public:
    virtual void v0();

    int       (*pfnOpen)(OptObj41B60 *);  /* +0x04 */
    void       *f008;
    int         f00C;
    short       w010;
    short       w012;
    char        pad014[0x50];
    void       *f064;
    int         f068;
    int         a06C[20];
    short       w0BC;
    char        pad0BE[2];
    NameList55 *pC0;
    NameList55 *pC4;

    OptObj41B60();
};

typedef char chk_sz[sizeof(OptObj41B60) == 0xC8 ? 1 : -1];

struct PhaseCtx5D10 {
    int   f000;
    int   f004;
    int (*pfnTick)(void *);       /* +0x08 */
};

struct Screen5D2C {
    char           pad[0x1E164];
    unsigned short w1E164;
};

extern "C" {
int           g_brAA2878;         /* 0x10AC5BD0 */
int           g_brAA287C;         /* 0x10AC5BD4 */
int           g_brAA2884;         /* 0x10AC5BDC */
int           g_brAA2898;         /* 0x10AC5BF0 */
char          g_aBrA9CDF0[];      /* 0x10AC3E80 */
OptObj41B60  *g_brPAA2950;        /* 0x10AC5CA8 */
OptObj41B60  *g_brPAA29B8;        /* 0x10AC5C5C */
Screen5D2C   *g_brPAA29D4;        /* 0x10AC5D2C */
void         *g_brPAA29D8;        /* 0x10AC5D30 */
PhaseCtx5D10 *g_brPAA2A18;        /* 0x10AC5D10 */
int           BrSub1003C260(void);          /* 0x100358F0 */
void          BrSub1003C1E0(void);          /* 0x10035870 */
int           BrOptFn10057C10(OptObj41B60 *);   /* 0x10050AC0 */
int           BrOptFn10044970(void *);          /* 0x1003DEC0 */
}

/* WHAT IT DOES: joins somebody else's network game and opens the lobby
 * screen for it. It refuses to go ahead when the player has not typed a
 * long enough name in one of the modes, and falls back to setting the
 * connection up first if there is nothing to join yet.  The lobby's menu
 * object is created on first use (its open hook installed and run), and
 * the phase's tick hook is pointed at the lobby stepper. */
/* @implements 0x1003D7D0 glide BrOptOpen2950A
 * @cpp_kind free
 * @cpp_symbol ?BrOptOpen2950A@@YAHPAX@Z
 *
 * cdecl, one (unused) arg, EH frame for the single `new OptObj41B60`
 * (unwind state 0 while its constructor runs; the raw pointer is spilled
 * to [esp+4] for the funclet).  The strlen is the repne-scasb intrinsic,
 * which is why edi is the one callee-saved register.  The failed-new
 * return hands back the null pointer in eax without a fresh zero.
 *
 * Byte-exact 2026-09-27.  Two source facts carry it:
 *   - forward `goto done` to ONE `return 1` after the open block, the
 *     2/3-mode arm ending in a single `goto done` after its if/else;
 *   - a function compiled ahead of it in the TU.  Alone in its TU, VC5 /O2
 *     tail-duplicates that epilogue at every unconditional jump (383-400 B);
 *     with any predecessor the original's single epilogue comes out.  The
 *     original's neighbour 0x1003D620 CtlD620 is filed above it here.
 * Earlier probes, kept so they are not re-run: early `return 1`s (400),
 * `goto done` at every site with the open block behind `goto open` (399),
 * a success flag tested once (334-340, VC5 keeps the `test/je`), `||`/`?:`
 * conditions and an inlined bool predicate (339-356, `neg/sbb`), /O1 /Os
 * /Ob0 /Ob2 /Ox /Gy /Gi-chain and the /O2 components one by one.
 */
int BrOptOpen2950A(void *pUnused)
{
    OptObj41B60 *p;

    g_brAA2884 = 0;
    g_brAA2898 = 0;

    if (g_brAA2878 == 0) {
        if (g_brAA287C == 2 || g_brAA287C == 3) {
            g_brAA2898 = 1;
            if (g_brAA287C == 2 && strlen(g_aBrA9CDF0) < 7)
                goto done;
            if (g_brPAA29D8 != 0 && g_brPAA29D4->w1E164 > 0) {
                if (BrSub1003C260() != 0)
                    goto open;
            } else {
                BrSub1003C1E0();
            }
            goto done;
        } else {
            if (BrSub1003C260() == 0)
                goto done;
        }
    }
open:
    if (g_brPAA2950 == 0) {
        p = new OptObj41B60;
        g_brPAA2950 = p;
        g_brPAA29B8 = p;
        if (p == 0)
            return 0;
        p->pfnOpen = BrOptFn10057C10;
        g_brPAA2950->pfnOpen(g_brPAA2950);
        g_brPAA29B8->f00C = 1;
        g_brPAA29B8->f068 = 1;
    } else {
        g_brPAA29B8 = g_brPAA2950;
    }
    g_brPAA2A18->pfnTick = BrOptFn10044970;
done:
    return 1;
}
