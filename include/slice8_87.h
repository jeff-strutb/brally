/* slice8_87.h -- the slice2_23 FAMILY of control hooks, transcribed over
 * br_ui.h's canonical `BrUiCtl_`.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS
 *
 * slice8_84.h's "NOT DONE (A)" lists fifteen addresses as the one family the
 * 0x1004xxxx installers could not reach:
 *
 *     "THE slice2_23 FAMILY -- pfn04 / pfn10 / pfn14 'poll, code and caption'
 *      hooks.  Bodies exist (port/src/slice2_23.c) but take
 *      `(BrUiObj *, BrUiGlobals *)`: two arguments, and the first is the byte
 *      array described above.  Adapting one needs a marshal, not a cast."
 *
 * This module is that family, minus the two members another pass already
 * owns.  Fourteen bodies, each transcribed a SECOND time -- over the merged
 * control -- for exactly the reason slice8_85.h states at length: a
 * byte-offset body cannot be pointed at a struct whose fields have moved
 * under LP64.  Where the two transcriptions disagree, the disagreement is
 * recorded below rather than quietly settled.
 *
 * ---------------------------------------------------------------------------
 * PRE-FLIGHT, tools/whereis.py, run on all sixteen addresses of the brief
 * BEFORE a line was written.  Four of its answers contradict the brief.
 *
 *   D3D addr    Glide partner  where the body already was
 *   ----------  -------------  ---------------------------------------------
 *   0x1003EAE0  0x10038220     slice2_23.c BrUiPoll1003EAE0   -> ported here
 *   0x1003EC30  0x10038250     ALREADY PORTED over BrUiCtl_.  See (1).
 *   0x1003ECB0  (none)         NOT PORTABLE.  See (2).
 *   0x1003EF90  0x100384C0     slice2_23.c BrUiFn1003EF90     -> ported here
 *   0x1003F020  0x10038550     slice2_23.c BrUiFn1003F020     -> ported here
 *   0x1003F210  0x10038750     slice2_23.c BrUiFn1003F210     -> ported here
 *   0x1003F280  0x100387C0     slice2_23.c BrUiFn1003F280     -> ported here
 *   0x1003F5E0  0x10038B20     slice2_23.c BrUiCode1003F5E0   -> ported here
 *   0x1003F680  0x10038BC0     slice2_23.c BrUiCode1003F680   -> ported here
 *   0x1003FA00  0x10038F40     slice2_23.c BrUiText1003FA00   -> ported here
 *   0x1003FC40  0x10039180     slice2_23.c BrUiText1003FC40   -> ported here
 *   0x1003FCB0  0x100391F0     slice2_23.c BrUiText1003FCB0   -> ported here
 *   0x1003FD30  0x10039270     slice2_23.c BrUiText1003FD30   -> ported here
 *   0x1003FDA0  0x100392E0     slice2_23.c BrUiText1003FDA0   -> ported here
 *   0x1003FE10  0x10039350     slice2_23.c BrUiText1003FE10   -> ported here
 *   0x1003FE80  0x100393C0     slice2_23.c BrUiText1003FE80   -> ported here
 *
 * (1) 0x1003EC30 IS ALREADY PORTED over the canonical control, and is WIRED
 *     here rather than transcribed a third time.  config/shared.csv pairs
 *     BOTH 0x1003EB10 and 0x1003EC30 to Glide 0x10038250 "matched by
 *     body+ptrsite", and slice2_23.c:491 says the same in prose: "0x1003EB10
 *     and 0x1003EC30 are byte-for-byte the same routine emitted twice".
 *     slice8_85.c holds that routine as BrUiHook85_1003EB10, and
 *     BrUiHook87Install72 installs THAT function pointer into p1003EC30.
 *     No second body, and nothing in slice8_85.c is touched.
 *
 * (2) 0x1003ECB0 IS NOT TRANSCRIBED, and the reason is NOT the one
 *     slice8_85.h gives.  Three separate findings, in order of weight:
 *
 *     a. IT DOES NOT EXIST IN THE REFERENCE BUILD.  config/shared.csv's row
 *        is `0x1003ECB0,,91,unknown,,` -- an empty glide_va and class
 *        `unknown`.  Every Glide function in the surrounding extent
 *        (0x10038220, 0x10038250, 0x100382A0, 0x100382D0, 0x10038320,
 *        0x10038350, 0x10038380) is already the partner of a DIFFERENT D3D
 *        address, so this is not an unmatched pairing waiting to be found:
 *        BRGlide.dll has no such routine.  The brief names orig/BRGlide.dll
 *        as the reference and Glide as the mature target; a D3D-only body has
 *        no reference text to transcribe from.
 *     b. ITS ONE CALLEE IS UNPORTABLE HERE.  The body (read from
 *        orig/BRD3D.dll) is
 *            i = 0x10AA2A2C; 0x10AA2860 = i;
 *            p = *(void **)(0x10AA29F0 + 0x1DE48 + 8*i);
 *            r = 0x1007A7D0(p); 0x118AC238 = r;
 *            memcpy(0x10B4E6F8, (char *)r + 4, 16);
 *            0x10046620(pCtl); return 0;
 *        0x1007A7D0 is classed `d3d_only` by config/shared.csv ("reached from
 *        a renderer entry point; no caller that exists in BRGlide"), is not
 *        in port/host/br_stubs.c, and is itself a linked-list walk over
 *        0x104BBE20 (next at +0x330, 16-byte key at +0x04) that tail-calls a
 *        further unported 0x1001A550.  A transcription of 0x1003ECB0 that
 *        cannot call it is a body that cannot run.
 *     c. slice8_85.h's STATED REASON IS WRONG, and is corrected here so the
 *        next pass does not re-derive it.  It says the 8-byte-strided array
 *        at +0x1DE48 "has no host offset that survives LP64".  br_ui.h's
 *        ADJ-6 already resolves that offset exactly:
 *            control +0x1DE48 + 8*i
 *          = list    +0x1A610 + 8*i          (list is at control +0x3838)
 *          = list.aBlobs[i].p                (aBlobs is at list +0x1A60C,
 *                                             BrTextBlob is {size, p})
 *        i.e. `g_br73.pAA29F0->list.aBlobs[i].p`, and slice3_39.h has named
 *        that field since ADJ-6 landed.  The offset is not the blocker.
 *        (a) and (b) are.
 *
 * (3) THE BRIEF'S "pfn08" FRAMING DOES NOT FIT THIS SUBSET.  Fifteen of the
 *     sixteen land in pfn04 or pfn10, not pfn08 -- read off the builders:
 *        pfn04  p1003EAE0 (slice6_71.c:451)   p1003EC30 (slice6_72.c:1289)
 *               p1003EF90 (slice6_72.c:686)   p1003F210 (slice6_71.c:635)
 *               p1003F5E0 (slice6_72.c:918)   p1003F680 (slice6_72.c:924)
 *               p1003FA00 (slice6_72.c:894, :1078)
 *               p1003FC40 (slice6_73.c:394)   p1003FCB0 (slice6_72.c:1520)
 *               p1003FD30 (slice6_72.c:1536)  p1003FDA0 (slice6_72.c:1568)
 *               p1003FE10 (slice6_72.c:1552)
 *               p1003FE80 (slice6_72.c:878, :1062)
 *        pfn10  p1003F020 (slice6_72.c:687)   p1003F280 (slice6_71.c:636)
 *        pfn08  p1003ECB0 (slice6_73.c:496)   <- the only one, and (2)
 *     So this packet fills per-frame caption / poll / read-back slots.  It
 *     does NOT unblock "the menus cannot leave a screen": the one ACTION hook
 *     in the subset is the single address that cannot be written.
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Every body below was read out of orig/BRGlide.dll with tools/dumpasm.py at
 * the GLIDE address in the table above, and cross-read against BRD3D.dll only
 * where the two disagree (CONFLICT 1).  The .rdata tables were extracted from
 * BOTH images and compared dword for dword; they are identical.
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND, reported rather than silently resolved
 *
 * 1. 0x1008C320 IS `_stricmp`, NOT `strcmp`.  slice2_23.c's banner ("0x1008C320
 *    is strcmp") and slice8_85.c's Br85TextReadBack ("`call 0x1008C320` is
 *    strcmp") both say strcmp, and both are wrong.  The Glide build settles it
 *    without any inference at all, because Glide imports the CRT instead of
 *    linking it statically:
 *        100384F4  call dword ptr [0x118F0554]   ; MSVCRT.dll!_stricmp
 *        10038784  call dword ptr [0x118F0554]   ; MSVCRT.dll!_stricmp
 *    -- the two sites 0x1003EF90 and 0x1003F210 use.  Reading D3D 0x1008C320
 *    directly confirms it from the other side: it folds 'A'..'Z' with the
 *    `sub 0x41 / cmp 0x1A / sbb / and 0x20` sequence and consults the locale
 *    table at 0x118AC358.  The comparison is CASE-INSENSITIVE, so a caption
 *    that differs only in case is treated as UNCHANGED and the copy is
 *    skipped.  This module implements that; slice2_23.c and slice8_85.c
 *    should be corrected to match.
 *
 * 2. 0x10AA28B8's SIGN.  slice6_73.h types it `uint8_t bAA28B8`; slice5_63.h
 *    types the same address `int8_t g_brAA28B8 -- read with movsx, SIGNED`.
 *    slice5_63.h is right: 0x10038F93 and 0x100393C0 both do
 *    `movsx eax, byte ptr [0x10AC5C10]` and then use the result as a signed
 *    multiplier (`base + 12*i`).  g_br73's storage is used here -- it is the
 *    one the 84/85 family already reaches -- and every read casts through
 *    int8_t at the point of use.  Two storages for one address remains a
 *    pre-existing defect this module does not add to.
 *
 * 3. 0x10B4E1E4 IS WRITTEN, and slice6_72.h types it read-only.  Its field is
 *    `const char *pszB4E1E4 -- 0x10B4E1E4, strcpy SOURCE`.  0x1003EF90 makes
 *    it a strcpy DESTINATION (0x10038537: `mov edi, 0x10B71544` -- the Glide
 *    address of the same buffer -- followed by `rep movsd`).  Nothing in the
 *    tree defines storage for it, so this module OWNS a bounded buffer for it
 *    (BrUiHook87Ctx::szB4E1E4) and slice6_72.h's pointer is the alias point:
 *    exactly one of the two must keep the storage.
 *
 * 4. 0x10AA2904 vs 0x10AA2964, the "session is solo" predicate.  The original
 *    compares two raw dwords.  In the port the two addresses live in
 *    DIFFERENT views: 0x10AA2904 is BrUiNav::pAA2904 (`BrPhase_ *`, the view
 *    this tree's host populates) while 0x10AA2964 exists only as
 *    BrScrGlobals::pAA2964 (`BrPhaseFull *`, the view br_uinav.h says a host
 *    leaves NULL).  The comparison is therefore made through `const void *`,
 *    which is what the original's `cmp eax,ecx` is.  A host that populates
 *    only one view will read "not solo" whatever the original would have
 *    said; that is a wiring gap, not a transcription choice, and it is
 *    recorded here because it is invisible from the call site.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, address by address
 *
 * REACHED, never duplicated:
 *   0x100AB3F4  record cursor          slice5_61.h  g_br0AB3F4
 *   0x10AA28D8  the "edit is live" latch  slice6_73.h  g_brAA28D8
 *   0x10AA285C  input override         br_uinav.h   BrActiveFlags::override
 *   0x10AA287C  mode selector          slice2_25.c  g_brAA287C
 *   0x10AA28E8  session flag           slice2_25.c  g_brAA28E8
 *   0x100AA010 0x100AC648 0x10AA28A4 0x10AA28AC 0x10AA28B8 0x10AA2A00
 *                                      slice6_73.h  g_br73
 *   0x10AA2A18 0x10AA2A1C 0x10AA2A20 0x10AA2A24 0x10AA2A28 0x118ABDBC
 *   0x10AA29E8 0x10A9CDF0              slice6_72.h  g_pBr72Env
 *   0x10AA29BC 0x10A9D018              slice6_71.h  g_brS71
 *   0x10AA28A8 0x10AA2964              slice3_32.h  BrScrGlobals, via
 *                                                   g_pBrUiNav->pG
 *   0x10AA2904                         br_uinav.h   BrUiNav::pAA2904
 *
 * EMBEDDED AS `static const`, exactly as slice8_85.c embeds 0x100AC62C, and
 * verified byte-identical in BOTH images:
 *   0x100AC308 / Glide 0x100ABAA8   16 dwords   string ids, byte-indexed
 *   0x100AC3B0 / Glide 0x100ABB50   12 dwords
 *   0x100AC3F0 / Glide 0x100ABB90    4 dwords
 *   0x100AC400 / Glide 0x100ABBA0    2 dwords
 *   0x100AC408 / Glide 0x100ABBA8    2 dwords
 *   0x100AC410 / Glide 0x100ABBB0    2 dwords
 *   0x100AC418 / Glide 0x100ABBB8    2 dwords
 *
 * OWNED HERE (BrUiHook87Ctx), each also declared -- but never defined -- by
 * slice2_23.h's BrUiGlobals:
 *   0x10B4E1E4  the mirror buffer, CONFLICT 3
 *   0x100B3820  INJECTED.  Two-byte records of live game data, indexed
 *               `base + 12*i` with no bound anywhere in the original.  Its
 *               extent is not established, so it is a host-supplied pointer
 *               plus a record count, the same shape slice2_23.h gives it.
 *   0x100BD2A8  INJECTED.  An array of POINTERS to live objects (byte +4 is
 *               tested against 0x10), so it cannot be a static table.
 * ---------------------------------------------------------------------------
 * NOT INSTALLED, and why
 *
 *   p1003ECB0                 see (2).  Left NULL, a visible hole.
 *   slice3_33.h's p1003FA00 / p1003FE80.  slice3_33.h carries a THIRD copy of
 *   these two slots on `BrUiCtlFn` over its own control model, which
 *   slice6_71.h says outright it "no longer uses".  Installing into it would
 *   mean forming an opinion about a model br_ui.h has already replaced.
 */
#ifndef SLICE8_87_H
#define SLICE8_87_H

#include <stddef.h>
#include <stdint.h>

/* slice8_85.h pulls slice6_73.h, slice7_81.h and br_uinav.h; slice6_72.h is
 * added after them.  Same order, and for the same reason, as slice8_84.h --
 * slice6_73.h's CONFLICT 3 is what makes the combination legal. */
#include "slice8_85.h"   /* BrUiHook85_1003EB10 -- 0x1003EC30's twin        */
#include "slice6_71.h"   /* BrS71Hooks, BrS71Globals, g_brS71               */
#include "slice6_72.h"   /* BrUi72Hooks, Br72Env, g_pBr72Env                */

/* The mirror buffer at 0x10B4E1E4.  The original's is an unbounded .data
 * array; every copy here is bounded by this. */
#define BR87_TEXT_MAX BR_TEXTBOX_MAX

typedef struct BrUiHook87Ctx {
    /* 0x10B4E1E4 -- see CONFLICT 3. */
    char szB4E1E4[BR87_TEXT_MAX];

    /* 0x100B3820 (Glide 0x100B3028).  Records of TWO BYTES: 0x1003FA00 reads
     * byte 0 of record k, 0x1003FE80 reads byte 1.  Indexed byte-wise so the
     * packing is explicit, exactly as slice2_23.h indexes it. */
    const uint8_t *pB3820;
    size_t         cB3820;   /* records, not bytes.  0 disables the lookup. */

    /* 0x100BD2A8 (Glide 0x100BCAB0).  Pointers to live objects; only byte +4
     * of the target is ever read, and only bit 0x10 of it. */
    void *const   *apBD2A8;
    size_t         cBD2A8;   /* 0 disables the lookup. */
} BrUiHook87Ctx;

/* The single instance, zero-initialised.  BrUiHook87Reset puts it back. */
extern BrUiHook87Ctx g_brHook87;

void BrUiHook87Reset(void);

/* ===========================================================================
 * The fourteen hooks.  Every one returns 1; none of them has a second exit.
 * ========================================================================== */

/* 0x1003EAE0 (Glide 0x10038220, 40 B).  Offer g_br0AB3F4 to the embedded
 * list's vtable +0x20 and keep the answer only when it is >= 0.
 * SLOT: control pfn04, port/src/slice6_71.c:451. */
int32_t BrUiHook87_1003EAE0(BrUiCtl_ *pCtl);

/* 0x1003EF90 (Glide 0x100384C0, 143 B).  Apply aText[0], then clear bit 4 of
 * 0x10AA29E8's flag word IF the caption is non-empty, then _stricmp/copy the
 * caption into 0x10A9CDF0 and mirror 0x10A9CDF0 into 0x10B4E1E4.
 * GOTCHA (a real defect, preserved): the mirror is INSIDE the differs-branch,
 * so 0x10B4E1E4 goes stale whenever the caption has not changed.
 * SLOT: control pfn04, slice6_72.c:686. */
int32_t BrUiHook87_1003EF90(BrUiCtl_ *pCtl);

/* 0x1003F020 (Glide 0x10038550, 39 B).  The bit-4 clear ALONE -- no apply, no
 * copy.  SLOT: control pfn10, slice6_72.c:687 (the only builder that fills
 * +0x10 on that row). */
int32_t BrUiHook87_1003F020(BrUiCtl_ *pCtl);

/* 0x1003F210 (Glide 0x10038750, 107 B).  0x1003EF90's shape over 0x10AA29BC
 * and 0x10A9D018, WITHOUT the second mirror.
 * SLOT: control pfn04, slice6_71.c:635. */
int32_t BrUiHook87_1003F210(BrUiCtl_ *pCtl);

/* 0x1003F280 (Glide 0x100387C0, 39 B).  0x1003F020's shape over 0x10AA29BC.
 * SLOT: control pfn10, slice6_71.c:636. */
int32_t BrUiHook87_1003F280(BrUiCtl_ *pCtl);

/* 0x1003F5E0 / 0x1003F680 (Glide 0x10038B20 / 0x10038BC0, 131 B each).  A
 * five-way jump table on 0x10AA2A18, compared UNSIGNED against 4, writing the
 * WORD at control +0x1E20C.
 * GOTCHA: the two disagree about the default.  0x1003F5E0 maps out-of-range
 * to 0x56, which is ALSO what index 0 maps to, so a bad index is
 * indistinguishable from index 0.  0x1003F680 maps BOTH index 0 and
 * out-of-range to 0xFFFF.
 * SLOT: control pfn04, slice6_72.c:918 and :924. */
int32_t BrUiHook87_1003F5E0(BrUiCtl_ *pCtl);
int32_t BrUiHook87_1003F680(BrUiCtl_ *pCtl);

/* 0x1003FA00 (Glide 0x10038F40, 567 B).  The largest member.  Three input
 * paths pick a string id, the caption is staged in a stack scratch buffer,
 * and an optional 0xB0 caption is drawn FIRST when the selected object's
 * byte +4 has bit 0x10 set.
 * GOTCHA (preserved): the value SAVED around that extra caption is the
 * CONTROL's +0x40, and the value RESTORED lands in aText[0].y (+0x2F70).  The
 * original saves one field and writes it back over a different one.
 * SLOT: control pfn04, slice6_72.c:894 and :1078. */
int32_t BrUiHook87_1003FA00(BrUiCtl_ *pCtl);

/* 0x1003FC40 (Glide 0x10039180, 100 B).  Caption = string id
 * k_AC3F0[0x10AA287C].  SLOT: control pfn04, slice6_73.c:394. */
int32_t BrUiHook87_1003FC40(BrUiCtl_ *pCtl);

/* 0x1003FCB0 (Glide 0x100391F0, 113 B).  Caption = k_AC400[0x10AA2A1C] when
 * 0x118ABDBC is set, else the literal id 0x74.
 * SLOT: control pfn04, slice6_72.c:1520. */
int32_t BrUiHook87_1003FCB0(BrUiCtl_ *pCtl);

/* 0x1003FD30 / 0x1003FDA0 / 0x1003FE10 (Glide 0x10039270 / 0x100392E0 /
 * 0x10039350, 100 B each).  The same body over three different (table,
 * index) pairs.  SLOTS: control pfn04, slice6_72.c:1536, :1568, :1552. */
int32_t BrUiHook87_1003FD30(BrUiCtl_ *pCtl);
int32_t BrUiHook87_1003FDA0(BrUiCtl_ *pCtl);
int32_t BrUiHook87_1003FE10(BrUiCtl_ *pCtl);

/* 0x1003FE80 (Glide 0x100393C0, 330 B).  0x1003FA00's sibling: the same three
 * input paths, but it reads BYTE 1 of the 0x100B3820 record where 0x1003FA00
 * reads byte 0, and it has no "extra caption" arm.  On the solo path it draws
 * the caption 8 units up and puts aText[0].y back afterwards.
 * GOTCHA: the restore is `fsub -8.0f`, not `fadd 8.0f`.  It is exact for
 * every finite y and is transcribed in the original's negated form.
 * SLOT: control pfn04, slice6_72.c:878 and :1062. */
int32_t BrUiHook87_1003FE80(BrUiCtl_ *pCtl);

/* ===========================================================================
 * Installation
 * ========================================================================== */

/* BrS71Hooks: p1003EAE0, p1003F210, p1003F280. */
void BrUiHook87Install71(BrS71Hooks *pHooks);

/* BrUi72Hooks: p1003EC30 (slice8_85.c's body -- see (1)), p1003EF90,
 * p1003F020, p1003F5E0, p1003F680, p1003FA00, p1003FCB0, p1003FD30,
 * p1003FDA0, p1003FE10, p1003FE80. */
void BrUiHook87Install72(BrUi72Hooks *pHooks);

/* BrUi73Hooks: p1003FC40 only.  p1003ECB0 is deliberately left NULL. */
void BrUiHook87Install73(BrUi73Hooks *pHooks);

#endif /* SLICE8_87_H */
