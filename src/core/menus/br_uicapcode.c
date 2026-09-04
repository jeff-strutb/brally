/* br_uicapcode.c -- menus: the two twin hooks that set a menu row's caption
 * code from a five-way choice on one option value (0x1003F5E0, 0x1003F680).
 * They differ only in the codes, and in what choice zero maps to.
 *
 * Filed out of slice8_87.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice8_87.c -- the slice2_23 family of control hooks over br_ui.h's
 * canonical BrUiCtl_.  See slice8_87.h for the pre-flight table, the four
 * places the brief and the tree disagree, the four conflicts this module
 * reports and the two slots it leaves NULL.
 *
 * Transcribed from orig/BRGlide.dll -- the project reference -- at the GLIDE
 * address of each pairing, with tools/dumpasm.py.  BRD3D.dll was read for
 * exactly two things: to settle what 0x1008C320 is (CONFLICT 1) and to
 * confirm that the seven .rdata tables are identical in both images.
 *
 * ==========================================================================
 * TWO PORT-WIDE DEVIATIONS, applied everywhere and stated once
 *
 * 1. NULL GUARDS.  The original dereferences a vtable pointer, a global
 *    object pointer and a global table pointer with no test, because in the
 *    original they are always there.  In this port the control vtable, the
 *    text-box vtable, g_pBr72Env, g_pBrUiNav and the two injected tables are
 *    all wired by the host and any of them can be NULL while the slot it
 *    would reach is unported.  Every one is guarded, and a guarded-out call
 *    is a MISSING EFFECT, not a no-op that has been argued to be equivalent.
 *    slice8_85.c and slice6_73.c's builders guard the same way.
 *
 * 2. BOUNDED COPIES.  Every string move in this range is an inlined
 *    `rep movsd` + `rep movsb` over strlen+1 bytes -- strcpy with no bound --
 *    into a fixed .data buffer or into the control's own 0x400-byte caption.
 *    Every one is bounded and NUL-terminated here.  0x1003FA00's scratch is
 *    a 0x74-byte stack buffer in the original (esp+0x10 inside a 0x84-byte
 *    frame) that a longer string smashes; it is BR87_TEXT_MAX here.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice8_87.h"

#include <string.h>

/* ==========================================================================
 * Code hooks -- a WORD into control +0x1E20C
 * ========================================================================== */

/* WHAT IT DOES: sets a menu row's caption code from a five-way choice on one
 * of the option values. An out-of-range value gives the same answer as
 * choice zero, so the two cannot be told apart. */
/* @implements 0x1003F5E0 d3d BrUiHook87_1003F5E0 */
#ifdef BR_MATCHING_BUILD
/* Global read directly; each arm carries its own store + return (the imm
 * values differ so nothing cross-jumps). The glide field offset is
 * 0x1E20C (the port struct maps the D3D 0x1E204). */
extern unsigned int DAT_10ac5d70;
#define BR87_W(p) (*(unsigned short *)((char *)(p) + 0x1E20C))

int32_t BrUiHook87_1003F5E0(BrUiCtl_ *pCtl)
{
    switch (DAT_10ac5d70) {
    case 0u: BR87_W(pCtl) = 0x56u; return 1;
    case 1u: BR87_W(pCtl) = 0x57u; return 1;
    case 2u: BR87_W(pCtl) = 0x59u; return 1;
    case 3u: BR87_W(pCtl) = 0x5Bu; return 1;
    case 4u: BR87_W(pCtl) = 0x5Du; return 1;
    default: BR87_W(pCtl) = 0x56u; return 1;
    }
}
#else
int32_t BrUiHook87_1003F5E0(BrUiCtl_ *pCtl)
{
    uint32_t k = (g_pBr72Env != NULL) ? (uint32_t)g_pBr72Env->nAA2A18 : 0u;
    uint16_t v;

    /* `cmp eax,4 / ja default` -- UNSIGNED, so a negative index is out of
     * range and lands on the default. */
    switch (k) {
    case 0u: v = 0x56u; break;
    case 1u: v = 0x57u; break;
    case 2u: v = 0x59u; break;
    case 3u: v = 0x5Bu; break;
    case 4u: v = 0x5Du; break;
    default: v = 0x56u; break;   /* the SAME value index 0 produces */
    }
    pCtl->w1E20C = v;
    return 1;
}
#endif

/* WHAT IT DOES: the twin of the hook above, on the same option value but
 * with a different set of caption codes. This one maps both choice zero and
 * an out-of-range value to the "no caption" sentinel, which is where the two
 * twins part company. */
/* @implements 0x1003F680 d3d BrUiHook87_1003F680 */
#ifdef BR_MATCHING_BUILD
int32_t BrUiHook87_1003F680(BrUiCtl_ *pCtl)
{
    switch (DAT_10ac5d70) {
    case 0u: BR87_W(pCtl) = 0xFFFFu; return 1;
    case 1u: BR87_W(pCtl) = 0x58u; return 1;
    case 2u: BR87_W(pCtl) = 0x5Au; return 1;
    case 3u: BR87_W(pCtl) = 0x5Cu; return 1;
    case 4u: BR87_W(pCtl) = 0x5Eu; return 1;
    default: BR87_W(pCtl) = 0xFFFFu; return 1;
    }
}
#else
int32_t BrUiHook87_1003F680(BrUiCtl_ *pCtl)
{
    uint32_t k = (g_pBr72Env != NULL) ? (uint32_t)g_pBr72Env->nAA2A18 : 0u;
    uint16_t v;

    switch (k) {
    case 0u: v = 0xFFFFu; break;   /* index 0 IS the sentinel in this twin */
    case 1u: v = 0x58u; break;
    case 2u: v = 0x5Au; break;
    case 3u: v = 0x5Cu; break;
    case 4u: v = 0x5Eu; break;
    default: v = 0xFFFFu; break;
    }
    pCtl->w1E20C = v;
    return 1;
}
#endif
