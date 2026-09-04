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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice8_84.h"

#include <stddef.h>
#include <string.h>

/* WHAT IT DOES: the Options button on the season-progress screen -- one of the
 * three picture buttons down its right-hand side. It opens the options screen
 * and wires that screen's Back row so the player comes back here when done. */
/* @implements 0x100457C0 d3d BrUiHook84_100457C0 */
int32_t BrUiHook84_100457C0(BrUiCtl_ *pCtl)
{
#ifdef BR_MATCHING_BUILD
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded. */
    ((int32_t (*)(BrUiCtl_ *))BrUiHook81Activate_100451E0)(pCtl);
    g_br73.pAA29C8->pfn08 = BrUiHook84_10046830;
    return 1;
#else
    BrUiCtl_ *pBack;

    (void)pCtl;                         /* pushed, ignored by the callee */
    (void)BrUiHook81Activate_100451E0();

    pBack = g_br73.pAA29C8;             /* 0x10AA29C8 -- a CONTROL */
    if (pBack != NULL)                  /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook84_10046830;
    return 1;
#endif
}

/* WHAT IT DOES: the Save button on the season-progress screen. It opens the
 * save-season screen and wires that screen's Back row to the variant that also
 * throws away the name being typed -- which is why backing out of a save
 * abandons the edit rather than keeping it. */
/* @implements 0x100457E0 d3d BrUiHook84_100457E0 */
int32_t BrUiHook84_100457E0(BrUiCtl_ *pCtl)
{
#ifdef BR_MATCHING_BUILD
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded -- the same
     * pair of defects as 0x100457C0 above. */
    ((int32_t (*)(BrUiCtl_ *))BrUiHook81Activate_10045BC0)(pCtl);
    g_br73.pAA29F4->pfn08 = BrUiHook84_10046870;
    return 1;
#else
    BrUiCtl_ *pBack;

    (void)pCtl;
    (void)BrUiHook81Activate_10045BC0();

    pBack = g_br73.pAA29F4;             /* 0x10AA29F4 -- a CONTROL */
    if (pBack != NULL)                  /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook84_10046870;
    return 1;
#endif
}
