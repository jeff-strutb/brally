/* br_ctrlquery.c -- controls.
 *
 * Asking the settings block what one action of one control layout is bound
 * to: what KIND of input it is (keyboard, joystick button, joystick axis)
 * and WHICH one, the pair the redefine screen needs to label a row.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* slice3_42.h declares these cdecl; the originals are thiscall with two
 * stack arguments.  Hide the prototypes so the matching bodies can carry
 * the __fastcall shape with struct-typed arguments (never register-eligible,
 * so neither can claim edx). */
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#endif
#include "slice3_42.h"
#ifdef BR_MATCHING_BUILD
#undef BrFn10069BC0
#undef BrFn10069C30
/* BOTH stack arguments are struct-wrapped. __fastcall skips a struct when it
 * hands out ecx/edx, so wrapping only the FIRST of them lets the SECOND take
 * edx and the function cleans 4 bytes instead of 8. Wrapping both leaves ecx
 * for `this` and puts the pair on the stack, which is thiscall exactly. */
typedef struct { int32_t v; } BrCtrlKindArg;
typedef struct { uint32_t v; } BrCtrlKeyArg;
#endif

#ifndef BR_MATCHING_BUILD
/* A PRIVATE COPY of slice3_42.c's file-static BrCtrlProfileIndex, verbatim.
 *
 * Why a copy is correct here, and why it is not a shared-state hazard: this
 * is a pure, stateless dispatch function, not a static variable -- two copies
 * cannot drift into two different values.  The tree already does exactly this
 * with BrFtol, which slice1_02.c and slice2_12.c each define privately for
 * the same reason.
 *
 * Why it is #ifndef BR_MATCHING_BUILD: the MATCHING arms below do not call
 * it.  Each original writes its 1/2/3/default dispatch out in full, and
 * factoring the profile choice into a helper collapses the four arms into one
 * indexed load and loses half the function -- so the matching build must not have it
 * in scope.  Only the PORT arms call them, and without this copy those
 * arms compile to a C4013 implicit declaration and leaves an unresolved external
 * `_BrCtrlProfileIndex` in the object: a link failure no sweep can see.
 *
 * slice3_42.c keeps its own definition; its unconditional users
 * (BrCtrlCfgCopy, BrCtrlCfgAssign) never leave that file. */
static int BrCtrlProfileIndex(int32_t sel)
{
    if (sel == 1) return 1;
    if (sel == 2) return 2;
    if (sel == 3) return 3;
    return 0;
}
#endif

/* 0x10069BC0 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says what KIND of thing an action is bound to -- keyboard,
 * joystick button or joystick axis -- for one action of one control layout,
 * which is how the redefine screen knows which sort of label to draw. */
/* @implements 0x10069BC0 d3d BrFn10069BC0 */
#ifdef BR_MATCHING_BUILD
/* thiscall: the config is in ecx and both selectors are on the stack, so the
 * `kind` argument is struct-wrapped to keep __fastcall out of edx -- the same
 * trick BrCtrlCfgLoadDefaults uses above.
 *
 * The four arms are written out IN FULL, and that is the whole shape of this
 * function. Factoring the profile choice into a helper (the portable body
 * below does exactly that) collapses them into one indexed load and loses
 * half the function. The original also folds the profile into the ROW index
 * before scaling -- `(key + 28*k) * 3` over one flat table -- rather than
 * indexing a profile and then a row, which is why each arm adds its own
 * literal to `key`. */
int32_t BR_THISCALL1 BrFn10069BC0(void *pThis, BrCtrlKindArg kind,
                                  BrCtrlKeyArg key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    switch (kind.v) {
    case 1:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x1C][0] & 0xFF00u);
    case 2:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x38][0] & 0xFF00u);
    case 3:
        return (int32_t)(pCfg->profile[0].e[key.v + 0x54][0] & 0xFF00u);
    }
    return (int32_t)(pCfg->profile[0].e[key.v][0] & 0xFF00u);
}
#else
int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);

    return (int32_t)(pCfg->profile[k].e[key][0] & 0xFF00u);
}
#endif

/* 0x10069C30 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says WHICH key, button or axis an action is bound to, as a
 * bare number, paired with the kind reported above. The two answers are not
 * symmetric -- for the keyboard layout it never looks at the axis case at
 * all -- so a caller has to know the kind before the number means anything. */
/* @implements 0x10069C30 d3d BrFn10069C30 */
#ifdef BR_MATCHING_BUILD
/* Same thiscall shape and the same written-out arms as BrFn10069BC0 above:
 * both stack arguments struct-wrapped, and each arm folds its own literal
 * into the flat row index rather than picking a profile first.
 *
 * The 0x8000 test exists ONLY on the 1/2/3 arms. The fall-through arm reads a
 * plain byte with `mov al,[..]` and never looks at the high half, which is why
 * it is written as a byte-typed read rather than as the shared expression with
 * the test skipped. VC5 cross-jumps the tails of arms 2 and 3 by itself (arm 3
 * ends in a `jmp` into arm 2) and leaves arm 1 with its own copy -- that is
 * the compiler's layout, not a difference in how the three are spelled. */
uint8_t BR_THISCALL1 BrFn10069C30(void *pThis, BrCtrlKindArg kind,
                                  BrCtrlKeyArg key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    switch (kind.v) {
    case 1: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x1C][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 2: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x38][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 3: {
        const uint16_t v = pCfg->profile[0].e[key.v + 0x54][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    }
    return (uint8_t)pCfg->profile[0].e[key.v][0];
}
#else
uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;
    const int        k    = BrCtrlProfileIndex(kind);
    const uint16_t   v    = pCfg->profile[k].e[key][0];

    if (k != 0 && v >= 0x8000u)
        return (uint8_t)(v >> 8);
    return (uint8_t)v;
}
#endif
