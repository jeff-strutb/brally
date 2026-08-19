/* br_uiroot.h -- GLIDE 0x100425E0 (D3D 0x100491B0, `shared`), 2,659 bytes:
 * THE ROOT PHASE'S ENTRY POINT.  It builds the MAIN MENU.
 *
 * RESPONSIBILITY: menus/ -- "the front end: pages, controls, navigation".
 *
 * ==========================================================================
 * WHY THIS ONE MATTERS
 * ==========================================================================
 *
 * br_uiboot.h's concern F is the single store
 *
 *     0x10058246  mov dword ptr [eax + 4], 0x100425E0
 *
 * -- the root phase's `pfnEnter`.  br_phase.h records that the constructor
 * 0x10041B60 does NOT initialise +0x04 and that operator new does not zero,
 * so until this function exists the ROOT PHASE HAS ONE ENTRY POINT AND IT
 * POINTS AT HEAP GARBAGE.  Nothing the front end does can happen before it.
 *
 * It is also where the main menu comes from.  There is no data file: the page
 * and its sixteen controls are built from immediates here, one control at a
 * time, and the row captions come from BRString.dll ids 1..8.
 *
 * ==========================================================================
 * WHAT IT BUILDS, MEASURED
 * ==========================================================================
 *
 * ONE page (0x348, ctor 0x100418C0 == D3D 0x10048470) with fX = 195.0f
 * (0x43430000) and fY = 125.0f (0x42FA0000), published into the incoming
 * phase's aPages[nPages], and SIXTEEN controls (0x1E214 each, ctor
 * 0x10040B10 == D3D 0x100476C0).  Fifteen go into apCtl[0..14] and cCtl ends
 * at 15; the sixteenth goes somewhere else entirely -- see THE CURSOR below.
 *
 *   apCtl  place x,y       flags      a7   caption (BRString id)
 *   -----  --------------  ---------  ---  ---------------------------------
 *     0      0.0,   0.0    0x000009    0   --                    backdrop
 *   [199]    0.0,   0.0    0x000009    1   --                    THE CURSOR
 *     1    195.0,  10.0    0x100009   -1   id 1  "Main Menu"     title
 *     2    195.0, 125.0    0x102001   -1   id 2  "Championship"  row 0
 *     3     80.0,  46.0    0x000809    6   --                    row 0 highlight
 *     4    195.0, 144.0    0x102001   -1   id 3  "Multiplayer"   row 1
 *     5     80.0,  46.0    0x000809    7   --                    row 1 highlight
 *     6    195.0, 163.0    0x102001   -1   id 4  "Time Attack"   row 2
 *     7     80.0,  46.0    0x000809    8   --                    row 2 highlight
 *     8    195.0, 182.0    0x102001   -1   id 5  "Quick Race"    row 3
 *     9     80.0,  46.0    0x000809  0xA   --                    row 3 highlight
 *    10    195.0, 201.0    0x102001   -1   id 6  "Options"       row 4
 *    11     80.0,  46.0    0x000809    9   --                    row 4 highlight
 *    12    195.0, 220.0    0x102001   -1   id 7  "Credits"       row 5
 *    13    195.0, 239.0    0x102001   -1   id 8  "Quit"          row 6
 *    14    195.0,  29.0    0x100009   -1   0x100ACAD8            status line
 *
 * cSel ends at 7 -- the seven 0x102001 rows.  Every f38 call passes a4 = 2 and
 * a5 = 5, and the OWNER argument is the PHASE, never the page, exactly as
 * slice6_71.c's four builders do.
 *
 * THE ROW SPACING IS ARITHMETIC AND IT PINS THE SIGNS.  Rows 1..6 compute
 * their y as `fld [page+0x33C] / fsub <.rdata const> / fstp`, and the six
 * constants at 0x10077648..0x1007765C are NEGATIVE -- -19, -38, -57, -76,
 * -95, -114 -- read out of BRGlide.dll with tools/pe.py, not assumed.  So the
 * `fsub` is an ADDITION of a row offset and the rows land on 125, 144, 163,
 * 182, 201, 220, 239: seven rows, nineteen pixels apart.  Reading the block
 * as positive puts every row ABOVE the first and the even spacing disappears,
 * which is what makes the sign checkable rather than a matter of taste.
 * (slice6_71.c records the same sign split for the D3D copy of this block.)
 *
 * ==========================================================================
 * THE CURSOR: a control parked at apCtl[199], and what proves it
 * ==========================================================================
 *
 * The second control is stored at page +0x334.  br_ui.h pins apCtl as 200
 * entries at +0x018 (the page constructor's `rep stosd` count is 0xC8, and
 * 0x018 + 200*4 == 0x338 == fX), so +0x334 is apCtl[199] -- THE LAST SLOT --
 * and cCtl is NOT incremented for it.  The page frame walks 0 .. cCtl-1, so
 * that control is unreachable from the frame loop by construction.
 *
 * It is not dead.  Glide 0x10041D10 is its consumer, and it is unambiguous:
 *
 *     eax = [0x10AC5C60]            ; the ROOT phase   (br_uiboot.h)
 *     esi = [0x10AC5C5C]            ; the CURRENT phase
 *     edx = [0x10AC61E0]            ; the {x,y} cursor position
 *     [0x10AC5C5C] = eax            ; make the root current for the call
 *     eax = [eax+0x14]              ; phase->aPages[0]
 *     ecx = [eax+0x334]             ; page->apCtl[199]
 *     fild [edx]   / fstp [ecx+0x3C]   ; control->x = (float)cursor.x
 *     fild [eax+4] / fstp [ecx+0x40]   ; control->y = (float)cursor.y
 *     call [[ecx]+0x0C]             ; the control frame, by hand
 *     [0x10AC5C5C] = esi            ; put the current phase back
 *
 * So apCtl[199] is the MOUSE POINTER: its position is written from the cursor
 * every frame and its frame method is called directly rather than in
 * sequence.  a7 = 1 makes its sprite index 1; the backdrop at apCtl[0] takes
 * a7 = 0.  This is the only reference to +0x334 in the whole of BRGlide's
 * .text besides the store here -- checked by decoding every disp32 in the
 * section, not by grepping a listing.
 *
 * ==========================================================================
 * THE FIVE "HIGHLIGHT" CONTROLS, AND WHY 0x800 DOES NOT MEAN DEAD
 * ==========================================================================
 *
 * The 0x809 controls carry br_uinav.h's 0x800 bit, which makes the page frame
 * 0x10048530 `continue` past them.  They are still drawn, and by a second
 * path: each 0x102001 row sets cChild = 1 and aChild[0] = cCtl + 1 -- the
 * index of the control created immediately after it -- and 0x10048530's tail
 * runs, for the row whose index equals the phase's +0xBC (the selection),
 *
 *     for (k = 0; k < cChild; ++k) apCtl[aChild[k]]->pVtbl->f0C(...)
 *
 * (br_uinav.c's BrUiNavPageFrame_10048530, already ported).  So a highlight
 * is drawn only while its row is selected, which is exactly what 0x800 +
 * 0x2000 buys.  br_ui.h's ADJ-5 predicted this shape from the other side --
 * "a builder storing cCtl + 1 is storing aChild[0]" -- and this builder is
 * seven more instances of it.
 *
 * TWO OBSERVATIONS, recorded with their addresses rather than labelled bugs:
 *
 *   1. The sprite indices are 6, 7, 8, 0xA, 9 in page order (0x100428F5,
 *      0x10042A32, 0x10042B6F, 0x10042CAC, 0x10042DE9).  0xA and 9 are
 *      SWAPPED relative to the rows.  A transcription that "tidies" them to
 *      6,7,8,9,A is wrong, and nothing in the code makes the order derivable.
 *
 *   2. `aChild[0] = cCtl + 1` is applied unconditionally to the first SIX
 *      rows, and only five highlights exist.  For "Credits" (apCtl[12],
 *      0x10042EB4..0x10042EC6) the next control created is the "Quit" ROW at
 *      apCtl[13], so selecting Credits runs Quit's f0C a second time.
 *      "Quit" itself gets no aChild at all (0x10042F81 goes straight to the
 *      two counters).  This is transcribed as-is.  It is NOT called a
 *      preserved bug: CONVENTIONS.md holds a claim that the original is wrong
 *      to a higher standard than a claim that it is right, and an extra f0C
 *      on a row that is about to be redrawn anyway is not observably broken.
 *
 * ==========================================================================
 * THE STATUS LINE, AND THE GLOBAL AT 0x10AC4C58
 * ==========================================================================
 *
 * The last control takes its text from the .data buffer at 0x100ACAD8 (a
 * literal pointer, not BrStrGet; the image contains a single space there) and
 * the builder then publishes ITS INDEX:
 *
 *     1004301E  mov cx, word ptr [esi+0x14]     ; cCtl, still 14
 *     10043027  mov dword ptr [0x10AC4C58], ecx
 *     1004302D  inc word ptr [esi+0x14]         ; ...and only then cCtl -> 15
 *
 * Glide 0x1003AF30 is the reader, and it settles what the slot is for:
 *
 *     pCtl = [0x10AC5C5C]->aPages[0]->apCtl[ [0x10AC4C58] ];
 *     if (pCtl) pCtl->pVtbl->f34(pCtl, pszText, 1, 1, 0x100AACF8);
 *
 * -- the same style pointer and the same (1, 1) that this builder passes.  So
 * 0x10AC4C58 is "which control on the root page is the status line", and
 * 0x1003AF30 is `SetStatusText`.  br_sprfont.c's rectangle-table banner lists
 * 0x10AC4C58 as "(read by 0x1003AF30, 0x100425E0)"; 0x100425E0 WRITES it.  It
 * is the first dword past table D (0x10AC4BC8 + 9*16), not part of it, and
 * nothing in port/ defined storage for it, so this module owns it.
 *
 * ==========================================================================
 * THE THREE STYLE RECTANGLES ARE ALREADY IN THE TREE
 * ==========================================================================
 *
 * The builder pushes three style pointers.  br_uinav.h fixes the Glide->D3D
 * delta for this region at 0x860, and all three land inside slice3_39.c's
 * g_aBrUiStyle, whose 21 entries were read out of BRD3D.dll:
 *
 *     Glide 0x100AACA8 -> D3D 0x100AB508 = entry 15 = { 100, 10, 410,  29 }
 *     Glide 0x100AABE8 -> D3D 0x100AB448 = entry  3 = { 148, 110, 358, 260 }
 *     Glide 0x100AACF8 -> D3D 0x100AB558 = entry 20 = {  80,  29, 430,  48 }
 *
 * The right-hand column was read INDEPENDENTLY out of BRGlide.dll for this
 * module and matches the D3D-derived table to the byte, which is the
 * "check it a second way" CONVENTIONS.md asks for.  Two of the three then
 * agree with the geometry this builder computes without being told to: the
 * title is placed at y = 10 and entry 15's top is 10; the status line is
 * placed at y = 29 and entry 20's top is 29; both boxes are 19 tall, which is
 * the row pitch.  No fourth name is coined -- the pointers are supplied by
 * the caller, exactly as slice6_71.h supplies p0AB438 / p0AB448 / p0AB508.
 *
 * ==========================================================================
 * THE STACK FRAME, TRACED, BECAUSE [esp+N] MEANS NOTHING WITHOUT ESP
 * ==========================================================================
 *
 * MSVC EH frame.  Let E be esp on entry (after the call):
 *
 *     push -1                E-0x04   the __try level
 *     push 0x1007516B        E-0x08   the handler
 *     push fs:[0]            E-0x0C   the previous registration link
 *     mov  fs:[0], esp
 *     push ebx               E-0x10
 *     push ebp               E-0x14
 *     push esi               E-0x18
 *     push edi               E-0x1C   <- esp for the whole body
 *     mov  edi, [esp+0x20]            == E+0x04 -- THE ARGUMENT
 *
 * so relative to the body's esp:
 *
 *     [esp+0x10] == E-0x0C   the saved fs:[0] link.  `mov ecx,[esp+0xC]` in
 *                            the epilogue is the same slot after `pop edi`
 *                            has already moved esp by four; it is the unlink,
 *                            not a data read.
 *     [esp+0x18] == E-0x04   the __try level.  It steps 0,1,2,...,0x10 and
 *                            back to -1 around each allocation, which is why
 *                            sixteen of the function's stores look like a
 *                            counter.  It is pure EH state.
 *     [esp+0x20] == E+0x04   THE ARGUMENT SLOT, and the function REUSES it as
 *                            scratch: every `mov [esp+0x20], eax` after an
 *                            operator new is the unwinder's copy of the raw
 *                            block, not a second argument.  edi was loaded
 *                            from it once, at 0x100425F9, and is never
 *                            reloaded.
 *
 * Both facts matter for the port and neither is visible from the
 * displacement alone: there is exactly ONE parameter, and the seventeen
 * `[esp+0x18]` stores and sixteen `[esp+0x20]` stores are worth zero bytes of
 * behaviour.  `add esp,0x10` in the epilogue pops the four EH dwords.
 *
 * ==========================================================================
 * THE RETURN VALUE
 * ==========================================================================
 *
 * There is one exit and it sets eax = 1 (0x10043022).  br_phase.h types
 * `pfnEnter` as `void (*)(BrPhase_ *)` and both known call shapes discard the
 * result -- br_uinav.c's BrUiNavHook_10045AF0 does `p->pfnEnter(p);` and
 * br_uiboot.h's slot is BrPhaseEnterFn_ -- so the port keeps the void type
 * rather than coining a rival one.  CONVENTIONS.md's rule cuts the other way
 * too: a dead return value says nothing about side effects, and this function
 * is nothing but side effects.
 *
 * ==========================================================================
 * INSTALLS: TEN REAL, ONE FALSE POSITIVE
 * ==========================================================================
 *
 * config/hookmap.csv credits 0x100425E0 with eleven installs.  Ten are real
 * stores into a control's +0x08 / +0x0C; the eleventh, 0x1007516B, is
 * `push 0x1007516B` in the prologue -- the EH handler, an address
 * MATERIALISED and never stored.  0x1007516B is ten bytes,
 * `mov eax, 0x10079B38 / jmp 0x10074566`, i.e. the MSVC FuncInfo thunk.  That
 * is the same false positive br_uiboot.h names for its own prologue and the
 * limit ARCHITECTURE.md states for the tool.
 *
 * ALL TEN TARGETS ARE NOW IN THIS TREE.  Nine are under their D3D addresses,
 * which is CONVENTIONS.md's "grep BOTH builds" trap in its purest form --
 * none of those Glide numbers appears anywhere in port/:
 *
 *   slot  Glide       D3D         already ported as
 *   ----  ----------  ----------  ------------------------------------------
 *   +08   0x1003ED90  0x10045900  BrPhaseActivate_10045900 (slice3_31.c:262)
 *   +08   0x1003D140  0x10043BF0  BrSub10043BF0            (slice4_50.c:203)
 *   +08   0x1003E0E0  0x10044B90  BrMenuSub10044B90        (slice4_53.c:235)
 *   +08   0x1003E4A0  0x10044F50  BrPhaseActivate_10044F50 (slice2_26.c:380)
 *   +08   0x1003E730  0x100451E0  BrPhaseActivate_100451E0 (slice2_26.c:408)
 *   +08   0x1003AED0  0x10041970  BrUiCreditsAction_1003AED0
 *                                                    (menus/br_uicredits.c)
 *   +08   0x1003F610  0x10046170  BrPhaseActivate_10046170 (slice3_31.c:358)
 *   +0C   0x100407B0  0x10047360  BrSub10047360            (slice3_31.c:745)
 *   +0C   0x10040AF0  0x100474B0  BrPhaseTick_100474B0     (slice3_31.c:798)
 *   +0C   0x10040A20  0x100475F0  BrPhaseTick_100475F0     (slice3_31.c:805)
 *
 * CORRECTION, and it is the trap this header was WARNING about, sprung on
 * this header: the three "-- NOT PORTED" entries that stood in those rows
 * were wrong about two of the three.  Glide 0x1003E4A0 and 0x1003E730 were
 * already transcribed in slice2_26.c under 0x10044F50 / 0x100451E0, and both
 * pairs were re-checked instruction by instruction across the two binaries
 * before this line was written.  Only 0x1003AED0 was genuinely absent, and it
 * is now port/src/menus/br_uicredits.c.  The same audit clears two more
 * addresses this header names: the cursor driver 0x10041D10 is slice3_32.c's
 * BrPhaseTick_100488C0 (D3D 0x100488C0), and SetStatusText 0x1003AF30 is
 * slice5_62.c's BrExt_100419D0 (D3D 0x100419D0).  See br_uicredits.h.
 *
 * DO NOT read 0x1003E0E0 or 0x10040A20 as D3D numbers: tools/whereis.py
 * reports both AMBIGUOUS (0x1003E0E0 is also a valid D3D address, whose Glide
 * twin is 0x10037780, and 0x10040A20's is 0x10039F60), and slice7_82.c /
 * slice2_24.h name the D3D readings.  Those are different functions.
 *
 * The three +0x0C hooks are one family: 0x10040AF0 is nineteen bytes,
 * `push [esp+4] / call 0x100407B0 / add esp,4 / mov eax,1 / ret`, and
 * 0x10040A20 is the same with an extra leading `call 0x10040A40`.  br_ui.h's
 * ADJ-8 types the slot `int32_t (*)(BrUiCtl_ *)`; all three agree with that
 * exactly -- one cdecl argument, `mov eax,1 / ret`.
 *
 * The seven ported bodies carry three DIFFERENT host signatures
 * (`int (void)`, `void (BrGameObj *)`, `void (int32_t)`), which is why the
 * hooks are reached through a TABLE here rather than declared: a table
 * creates no new name and contradicts no existing declaration.  slice6_71.h
 * states the same reason for the same construct.
 */
#ifndef BR_UIROOT_H
#define BR_UIROOT_H

#include <stddef.h>
#include <stdint.h>

#include "br_phase.h"    /* BrPhase_, BrPhaseEnterFn_, BR_PHASE_PAGES        */
#include "br_ui.h"       /* BrUiPage_ / BrUiCtl_ / BrUiCtlHookFn_ -- CANONICAL
                          * (pulls slice3_39.h for BrTextBox / BrTextList)   */
#include "br_crt.h"      /* BrOperatorNew -- does NOT zero                   */
#include "slice1_06.h"   /* BrErrHost / BrErrShow  (Glide 0x100378C0)        */

/* ==========================================================================
 * Shape constants, each pinned by the disassembly rather than counted off a
 * listing.  The assertions at the bottom re-derive the two that are
 * arithmetic.
 * ========================================================================== */

/* The seven 0x102001 rows -- BRString ids 1+1 .. 1+7, i.e. 2..8. */
#define BR_UIROOT_ROWS          7

/* The five 0x809 highlights.  Fewer than the rows: see OBSERVATION 2. */
#define BR_UIROOT_HILITES       5

/* apCtl[0..14]; cCtl ends here. */
#define BR_UIROOT_CTL_COUNT     15

/* page +0x334 == apCtl[199]. The cursor. NOT counted in cCtl. */
#define BR_UIROOT_CURSOR_SLOT   199

/* The index every f38 site's a7 gives the status line, published to
 * 0x10AC4C58 before cCtl is bumped. */
#define BR_UIROOT_STATUS_INDEX  14

/* The error index both failure paths report through Glide 0x100378C0.  It is
 * FATAL in slice1_06.c's g_aBrErrTable, so the original never returns from
 * it; see the DEVIATION note in br_uiroot.c. */
#define BR_UIROOT_ERR_ALLOC     4

/* The page's two immediates: 0x43430000 and 0x42FA0000. */
#define BR_UIROOT_PAGE_X        195.0f
#define BR_UIROOT_PAGE_Y        125.0f

/* ==========================================================================
 * The ten hooks this function installs.
 *
 * Named for the GLIDE address, because BRGlide.dll is the reference
 * (CONVENTIONS.md, "Source precedence").  The D3D twin and the symbol that
 * already implements it, where one exists, are in the banner's table.
 *
 * Type is br_ui.h's BrUiCtlHookFn_ (ADJ-8): __cdecl, one argument, and the
 * argument is the control.  A NULL slot is a state this engine has
 * everywhere, so nothing here is required.
 * ========================================================================== */
typedef struct BrUiRootHooks {
    /* control +0x08 -- the ACTION, run when the ACTIVATE bit is set.  One per
     * menu row, in the page's order. */
    BrUiCtlHookFn_ p1003ED90;   /* row 0  "Championship"  D3D 0x10045900 */
    BrUiCtlHookFn_ p1003D140;   /* row 1  "Multiplayer"   D3D 0x10043BF0 */
    BrUiCtlHookFn_ p1003E0E0;   /* row 2  "Time Attack"   D3D 0x10044B90 */
    BrUiCtlHookFn_ p1003E4A0;   /* row 3  "Quick Race"    D3D 0x10044F50 */
    BrUiCtlHookFn_ p1003E730;   /* row 4  "Options"       D3D 0x100451E0 */
    BrUiCtlHookFn_ p1003AED0;   /* row 5  "Credits"       D3D 0x10041970 */
    BrUiCtlHookFn_ p1003F610;   /* row 6  "Quit"          D3D 0x10046170 */

    /* control +0x0C -- the per-frame caption setter, NOT an action.  Three
     * distinct values over the seven rows; 0x100407B0 is the base and the
     * other two wrap it. */
    BrUiCtlHookFn_ p100407B0;   /* rows 0 and 3           D3D 0x10047360 */
    BrUiCtlHookFn_ p10040AF0;   /* rows 1, 2, 4 and 6     D3D 0x100474B0 */
    BrUiCtlHookFn_ p10040A20;   /* row 5                  D3D 0x100475F0 */
} BrUiRootHooks;

/* ==========================================================================
 * Everything else the builder reaches outside itself.
 *
 * Supplied, never defaulted, for br_boot.h's reason: a caller that supplies
 * nothing must not receive a plausible main menu.
 * ========================================================================== */
typedef struct BrUiRootCtx {
    const BrUiRootHooks *pHooks;

    /* 0x100378C0.  May not return -- it can call exit(1). */
    const BrErrHost *pErrHost;

    /* The three style rectangles.  See the banner: these are entries 15, 3
     * and 20 of slice3_39.c's g_aBrUiStyle, reached as
     * BR_UI_STYLE(0x100AB508) / BR_UI_STYLE(0x100AB448) /
     * BR_UI_STYLE(0x100AB558).  Held as `const void *` because that is what
     * the control vtable's +0x34 takes and because it keeps this module free
     * of a link dependency on slice3_39.o -- slice6_71.h does the same. */
    const void *pStyleTitle;    /* Glide 0x100AACA8 */
    const void *pStyleRow;      /* Glide 0x100AABE8 */
    const void *pStyleStatus;   /* Glide 0x100AACF8 */

    /* 0x100ACAD8 -- the status line's text.  A literal pointer into .data,
     * not a string-table id.  The shipped image holds a single space. */
    const char *pszStatus;
} BrUiRootCtx;

extern BrUiRootCtx g_brUiRoot;

/* Non-zero when every slot the builder dereferences is filled.  pHooks'
 * members are NOT checked: a NULL hook is a state the engine has everywhere,
 * and the original stores whatever is there. */
int BrUiRootCtxComplete(const BrUiRootCtx *pCtx);

/* 0x10AC4C58 -- "which control on the root page is the status line".  Written
 * by this builder, read by Glide 0x1003AF30.  Storage lives here because
 * nothing in port/ defined it; br_sprfont.c only mentions the address as the
 * upper bound of its rectangle table D. */
extern int32_t g_brUiRootStatusIdx;

/* ==========================================================================
 * 0x100425E0 itself.
 *
 * `void` rather than `int32_t`: see THE RETURN VALUE in the banner.  The type
 * is br_phase.h's BrPhaseEnterFn_, so this is assignable straight into
 * br_uiboot.h's `pfnPhaseEnter` slot with no cast.
 * ========================================================================== */
void BrUiRootEnter_100425E0(BrPhase_ *pPhase);

/* Reset the one global this module owns, so a test can build twice. */
void BrUiRootResetForTest(void);

/* --- arithmetic that pins the counts (host-independent) ------------------ */
#define BR_UIROOT_ASSERT(name, cond) typedef char BR_UIROOT_##name[(cond)?1:-1]

/* The cursor really is the LAST apCtl slot, and it is inside the array. */
BR_UIROOT_ASSERT(cursor_is_last_apctl,
                 BR_UIROOT_CURSOR_SLOT == BR_UI_PAGE_CTL_MAX - 1);
/* page +0x018 + 199*4 == +0x334, the displacement at 0x1004271F. */
BR_UIROOT_ASSERT(cursor_slot_is_334,
                 0x018u + (unsigned)BR_UIROOT_CURSOR_SLOT * 4u == 0x334u);
/* One backdrop + one title + seven rows + five highlights + one status. */
BR_UIROOT_ASSERT(ctl_count_adds_up,
                 1 + 1 + BR_UIROOT_ROWS + BR_UIROOT_HILITES + 1
                 == BR_UIROOT_CTL_COUNT);
/* The status line is the last counted control. */
BR_UIROOT_ASSERT(status_is_last,
                 BR_UIROOT_STATUS_INDEX == BR_UIROOT_CTL_COUNT - 1);

#endif /* BR_UIROOT_H */
