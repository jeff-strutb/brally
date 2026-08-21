/* slice8_90.h -- the last unwired control hooks the three menu builders
 * install, as BrUiCtlHookFn_ slots.
 *
 * ===========================================================================
 * WHAT THIS MODULE IS, AND WHAT IT IS NOT
 * ===========================================================================
 * slice6_71.c, slice6_72.c and slice6_73.c each read a table of function
 * pointers and copy them into the controls they build.  108 distinct slots
 * exist across the three tables.  This packet fills thirteen of the ones that
 * were still NULL, and DECLINES eight more with the evidence for each refusal
 * written out below rather than left as a silent hole.
 *
 * NOTHING HERE IS A SECOND TRANSCRIPTION.  Every body already exists in
 * port/src -- slice2_24.c, slice3_31.c/br_sprfont.c -- and each was
 * re-checked against orig/BRGlide.dll for this pass.  What this file adds is
 * the ARGUMENT MARSHAL that lets a `BrUiCtl_ *` reach a body written against
 * a different model of the same original object.  That is the whole content:
 * a marshal is not a cast, and the difference is the point of the packet.
 *
 * ===========================================================================
 * THE HAZARD, STATED ONCE
 * ===========================================================================
 * A control hook slot is
 *
 *     typedef int32_t (*BrUiCtlHookFn_)(BrUiCtl_ *pCtl);      (br_ui.h)
 *
 * and BrUiCtl_ is the canonical 0x1E214 control.  The bodies are NOT written
 * against it:
 *
 *   slice2_24.c  `BrMenuItem` -- a BYTE IMAGE of the same object, carrying
 *                f1C at +0x1C, text at +0x2B5C and f1E20C at +0x1E20C.  It
 *                used to be a three-field compression; it was widened to the
 *                true displacements because the caption setters encode
 *                `mov word ptr [edx + 0x1E20C], cx` and cannot otherwise come
 *                out bit-identical.  That makes the two models AGREE on a
 *                32-bit target -- and still disagree under LP64, where every
 *                pointer ahead of the reached fields widens, by different
 *                amounts in each struct.  A cast is therefore still wrong
 *                here, for the same reason as the slice2_25 case below.
 *   slice2_25.c  `BrGameObj` -- a BYTE IMAGE with `pSub` pinned at +0x2AE8.
 *                On LP64 `offsetof(BrUiCtl_, pOwner)` is NOT 0x2AE8: the
 *                vtable pointer and the six hook pointers ahead of it widen
 *                by four bytes each, so the same displacement names a
 *                different field.  br_ui.h says this in its own banner.
 *
 * So the slice2_24 family gets a marshal (section 1) and the slice2_25 /
 * slice3_31 family is DECLINED (section 4).
 *
 * ===========================================================================
 * 1. WHAT IS WIRED, AND THE BUILDER LINE THAT PROVES EACH PAIRING
 * ===========================================================================
 *   D3D addr    Glide addr  slot   table  builder line
 *   ----------  ----------  -----  -----  --------------------------------
 *   0x100408D0  0x10039E10  pfn04    72   slice6_72.c:903
 *   0x100409B0  0x10039EF0  pfn04    72   slice6_72.c:1560
 *   0x100409D0  0x10039F10  pfn04    72   slice6_72.c:1544
 *   0x10040B30  0x1003A070  pfn04    72   slice6_72.c:1165
 *   0x10041040  0x1003A580  pfn04    72   slice6_72.c:1333
 *   0x10041180  0x1003A6D0  pfn04    72   slice6_72.c:1348
 *   0x10041300  0x1003A860  pfn04  71/72/73  slice6_71.c:570, slice6_72.c:1138,
 *                                            slice6_73.c:759 and :879
 *   0x100415A0  0x1003AB00  pfn04    72   slice6_72.c:1101
 *   0x10041670  0x1003ABD0  pfn04    73   slice6_73.c:901
 *   0x10041710  0x1003AC70  pfn04    73   slice6_73.c:939
 *   0x100417B0  0x1003AD10  pfn04    73   slice6_73.c:922
 *   0x10041890  0x1003ADF0  pfn04    71   slice6_71.c:476
 *   0x100474B0  0x10040AF0  pfn0C    72   slice6_72.c:798
 *
 * ARITY AND RETURN, read out of the Glide listings rather than taken from a
 * parameter name.  All twelve slice2_24 bodies take exactly one stack
 * argument at [esp+4] and reach it at four displacements and no others --
 * +0x1C, +0x1E20C, +0x2B5C (the text box's vtable and `this`) and +0x2B65
 * (its buffer), plus +0x2B64 in 0x10041890.  Every one leaves eax = 1 except
 * 0x10041300 / 0x100415A0 / 0x100417B0, whose ported bodies return 0 on the
 * "no stage table" path this port is in; 0x10048180 tests a pfn04 result only
 * against -1 and -2, so 0 is inert there and NOT a page stop.
 *
 * 0x100474B0 is nineteen bytes -- `push [esp+4]; call 0x10047360; add esp,4;
 * mov eax,1; ret`.  br_sprfont.c already has 0x10047360 typed over BrUiCtl_
 * (BrSprFontKindHook_10047360), so this one needs no marshal at all.
 *
 * ===========================================================================
 * 2. HOW THE MARSHAL WORKS, AND WHAT IT COSTS
 * ===========================================================================
 * Br90 builds a BrMenuItem VIEW on the stack, seeded from the control:
 *
 *     item.f1C        <- pCtl->flags1C
 *     item.f1E20C     <- pCtl->w1E20C            (uint16 <-> int16, bit exact)
 *     item.text.f04   <- pCtl->aText[0].f04
 *     item.text.f08   <- pCtl->aText[0].f08
 *     item.text.sz    <- pCtl->aText[0].sz
 *     item.text.pVtbl <- a SHIM vtable, or NULL when the box has none
 *
 * runs the body, and copies the same five back.  The shim is the part that
 * matters: slice2_24.h's BrMenuTextVtbl and slice3_39.h's BrTextBoxVtbl are
 * different C types over the same original vtable, so the shim's four reached
 * slots (+0x04, +0x08, +0x10, +0x2C -- and no others are ever called; see the
 * two tails in slice2_24.c) flush the view into the REAL box, call the REAL
 * box's slot with the REAL box pointer, and read the result back.  No
 * function pointer is ever called through a type it was not defined with.
 *
 * Bindings nest: the shim finds its control through a file-static that
 * Br90Call saves and restores, so a slot that re-enters a hook is safe.
 *
 * DEVIATION (bounded buffers).  BR_MENUTEXT_MAX is 256 and BR_TEXTBOX_MAX is
 * 1024, both already the port's choices over an unbounded original.  A box
 * holding more than 255 characters would be truncated by the copy IN, so the
 * copy BACK is suppressed when the body left the buffer byte-identical to
 * what it was given.  An untouched long caption therefore survives intact;
 * one the body rewrites is bounded by the smaller of the two, as it already
 * is inside slice2_24.c.
 *
 * ===========================================================================
 * 3. ALIASED STORAGE THIS PASS CREATES, AND THE ONE BRIDGE IT NEEDS
 * ===========================================================================
 * CONVENTIONS.md's "aliased storage: a link-clean bug" applies to two of the
 * hooks here, and only two, because only two are paired IN THIS SAME PASS
 * with a writer that lives in another module's storage:
 *
 *   0x100409B0 reads 0x10AA2A20 and 0x10043650 writes it   (Car Shadow)
 *   0x100409D0 reads 0x10AA2A24 and 0x100436B0 writes it   (Specular)
 *
 * Both rows are BrExt_1004E830's, both are wired by this pass (the reader) and
 * by slice7_80.c's BrUiOptInstall72 (the writer), and the two words have
 * separate host storage -- slice2_24.c's `g_menu.gAA2A20/gAA2A24` and
 * slice2_25.c's `g_brAA2A20/g_brAA2A24`.  Wire both halves and leave them
 * apart and the toggle changes the value while the caption never moves, which
 * is a defect this pass would have introduced.
 *
 * So Br90 seeds the reader from the writer immediately before the call, in
 * ONE place, marked DEVIATION.  That is a bridge, not a resolution: the
 * proper fix is for slice2_24's two fields to become views onto slice2_25's
 * words (the precedent is g_brAA26F4 in slice5_63), and this bridge should be
 * deleted the day that happens.
 *
 * NO OTHER BRIDGE IS ADDED, deliberately.  The remaining hooks read globals
 * that several packets model separately -- 0x10AA28A4, 0x10AA28C4,
 * 0x10AA28C8, 0x10AA28CC, 0x10AA28E0, 0x100BD3E0, 0x100B3810 -- and EVERY
 * model of every one of them is zero in this host, so wiring the hook creates
 * no divergence that was not already there.  What it does create is a visible
 * default: see section 5.
 *
 * ===========================================================================
 * 4. DECLINED, WITH THE EVIDENCE
 * ===========================================================================
 * 0x100437D0 (pfn18, slice6_72.c:847)  and
 * 0x100444C0 (pfn08, slice6_71.c:704)
 *     Bodies: slice2_25.c BrOpt37D0 / BrOpt44C0, both `int (BrGameObj *)`.
 *     Glide 0x1003CD20 / 0x1003DA10 both read [esp+4] and immediately
 *     dereference `[eax+0x2AE8]`, so the argument IS read and IS the control.
 *     A `BrGameObj *` cast is the LP64 hazard in section 0: the model pins
 *     `pSub` at struct offset 0x2AE8 and BrUiCtl_'s `pOwner` is 28 bytes past
 *     that on this host.
 *     A shim object would fix the pointer arithmetic and NOT the second half:
 *     0x100444C0's whole navigational effect is `0x10AA2904 <- 0x10AA2948`,
 *     and slice2_25.c performs it on `g_brPAA2904`, a private storage that the
 *     navigation frame never reads (br_uinav.h owns 0x10AA2904 as
 *     BrUiNav::pAA2904, and slice7_81.c / slice8_84.c / slice8_85.c all write
 *     THAT one).  Wiring it would tear the screen down and navigate nowhere.
 *
 * 0x10046260 (pfn08, slice6_72.c:1202)
 *     Body: slice3_31.c BrPhaseActivate_10046260(void).  Glide 0x1003F700's
 *     three [esp+N] references are all inside its SEH frame, so the arity IS
 *     `(void)` and an adapter WOULD be legal.  It is declined for a different
 *     and harder reason: the body's just-built epilogue is
 *         g_pExt->pAA29AC->pfnHook = ...      (slice3_31.c:397)
 *     with no NULL guard, and nothing in port/ ever constructs 0x10AA29AC.
 *     Worse, `g_pExt` itself is NULL in this host -- BrPhase31SetCtx is
 *     called by port/tests only -- so the FIRST line faults.  Wiring it is a
 *     guaranteed crash, not a hole.
 *
 * 0x10047290 (pfn08, slice6_73.c:817)
 *     Body: slice3_31.c BrPhaseLeave_10047290(void *).  Glide 0x100406E0 ends
 *     `xor eax,eax / ret`, so the slot's int32 return is 0 -- a page stop, the
 *     same shape as slice8_85.c's 0x100466C0 -- and slice3_31.h's `void`
 *     return is a lost constant, not a conflict.  Declined for the argument:
 *     Br31LeavePrologue casts it to `BrGameObj *` and dereferences `pSub`
 *     (the LP64 hazard again), and the same unwired `g_pExt` faults first.
 *
 * 0x10047210 (pfn04, slice6_73.c:774)  and
 * 0x10047250 (pfn04, slice6_72.c:1390)
 *     Bodies: slice3_31.c BrPhaseEdit_10047210 / _10047250, `int (void *)`.
 *     Both read the argument and pass it straight on, and both callee pairs
 *     (Glide 0x1003AF60 / 0x1003B020 and 0x1003B970 / 0x1003BA30) open with
 *     `mov ecx,[eax+0x2AE8] / mov [ecx+0x70],0` -- the same byte offset, and
 *     the ported callees (slice6_73.c BrExt_10041A00 / BrExt_100424D0,
 *     slice5_61.c BrExt_10042410) reach it through `BrGameObj` too.  Same
 *     hazard, same unwired `g_pExt`.
 *
 * 0x100409F0 and 0x10040A20  (PAGE slots pfn04 / pfn08, slice6_73.c:804/805)
 *     These are the one pair whose SIGNATURE is clean: Glide 0x10039F30 and
 *     0x10039F60 touch no stack argument at all and return 1, so
 *     `BrUiPageHookFn_` (void (*)(void)) needs only a one-line adapter over
 *     slice2_24.c's BrMenuSeedFrom25D4 / BrMenuSeedFrom26F0.
 *     They are declined on STORAGE, which is slice8_85.h's finding and it
 *     still holds -- and it is now stronger than when it was made.  Both
 *     hooks WRITE 0x10AA28A0 / 0x10AA28A4 / 0x10AA28B8 into slice2_24's own
 *     state block, and 0x10AA28A0 / 0x10AA28A4 have four other live writers
 *     in port/src today: slice6_73.c:1044 and :1045, slice8_84.c:468 and
 *     slice8_88.c, all through `g_br73`.  A one-way bridge cannot be built
 *     here in either direction without one writer clobbering another.
 *     Merging the two models is the fix and it belongs to whoever owns
 *     g_br73, not to an adapter packet.
 *
 * ===========================================================================
 * 5. WIRED BUT INERT, AND WHY THAT IS STILL WORTH DOING
 * ===========================================================================
 * Four of the thirteen run to completion and change nothing today, because
 * the DATA they read is unwired -- not because the wiring is wrong:
 *
 *   0x100408D0  bails on slice2_24's idle guard, which is
 *               `gAA2904 == gAA2964 && gAA28E8 == 0`.  Both phase words are 0
 *               in this host, so the guard is true and the hook returns 1
 *               without touching the item.  0x10AA2964 is modelled nowhere
 *               else in port/, so there is nothing honest to seed it from.
 *   0x10041300  } all three return 0 immediately because
 *   0x100415A0  } BrMenuState::pStages (0x100B3810, the stage table) is NULL.
 *   0x100417B0  } slice2_24.c guards it as a DEVIATION where the original
 *               would fault.
 *
 * They are wired anyway because the hole they leave is a DATA hole with a
 * named owner (BrMenuGetState()), and a filled slot with an empty table is
 * the state a host can fix in one assignment.  A NULL slot is not.
 *
 * ===========================================================================
 * 6. WIRING
 * ===========================================================================
 *   BrUiHook90Install71(&hooks71);    before any slice6_71 builder runs
 *   BrUiHook90Install72(&hooks72);    before any slice6_72 builder runs
 *   BrUiHook90Install73(&hooks73);    before any slice6_73 builder runs
 *
 * Each writes ONLY the slots listed in section 1 and tolerates NULL, the same
 * contract BrUiOptInstall73, BrUiHook81Install, BrUiHook84Install71 and
 * BrUiHook85Install all keep, so all of them compose in any order.
 */
#ifndef SLICE8_90_H
#define SLICE8_90_H

#include <stdint.h>

/* slice8_84.h pulls slice7_81.h (which pulls slice6_73.h, slice6_71.h and
 * br_uinav.h in the one order that works), then slice6_72.h, then
 * br_sprfont.h.  That is every table this module installs into plus the
 * 0x10047360 body it delegates to, in one include. */
#include "slice8_84.h"
#include "slice2_24.h"   /* BrMenuItem and the twelve bodies */

/* ===========================================================================
 * The adapters.  Each is `Br90Call(pCtl, <the slice2_24.c body>)` except the
 * last, which needs no marshal.
 * ========================================================================== */

int32_t BrUiHook90_100408D0(BrUiCtl_ *pCtl);
int32_t BrUiHook90_100409B0(BrUiCtl_ *pCtl);
int32_t BrUiHook90_100409D0(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10040B30(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041040(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041180(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041300(BrUiCtl_ *pCtl);
int32_t BrUiHook90_100415A0(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041670(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041710(BrUiCtl_ *pCtl);
int32_t BrUiHook90_100417B0(BrUiCtl_ *pCtl);
int32_t BrUiHook90_10041890(BrUiCtl_ *pCtl);
int32_t BrUiHook90_100474B0(BrUiCtl_ *pCtl);

/* The marshal itself, exported so a test can drive it directly against a
 * control it built with the real constructor. Returns whatever the body
 * returns; a NULL control or a NULL body answers 1, which is the value
 * 0x10048180 treats as "carry on". */
int32_t Br90Call(BrUiCtl_ *pCtl, int32_t (*pfnBody)(BrMenuItem *pItem));

/* ===========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook90Install71(BrS71Hooks  *pHooks);   /* 0x10041300 0x10041890   */
void BrUiHook90Install72(BrUi72Hooks *pHooks);   /* nine slots              */
void BrUiHook90Install73(BrUi73Hooks *pHooks);   /* four slots              */

#endif /* SLICE8_90_H */
