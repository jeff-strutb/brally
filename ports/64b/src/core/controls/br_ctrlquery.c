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

/* slice3_42.h declares these cdecl; the originals are thiscall with two
 * stack arguments.  Hide the prototypes so the matching bodies can carry
 * the __fastcall shape with struct-typed arguments (never register-eligible,
 * so neither can claim edx). */
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#include "slice3_42.h"
#include "br_ctrltoggle.h"   /* port: the Toggle Remaster action */
#undef BrFn10069BC0
#undef BrFn10069C30
/* BOTH stack arguments are struct-wrapped. __fastcall skips a struct when it
 * hands out ecx/edx, so wrapping only the FIRST of them lets the SECOND take
 * edx and the function cleans 4 bytes instead of 8. Wrapping both leaves ecx
 * for `this` and puts the pair on the stack, which is thiscall exactly. */
typedef struct { int32_t v; } BrCtrlKindArg;
typedef struct { uint32_t v; } BrCtrlKeyArg;


/* 0x10069BC0 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says what KIND of thing an action is bound to -- keyboard,
 * joystick button or joystick axis -- for one action of one control layout,
 * which is how the redefine screen knows which sort of label to draw. */
/* @implements 0x10069BC0 d3d BrFn10069BC0 */
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
int32_t BR_THISCALL1 BrFn10069BC0(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    if (key == BR_ACT_TOGGLE_REMASTER)          /* port: its own record */
        return kind >= 1 && kind <= 3 ? (int32_t)(BrToggleBindRec(kind)[0] & 0xFF00u)
                                      : (int32_t)(BrToggleBindRec(0)[0] & 0xFF00u);
    switch (kind) {
    case 1:
        return (int32_t)(pCfg->profile[0].e[key + 0x1C][0] & 0xFF00u);
    case 2:
        return (int32_t)(pCfg->profile[0].e[key + 0x38][0] & 0xFF00u);
    case 3:
        return (int32_t)(pCfg->profile[0].e[key + 0x54][0] & 0xFF00u);
    }
    return (int32_t)(pCfg->profile[0].e[key][0] & 0xFF00u);
}

/* 0x10069C30 -- name fixed by the XSLICE declaration in slice2_23.h. */
/* WHAT IT DOES: says WHICH key, button or axis an action is bound to, as a
 * bare number, paired with the kind reported above. The two answers are not
 * symmetric -- for the keyboard layout it never looks at the axis case at
 * all -- so a caller has to know the kind before the number means anything. */
/* @implements 0x10069C30 d3d BrFn10069C30 */
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
uint8_t BR_THISCALL1 BrFn10069C30(void *pThis, int32_t kind, uint32_t key)
{
    const BrCtrlCfg *pCfg = (const BrCtrlCfg *)pThis;

    if (key == BR_ACT_TOGGLE_REMASTER) {        /* port: its own record */
        const uint16_t v = BrToggleBindRec(kind >= 1 && kind <= 3 ? kind : 0)[0];
        if (kind >= 1 && kind <= 3 && v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    switch (kind) {
    case 1: {
        const uint16_t v = pCfg->profile[0].e[key + 0x1C][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 2: {
        const uint16_t v = pCfg->profile[0].e[key + 0x38][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    case 3: {
        const uint16_t v = pCfg->profile[0].e[key + 0x54][0];
        if (v >= 0x8000u) return (uint8_t)(v >> 8);
        return (uint8_t)v;
    }
    }
    return (uint8_t)pCfg->profile[0].e[key][0];
}
