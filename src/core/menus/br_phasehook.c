/* br_phasehook.c -- menus: the screen-opening hook installers and the click
 * dispatcher at 0x10045050 / 0x10045090 / 0x100450C0 / 0x100450F0.
 *
 * Filed out of slice2_26.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice2_26.c -- BRD3D.dll 0x100447D0-0x100456B0, a later pass.
 *
 * The phase (screen/mode) switcher. See slice2_26.h for the mechanism, the
 * calling-convention deviations, and the per-function notes.
 *
 * Structural notes that apply throughout:
 *
 *  - Every global in this range is read through pCtx on every use, never
 *    cached in a local, because the original reloads them across calls and
 *    several of those reloads are observable (a hook that re-points the
 *    current phase, or 0x1003C020 changing nAA287C). Where the original DOES
 *    cache a value across a call, a local is used and the fact is commented.
 *
 *  - The SEH frames the original pushes (push -1 / push handler / fs:[0]) are
 *    pure MSVC exception bookkeeping around the `new` expression. There is no
 *    throw and no catch here, so nothing is emitted for them.
 *
 *  - The `push ecx` / `sub esp,8` prologues only reserve the stack slot the
 *    unwinder needs for the object under construction; they carry no data
 *    beyond the object pointer, which is a local here.
 */

#include <stddef.h>
#include "br_phase.h"   /* BR_PHASE_ALLOC_SIZE */

#ifdef BR_MATCHING_BUILD
/* Header prototypes thread pCtx; the original leave/hook/dispatch
 * functions are one-arg cdecl (entity or click arg). */
#define BrPhaseHook_100450C0     BrPhaseHook_100450C0_port
#define BrPhaseLeave_10044B40    BrPhaseLeave_10044B40_port
#define BrPhaseLeave_10044C70    BrPhaseLeave_10044C70_port
#define BrPhaseLeave_10044DE0    BrPhaseLeave_10044DE0_port
#define BrPhaseLeave_10044F00    BrPhaseLeave_10044F00_port
#define BrPhaseHook_10045050     BrPhaseHook_10045050_port
#define BrPhaseHook_10045090     BrPhaseHook_10045090_port
#define BrPhaseDispatch_100450F0 BrPhaseDispatch_100450F0_port
#endif
#include "slice2_26.h"
#ifdef BR_MATCHING_BUILD
#include "br_match.h"
#undef BrPhaseHook_100450C0
#undef BrPhaseLeave_10044B40
#undef BrPhaseLeave_10044C70
#undef BrPhaseLeave_10044DE0
#undef BrPhaseLeave_10044F00
#undef BrPhaseHook_10045050
#undef BrPhaseHook_10045090
#undef BrPhaseDispatch_100450F0
extern int32_t  g_br0AA010;        /* 0x100AA010 */
extern int32_t  g_br0AC304;        /* 0x100ABAA4 */
extern BrPhase *g_brPhaseAA2904;   /* 0x10AA2904 current phase */
extern BrPhase *g_brAA2908;
extern BrPhase *g_brAA2940;
extern BrPhase *g_brAA295C;
extern BrPhase *g_brAA2964;
extern BrPhase *g_brAA2968;
extern BrPhase *g_brAA29B0;        /* 0x10AA29B0 */
extern BrPhase *g_brAA29B4;
extern BrPhase *g_brAA29F4;
extern int32_t  g_brAA298C;
extern int32_t  g_brAA29E8;
#define BR26_AA29B0  g_brAA29B0
#define BR26_0AA010  g_br0AA010
typedef void (BR_THISCALL1 *Br26F1C)(BrEntSub *);
/* Slot 0 thiscall with one stack arg: edx must be a LIVE value (the
 * vtbl) so the site is `push 1; call [edx]`, not `xor edx,edx` and not
 * `mov eax,1; push eax` from a struct temp. */
typedef void *(__fastcall *Br26F00)(BrPhase *, const BrPhaseVtbl *, int32_t);
#else
#define BR26_AA29B0  (pCtx->pAA29B0)
#define BR26_0AA010  (pCtx->n0AA010)
#endif

/* ==========================================================================
 * Hook installers and the dispatcher
 * ========================================================================== */

/* WHAT IT DOES: opens a screen and then wires up its Back row so the player
 * can get out again. It suppresses one flag across the opening and restores it
 * afterwards, and it deliberately reads the row to wire AFTER the screen is
 * built, so it catches the row the new screen just published rather than a
 * stale one. */
/* @implements 0x10045050 d3d BrPhaseHook_10045050 */
/* @n64 0x80211194 located */
#ifdef BR_MATCHING_BUILD
int BrPhaseHook_10045050(void *pArg)
{
    /* Orig is one-arg cdecl; it pushes that arg at Activate_45110, which
     * ignores it. */
    g_br0AC304 = 0;
    (void)BrPhaseActivate_10045110((BrPhaseCtx *)pArg);
    g_br0AC304 = 1;
    g_brAA29B4->pfnHook = BrExt_10046CD0;
    g_br0AA010 = 0;
    return 1;
}
#else
int BrPhaseHook_10045050(BrPhaseCtx *pCtx, void *pArg)
{
    /* The original pushes pArg at 0x10045110, which ignores it (both are
     * __cdecl, so the extra argument is harmless). Nothing else uses pArg. */
    (void)pArg;

    pCtx->n0AC304 = 0;
    (void)BrPhaseActivate_10045110(pCtx);   /* result discarded */
    pCtx->n0AC304 = 1;

    /* pAA29B4 is read after the activation, so an activation that changes it
     * is what gets hooked. */
    pCtx->pAA29B4->pfnHook = BrExt_10046CD0;
    pCtx->n0AA010 = 0;
    return 1;
}
#endif

/* WHAT IT DOES: opens a screen and then wires its Back row to the routine that
 * returns the player to the previous screen. Same pattern as its neighbour
 * above, without the flag juggling. */
/* @implements 0x10045090 d3d BrPhaseHook_10045090 */
#ifdef BR_MATCHING_BUILD
int BrPhaseHook_10045090(void *pArg)
{
    BrExt_10045C90(pArg);
    g_brAA29B0->pfnHook = BrExt_10046DC0;
    g_br0AA010 = 0;
    return 1;
}
#else
int BrPhaseHook_10045090(BrPhaseCtx *pCtx, void *pArg)
{
    BrExt_10045C90(pArg);

    pCtx->pAA29B0->pfnHook = BrExt_10046DC0;
    pCtx->n0AA010 = 0;
    return 1;
}
#endif

/* WHAT IT DOES: the same as the routine above -- open a screen, wire its Back
 * row -- with one extra preparation call in front of it. That call is a
 * do-nothing stub in this build, so the two behave identically here; the
 * difference is preserved because the order is what the original recorded. */
/* @implements 0x100450C0 d3d BrPhaseHook_100450C0 */
#ifdef BR_MATCHING_BUILD
int BrPhaseHook_100450C0(void *pArg)
#else
int BrPhaseHook_100450C0(BrPhaseCtx *pCtx, void *pArg)
#endif
{
    BrExt_10041BD0();
    BrExt_10045C90(pArg);

    BR26_AA29B0->pfnHook = BrExt_10046DC0;
    BR26_0AA010 = 0;
    return 1;
}

/* WHAT IT DOES: passes a click straight on to whatever routine a particular
 * remembered control has had plugged into it, then puts the menus back into
 * their default mode. Unlike the three routines above it reports failure rather
 * than success, on every path. */
/* @implements 0x100450F0 d3d BrPhaseDispatch_100450F0 */
/* @n64 0x80241F88 located */
#ifdef BR_MATCHING_BUILD
int BrPhaseDispatch_100450F0(void *pArg)
{
    g_brAA29F4->pfnHook(pArg);
    g_br0AA010 = 0;
    return 0;
}
#else
int BrPhaseDispatch_100450F0(BrPhaseCtx *pCtx, void *pArg)
{
    pCtx->pAA29F4->pfnHook(pArg);
    pCtx->n0AA010 = 0;
    return 0;   /* GOTCHA: 0, unlike its three neighbours */
}
#endif
