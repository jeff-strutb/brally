/* br_uiseasonbtn.c -- menus: the Options and Save picture buttons down the
 * right-hand side of the season-progress screen (0x100457C0, 0x100457E0).
 * Each opens its screen and wires that screen's Back row.
 *
 * Filed out of slice8_84.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice8_84.c -- the control hooks the slice6_71 and slice6_72 screen
 * builders install.  See slice8_84.h for what this module is, which pairing
 * each hook was read out of, the five conflicts it reports and the list of
 * slots it deliberately leaves NULL.
 *
 * The file has three kinds of function and nothing else:
 *
 *   ADAPTER      a one-line forward to an existing, verified body.  Six of
 *                them, and every one is over a body whose stack argument the
 *                original PROVABLY never reads -- see the disassembly evidence
 *                in the header.  No second opinion is formed here; if one of
 *                those bodies is wrong it is slice2_25.c that is wrong.
 *   TRANSCRIPTION  a control-typed body for an address whose only existing
 *                port is over a byte-image model that cannot take a
 *                `BrUiCtl_ *` on LP64.  Same situation, same remedy and the
 *                same file-level shape as port/src/slice7_81.c.
 *   INSTALLER    the two table fills at the bottom.
 *
 * Transcribed from orig/BRD3D.dll (these are D3D addresses) and cross-checked
 * against orig/BRGlide.dll.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_ui.h"   /* br_globals: its objects */
#include "slice8_84.h"

#include <stddef.h>
#include <string.h>

/* g_br73 is the port's gathering of separate originals.  The matching build
 * names the ones used here as the globals they are (config/globals_glide.csv),
 * so each relocation resolves to its own variable. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5D4C */
#define BR73_PAA29F4 g_brUipAA29F4
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC5D20 */
#define BR73_PAA29C8 g_brUipAA29C8

/* WHAT IT DOES: the Options button on the season-progress screen -- one of the
 * three picture buttons down its right-hand side. It opens the options screen
 * and wires that screen's Back row so the player comes back here when done. */
/* @implements 0x100457C0 d3d BrUiHook84_100457C0 */
int32_t BrUiHook84_100457C0(BrUiCtl_ *pCtl)
{
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded. */
    ((int32_t (*)(BrUiCtl_ *))Ctl3E730_fn)(pCtl);
    BR73_PAA29C8->pfn08 = BrOpt6830;
    return 1;
}

/* WHAT IT DOES: the Save button on the season-progress screen. It opens the
 * save-season screen and wires that screen's Back row to the variant that also
 * throws away the name being typed -- which is why backing out of a save
 * abandons the edit rather than keeping it. */
/* @implements 0x100457E0 d3d BrUiHook84_100457E0 */
int32_t BrUiHook84_100457E0(BrUiCtl_ *pCtl)
{
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded -- the same
     * pair of defects as 0x100457C0 above. */
    ((int32_t (*)(BrUiCtl_ *))CtlF060_fn)(pCtl);
    BR73_PAA29F4->pfn08 = BrUiHook84_10046870;
    return 1;
}
