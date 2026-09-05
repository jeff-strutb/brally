/* br_itemiconrow.c -- menus: draw a menu item's row of repeated icons
 * (0x10037FA0).
 *
 * Same family as the C++-lane pair 0x10037EF0 / 0x100380B0 that bracket it:
 * a cdecl free function taking one object pointer, doing its work through
 * that object's vtable and returning 1.
 *
 * ‼ THIS BELONGS TO THE C++ LANE, for exactly one reason, and the structure
 * below is otherwise complete: 40 instructions against the original's 40 in
 * shape, REGNORM 4+2, and the ONLY rows are
 *
 *      recomp EXTRA    2x `mov R, I`  +  2x `push R`
 *      recomp MISSING  2x `push I`
 *
 * i.e. `push 0x74` and `push 0x75`.  A real thiscall pushes a constant
 * argument as an immediate; the C twin cannot.  The only way a C
 * `__fastcall` keeps its arguments off ecx/edx is the struct wrapper
 * (br_match.h), and a struct is a COPY -- VC5 materialises it into a
 * register first, turning a 2-byte `push imm8` into `mov eax,N` + `push
 * eax` (7 bytes).  That is the whole of the +8 bytes: 2 x 4.
 *
 * Identical finding, same week, on 0x10039C00 BrCtrlCfgReloadPreset
 * (src/core/generated/0x10039C00.c), whose four arms are the same shape.
 * Between them: the struct wrapper reaches a thiscall stack argument whose
 * value is in a VARIABLE, and does not reach one whose value is a LITERAL.
 * When the original pushes an immediate into a thiscall, route the function
 * to .cpp instead of probing C spellings.
 *
 * Everything else here is pinned by the bytes and is worth keeping for
 * whoever writes the .cpp: the object's position floats at +0x3C / +0x40,
 * `_ftol` on each, +0x13 added to y only, vtable slot +0x14 (index 5)
 * cached in a stack local and re-called through that slot, the head code
 * 0x74 and repeat code 0x75, the 12-pixel x step, and the count global
 * 0x10B71A68 re-read from memory on every iteration (both the entry test
 * and the back edge are UNSIGNED: `test/jbe` then `cmp/jb`).
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#include "br_match.h"   /* BR_THISCALL1 */

/* How many icons follow the first one. */
extern unsigned int g_brItemIconCount;   /* 0x10B71A68 */

/* Vtable slot +0x14 is a thiscall taking THREE stack arguments, so every
 * argument after `this` wears the struct wrapper -- see br_match.h. */
typedef struct { int v; } BrDrawArg;
typedef int (BR_THISCALL1 *BrItemDrawFn)(void *pThis, BrDrawArg code,
                                         BrDrawArg x, BrDrawArg y);

/* The object: a float pair at +0x3C / +0x40 is the item's position, and
 * the vtable is at +0. */
typedef struct {
    void  **vt;
    char    pad[0x3c - 4];
    float   x;      /* +0x3C */
    float   y;      /* +0x40 */
} BrIconItem;

/* WHAT IT DOES: paints the row of little icons that shows a menu setting's
 * value -- one "head" icon (code 0x74) at the item's own position, then one
 * repeat icon (code 0x75) per counted step, each 12 pixels further right on
 * the same baseline.  The baseline sits 19 pixels below the item's y.  The
 * count is re-read from the global on every step, so the drawing call is
 * free to change it.  Always reports success. */
/* @implements 0x10037FA0 glide BrItemDrawIconRow */
int BrItemDrawIconRow(BrIconItem *pItem)
{
    BrItemDrawFn fn;
    BrDrawArg code, x, y;
    unsigned int i;

    x.v = (int)pItem->x;
    y.v = (int)pItem->y + 0x13;
    fn = (BrItemDrawFn)pItem->vt[5];
    code.v = 0x74;
    fn(pItem, code, x, y);
    for (i = 0; i < g_brItemIconCount; i++) {
        code.v = 0x75;
        fn(pItem, code, x, y);
        x.v += 0xc;
    }
    return 1;
}

#endif /* BR_MATCHING_BUILD */
