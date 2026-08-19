/* br_uivt.h -- the three UI entry points the BrUiPage_ / BrUiCtl_ model needs:
 * the page constructor 0x10048470 and the control vtable's +0x34 and +0x38.
 *
 * READ THIS BEFORE ADDING ANYTHING HERE
 *
 * All three addresses ARE ALREADY PORTED, in port/src/slice3_32.c, over that
 * module's byte-image object (`BrUiObj` / `BrUiPage`, indexed with
 * BrScrLd32 / BrScrSt32 at the original's 32-bit offsets plus an appended
 * pointer-slot array). Those bodies are correct; this file was written after
 * disassembling all three independently and they agree instruction for
 * instruction.
 *
 * This module exists because slice6_72 and slice6_73 -- the two packets the
 * host actually boots -- model the same objects as ORDINARY C STRUCTS
 * (`BrUiPage_`, `BrUiCtl_`), and a byte-offset body cannot be pointed at a
 * struct whose fields have moved. So the choice was a second transcription or
 * a rewrite of one of the two models, and the second transcription is the
 * smaller, reviewable change.
 *
 * UPDATE: those two structs are no longer slice6_73's. Both packets have been
 * migrated onto br_ui.h, which is now the single owner of `struct BrUiPage_`
 * and `BrUiCtl_`; this file is typed over br_ui.h's model, reached through
 * br_uictl.h. The duplication below is unchanged -- it is against
 * slice3_32.c's byte-image bodies, not against slice6_73.
 *
 * DUPLICATE OWNERSHIP, stated the way slice6_73.c states its own:
 *
 *   0x10048470  slice3_32.c BrUiPageCtor_10048470(BrUiPage *)   byte-image
 *               br_uivt.c   BrUiPageCtor_10048470(BrUiPage_ *)  struct
 *   0x10047EB0  slice3_32.c BrUiItemInit_10047EB0(BrUiObj *)    byte-image
 *               br_uivt.c   BrUiCtlSetText_10047EB0(BrUiCtl_ *) struct
 *   0x10047FB0  slice3_32.c BrUiInit_10047FB0(BrUiObj *)        byte-image
 *               br_uivt.c   BrUiCtlPlace_10047FB0(BrUiCtl_ *)   struct
 *
 * The page constructor is the sharp one: it is ONE SYMBOL NAME declared by
 * both slice3_32.h and slice6_73.h over incompatible pointee types. Before
 * this module, the host link bound slice6_73.c's call to slice3_32.c's body,
 * which wrote a BrUiPage layout into a BrUiPage_ allocation -- off by one
 * pointer field from +0x00C onward, and eight bytes past the end of what
 * BR73_ALLOC asks for. slice3_32.c's copy is renamed under BR_HOST_LINK so
 * each model reaches its own body; see the banner at the top of that file.
 *
 * The eventual fix is to merge the models, exactly as br_phase.h merged the
 * three views of the phase object. Until then the duplication is deliberate,
 * recorded here, and covered on both sides by tests.
 */
#ifndef BR_UIVT_H
#define BR_UIVT_H

#include "br_uictl.h"    /* BrUiCtl_, BrUiCtlVtbl_, BrUiPage_, BrTextBox */

/* XSLICE 0x10048470 -- __thiscall page constructor; returns `this`.
 *
 * Same name slice3_32.h and slice6_73.h both give it; this definition is the
 * one typed over br_phase.h's BrUiPage_. */
BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);

/* 0x1008F6F8 -- the page vtable the constructor stores.
 *
 * THIS HEADER USED TO DEFINE `BrUiPageVtbl_` ITSELF, as 24 `void *` slots
 * "bounded above by 0x1008F758, which slice3_39.h identifies as
 * BrTextListVtbl". That bound was wrong, and the image says so plainly:
 *
 *     1008F6F8  100484C0     <- slot 0
 *     1008F6FC  10048530     <- slot 1
 *     1008F700  10048850     <- the PHASE's vtable starts here
 *     ...
 *     1008F724  00000000     <- and ends
 *     1008F728  1005B0A0     <- BrTextBoxVtbl (slice3_39.h)
 *     1008F758  1005B8D0     <- BrTextListVtbl
 *
 * so a 24-slot page vtable swallows the phase's nine slots AND the text
 * box's twelve. br_ui.h read the same bytes and says TWO slots, which is what
 * this header now uses -- see the comment on br_ui.h's BrUiPageVtbl_, which
 * also explains why the .rdata adjacency implies no class hierarchy.
 *
 * Nothing is lost by shrinking it: every slot was NULL and nothing indexes
 * one. The type is br_ui.h's; only the storage lives here. */
extern const BrUiPageVtbl_ g_brUiPageVtbl_1008F6F8;

/* The pointer BrUiPageCtor_10048470 stores. Defaults to
 * &g_brUiPageVtbl_1008F6F8, i.e. non-NULL with NULL slots, so the store is
 * faithful and an unported method cannot be mistaken for a no-op. */
extern const void *g_pBrUiPageVtbl;

/* 0x10047EB0 -- control vtable slot +0x34, `set the control's text`.
 *
 * __thiscall in the original, four stack arguments (`ret 0x10`).
 *   pText   the string, copied WITH its NUL into the item's text buffer
 *   a2      flags OR-ed into the item's +0x04; bit 0 also triggers the
 *           item's vtable +0x28
 *   a3      the item's `kind` byte; 3 selects the item's vtable +0x08,
 *           anything else selects +0x04
 *   pStyle  a rectangle of four int32; only [0] and [2] are read
 */
void BrUiCtlSetText_10047EB0(BrUiCtl_ *pThis, const void *pText,
                             int32_t a2, int32_t a3, const void *pStyle);

/* 0x10047FB0 -- control vtable slot +0x38, `place the control`.
 *
 * __thiscall, eight stack arguments (`ret 0x20`), in the original's order.
 * Pure stores and ORs; nothing is read back. a4 and a5 are 2 and 5 at every
 * call site in the corpus, and this function never looks at either.
 */
void BrUiCtlPlace_10047FB0(BrUiCtl_ *pThis, BrPhase_ *pOwner,
                           float x, float y, int32_t flags,
                           int32_t a4, int32_t a5, int32_t a6, int32_t a7);

/* 0x1008F6B8 -- the control vtable, with +0x34 and +0x38 filled in and every
 * other slot NULL. Assign it to g_pBrUiCtlVtbl to give constructed controls
 * real behaviour:
 *
 *     g_pBrUiCtlVtbl = &g_brUiCtlVtbl_1008F6B8;
 *
 * Declared by br_uictl.h; defined here, because these are its only ported
 * slots. */

#endif /* BR_UIVT_H */
