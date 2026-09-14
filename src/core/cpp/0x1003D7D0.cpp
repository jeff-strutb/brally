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
 * T2 2026-09-13 (fresh C++ transcription, 9 cpp_score probes): every
 * instruction of the original is present, FuncInfo matches; the residue is
 * BLOCK LAYOUT.  The original funnels all four early exits through one
 * `mov eax,1` epilogue at the end (`jmp done`) and keeps the "connect
 * first" arm inline; VC5 here duplicates the epilogue at the else-arm's
 * exit, moves that arm out of line, and copies the join (tick-hook store
 * + return) into the lobby-object else-arm -- 400/340 B.  Dead: early
 * `return 1`s (400), `goto done` everywhere (432, every site duplicated),
 * the else-arm as `goto open; return 1` (404), all four sites as
 * goto/return pairs (404), the mode block nested under `g_brAA2878 == 0`
 * with returns (396) and with gotos (399), /O1, /Os and /Og- shapes
 * (257-408 B).  The C twin in br_optcycle.c cannot spell the EH frame at
 * all (192/340), so this file is the lane for the row.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

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

int BrOptOpen2950A(void *pUnused)
{
    OptObj41B60 *p;

    g_brAA2884 = 0;
    g_brAA2898 = 0;

    if (g_brAA2878 != 0)
        goto open;
    if (g_brAA287C == 2 || g_brAA287C == 3) {
        g_brAA2898 = 1;
        if (g_brAA287C == 2 && strlen(g_aBrA9CDF0) < 7)
            return 1;
        if (g_brPAA29D8 != 0 && g_brPAA29D4->w1E164 > 0) {
            if (BrSub1003C260() != 0)
                goto open;
        } else {
            BrSub1003C1E0();
        }
    } else {
        if (BrSub1003C260() != 0)
            goto open;
    }
    return 1;

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
    return 1;
}
