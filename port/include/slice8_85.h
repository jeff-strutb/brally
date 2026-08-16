/* slice8_85.h -- the CONTROL HOOKS the six slice6_73.c builders install.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS
 *
 * slice6_73.c builds six of the sixteen menu screens.  Every row it builds
 * gets its behaviour from the function pointers it copies out of
 * `g_br73.pHooks` -- a BrUi73Hooks of 51 slots -- into the control's +0x04 /
 * +0x08 / +0x0C / +0x10 / +0x14 slots, into the page's +0x04 / +0x08, or into
 * the embedded list's +0x04 / +0x14.  The builders never CALL them; the frame
 * (0x10048530 -> 0x10048180, br_uinav.c) does.
 *
 * Three modules already fill part of that table:
 *
 *   slice7_80.c  BrUiOptInstall73    0x10042CF0  0x10042D60
 *   slice7_81.c  BrUiHook81Install   0x10045880  0x100458A0  0x10045AA0
 *                                    0x100450F0  0x100463C0  0x10046620
 *                                    0x10046EB0  0x100470E0
 *   the host                         0x10045AF0  0x10046C90 (br_uinav.c)
 *                                    0x10047360             (br_sprfont.c)
 *
 * This module is the next block of that same table -- 25 more slots -- and it
 * is scoped to what the slice6_73.c BUILDERS install, read out of those
 * builders' own transcription.  Every pairing below cites the line of
 * port/src/slice6_73.c that makes it; nothing here is inferred from a name.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS PORTED HERE, AND WHICH BUILDER / SLOT PROVES THE PAIRING
 *
 *   addr        builder (slice6_73.c line)          slot it lands in
 *   ----------  ----------------------------------  --------------------
 *   0x1003E7A0  BrExt_10050060      :775            control pfn14
 *   0x1003E950  BrExt_1004DFC0      :617            control pfn04
 *   0x1003E980  BrExt_1004DFC0      :629            control pfn04
 *   0x1003E9E0  BrExt_1004DFC0      :635            control pfn04
 *   0x1003EA40  BrExt_1004DFC0      :624            control pfn04
 *   0x1003EB10  BrExt_10050060      :686            control pfn04
 *   0x1003ED10  BrExt_1004D640      :486            control pfn04
 *   0x1003EE20  BrExt_1004DFC0      :544            control pfn04
 *   0x1003F050  BrOptFn100558A0     :340            control pfn04
 *   0x1003F0B0  BrOptFn100558A0     :367            control pfn04
 *   0x10040930  BrOptFn100558A0     :388            control pfn04
 *   0x10040A50  BrExt_10050060      :737            control pfn04
 *   0x10040AC0  BrExt_10050060      :748            control pfn04
 *   0x100418D0  BrExt_10050060      :715            control pfn04
 *   0x10042AC0  BrOptFn100558A0     :339, :366      control pfn08
 *   0x10042AF0  BrOptFn100558A0     :341, :368      control pfn10
 *   0x10043FA0  BrExt_10054B50      :851            control pfn08
 *   0x10044010  BrOptFn100558A0     :291            control pfn08
 *   0x10044030  BrOptFn100558A0     :290            control pfn0C
 *   0x10044050  BrOptFn100558A0     :298            control pfn08
 *   0x10044070  BrOptFn100558A0     :297            control pfn0C
 *   0x10044090  BrOptFn100558A0     :305            control pfn08
 *   0x100440B0  BrOptFn100558A0     :304            control pfn0C
 *   0x100466C0  BrExt_1004DFC0      :610            control pfn08
 *   0x1004E810  BrExt_1004DFC0      :557            list    f04
 *
 * ---------------------------------------------------------------------------
 * WHY A SECOND TRANSCRIPTION, AND WHERE THE FIRST ONE IS
 *
 * Sixteen of the twenty-five already exist in this tree.  None of them can be
 * DELEGATED to, and the reason is the one br_uinav.h, slice7_81.h and
 * br_sprfont.c all state:
 *
 *     a byte-offset body cannot be pointed at a struct whose fields have
 *     moved under LP64.
 *
 * Concretely, the existing bodies are typed against three partial models that
 * are all BYTE IMAGES or reduced structs of the SAME original object the
 * builders actually hand these hooks -- br_ui.h's `BrUiCtl_`:
 *
 *   slice2_23.c  `BrUiObj` == `unsigned char *`, every access at the ORIGINAL
 *                byte offset through memcpy helpers.  0x1003E7A0, 0x1003E950,
 *                0x1003E980, 0x1003E9E0, 0x1003EA40, 0x1003EB10, 0x1003ED10,
 *                0x1003EE20, 0x1003F050, 0x1003F0B0 live there.
 *   slice2_24.c  `BrMenuItem` -- a THREE-FIELD struct {f1C, f1E20C, text}.
 *                0x10040930, 0x10040A50, 0x10040AC0, 0x100418D0 live there.
 *   slice2_25.c  `BrGameObj` byte image.  0x10042AC0, 0x10043FA0 and the six
 *                0x100440xx one-liners live there.
 *   slice3_31.c  byte image.  0x100466C0 lives there.
 *
 * Handing a `BrUiCtl_ *` to any of them links cleanly and reads the wrong
 * members.  So each body is transcribed here over br_ui.h's canonical control,
 * cross-referenced to its twin so neither looks like a duplicate nobody
 * noticed.  Where the two disagree, one of them is wrong and the diff is a
 * diff of behaviour, not of documentation.
 *
 * THE EXCEPTIONS -- three genuine adapters, no second body:
 *   0x10044030 / 0x10044070 / 0x100440B0 tail-call 0x10047360, and
 *   br_sprfont.c ALREADY has 0x10047360 over `BrUiCtl_ *`
 *   (BrSprFontKindHook_10047360).  They delegate.
 *   0x10044010 / 0x10044050 / 0x10044090 tail-call 0x10043E70, whose body
 *   (Glide 0x1003D3C0, read out of BRGlide.dll) NEVER READS its pushed
 *   argument -- so slice2_25.c's `BrOptOpen2948` is called directly, exactly
 *   as slice7_80.c calls its six cyclers.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, stated address by address
 *
 * REACHED, never duplicated:
 *   0x100AB3D8  volume selector      slice2_25.c  g_br0AB3D8
 *   0x10B4E708  SFX volume index     slice2_25.c  g_brB4E708
 *   0x10B4E70C  CD volume index      slice2_25.c  g_brB4E70C
 *   0x10AA287C  mode selector        slice2_25.c  g_brAA287C
 *   0x100AB3F4  record cursor        slice5_61.h  g_br0AB3F4
 *   0x10AA28D8  the "edit is live" latch  slice6_73.h  g_brAA28D8
 *   0x10AA2A34  car cursor           slice6_73.h  g_br73.nAA2A34
 *   0x10AA28A0 / 0x10AA28A4          slice6_73.h  g_br73.nAA28A0 / nAA28A4
 *   0x10AA2518 / 0x10A9D618          slice6_73.h  g_br73.szAA2518 / szA9D618
 *   0x10AA28E4                       slice7_81.h  g_brHook81.nAA28E4
 *   0x10AA2918                       slice7_81.h  g_brHook81.pAA2918
 *   0x10AA2904 / 0x10AA2908          br_uinav.h   BrUiNav::pAA2904 / pAA2908
 *   0x10AA285C                       br_state.h   BrActiveFlags::override
 *   0x10B4DF30 / 0x10B4FBE8          slice3_32.h  BrScrGlobals::pB4DF30/pB4FBE8
 *
 * OWNED HERE (BrUiHook85Ctx).  Each is ALSO declared, but never defined, by
 * slice2_23.h's BrUiGlobals; if a host ever instantiates that struct, exactly
 * one of the two must keep the storage and the other must alias into it:
 *   0x10AA2A2C   0x10AA2984   0x10B4E740   0x10B4E760
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND, reported rather than silently "resolved"
 *
 * 1. slice3_39.h BrTextListVtbl +0x20 / +0x24 are `void *`.
 *    0x1003EB10, 0x1003ED10 and 0x1003EE20 CALL them:
 *        +0x20  int32_t (*)(BrTextList *, int32_t)   -- <0 means "no answer"
 *        +0x24  void    (*)(BrTextList *, int32_t)
 *    slice2_23.h already types the same two slots that way on its own
 *    `BrUiWidgetVtbl` (f20 / f24), so this is a SECOND, independent sighting.
 *    slice3_39.h should adopt them.  Until it does, this module casts at the
 *    three call sites -- see Br85ListSel / Br85ListAck in the .c.
 *
 * 2. slice3_39.h BrTextBoxVtbl +0x14 is `void (*)(BrTextBox *)`.
 *    0x1003EE50 -- which 0x1003F050 and 0x1003F0B0 both call -- reads the LOW
 *    BYTE of its result and tests it SIGNED (`call [ebp+0x14] / test al,al /
 *    jle`).  It returns an int.  slice2_23.h's BrUiWidgetVtbl::f14 already
 *    says `int32_t (*)(BrUiObj *)` and adds "low byte tested SIGNED".  Cast
 *    locally, reported here.
 *
 * 3. slice6_73.h types 0x1004E810 as `BrTextListCbFn`, i.e. `void (*)(void)`.
 *    The body takes TWO cdecl arguments, ignores the first, dereferences the
 *    second as `int32_t *`, and returns 1:
 *        int32_t (*)(void *, const int32_t *)
 *    Nothing in the tree calls list.f04 yet, so the wrong type has never been
 *    exercised.  The slot is filled through a cast and the true signature is
 *    published below as BrUiHook85_1004E810.
 *
 * 4. br_phase.h types BrPhaseVtbl_ +0x18 as `void (*)(BrPhase_ *, void *)`;
 *    slice2_25.h types the same slot's argument as an int (`pfnSlot6(pSub,
 *    1)`), and 0x10043FA0 pushes the LITERAL 1.  br_uinav.c passes NULL, so
 *    both readings exist in the tree already.  0x10043FA0 is transcribed with
 *    `(void *)(size_t)1`; if the slot is ever retyped, this is the call site
 *    that changes.
 *
 * 5. config/shared.csv pairs 0x10044050 AND 0x10044070 to the SAME Glide
 *    address 0x1003D5A0, and likewise 0x10044010/0x10044030 to 0x1003D560 and
 *    0x10044090/0x100440B0 to 0x1003D5E0.  They are NOT the same function:
 *    each pair differs only in its tail call (0x10043E70 vs 0x10047360), which
 *    a body match normalises away.  Read the D3D bodies, not the pairing.
 *
 * ---------------------------------------------------------------------------
 * NOT PORTED, AND WHY -- the slots this module leaves NULL
 *
 *   0x1003ECB0  BrExt_1004D640 :496.  Indexes `*(void **)(g_br73.pAA29F0 +
 *               0x1DE48 + 8*i)` -- an 8-byte-strided array INSIDE the control
 *               object, which br_ui.h models as the embedded BrTextList and
 *               which therefore has no host offset that survives LP64.  It
 *               then hands the dword to 0x1007A7D0 and copies 16 bytes into
 *               0x10B4E6F8...  Writing it needs the list's own model to name
 *               that array first.
 *   0x1003FC40  0x10041300  0x100413B0  0x10041670  0x10041710  0x100417B0
 *               The "caption/text setter" family.  Every one of them is
 *               slice2_24.c's, they share BrMenuSetCaptionId / the two-step
 *               format-and-notify shape, and they should be transcribed as ONE
 *               family in one pass rather than piecemeal here.
 *   0x10041DF0  0x10042020   the two list callbacks 0x10050060 installs.
 *               0x10041DF0 is 553 bytes and br_save.h already claims it as
 *               "the named save"; the two readings have to be reconciled
 *               before either is installed.
 *   0x10047210  0x10047290   slice3_31.c has both, over its byte image, and
 *               both reach slice3_31.h's BrPhaseCtx31 -- the struct whose
 *               second storage for 0x10AA2934 / 0x10AA2938 slice7_81.h
 *               already flags.  They belong with slice7_81.c's family.
 *   0x100409F0  0x10040A20   PAGE hooks (BrExt_10054B50 :804/:805).
 *               slice2_24.c has both (BrMenuSeedFrom25D4 / BrMenuSeedFrom26F0)
 *               and they take no argument, so an adapter WOULD be sound --
 *               except that they write 0x10AA28A0 / 0x10AA28A4 / 0x10AA28B8
 *               into slice2_24's OWN state block while slice6_73.h models the
 *               same three addresses in g_br73.  Installing them would create
 *               exactly the aliased storage CONVENTIONS.md forbids.  Left NULL
 *               until one of the two owns the words.
 */
#ifndef SLICE8_85_H
#define SLICE8_85_H

#include <stddef.h>
#include <stdint.h>

#include "slice6_73.h"    /* BrUi73Hooks, g_br73, g_brAA28D8, BrUiCtl_,
                           * BrTextBox / BrTextList, BrFtolTrunc            */
#include "slice7_81.h"    /* g_brHook81 -- 0x10AA28E4 and 0x10AA2918        */
#include "br_uinav.h"     /* g_pBrUiNav -- 0x10AA2904 / 0x10AA2908 / the
                           * BrActiveFlags override and BrScrGlobals        */

/* The two edit buffers 0x1003F050 / 0x1003F0B0 read back into.  The original's
 * are unbounded .data arrays; every copy here is bounded by this. */
#define BR85_TEXT_MAX BR_TEXTBOX_MAX

typedef struct BrUiHook85Ctx {
    int32_t nAA2A2C;                 /* 0x10AA2A2C  0x1003ED10's cursor     */
    int32_t nAA2984;                 /* 0x10AA2984  cleared by 0x100466C0   */
    char    szB4E740[BR85_TEXT_MAX]; /* 0x10B4E740  0x1003F050's buffer     */
    char    szB4E760[BR85_TEXT_MAX]; /* 0x10B4E760  0x1003F0B0's buffer     */
} BrUiHook85Ctx;

/* The single instance, zero-initialised.  BrUiHook85Reset puts it back. */
extern BrUiHook85Ctx g_brHook85;

void BrUiHook85Reset(void);

/* ==========================================================================
 * The hooks.  All are `int32_t (BrUiCtl_ *)` -- br_ui.h's BrUiCtlHookFn_ --
 * except the last, whose true shape is CONFLICT 3 above.
 * ========================================================================== */

/* 0x1003E7A0 (148 bytes).  Three messages through control vtable +0x14: 0x3D
 * once at x-8, 0x3B once per 16 pixels of aText[0].width, 0x3C once at the
 * end.  Always returns 1.
 *
 * GOTCHA, preserved: the repeat count is `width/16 + 1` computed SIGNED but
 * the loop guard is `test/jbe` -- UNSIGNED.  A width <= -16 makes the count
 * negative and the original then runs ~2^32 iterations.  Do NOT "fix" it by
 * making the counter signed; slice2_23.c's twin keeps the same shape. */
int32_t BrUiHook85_1003E7A0(BrUiCtl_ *pCtl);

/* 0x1003E950 (43 bytes).  0x68 when 0x100AB3D8 is non-zero, 0x69 when it is
 * zero -- note the inversion -- into BOTH aStepId[0] (+0x2A40) and w1E20C. */
int32_t BrUiHook85_1003E950(BrUiCtl_ *pCtl);

/* 0x1003E980 / 0x1003E9E0 (92 bytes each).  Message 0x74 once at
 * (ftol(x), ftol(y)+0x13), then 0x75 once per element of 0x10B4E708 /
 * 0x10B4E70C respectively, stepping x by 0x0C.
 * GOTCHA: the bound is RE-READ from the global on every iteration. */
int32_t BrUiHook85_1003E980(BrUiCtl_ *pCtl);
int32_t BrUiHook85_1003E9E0(BrUiCtl_ *pCtl);

/* 0x1003EA40 (78 bytes).  x (+0x3C) = (float)(8*n + 0x4A), n being 0x10B4E708
 * when 0x100AB3D8 is set and 0x10B4E70C when it is not.  The sum is formed in
 * 32-bit wraparound arithmetic and then read back SIGNED by `fild`. */
int32_t BrUiHook85_1003EA40(BrUiCtl_ *pCtl);

/* 0x1003EB10 (74 bytes).  Ask the embedded list's +0x20 for a new value for
 * 0x100AB3F4; store it back only when it is >= 0, and when it is NOT, carry
 * the OLD value forward.  Then, only if 0x10AA28D8 is set and the value is
 * >= 0, acknowledge through +0x24. */
int32_t BrUiHook85_1003EB10(BrUiCtl_ *pCtl);

/* 0x1003ED10 (40 bytes).  The same shape without the acknowledge, over
 * 0x10AA2A2C. */
int32_t BrUiHook85_1003ED10(BrUiCtl_ *pCtl);

/* 0x1003EE20 (48 bytes).  The same shape over 0x10AA2A34, but the value SENT
 * is first replaced by -1 unless it is in [0, 12).  The clamp applies to what
 * is sent, NOT to what comes back: whatever +0x20 returns is stored unfiltered
 * as long as it is >= 0. */
int32_t BrUiHook85_1003EE20(BrUiCtl_ *pCtl);

/* 0x1003F050 / 0x1003F0B0 (81 bytes each).  Apply aText[0] (0x1003EE50), then
 * -- only when it differs from the buffer -- copy the box's text over
 * 0x10B4E740 / 0x10B4E760.
 * DEVIATION: the original's copy is an unbounded `rep movs`. */
int32_t BrUiHook85_1003F050(BrUiCtl_ *pCtl);
int32_t BrUiHook85_1003F0B0(BrUiCtl_ *pCtl);

/* 0x10040930 (30 bytes).  w1E20C = (int16)(int8)0x100AC62C[0x10AA287C].
 * DEVIATION: the index is unbounded in the original; a bad mode reads past the
 * four-byte table.  Bounded here, as slice2_24.c's twin bounds it. */
int32_t BrUiHook85_10040930(BrUiCtl_ *pCtl);

/* 0x10040A50 / 0x10040AC0 (107 bytes each).  sprintf("%d", g+1) into the
 * scratch buffer, copy that into aText[0]'s text, then the box's vtable +0x08
 * followed by +0x2C.
 *   0x10040A50  0x10AA28A0 -> 0x10AA2518
 *   0x10040AC0  0x10AA28A4 -> 0x10A9D618
 * GOTCHA: the +0x2C call is guarded by `test ebx,ebx` where ebx is the text
 * buffer's ADDRESS, which is never null -- so it always runs. */
int32_t BrUiHook85_10040A50(BrUiCtl_ *pCtl);
int32_t BrUiHook85_10040AC0(BrUiCtl_ *pCtl);

/* 0x100418D0 (26 bytes).  The odd one out: when 0x10AA28E4 is CLEAR it does
 * nothing at all.  When it is set it clears bits 0x1010 of flags1C. */
int32_t BrUiHook85_100418D0(BrUiCtl_ *pCtl);

/* 0x10042AC0 (48 bytes).  A one-shot latch: if 0x10AA28D8 is clear, set it and
 * TOGGLE aText[0].f420 (+0x2F7C) to exactly 0 or 1.  If it is already set the
 * hook does nothing.  Always returns 1. */
int32_t BrUiHook85_10042AC0(BrUiCtl_ *pCtl);

/* 0x10042AF0 (6 bytes).  `mov eax,1 / ret`.  slice3_39.h records the same
 * address as text-list vtable slot +0x0C and slice5_61.h names it
 * BrGfx42AF0_1; this is the control-hook-typed face of it. */
int32_t BrUiHook85_10042AF0(BrUiCtl_ *pCtl);

/* 0x10043FA0 (30 bytes).  Phase vtable +0x18 on the OWNING phase with 1, then
 * make the root phase current.  Returns 0 -- which stops 0x10048180 and,
 * through it, the page walk. */
int32_t BrUiHook85_10043FA0(BrUiCtl_ *pCtl);

/* 0x10044010 .. 0x100440B0 (29 bytes each).  Six one-liners: set 0x10AA287C
 * to 0 / 1 / 2 and then tail-call one of two functions.  The +0x08 members
 * open a screen (0x10043E70); the +0x0C members recolour (0x10047360). */
int32_t BrUiHook85_10044010(BrUiCtl_ *pCtl);
int32_t BrUiHook85_10044030(BrUiCtl_ *pCtl);
int32_t BrUiHook85_10044050(BrUiCtl_ *pCtl);
int32_t BrUiHook85_10044070(BrUiCtl_ *pCtl);
int32_t BrUiHook85_10044090(BrUiCtl_ *pCtl);
int32_t BrUiHook85_100440B0(BrUiCtl_ *pCtl);

/* 0x100466C0 (76 bytes).  The car screen's "Back".  Same LEAVE prologue as
 * slice7_81.c's family (owner vtable +0x1C, then the current phase's +0x00
 * with 1), then 0x10AA2904 <- 0x10AA2918, 0x10AA2984 <- 0, and the two-call
 * teardown br_uinav.c's BrNavPhaseBail also runs.  Returns 0.
 *
 * GOTCHA, and it is the original's order: 0x10AA2918 is loaded BEFORE
 * 0x10AA2984 is cleared, and only then stored into 0x10AA2904. */
int32_t BrUiHook85_100466C0(BrUiCtl_ *pCtl);

/* 0x1004E810 (24 bytes).  The car list's row callback: play CD track
 * `*pRow + 2`.  See CONFLICT 3 -- this is NOT `BrTextListCbFn`. */
int32_t BrUiHook85_1004E810(void *pUnused, const int32_t *pRow);

/* ==========================================================================
 * Installation
 * ========================================================================== */

/* Fills the twenty-five slots above and touches NOTHING else, so it composes
 * with BrUiOptInstall73 and BrUiHook81Install in any order.  A NULL argument
 * is a no-op, as in both of those. */
void BrUiHook85Install(BrUi73Hooks *pHooks);

#endif /* SLICE8_85_H */
