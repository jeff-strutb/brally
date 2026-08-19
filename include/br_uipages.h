/* br_uipages.h -- RESPONSIBILITY: menus/ -- the front end: pages, controls,
 * navigation.  Two more of the root menu's destination screens:
 *
 *     Glide 0x1004F290  (D3D 0x100563E0)  the MULTIPLAYER NAME screen
 *     Glide 0x10043050  (D3D 0x10049C20)  the QUIT CONFIRMATION screen
 *
 * Both are phase ENTER hooks -- br_phase.h's BrPhaseEnterFn_ -- installed into
 * a phase's +0x04 slot and run once when the phase is opened.  They are the
 * same family as br_uiroot.c's 0x100425E0 and slice6_71.c's / slice6_73.c's
 * builders: straight-line transcriptions with no algorithm in them, whose
 * whole value is the coordinates, flag words, string ids, hook addresses and
 * ORDERING.
 *
 * They share a file because they share the page and control prologues and
 * nothing else in the tree owns them; the alternative was a fourth and fifth
 * copy of the same twelve lines.
 *
 * ==========================================================================
 * REFERENCE BINARY: orig/BRGlide.dll
 * ==========================================================================
 *
 * config/shared.csv classes both `shared`, matched by BODY, and the extents
 * agree to the byte across the two builds (1570 and 794).  Every listing
 * below was read out of BRGlide.dll and then CHECKED A SECOND WAY against
 * BRD3D.dll: same instruction sequence, same immediates, same float patterns,
 * with only the .data addresses differing.  That second read is what pins the
 * Glide->D3D global pairs in the tables below -- they were not assumed from a
 * delta.
 *
 * ==========================================================================
 * WHAT THE TWO SCREENS ARE
 * ==========================================================================
 *
 * The captions are not guesses.  testdata/strings.txt is the recovered
 * BRString.dll table (tools/extract_strings.py), and the ids these builders
 * pass to BrStrGet resolve to:
 *
 *     0x0C  12   "Back"
 *     0x0E  14   "Quit Game"
 *     0x0F  15   "Yes, Quit"
 *     0x1E  30   "Continue"
 *     0x3C  60   "Player Name"
 *     0x5C  92   "Multi-Player Name"
 *     0xC0 192   "Player"
 *
 * QUIT CONFIRMATION -- four controls, two of them selectable:
 *
 *     #  x       y        flags     a6  a7   caption
 *     0    0.0     0.0    0x000009   0   0   (backdrop)
 *     1  195.0    10.0    0x100009   1  -1   "Quit Game"        title style
 *     2  195.0   130.0    0x102001   1  -1   "Yes, Quit"        row style
 *     3  195.0   244.0    0x102001   1  -1   "Back"             row style
 *
 * MULTIPLAYER NAME -- eight controls, three of them selectable.  Note the
 * page's fX is 190.0 here and 195.0 in every other builder in the corpus;
 * br_ui.h already records that fX is "190.0 or 195.0, per builder", and this
 * is one of the 190.0 ones (0x433E0000, read from both images):
 *
 *     #  x       y        flags     a6  a7   caption
 *     0    0.0     0.0    0x000009   0   0   (backdrop)
 *     1  190.0    10.0    0x100009   1  -1   "Multi-Player Name"  title style
 *     2  190.0   130.0    0x100009   1   0   "Player Name"        row style,
 *                                                w1E20C = 0x34, a3 = 4
 *     3  156.0   172.0    0x000009   0  0x39 (decoration)
 *     4  190.0   174.0    0x200001   1  -1   the NAME EDIT BOX
 *     5  190.0   225.0    0x102001   1  -1   "Continue"           row style
 *     6  190.0   244.0    0x102001   1  -1   "Back"               row style
 *     7   80.0    46.0    0x000009   0   7   (decoration)
 *
 * Row 2's w1E20C is 0x34 and not 3.  CONVENTIONS.md pins what that means:
 * the menu font is a SPRITE SHEET per colour (images\type_gry|wit|mid|yel.bmp,
 * sprites 2, 3, 4 and 0x34), so this label is drawn in a different colour from
 * every other caption on the page.  It is a font sheet, not a tint.
 *
 * ==========================================================================
 * THE EDIT BOX, WHICH IS THE ONLY PART THAT IS NOT A TABLE
 * ==========================================================================
 *
 * Control 4 is the one place either builder does work.  In the original,
 * inline (`repne scasb` / `rep movsd` + `rep movsb`, i.e. MSVC's intrinsic
 * strlen and strcpy):
 *
 *     1004F5DA  strlen(NAME)                        NAME = Glide 0x10B71648
 *     1004F5E9  if (strlen(NAME) > 1) skip          unsigned `ja`
 *     1004F5F3  strcpy(NAME, BrStrGet(0xC0))        "Player"
 *     1004F61D  strcpy(ctl->aText[0].sz, NAME)      sz is aText[0] + 9
 *     1004F651  ctl->aText[0].pVtbl->pfn04(&ctl->aText[0])
 *
 * so the box is seeded with the player's saved multiplayer name, and with the
 * word "Player" the first time (or whenever the name has been reduced to one
 * character or none).  It then re-measures the box through the text object's
 * own vtable.
 *
 * The four rectangle constants that follow are written TWICE each -- once to
 * the control's rcLeft/rcTop/rcRight/rcBottom and once to the SAME numbers in
 * aText[0]'s own left/f428/right/f430.  br_ui.h ADJ-2 already establishes
 * that +0x2F80/+0x2F84/+0x2F88/+0x2F8C are fields of aText[0] and not of the
 * control, so this is one object written through two members, not aliasing:
 *
 *     rcLeft   +0x50 = 0xBA      aText[0].left  +0x2F80 = 0xBA
 *     rcRight  +0x58 = 0x13C     aText[0].right +0x2F88 = 0x13C
 *     rcTop    +0x54 = 0xAC      aText[0].f428  +0x2F84 = 0xAC
 *     rcBottom +0x5C = 0xBC      aText[0].f430  +0x2F8C = 0xBC
 *
 * and then a width limit derived from the pair, computed IN 16 BITS:
 *
 *     1004F691  mov ax, [ebx+0x2F88]      ; right, low word
 *     1004F698  sub ax, [ebx+0x2F80]      ; minus left, low word -- 16-bit
 *     1004F69F  sub eax, 0x10             ; then 32-bit
 *     1004F6A2  mov [ebx+0x2F78], ax      ; store the low word: f41C
 *
 * == 0x13C - 0xBA - 0x10 == 0x72.  The 16-bit intermediate cannot be observed
 * with these constants, and the expression is transcribed rather than folded
 * so that the mechanism is checkable; slice6_73.c writes the identical
 * expression for the identical instruction sequence in 0x1004D640.
 *
 * ==========================================================================
 * ORDER, which is the part a reader cannot re-derive from a screenshot
 * ==========================================================================
 *
 * Both builders, at every control:
 *
 *   - the page-count word is read BEFORE the allocation (for aFlags) and
 *     RE-READ after it (for aPages);
 *   - the apCtl[] store happens BEFORE the null test;
 *   - the error report happens BEFORE the count is bumped, and the count is
 *     bumped even when the allocation failed;
 *   - +0x0C is written BEFORE +0x08 at every row;
 *   - w1E20C is written AFTER the two hooks and BEFORE the caption;
 *   - cCtl is bumped at the END of a control's block, and cSel after it.
 *
 * The multiplayer builder additionally publishes its "Continue" control into
 * a global BEFORE bumping either counter (1004F760, ahead of 1004F766), so
 * the global names THAT control and not the next slot.  See the FRONTIER
 * section.
 *
 * ==========================================================================
 * THE STACK FRAME, TRACED, BECAUSE [esp+N] MEANS NOTHING WITHOUT ESP
 * ==========================================================================
 *
 * Both are MSVC EH frames.  For the multiplayer builder, with E = esp on
 * entry (after the call):
 *
 *     push -1                 E-0x04   the __try level
 *     push 0x100760D3         E-0x08   the handler
 *     push fs:[0]             E-0x0C   the previous registration link
 *     mov  fs:[0], esp
 *     push ecx                E-0x10   a one-dword local, not an argument
 *     push ebx                E-0x14
 *     push ebp                E-0x18
 *     push esi                E-0x1C
 *     mov  esi, [esp+0x20]             == E+0x04 -- THE ARGUMENT
 *     push edi                E-0x20   <- esp for the whole body
 *
 * so [esp+0x1C] == E-0x04 is the __try level (it steps 0,1,2,...,8 and back
 * to -1 around each allocation, which is why eight of the stores look like
 * data and are not), [esp+0x10] == E-0x10 is the scratch that holds the
 * `operator new` result across the constructor call, and -- the one that
 * matters -- at 1004F716 `mov ebx, [esp+0x28]` is taken when esp is E-0x24
 * (one `push -1` deeper), so it reads E+0x04: it RELOADS THE PHASE ARGUMENT,
 * because ebx was reused as a control pointer for control 4.  Later the
 * function writes its own incoming argument slot ([esp+0x24] == E+0x04) as a
 * second scratch; that is dead by then, ebx holding the live copy.
 *
 * Nothing in either frame is observable from a path that returns, so the EH
 * registration is not reproduced -- the same choice slice6_71.c, slice6_73.c
 * and br_uiroot.c make and for the same reason.
 *
 * ==========================================================================
 * FLOATS
 * ==========================================================================
 *
 * Every float instruction in both functions is `fld m32 / fsub m32 /
 * fstp m32` -- the non-reversed form, st(0) = st(0) - m32 -- and there is not
 * one `fxch` in either, so no operand order is ambiguous.  There is no
 * comparison of any kind in either function, so CONVENTIONS.md's
 * unordered-compare rule has nothing to bite on here.
 *
 * The pushed patterns, and the two `fsub` operands, read out of BRGlide.dll
 * with tools/pe.py (and independently out of BRD3D.dll):
 *
 *     0x43430000 == 195.0    0x433E0000 == 190.0    0x43020000 == 130.0
 *     0x41200000 ==  10.0    0x431C0000 == 156.0    0x432C0000 == 172.0
 *     0x432E0000 == 174.0    0x42A00000 ==  80.0    0x42380000 ==  46.0
 *
 *     Glide 0x10077658 == -95.0     D3D 0x1008F690 == -95.0
 *     Glide 0x1007765C == -114.0    D3D 0x1008F694 == -114.0
 *
 * They are NEGATIVE, so `fsub` ADDS a row offset.  Same block br_uiroot.h and
 * slice6_71.c already document.
 *
 * ==========================================================================
 * FRONTIER -- what is DECLARED AND NOT DONE, and why
 * ==========================================================================
 *
 * 1. THE PUBLISHED CONTROL, Glide 0x10AC5D00 / D3D 0x10AA29A8.
 *
 *    The multiplayer builder stores its "Continue" control's ADDRESS there.
 *    slice2_25.c already owns storage for the D3D address, as
 *    `int32_t g_brAA29A8`, and its BrOpt3FC0 (0x10043FC0 -- which is this
 *    screen's own "Back" action) sets it to 0.  slice2_23.h models the same
 *    address a third way, as `BrUiObj *pAA29A8`.
 *
 *    A host pointer does not fit in an int32_t on LP64, so the two models
 *    CANNOT be reconciled by a cast, and defining a second `BrUiCtl_ *` here
 *    would be the aliased-storage bug CONVENTIONS.md describes: one original
 *    object, two host objects, drifting apart after the first write.
 *
 *    So this module does not own the address.  The context carries
 *    `ppCtlContinue`, a pointer to WHOEVER owns it, and the multiplayer
 *    builder REFUSES TO BUILD when it is not supplied.  It refuses rather
 *    than skipping the store, because "Back" reads that slot: a screen built
 *    with the slot never written would leave the caller acting on a stale
 *    value as though the screen had opened.  Retyping slice2_25.h's global to
 *    a pointer (or to an apCtl index) is the fix, and it is a separate job.
 *
 * 2. THE INSTALLED HOOKS.  Neither builder CALLS any of them; both only store
 *    them.  A NULL slot is a state this engine has everywhere
 *    (ARCHITECTURE.md), so they are supplied by the caller and defaulted to
 *    nothing.  Five of the seven already exist in the tree under their D3D
 *    addresses; two do not exist anywhere yet.  See the table below.
 *
 * 3. THE PLAYER-NAME BUFFER, Glide 0x10B71648 / D3D 0x10B4E2E8.
 *    slice4_50.c owns `char *g_brPB4E2E8` -- a POINTER whose value is that
 *    address, bound by the host.  This module reads and writes THROUGH it
 *    rather than declaring a buffer of its own, so there is one object.  The
 *    multiplayer builder refuses to build when it is NULL: the original
 *    always has the buffer and would `repne scasb` off a null pointer.
 *
 * 4. NOT A FRONTIER, stated because it looks like one: the return value.
 *    Both originals end `mov eax, 1` and every caller discards it.  The port
 *    is `void`, which is br_phase.h's BrPhaseEnterFn_ and therefore
 *    assignable straight into a phase's +0x04 slot with no cast.
 * ========================================================================== */
#ifndef BR_UIPAGES_H
#define BR_UIPAGES_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, BrPhaseEnterFn_, BR_PHASE_PAGES        */
#include "br_ui.h"       /* BrUiPage_ / BrUiCtl_ / BrUiCtlHookFn_ -- CANONICAL
                          * for both objects; see its banner                 */
#include "br_crt.h"      /* BrOperatorNew -- does NOT zero                   */
#include "slice1_06.h"   /* BrErrHost / BrErrShow  (Glide 0x100378C0)        */
#include "slice3_39.h"   /* BrTextBox -- the 0x438 element at control +0x2B5C,
                          * and `extern char g_aBr39B720[]`                  */

/* --- counts, so a test can assert against a name rather than a literal --- */
#define BR_UIQUIT_CTL_COUNT     4
#define BR_UIQUIT_SEL_COUNT     2
#define BR_UIMULTI_CTL_COUNT    8
#define BR_UIMULTI_SEL_COUNT    3

/* Both builders report allocation failure with index 4. */
#define BR_UIPAGES_ERR_ALLOC    4

/* Page origins.  The quit screen is at the corpus-wide 195.0; the multiplayer
 * screen is one of the 190.0 ones. */
#define BR_UIQUIT_PAGE_X      195.0f   /* 0x43430000 */
#define BR_UIMULTI_PAGE_X     190.0f   /* 0x433E0000 */
#define BR_UIPAGES_PAGE_Y     130.0f   /* 0x43020000, both */

/* The edit box's rectangle, written to the control AND to aText[0]. */
#define BR_UIMULTI_EDIT_LEFT    0xBA
#define BR_UIMULTI_EDIT_TOP     0xAC
#define BR_UIMULTI_EDIT_RIGHT   0x13C
#define BR_UIMULTI_EDIT_BOTTOM  0xBC

/* String-table ids, named so the transcription reads as the screen it is. */
#define BR_UISTR_BACK           0x0C   /* "Back"              */
#define BR_UISTR_QUIT_TITLE     0x0E   /* "Quit Game"         */
#define BR_UISTR_QUIT_YES       0x0F   /* "Yes, Quit"         */
#define BR_UISTR_CONTINUE       0x1E   /* "Continue"          */
#define BR_UISTR_PLAYER_NAME    0x3C   /* "Player Name"       */
#define BR_UISTR_MULTI_TITLE    0x5C   /* "Multi-Player Name" */
#define BR_UISTR_DEFAULT_NAME   0xC0   /* "Player"            */

/* ==========================================================================
 * The hooks these two builders INSTALL.  Never called here.
 *
 * Named for the GLIDE address, because BRGlide.dll is the reference
 * (CONVENTIONS.md, "Source precedence").  The D3D twin and the symbol that
 * already implements it, where one exists, are in the comments -- every pair
 * was taken from tools/whereis.py, not from an address delta.
 *
 * Type is br_ui.h's BrUiCtlHookFn_ (ADJ-8): __cdecl, one argument, and the
 * argument is the control.  A NULL slot is a state this engine has
 * everywhere, so nothing here is required.
 * ========================================================================== */
typedef struct BrUiPagesHooks {
    /* control +0x0C -- the per-frame CAPTION SETTER, not an action.  The same
     * one on all four menu rows across both screens.  br_uiroot.h already
     * carries this address as its p100407B0. */
    BrUiCtlHookFn_ p100407B0;   /* D3D 0x10047360 -- slice3_31.c BrSub10047360 */

    /* control +0x08 -- the ACTION, run when the ACTIVATE bit is set. */
    BrUiCtlHookFn_ p1003CC60;   /* quit  "Yes, Quit"  D3D 0x10043710 -- UNPORTED */
    BrUiCtlHookFn_ p1003F940;   /* quit  "Back"       D3D 0x100464A0 -- UNPORTED */
    BrUiCtlHookFn_ p1003D220;   /* multi "Continue"   D3D 0x10043CD0
                                 *                    slice2_25.c BrOptOpen2940 */
    BrUiCtlHookFn_ p1003D510;   /* multi "Back"       D3D 0x10043FC0
                                 *                    slice2_25.c BrOpt3FC0     */

    /* the edit box's three slots, stored in the original's order 08, 04, 10 */
    BrUiCtlHookFn_ p1003BFF0;   /* +0x08  D3D 0x10042A90 -- UNPORTED            */
    BrUiCtlHookFn_ p10038420;   /* +0x04  D3D 0x1003EEF0
                                 *        slice2_23.c BrUiFn1003EEF0            */
    BrUiCtlHookFn_ p10038490;   /* +0x10  D3D 0x1003EF60
                                 *        slice2_23.c BrUiFn1003EF60            */
} BrUiPagesHooks;

/* ==========================================================================
 * Everything else the two builders reach outside themselves.
 *
 * Supplied, never defaulted, for br_boot.h's reason and br_uiroot.h's: a
 * caller that supplies nothing must not receive a plausible menu.
 * ========================================================================== */
typedef struct BrUiPagesCtx {
    const BrUiPagesHooks *pHooks;

    /* Glide 0x100378C0.  May not return -- it can call exit(1). */
    const BrErrHost *pErrHost;

    /* The two style rectangles.  Both are entries of slice3_39.c's
     * g_aBrUiStyle and both are already named by br_uiroot.h, which pins the
     * pairing independently out of BRGlide.dll:
     *
     *     Glide 0x100AACA8 -> D3D 0x100AB508 = entry 15 = { 100, 10, 410, 29 }
     *     Glide 0x100AABE8 -> D3D 0x100AB448 = entry  3 = { 148, 110, 358, 260 }
     *
     * Held as `const void *` because that is what the control vtable's +0x34
     * takes, and because it keeps this module free of a link dependency on
     * slice3_39.o -- br_uiroot.h and slice6_71.h both do the same.
     *
     * Entry 15's top is 10 and the title is placed at y = 10 on both screens,
     * which is the same independent agreement br_uiroot.h reports. */
    const void *pStyleTitle;    /* Glide 0x100AACA8 */
    const void *pStyleRow;      /* Glide 0x100AABE8 */

    /* Glide 0x10AC5D00 / D3D 0x10AA29A8 -- where the multiplayer builder
     * publishes its "Continue" control.  A pointer to the OWNER's storage;
     * this module deliberately does not own it.  See FRONTIER 1. */
    BrUiCtl_ **ppCtlContinue;
} BrUiPagesCtx;

extern BrUiPagesCtx g_brUiPages;

/* Non-zero when every slot BOTH builders dereference is filled.  pHooks'
 * members are NOT checked: a NULL hook is a state the engine has everywhere,
 * and the original stores whatever is there. */
int BrUiPagesCtxComplete(const BrUiPagesCtx *pCtx);

/* Non-zero when the multiplayer builder's two EXTRA requirements are met as
 * well -- the published-control slot (FRONTIER 1) and slice4_50.c's
 * `g_brPB4E2E8` (FRONTIER 3).  Separate from the above because the quit
 * screen needs neither. */
int BrUiMultiCtxComplete(const BrUiPagesCtx *pCtx);

/* ==========================================================================
 * The two builders.
 *
 * `void` rather than `int32_t`: see FRONTIER 4.  The type is br_phase.h's
 * BrPhaseEnterFn_, so either is assignable straight into a phase's +0x04.
 * ========================================================================== */

/* Glide 0x1004F290 / D3D 0x100563E0 */
void BrUiMultiEnter_1004F290(BrPhase_ *pPhase);

/* Glide 0x10043050 / D3D 0x10049C20 */
void BrUiQuitEnter_10043050(BrPhase_ *pPhase);

/* --- arithmetic that pins the counts (host-independent) ------------------ */
#define BR_UIPAGES_ASSERT(name, cond) typedef char BR_UIPAGES_##name[(cond)?1:-1]

/* Backdrop + title + two rows. */
BR_UIPAGES_ASSERT(quit_ctl_count_adds_up,
                  BR_UIQUIT_CTL_COUNT == 1 + 1 + BR_UIQUIT_SEL_COUNT);
/* Backdrop + title + label + decoration + edit + two rows + decoration, and
 * the three selectable ones are the edit box and the two rows. */
BR_UIPAGES_ASSERT(multi_ctl_count_adds_up,
                  BR_UIMULTI_CTL_COUNT == 1 + 1 + 1 + 1 + BR_UIMULTI_SEL_COUNT + 1);
/* Both pages fit the array many times over, so the bounds in the prologues
 * are a port-side guard and never a behaviour change. */
BR_UIPAGES_ASSERT(pages_fit,
                  BR_UIMULTI_CTL_COUNT < BR_UI_PAGE_CTL_MAX
                  && BR_UIQUIT_CTL_COUNT < BR_UI_PAGE_CTL_MAX);
/* aText[0] + 9 == control +0x2B65, the displacement at 1004F627. */
BR_UIPAGES_ASSERT(edit_text_is_at_2b65,
                  0x2B5Cu + 9u == 0x2B65u);
/* The width limit really is 0x72 -- 0x13C - 0xBA - 0x10. */
BR_UIPAGES_ASSERT(edit_width_limit_is_72,
                  BR_UIMULTI_EDIT_RIGHT - BR_UIMULTI_EDIT_LEFT - 0x10 == 0x72);

#endif /* BR_UIPAGES_H */
