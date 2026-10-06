/* br_optavail.c -- Front-end option availability: BrOptAvailB decides whether an option of the
 * second family is offered, from mode-dependent capability masks.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#undef BrOptSave
#undef BrOptAvailB

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* DEVIATION: `shl eax,cl` masks the count to 5 bits on x86; `1u << n` with
 * n >= 32 is undefined in C. The mask is applied explicitly so the C matches
 * the hardware. */
#define BR06_BIT(n) (1u << ((unsigned)(n) & 31u))

/* ==========================================================================
 * 0x1003F320
 * ========================================================================== */

/* WHAT IT DOES: decides whether one particular option is offered to the player
 * or shown unavailable, by testing its number against a bitmask of what the
 * machine and the current mode support. Which mask applies depends on the mode,
 * and there are several special cases -- some modes fold a high number back
 * into the low range, some declare everything below sixteen always available,
 * and one number is remapped to a different one entirely. Its sibling above
 * answers the same question for the other family of options. */
/* @t4-pass 0x10038860 1 2026-09-09 probes 10 bytes 284 insns 99 regions 2 rows 2 census no  (hand, fn.py variants: fixup spellings minus/hex/neg-add/unsigned, guard forms, remap and mask spellings, all inert or worse) */
/* @t4-pass 0x10038860 2 2026-09-09 probes 10 bytes 284 insns 99 regions 2 rows 2 census yes  (hand, fn.py variants: keep-sub mechanism experiment -- pointer difference, loop-carried, split constant, nested ifs, named subtrahend -- all still emit add-negative) */
/* @implements 0x1003F320 d3d BrOptAvailB */
/* One argument; every input is a loose global (fAlt and maskPair are each
 * loaded ONCE and live in registers across the whole function).  Raw
 * `1u << idx` (x86 masks the count in hardware; BR06_BIT's explicit &31
 * emits four real ANDs).
 * The mode-0 arm splits on fAlt FIRST and duplicates the idx-fixup and
 * fLowAlways test into both sub-arms -- the shared-logic form is 45 bytes
 * short. */
extern int32_t g_br6EE1DC_fRebaseB;      /* 0x10AC5C4C */
extern int32_t g_br6EE184_fAlt;          /* 0x10AC5BF4 */
extern int32_t g_br6EE0C8_maskPair;      /* 0x10AC5B38 */
extern int32_t g_br0A9360_mode;          /* 0x100A9360 */
extern int32_t g_br6EE1D8_fLowAlways;    /* 0x10AC5C48 */
extern int32_t g_br6EDE80_maskB;         /* 0x10AC58F0 */
extern int32_t g_brAAB88_maskB6;         /* 0x100AAB88 */
extern int32_t g_brAF3CE4_nAlwaysB;      /* 0x10AF3CE4 */
extern int16_t g_brAAB84_maskBDef;       /* 0x100AAB84 */

/* The option number is decremented IN PLACE in its own parameter (that is
 * what keeps `sub ecx,0x10` rather than `add ecx,-0x10`), and the alt mask
 * is the low half of the pair read as a 16-bit value (`and esi,0xFFFF` on
 * the dword already loaded for the 0x8000 test, ahead of the shift). */
int32_t BrOptAvailB(int32_t idx)
{

    if (g_br6EE1DC_fRebaseB != 0 && idx > 15)
        idx -= 16;
    if (g_br6EE184_fAlt != 0 && idx > 15
        && (g_br6EE0C8_maskPair & 0x8000) != 0)
        idx -= 16;

    if (g_br0A9360_mode == 0) {
        if (g_br6EE184_fAlt != 0) {
            if (idx == 15)
                idx = 11;
            if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
                return 1;
            return (int32_t)((1u << idx) & (uint16_t)g_br6EE0C8_maskPair);
        }
        if (idx == 15)
            idx = 11;
        if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
            return 1;
        return (int32_t)((1u << idx) & (uint32_t)g_br6EDE80_maskB);
    }

    if (g_br0A9360_mode == 6) {
        if (idx == 15)
            idx = 7;            /* 7 here, 11 everywhere else */
        if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
            return 1;
        return (int32_t)((1u << idx) & (uint32_t)g_brAAB88_maskB6);
    }

    if (g_br0A9360_mode == 2 && idx == g_brAF3CE4_nAlwaysB)
        return 1;

    if (idx == 15)
        idx = 11;
    if (g_br6EE1D8_fLowAlways != 0 && idx <= 15)
        return 1;
    /* movsx: SIGN-extended, unlike the zero-extended masks above. */
    return (int32_t)((1u << idx) & (uint32_t)(int32_t)g_brAAB84_maskBDef);
}
