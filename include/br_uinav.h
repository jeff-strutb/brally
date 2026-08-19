/* br_uinav.h -- the MENU NAVIGATION layer: the eight functions that move the
 * selection, mark a control current, and fire a control's hooks.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS FOR
 *
 * Sixteen screen builders already run and lay out real controls, but nothing
 * responded to anything: no selection, no activation, no transition. The code
 * that does all three is the per-frame chain
 *
 *     page  +0x04  0x10048530   walk the page's controls
 *       ctl +0x0C  0x10048180   run one control
 *         ctl +0x20  0x10047A60   is this control the current one?
 *         ctl +0x04  0x100480A0   advance its step timer
 *         ctl +0x08  0x10048010   its "enter"/draw dispatch
 *         ctl +0x3C  0x10048060   the "another control owns the frame" veto
 *         ctl +0x10  0x10047A10   hand the current code to a draw slot
 *       page-level 0x100484F0     clamp the selection cursor into range
 *
 * and this module is that chain, typed over br_ui.h's `BrUiPage_` /
 * `BrUiCtl_`.
 *
 * ---------------------------------------------------------------------------
 * DUPLICATE OWNERSHIP -- READ THIS BEFORE ADDING OR "FIXING" ANYTHING
 *
 * SEVEN of the eight addresses below ARE ALREADY PORTED, in
 * port/src/slice3_32.c, over that module's BYTE-IMAGE objects (`BrUiObj` /
 * `BrUiPage`, indexed with BrScrLd32/BrScrSt32 at the original's 32-bit
 * offsets). Those bodies are correct and are NOT being replaced. This file is
 * a second transcription over the struct model, for exactly the reason
 * br_uivt.h gives for 0x10048470 / 0x10047EB0 / 0x10047FB0: a byte-offset body
 * cannot be pointed at a struct whose fields have moved under LP64, and the
 * packets the host actually boots (slice6_71/72/73) build struct objects.
 *
 *   0x100484F0  slice3_32.c BrUiPageSelect_100484F0(BrUiPage *)   byte-image
 *               br_uinav.c  BrUiNavPageSelect_100484F0(BrUiPage_ *)  struct
 *   0x10048530  slice3_32.c BrUiPageFrame_10048530                 byte-image
 *               br_uinav.c  BrUiNavPageFrame_10048530              struct
 *   0x10048180  slice3_32.c BrUiFrame_10048180                     byte-image
 *               br_uinav.c  BrUiNavCtlFrame_10048180               struct
 *   0x100480A0  slice3_32.c BrUiTickSteps_100480A0                 byte-image
 *               br_uinav.c  BrUiNavCtlTick_100480A0                struct
 *   0x10048010  slice3_32.c BrUiEnter_10048010                     byte-image
 *               br_uinav.c  BrUiNavCtlEnter_10048010               struct
 *   0x10048060  slice3_32.c BrUiCheckOther_10048060                byte-image
 *               br_uinav.c  BrUiNavCtlOther_10048060               struct
 *   0x10047A10  slice3_32.c BrUiStepCode_10047A10                  byte-image
 *               br_uinav.c  BrUiNavCtlStepCode_10047A10            struct
 *   0x100489A0  slice3_32.c BrPhaseRun_100489A0(BrPhaseFull *)     byte-image
 *               br_uinav.c  BrUiNavPhaseRun_100489A0(BrPhase_ *)   struct
 *   0x10048AA0  slice3_32.c BrPhaseReleasePages_10048AA0           byte-image
 *               br_uinav.c  BrUiNavPhaseRelease_10048AA0           struct
 *
 * The EIGHTH, 0x10047A60 (control vtable +0x20), was not ported anywhere and
 * is a first transcription. It is the one that actually decides which control
 * is current: see the flag notes below.
 *
 * 0x100489A0 -- the PHASE's frame, vtable +0x0C -- was the last piece of the
 * chain the host was reproducing by hand rather than calling. It is the top of
 * it: the whole tree below runs because 0x100489A0 walks the phase's pages and
 * calls each one's +0x04. See its declaration further down for the one thing
 * inside it that is host-injected and why.
 *
 * The names are deliberately NOT the same as slice3_32.c's. CONVENTIONS.md's
 * aliased-storage rule is about STORAGE, and no storage is duplicated here --
 * every global this module touches is reached through the caller's
 * `BrScrGlobals`, which is slice3_32.h's one object. The one exception is
 * called out on `BrUiNav::pAA29C0` below.
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Derived from orig/BRGlide.dll and cross-checked against orig/BRD3D.dll.
 * config/shared.csv classes every address in the table above `shared`, and the
 * two builds' listings for 0x10047A60 (Glide 0x10040EB0) are identical
 * instruction for instruction, differing only in the global addresses:
 *
 *     0x10AA286C <-> 0x10AC5BC4   selection cursor
 *     0x10AA2870 <-> 0x10AC5BC8   ordinal counter
 *     0x100AB3DC <-> 0x100AAB7C   the step added to the cursor
 *     0x10AA2A78 <-> 0x10AC5DD8   pointer to the {x,y} cursor position
 *     0x100AB418 <-> 0x100AABB8   hot rect 1        (all three are entries of
 *     0x100AB428 <-> 0x100AABC8   hot rect 2         the style pool that
 *     0x100AB448 <-> 0x100AABE8   hot rect 0         slice3_39.h models)
 *
 * (0x100AB3DC - 0x100AAB7C == 0x860, and every one of the three rects and the
 * style pool's own base sits at exactly that delta, which is an independent
 * check on the pairing.)
 *
 * INDEPENDENT FINDING, and it settles an open question in slice3_39.h: that
 * header says the style pool's LOWER bound "is NOT pinned, and this is the
 * weakest claim in the block", having only guessed that 0x100AB428 and
 * 0x100AB418 look like rectangles. 0x10047A60 READS both of them as
 * rectangles, four int32 deep, in the same left/top/right/bottom order as
 * every entry above the old base. The pool therefore starts at or below
 * 0x100AB418, and slice3_39 has been extended down to it.
 *     0x10AA284C <-> 0x10AC5BA4   "cursor is over a hot rect"
 *
 * D3D addresses are used in the names below, because that is what the rest of
 * this tree uses and what slice3_32.h's BrScrGlobals is keyed on.
 *
 * MAP EXTENT WARNING, measured: config/functions.csv gives 0x10047A60 a size
 * of 161 bytes and config/functions_glide.csv gives 0x10040EB0 a size of 587.
 * The function is 587 bytes in BOTH -- the D3D entry is short, and asking
 * dumpasm for the map's 161 bytes stops in the middle of the first hit test.
 * The Glide extent is the correct one. (CONVENTIONS.md's "treat a suspicious
 * extent as a split, not a measurement" -- here it is the D3D map that is
 * wrong, which is the mirror image of the font-emitter case.)
 *
 * ---------------------------------------------------------------------------
 * THE FLAG BITS AT control +0x1C, since the whole module turns on them
 *
 *   0x00000002  ACTIVATE. Set by 0x10047A60 on the current control when the
 *               "anything active" predicate (0x1003E080, br_state.h) is true.
 *               0x10048180 sees it, calls the control's +0x08 HOOK, and clears
 *               it. This is what makes a menu item DO something.
 *   0x00000008  inert: 0x10047A60 returns 0 immediately. Every builder's
 *               backdrop/title control has it (place flags 9 / 0x100009).
 *   0x00000010  DISABLED. 0x10048180's first act is to see this bit, call
 *               +0x08 and return, so the control never reaches 0x10047A60 and
 *               can never become current. Nine sites in the image clear it
 *               (`and dword ptr [eax+0x1c], 0xffffffef`) and the builders set
 *               it: slice6_71's 0x1004F700 computes
 *               `flags = fAutoSave ? 0x102001 : 0x102011`, i.e. it greys the
 *               "Restore Autosave" row out when the file is missing. That
 *               branchless select is the clearest single statement of what
 *               the bit means anywhere in the corpus.
 *   0x00000020  CURRENT. Set by 0x10047A60 on the selected control and cleared
 *               on every other. This is the "marks a control as current" bit.
 *   0x00000800  0x10048530 skips the control's frame entirely.
 *   0x00001000  0x10048530 runs the control's +0x04 tick and +0x04 hook, then
 *               skips it unless 0x10 is also set.
 *   0x00002000  0x10048530 records the current control's index in the PHASE's
 *               +0xBC when 0x20 is set.
 *   0x00040000  0x10047A60 consults BrObjAA2E80's +0x2C/+0x30 instead of the
 *               activity predicate.
 *   0x00080000  0x10047A60's latch: while set, an active predicate re-asserts
 *               0x20|0x02 and returns without hit-testing.
 *
 * The place flags the sixteen ported builders actually pass are 1, 9,
 * 0x100001, 0x100009, 0x102001, 0x102011, 0x103011, 0x200001, 0x3001,
 * 0x402001 and 0x5001. The ordinary menu row is 0x102001: it has NEITHER
 * 0x1000 NOR 0x10, so it goes straight to 0x10048180 and its selection is
 * decided in 0x10047A60. Only the rows carrying 0x1000 (0x3001, 0x5001,
 * 0x103011) take the page frame's own ordinal arm, and only 0x103011 has the
 * 0x10 that makes that arm apply the STEP.
 *
 * This is the fact it is easiest to get backwards, and getting it backwards
 * makes the menu look dead rather than broken: if you wire the step and
 * nothing else, every ordinary row is inert, because for those rows the
 * original does not use the step at all -- its input handler moves the cursor
 * directly. See the input seam at the bottom of this header.
 * ---------------------------------------------------------------------------
 */
#ifndef BR_UINAV_H
#define BR_UINAV_H

/* slice3_32.h and slice6_73.h both DECLARE `BrUiPageCtor_10048470`, over
 * incompatible page models, so the two headers cannot be included together
 * as they stand. slice3_32.c already solves this for the host link by
 * renaming its own copy (`#define BrUiPageCtor_10048470
 * BrUiPageCtor_10048470_scr32` before the include, see the banner at the top
 * of that file, and build.sh's -DBR_HOST_LINK). The same rename is applied
 * here so this header can pull in BOTH models -- nothing in this module calls
 * either constructor, so the declaration is only there to be consistent with
 * the body the host actually links.
 *
 * CONSEQUENCE, stated so it is not discovered the hard way: a translation unit
 * that includes slice3_32.h BEFORE br_uinav.h gets the unrenamed declaration
 * and will then fail to include br_uictl.h. Include br_uinav.h first. */
#ifndef BrUiPageCtor_10048470
#define BrUiPageCtor_10048470 BrUiPageCtor_10048470_scr32
#define BR_UINAV_UNDEF_PAGECTOR 1
#endif
#include "slice3_32.h"   /* BrScrGlobals -- the ONE globals object, shared    */
#ifdef BR_UINAV_UNDEF_PAGECTOR
#undef BrUiPageCtor_10048470
#undef BR_UINAV_UNDEF_PAGECTOR
#endif

#include "slice3_39.h"   /* BrTextStyle -- the style/hot-rect pool            */
#include "br_state.h"    /* BrActiveFlags / BrIsAnyActive (0x1003E080)        */
#include "br_uictl.h"    /* br_ui.h's BrUiPage_ / BrUiCtl_ / the vtables      */

/* ===========================================================================
 * The navigation context.
 *
 * `pG` is slice3_32.h's globals object and is NOT copied: the selection cursor
 * (0x10AA286C), the ordinal counter (0x10AA2870) and the step (0x100AB3DC)
 * live there and nowhere else, so the byte-image path and this one read and
 * write the same words. That is deliberate -- see CONVENTIONS.md's
 * "aliased storage: a link-clean bug".
 *
 * The remaining members are the globals BrScrGlobals cannot express, either
 * because it never saw them (the cursor and the three hot rects are only read
 * by 0x10047A60, which slice3_32.c does not port) or because their type is
 * model-dependent.
 * ========================================================================== */
typedef struct BrUiNav BrUiNav;

/* ---------------------------------------------------------------------------
 * THE ONE HOST-INJECTED CALL INSIDE THE PHASE FRAME.
 *
 * 0x100489A0 polls DirectInput at a fixed point in its body -- between the
 * phase's +0x04 method and the page walk -- through 0x10060260, whose entire
 * body is `0x100603A0(g_pAA2E80, g_p680584)`: acquire the mouse device, fold
 * its deltas into the menu cursor, then read the keyboard's DIK_UP / DIK_DOWN
 * bytes and inc/dec the selection cursor 0x10AA286C. That is a DirectInput
 * device read, so the host has to supply it.
 *
 * The seam is here rather than in the host's frame loop precisely because the
 * ORIGINAL's is here: input reaches the menu from inside the frame, after the
 * phase has been checked and before any page runs, and a host that moves the
 * cursor before calling the frame is only approximately in the right place.
 *
 * NOTE, and it is why the real 0x10060260 is not simply called: 0x10AA2E80 is
 * modelled THREE ways in this tree -- slice3_39.h `BrPointI *g_pBrAA2E80`
 * {x,y}, slice5_60.h `BrMouseState` (the full 0x54 the input handler reads and
 * writes), and slice3_32.h `BrObjAA2E80` (the +0x2C / +0x30 button flags
 * 0x10047A60 and 0x10048180 consult). They are the same object, and the
 * correspondence is exact rather than approximate: BrObjAA2E80's f00 / f04 are
 * BrMouseState's x / y (which is also all BrPointI models), and its
 * f2C / f30 / f34 / f38 are BrMouseState's aDown[0..3] -- the four mouse
 * buttons, which is exactly what "a button is held" means at 0x10047A60.
 * Nothing has merged them, so g_pBrAA2E80 is NULL and 0x100603A0 would fault
 * on `pMs->pDev`. Merging the three is what makes this seam shrink to the
 * device read alone; until then it covers the whole call.
 *
 * A NULL hook means "no input this frame", which is a state the original has
 * (every frame in which nothing is pressed) and not a port-only shortcut.
 * ------------------------------------------------------------------------- */
typedef void (*BrUiNavPollFn)(BrUiNav *pNav);

struct BrUiNav {
    BrScrGlobals *pG;                /* the shared globals; never NULL       */

    /* 0x10060260's call site inside 0x100489A0. See the banner above. */
    BrUiNavPollFn pfnPoll;

    /* 0x10AA2A78 -- a pointer to two int32, the cursor's x and y. */
    const int32_t *pCursor;

    /* The three rectangles 0x10047A60 hit-tests, IN THE ORDER IT TESTS THEM:
     * 0x100AB448, then 0x100AB418, then 0x100AB428. All three are entries of
     * slice3_39.h's style pool; wire them with BR_UI_STYLE(0x100AB448) etc.
     * A NULL entry is treated as "no such rect" -- a port-only guard, since
     * the original always has all three. */
    const BrTextStyle *apHot[3];

    /* The nine globals 0x1003E080 reads. 0x10047A60 inlines four of them
     * (0x10AA33D0..0x10AA33DC) and three more (0x10AA285C, 0x10AA2AF0,
     * 0x10AA2CF0) before calling the whole predicate, so it needs the struct
     * and not just the predicate's answer. */
    BrActiveFlags *pActive;

    /* 0x10AA284C -- set to 1 when the cursor is inside one of the three hot
     * rects and to 0 otherwise, on every control that reaches the hit test. */
    int32_t nAA284C;

    /* 0x10AA29C0, the control 0x10048060 compares against.
     *
     * ALIAS, stated rather than hidden: slice3_32.h models the same original
     * address as `BrUiObj *pAA29C0` inside BrScrGlobals. The two views cannot
     * be merged because the pointee models differ, and a host must therefore
     * populate exactly ONE of them -- whichever matches the object graph it
     * actually built. The host in this tree builds struct controls, so it
     * populates this one and leaves BrScrGlobals::pAA29C0 NULL. */
    BrUiCtl_ *pAA29C0;

    /* The three phase slots the two ported control hooks below touch. Same
     * alias rule as pAA29C0: BrScrGlobals models these as `BrPhaseFull *` and
     * a host populates exactly one view.
     *
     *   0x10AA2904  the CURRENT phase
     *   0x10AA2908  the root/shell phase -- where 0x10046C90 goes "back" to
     *   0x10AA2924  the singleton slot 0x10045AF0 fills, whose enter hook is
     *               the ported builder 0x1004F700
     */
    BrPhase_ *pAA2904;
    BrPhase_ *pAA2908;
    BrPhase_ *pAA2924;

    int32_t n0AA010;    /* 0x100AA010 -- the phase id 0x10045AF0's family sets */
    int32_t nACED34;    /* 0x10ACED34 */
    int32_t nAA291C;    /* 0x10AA291C -- 0x10046C90 clears it */
};

/* The context the vtable adapters below reach, because a vtable slot has no
 * room for one. Set it once, the way g_pBrUiCtlVtbl and g_br73 are set. */
extern BrUiNav *g_pBrUiNav;

/* ===========================================================================
 * The eight functions. Each takes its context explicitly; the adapters at the
 * bottom are what goes in a vtable.
 * ========================================================================== */

/* 0x100484F0  page vtable's helper, __thiscall. Clamps the GLOBAL selection
 * cursor against this page's cSel and copies the result into page->iSel.
 *
 *   cursor >= cSel   -> cursor = 0
 *   cursor <  0      -> cursor = cSel - 1
 *   otherwise        -> the global is NOT written (only iSel is)
 *
 * Always returns 1. `cSel` is zero-extended and the cursor sign-extended
 * (`movsx esi, ax`), which is what makes -1 mean "one past the top" rather
 * than 65535. */
int BrUiNavPageSelect_100484F0(BrUiNav *pNav, BrUiPage_ *pPage);

/* 0x10048530  page vtable +0x04, __thiscall: one frame of one page. Returns 0
 * as soon as any control's hook or frame says no, 1 otherwise. */
int BrUiNavPageFrame_10048530(BrUiNav *pNav, BrUiPage_ *pPage);

/* 0x10048180  control vtable +0x0C, __thiscall: one frame of one control.
 * This is where the +0x08 hook is invoked on the ACTIVATE bit. */
int BrUiNavCtlFrame_10048180(BrUiNav *pNav, BrUiCtl_ *pCtl);

/* 0x10047A60  control vtable +0x20, __thiscall. Decides whether the control
 * runs its body this frame, and on the way marks it current (+0x20) and,
 * when the activity predicate is true, activated (+0x02). */
int BrUiNavCtlHit_10047A60(BrUiNav *pNav, BrUiCtl_ *pCtl);

/* 0x100480A0  control vtable +0x04, __thiscall. Advances the step index.
 *
 * SIGNATURE CONFLICT: br_ui.h types slot +0x04 `void (*)(BrUiCtl_ *)`. The
 * body ends `mov eax,1 / ret` on every path, so it is `int32_t`. Every
 * observed caller discards it, which is why the void reading survived. */
int32_t BrUiNavCtlTick_100480A0(BrUiCtl_ *pCtl);

/* 0x10048010  control vtable +0x08, __thiscall.
 *
 * SIGNATURE CONFLICT, and this one is not cosmetic: br_ui.h types slot +0x08
 * `void (*)(BrUiCtl_ *)`, but the body returns 0 on one path and 1 on the
 * others. No caller in this range inspects it either. */
int32_t BrUiNavCtlEnter_10048010(BrUiNav *pNav, BrUiCtl_ *pCtl);

/* 0x10048060  control vtable +0x3C, __thiscall. Non-zero means "some other
 * control owns this frame; skip". */
int32_t BrUiNavCtlOther_10048060(BrUiNav *pNav, const BrUiCtl_ *pCtl);

/* 0x10047A10  control vtable +0x10, __thiscall. */
int32_t BrUiNavCtlStepCode_10047A10(BrUiCtl_ *pCtl);

/* ===========================================================================
 * The two control hooks that MOVE BETWEEN SCREENS.
 *
 * These are not part of the frame chain; they are two of the ~50 `pfn08`
 * bodies the builders install, and they are ported here because they are the
 * two that make activation observable. Both are installed by
 * 0x1004F2B0 (harness builder index 4) and nothing about the pairing is
 * invented -- slice6_73.c's transcription of that builder reads
 *
 *     control 2  (string 0x0A)   pfn08 = 0x10045AF0
 *     control 3  (string 0x0B)   pfn08 = 0x10045AA0     (NOT ported)
 *     control 4  (string 0x0C)   pfn08 = 0x10046C90
 *
 * ---------------------------------------------------------------------------
 * 0x10045AF0  __cdecl(pCtl) -> int32. FORWARD.
 *
 * The singleton-activate shape slice2_26.c already factors as
 * BrPhaseActivateSlot: if 0x10AA2924 is already built, publish it as the
 * current phase and return 1; otherwise allocate, construct with 0x10048710,
 * store into BOTH globals, install 0x1004F700 as the enter hook, CALL it, and
 * set +0x0C and +0x68 to 1 on the (re-read) current phase.
 *
 * It is not routed through slice2_26.c's helper because that helper is static
 * to that module and this one has no BrPhaseCtx: the two globals here are
 * 0x10AA2924 and 0x10AA2904, which BrPhaseCtx does not carry.
 *
 * GOTCHA preserved: on the already-built path the global is stored from EAX,
 * which still holds the slot's value, and 1 is returned without the enter hook
 * running -- so a second activation does NOT rebuild the screen.
 *
 * HARDENING (port), the same one slice2_26.c documents: the original pushes
 * the literal 0xC8 at operator new. BrPhase_ is bigger than that on LP64 and
 * 0x10048710 writes the whole object, so BR_PHASE_ALLOC_SIZE is used.
 *
 * ---------------------------------------------------------------------------
 * 0x10046C90  __cdecl(pCtl) -> int32. BACK.
 *
 * Calls the owning phase's vtable +0x1C (0x10048AA0, "release every page"),
 * then the CURRENT phase's +0x00 with 1 (the scalar deleting destructor),
 * then clears 0x10AA291C and republishes the root phase 0x10AA2908 as
 * current. Returns 0 -- and 0 from a +0x08 hook is what makes 0x10048180
 * return 0, which makes 0x10048530 stop the page, which is how the frame that
 * tore its own screen down does not go on walking it.
 * ========================================================================== */
int32_t BrUiNavHook_10045AF0(BrUiCtl_ *pCtl);
int32_t BrUiNavHook_10046C90(BrUiCtl_ *pCtl);

/* ===========================================================================
 * 0x100489A0 -- PHASE vtable +0x0C, __thiscall. ONE FRAME OF ONE PHASE, and
 * the top of the whole chain listed at the top of this header.
 *
 * Derived from BRGlide 0x10041DD0 (config/shared.csv: `shared`, 249 bytes in
 * both maps, and the two listings agree instruction for instruction apart
 * from the globals):
 *
 *     0x10AA2900 <-> 0x10AC5C58   the object 0x10060260 is called on
 *     0x10AA2904 <-> 0x10AC5C5C   the CURRENT phase
 *     0x10AA2908 <-> 0x10AC5C60   the root/shell phase
 *     0x10AA2868 <-> 0x10AC5BC0   "the current phase IS the root"
 *
 * The body, in order:
 *
 *   1. +0x68 clear -> teardown and return 0. That flag is how a screen asks
 *      to be dropped; the constructor sets it to 1.
 *   2. the phase's OWN vtable +0x04 (0x100488B0).
 *   3. the DirectInput poll, with 0x10AA2904 temporarily pointing at the root
 *      phase 0x10AA2908 and restored straight afterwards -- see BrUiNavPollFn.
 *   4. 0x1005FFB0, the keyboard state read and its edge pass. Ported
 *      (slice3_39.c BrDikPollAndEdge) and called for real: the device read at
 *      the bottom of it is where the host injection actually belongs.
 *   5. 0x10AA2868 := (current phase == root phase).
 *   6. THE PAGE LOOP: for each of the phase's +0x10 pages, store it into
 *      +0x64, then run its vtable +0x04 when the parallel +0x6C flag is set.
 *   7. the phase's vtable +0x08 (0x100488C0, the tick), then +0x68 again.
 *
 * GOTCHA: +0x64 (pCur) is written with the page pointer BEFORE the NULL test,
 * so a NULL entry leaves pCur pointing at NULL and the function returns 0
 * having already published it.
 * GOTCHA: step 7 re-tests +0x68 and runs the SAME two-call teardown as step 1
 * when a hook cleared it during the page walk. Both exits return 0.
 * GOTCHA: the second teardown calls +0x18 through the vtable pointer loaded
 * BEFORE step 7, not through a fresh read of +0x00.
 *
 * The teardown is `0x1003E310` (write the options block) then `0x1006A4A0`
 * (write the config file), then +0x12 := 0 and vtable +0x18 with a NULL
 * argument. Both are reached through BrScrGlobals, so no storage is
 * duplicated. Returns 1 only when the whole frame ran and +0x68 survived.
 * ========================================================================== */
int BrUiNavPhaseRun_100489A0(BrUiNav *pNav, BrPhase_ *pThis);

/* 0x10048AA0 -- phase vtable +0x1C. Releases all 200 control slots of every
 * page, releases the page, and resets the selection cursor to 0.
 *
 * DUPLICATE OWNERSHIP: slice3_32.c has this as
 * BrPhaseReleasePages_10048AA0(BrScrGlobals *, BrPhaseFull *), byte-image.
 *
 * DEVIATION carried over from that transcription: the original walks
 * pPg->apCtl[0..199] and only THEN null-checks pPg, so a NULL page slot
 * dereferences address 0x18. This skips the whole entry instead. */
void BrUiNavPhaseRelease_10048AA0(BrUiNav *pNav, BrPhase_ *pPhase);

/* ===========================================================================
 * Vtable adapters -- the shapes br_ui.h declares, reaching g_pBrUiNav.
 *
 * Install them with BrUiNavInstall(); the slots this module does NOT port
 * (+0x14, +0x18, +0x1C, +0x24, +0x28, +0x2C, +0x30) are left exactly as the
 * caller had them, so an unported method still faults instead of quietly
 * doing nothing.
 * ========================================================================== */
void BrUiNavInstallCtlVtbl(BrUiCtlVtbl_ *pVtbl);
void BrUiNavInstallPageVtbl(BrUiPageVtbl_ *pVtbl);

/* The PHASE vtable at 0x1008F700. Two of its nine slots are ported here:
 *
 *     +0x0C  0x100489A0  the frame           BrUiNavPhaseRun_100489A0
 *     +0x1C  0x10048AA0  release every page  BrUiNavPhaseRelease_10048AA0
 *
 * +0x00 (0x10048850), +0x04 (0x100488B0), +0x08 (0x100488C0), +0x10, +0x14,
 * +0x18 (0x10048B20) and +0x20 are left exactly as the caller had them. The
 * frame DISPATCHES through +0x04, +0x08 and -- on its two failure exits --
 * +0x18, so a host that installs this table and leaves those NULL will fault
 * in the frame rather than skip them. That is deliberate: it is the same
 * "an unported method must still fault" rule the other two installers follow. */
void BrUiNavInstallPhaseVtbl(BrPhaseVtbl_ *pVtbl);

/* ===========================================================================
 * The input seam.
 *
 * The game reads its keyboard and joystick through DirectInput, which is
 * neither portable nor separable from Win32, and none of it is dragged in
 * here. The seam is narrow because the original made it narrow: the WHOLE of
 * what its input handler does to the menu is four stores, and they are all in
 * 0x100603A0 (BRGlide 0x10059410, `shared`).
 *
 * MOVE. There are exactly two writers of the selection cursor outside the
 * clamp, and both are in that function:
 *
 *     10059613  dec word ptr [0x10AC5BC4]      ; the "up" edge
 *     10059628  inc word ptr [0x10AC5BC4]      ; the "down" edge
 *
 * and each is paired, thirty instructions earlier, with a store of -1 or +1
 * into the step word 0x100AB3DC. BrUiNavMove is those two stores and nothing
 * else. The cursor is UNSIGNED and is deliberately allowed to underflow to
 * 0xFFFF: 0x100484F0 sign-extends it, so 0xFFFF is -1 and -1 wraps to
 * cSel-1. That underflow is the wrap, not a bug to guard.
 *
 * NOTE, because getting this backwards makes the menu look dead: the STEP is
 * NOT what moves an ordinary menu row. It is added to the cursor only by the
 * two `+0x10` arms (in 0x10047A60 and in 0x10048530), and 0x10 is the
 * DISABLED bit -- a row carrying it cannot be selected in the first place.
 * An ordinary row moves because the input handler moved the cursor directly.
 * Both writes are wired here; the sixteen ported screens exercise the cursor.
 *
 * ACTIVATE. 0x10AA2AF0, one of the nine globals 0x1003E080 reads. Setting it
 * is what makes 0x10047A60 raise the ACTIVATE bit on the current control, and
 * 0x10048180 then calls that control's +0x08 hook. */
void BrUiNavMove(BrUiNav *pNav, int dir);
void BrUiNavSetStep(BrUiNav *pNav, int step);
void BrUiNavSetActivate(BrUiNav *pNav, int fDown);

/* The current selection cursor, sign-extended the way 0x100484F0 reads it.
 * -1 is a legal value and means "before the first"; 0 is a legal INDEX.
 * (CONVENTIONS.md's `rep stosd` trap, one level up: do not conflate them.) */
int BrUiNavSelection(const BrUiNav *pNav);

#endif /* BR_UINAV_H */
