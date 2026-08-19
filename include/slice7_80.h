/* slice7_80.h -- the OPTION-CHANGING control hooks, and the input edge that
 * drives them.
 *
 * ===========================================================================
 * WHAT THIS MODULE IS
 * ===========================================================================
 * Two menu screens change persistent game state when you activate a row:
 *
 *   0x1004DFC0  AUDIO OPTIONS  (title string 0x21 == "Audio Options")
 *                 row 0x2E "SFX Volume"  pfn08 = 0x10042CF0
 *                 row 0x2F "CD VOlume"   pfn08 = 0x10042D60   (sic, the
 *                                        original's capitalisation)
 *   0x1004E830  GAME OPTIONS   (title string 0x22 == "Game Options")
 *                 row 0x30 "Force Feedback"    pfn08 = 0x10043590
 *                 row 0x31 "Skid Marks"        pfn08 = 0x100435F0
 *                 row 0x32 "Specular Lighting" pfn08 = 0x100436B0
 *                 row 0x33 "Car Shadow"        pfn08 = 0x10043650
 *
 * Those six addresses are the whole option-changing set.  The pairing is read
 * out of the two ported builders, not guessed: slice6_73.c's BrExt_1004DFC0
 * and slice6_72.c's BrExt_1004E830 each store the hook two lines after the
 * BrStrGet id that names the row.
 *
 * ===========================================================================
 * ALL SIX BODIES WERE ALREADY PORTED.  THIS FILE IS ADAPTERS.
 * ===========================================================================
 * CONVENTIONS.md's "grep the address before naming anything" applies, and it
 * pays out here in full: every one of the six already has a transcription in
 * port/src/slice2_25.c, under a name derived from the global it writes.
 *
 *   0x10042CF0  slice2_25.c  BrOptCycleB4E708   SFX volume   -> 0x10B4E708
 *   0x10042D60  slice2_25.c  BrOptCycleB4E70C   CD volume    -> 0x10B4E70C
 *   0x10043590  slice2_25.c  BrOptCycleAA2A1C   force fdbk   -> 0x10AA2A1C
 *   0x100435F0  slice2_25.c  BrOptCycleAA2A28   skid marks   -> 0x10AA2A28
 *   0x10043650  slice2_25.c  BrOptCycleAA2A20   car shadow   -> 0x10AA2A20
 *   0x100436B0  slice2_25.c  BrOptCycleAA2A24   specular     -> 0x10AA2A24
 *
 * All six were re-checked against orig/BRGlide.dll for this pass and all six
 * are right, instruction for instruction (see the VERIFIED table below).  So
 * nothing is decompiled a second time; what this file adds is the three
 * things that were missing between a correct body and an observable toggle:
 *
 *   1. ADAPTERS.  The bodies are `int (void)` -- correctly, they read no
 *      argument -- and a control's +0x08 slot is `int32_t (*)(BrUiCtl_ *)`.
 *      In the original's __cdecl that mismatch is invisible; in C it is a
 *      type error, so each hook gets a one-line adapter.
 *   2. INSTALLERS.  A builder reads its hook table and stores what it finds;
 *      an all-NULL table is why activating a toggle did nothing.
 *   3. THE EDIT EDGE.  See "THE SEAM" below -- this is the part that was
 *      actually load-bearing, and it is not what it looks like.
 *
 * ===========================================================================
 * VERIFIED AGAINST orig/BRGlide.dll
 * ===========================================================================
 * config/shared.csv classes all six `shared`.  The four two-state cyclers are
 * byte-identical apart from their operands, which is why crossdiff maps all
 * four D3D addresses onto the ONE Glide address 0x1003C3D0 -- a hash match on
 * four functions, not evidence that three of them are missing.
 *
 *   D3D         Glide       bytes  what it is
 *   0x10042CF0  0x1003C240    99   SFX volume,  max 9, sets   0x100AB3D8
 *   0x10042D60  0x1003C2B0    92   CD volume,   max 9, clears 0x100AB3D8
 *   0x10043590  0x1003C3D0    93   two-state, table 0x100AC530 -> 0x10B4E1E0
 *   0x100435F0     "          93   two-state, table 0x100AC548 -> 0x10B4E7A0
 *   0x10043650     "          93   two-state, table 0x100AC538 -> 0x10B4E1D8
 *   0x100436B0     "          93   two-state, table 0x100AC540 -> 0x10B4E1DC
 *
 * All four two-state tables are `{ 1, 0 }` in the image (read out of the DLL,
 * not assumed), so the INDEX and the VALUE run opposite ways: index 0 stores
 * 1 and index 1 stores 0.  A test that asserts "the value went up" would be
 * asserting the wrong direction.
 *
 * ===========================================================================
 * THE SEAM, AND WHY "ACTIVATE" IS NOT ENOUGH
 * ===========================================================================
 * br_uinav.h documents the ACTIVATE seam: set 0x10AA2AF0, and 0x10047A60
 * raises +0x1C bit 0x02 on the current control, and 0x10048180 calls its
 * +0x08 hook.  That is true, and for a "Back" row it is the whole story.
 *
 * It is NOT the whole story for an option row, because every one of these six
 * bodies begins by reading a DIFFERENT pair of globals:
 *
 *     0x10AA33D4   step this option UP     (wins when both are set)
 *     0x10AA33D0   step this option DOWN
 *
 * and does nothing at all when both are clear.  Those two words are the first
 * two of the four that 0x1003E080 -- the "is anything active" predicate --
 * also reads, which br_state.h models as BrActiveFlags::a0 and ::a1.
 *
 * So in the ORIGINAL the same key press does both jobs at once: pressing
 * right sets 0x10AA33D4, which makes 0x1003E080 answer yes, which makes
 * 0x10047A60 raise the ACTIVATE bit on the row under the cursor, which makes
 * 0x10048180 call the hook, which reads 0x10AA33D4 again and steps the option
 * up.  One word, two roles.  BrUiOptSetEdit() below is that key press.
 *
 * Firing the hook through 0x10AA2AF0 instead reproduces the original exactly
 * and changes nothing: the cycler takes neither arm.  That is not a defect in
 * this port -- it is what the shipped game does, and the two volume hooks
 * still run their side effects (0x100AB3D8 and 0x10060D90) on that path.
 * BrUiOptSetActivateOnly() exists so a caller can demonstrate the difference.
 *
 * ===========================================================================
 * AND THE KEYBOARD-SELECTED ROW IS THE ONE CASE THE EDGE CANNOT ACTIVATE
 * ===========================================================================
 * Found while wiring this, and it is not obvious from br_uinav.h.  0x10047A60
 * computes `fCurrent = (selection cursor == this control's ordinal)` and then,
 * at D3D 0x10047BCF / Glide 0x1004101F:
 *
 *     test edx,edx / je <flag block>       ; NOT current -> the normal path
 *     [0x10AA33D0] != 0  -> return 1       ; current AND any edit edge set
 *     [0x10AA33D4] != 0  -> return 1       ;   -> return 1 with the control's
 *     [0x10AA33D8] != 0  -> return 1       ;      flags UNTOUCHED, so the
 *     [0x10AA33DC] != 0  -> return 1       ;      ACTIVATE bit is never raised
 *
 * byte-identical in both builds.  br_uinav.c transcribes it correctly and
 * br_uinav.h glosses those four reads as "a modal thing is running: leave the
 * flags alone", which is right about the mechanism and understates the
 * consequence: the row the keyboard has selected is precisely the row that
 * cannot be edited while an edge is held.
 *
 * The row that DOES activate is the one with `fCurrent == 0` whose own
 * rectangle contains the cursor at 0x10AA2A78 -- i.e. the POINTER.  So these
 * option rows are pointer controls in the shipped game, and any harness that
 * wants to see a volume change has to move the cursor, not just press a key.
 *
 * Consequence for port/host/brally.c's `-keys`, stated so it is not filed as
 * a bug in this module: that driver parks the cursor at (-1,-1) permanently
 * and by design ("pure KEYBOARD evidence"), and has no key for the edit edge.
 * Installing these hooks there makes the ACTIVATE path observable -- 'j' on a
 * volume row runs the hook and moves 0x100AB3D8 -- but it can never move a
 * volume, because both preconditions are missing.  port/tests/test_slice7_80.c
 * carries the same driver with a movable cursor and the two extra keys, and
 * that is where the before/after numbers are.
 *
 * ===========================================================================
 * ALIASED STORAGE -- 0x10AA33D0..0x10AA33DC HAS THREE OWNERS TODAY
 * ===========================================================================
 * This is CONVENTIONS.md's "aliased storage: a link-clean bug", live, and it
 * is the reason the seam below writes what looks like the same value three
 * times.  One original array of four dwords is modelled by three modules:
 *
 *   port/src/slice3_39.c   int32_t g_BrBtnEdge[4]        <- owns the storage
 *                          (0x10AA33D0, and slice3_39.h already says in prose
 *                          that slice2_25.h's scalars are the same words)
 *   port/src/slice2_25.c   int32_t g_brAA33D0, g_brAA33D4
 *   br_state.h             BrActiveFlags::a0 .. ::a3, whose storage the HOST
 *                          owns (port/host/brally.c's `g_active`)
 *
 * Three storages, one original object, no duplicate symbol -- it links
 * cleanly and is still wrong.  It is also exactly why the menu looked inert:
 * the harness writes the host's view, the cyclers read slice2_25's, and
 * neither can see the other.
 *
 * PROPER FIX, stated so it is not re-derived: slice2_25's two scalars become
 * macros over g_BrBtnEdge[0] / [1] (the precedent is g_brAA26F4, resolved
 * that way in slice5_63), and BrActiveFlags::a0..a3 stop being host storage
 * and become a view onto the same array.  Both edits are outside this pass's
 * scope -- slice2_25.c belongs to another packet and the BrActiveFlags
 * storage is the host's -- so this module keeps the three consistent instead,
 * in ONE place, marked DEVIATION.  That is a bridge, not a resolution, and it
 * should be deleted the day the storage is merged.
 *
 * ===========================================================================
 * WIRING (one call each; nothing in this module installs itself)
 * ===========================================================================
 *   BrUiOptInstall73(&hooks73);        before BrExt_1004DFC0 runs
 *   BrUiOptInstall72(&hooks72);        before BrExt_1004E830 runs
 *
 * and, if the host uses br_uinav's control frame, one more store the frame
 * itself needs -- 0x10048180 compares the hook it is about to call against
 * BrScrGlobals::pfn10042CF0 (slice3_32.h) and takes a different audio path
 * when they match:
 *
 *   g_scr.pfn10042CF0 = (void *)BrUiOptHook_10042CF0;
 *
 * That store is not made here because BrScrGlobals cannot be included in the
 * same translation unit as slice6_73.h (slice6_73.h CONFLICT 1: the two
 * headers declare BrOptObjCtor over different pointee types).
 *
 * ===========================================================================
 * WHAT slice6_73.h GETS WRONG ABOUT 0x1004DFC0, since this module depends on it
 * ===========================================================================
 * slice6_73.h calls 0x1004DFC0 "the car screen" and BrUi73Ctx names its
 * twelve string pointers `apCarName[12]` (0x100B89C8..0x100B89F4).  Both are
 * wrong, and the screen is Audio Options:
 *
 *   - its title is BrStrGet(0x21), and string 33 is "Audio Options";
 *   - its three menu rows are 0x2E "SFX Volume", 0x2F "CD VOlume" and
 *     0x0C "Back";
 *   - the twelve pointers read, in order, "Fear and Loathing", "Alice",
 *     "Tre", "Nagasaki", "Chewbaca", "Projectile", "Numb VI", "Gelex",
 *     "Three Blind Lice", "10 to 0", "Bottle Rocket", "Fancy Car" -- the CD
 *     soundtrack, not cars.  (Read out of orig/BRD3D.dll, not inferred from
 *     the name.)
 *
 * So 0x10AA2A34, which that builder clamps to [0,11] three times over, is the
 * selected MUSIC TRACK.  The field is left alone here rather than renamed,
 * because slice6_73.h belongs to another packet; this note is the report.
 */
#ifndef SLICE7_80_H
#define SLICE7_80_H

#include <stdint.h>

#include "slice6_73.h"   /* BrUi73Hooks, BrUiCtl_, BrUiCtlHookFn_ */
#include "slice6_72.h"   /* BrUi72Hooks                           */
#include "br_state.h"    /* BrActiveFlags -- the nine 0x1003E080 reads */

/* ===========================================================================
 * The six hooks, as control +0x08 slots.
 *
 * ADAPTERS ONLY.  Each is `return (int32_t)<the slice2_25.c body>();` and the
 * control argument is discarded because the original discards it -- none of
 * the six reads [esp+4].  Every one returns 1 on every path, and 0x10048180
 * treats a 0 from a +0x08 hook as "stop the page", so a non-1 return here
 * would be a behavioural change and not a cosmetic one.
 * ========================================================================== */

int32_t BrUiOptHook_10042CF0(BrUiCtl_ *pCtl);   /* Audio: SFX Volume        */
int32_t BrUiOptHook_10042D60(BrUiCtl_ *pCtl);   /* Audio: CD VOlume         */
int32_t BrUiOptHook_10043590(BrUiCtl_ *pCtl);   /* Game:  Force Feedback    */
int32_t BrUiOptHook_100435F0(BrUiCtl_ *pCtl);   /* Game:  Skid Marks        */
int32_t BrUiOptHook_10043650(BrUiCtl_ *pCtl);   /* Game:  Car Shadow        */
int32_t BrUiOptHook_100436B0(BrUiCtl_ *pCtl);   /* Game:  Specular Lighting */

/* ===========================================================================
 * Installers.  Each touches ONLY the slots it owns, the way
 * BrUiNavInstallCtlVtbl does: a slot this pass has no body for must stay a
 * visible NULL rather than become a silent no-op.
 * ========================================================================== */

/* Audio Options' two volume hooks. */
void BrUiOptInstall73(BrUi73Hooks *pHooks);

/* Game Options' four toggles. */
void BrUiOptInstall72(BrUi72Hooks *pHooks);

/* ===========================================================================
 * The edit seam -- the analogue of br_uinav.h's BrUiNavMove.
 * ========================================================================== */

/* Assert the option-edit edge for one frame.
 *
 *   dir > 0   0x10AA33D4 = 1, 0x10AA33D0 = 0    step the option UP
 *   dir < 0   0x10AA33D0 = 1, 0x10AA33D4 = 0    step the option DOWN
 *   dir == 0  both cleared
 *
 * `pFlags` may be NULL, in which case only the two module-level views are
 * written; pass the host's BrActiveFlags to keep 0x1003E080's answer in step
 * with what the hooks will read.  See the ALIASED STORAGE banner: writing
 * more than one object here is a DEVIATION that exists only because the port
 * has more than one object.
 *
 * The original asserts these words for exactly the frame the key goes down
 * (0x1005FFF0 computes a rising edge), so a caller drives one frame per call
 * and clears with dir == 0 afterwards. */
void BrUiOptSetEdit(BrActiveFlags *pFlags, int dir);

/* Raise ONLY 0x10AA2AF0, the plain activate flag, leaving both edit words
 * clear.  This is the "press the fire button on a volume row" case, and the
 * original's answer is that the row activates and the value does not move.
 * Provided so that fact can be demonstrated rather than asserted. */
void BrUiOptSetActivateOnly(BrActiveFlags *pFlags, int fDown);

/* ===========================================================================
 * Observation.
 *
 * These are NOT decompiled; they exist so a host or a test can dump the state
 * a hook wrote without having to include slice2_25.h, which cannot share a
 * translation unit with slice6_73.h (CONFLICT 1).  A claim that a toggle
 * changed something is worth nothing without the number.
 * ========================================================================== */
typedef enum BrUiOptId {
    BR_OPT_SFX_VOLUME = 0,   /* 0x10B4E708, 0..9 */
    BR_OPT_CD_VOLUME,        /* 0x10B4E70C, 0..9 */
    BR_OPT_FORCE_FEEDBACK,   /* 0x10AA2A1C, 0..1 */
    BR_OPT_SKID_MARKS,       /* 0x10AA2A28, 0..1 */
    BR_OPT_CAR_SHADOW,       /* 0x10AA2A20, 0..1 */
    BR_OPT_SPECULAR,         /* 0x10AA2A24, 0..1 */
    BR_OPT_COUNT
} BrUiOptId;

/* The option's own index word (0x10B4E708 and friends). */
int32_t     BrUiOptGetIndex(BrUiOptId id);

/* The word the hook PUBLISHES from that index -- 0x10B4E1E0 and friends for
 * the four toggles.  The two volume options publish nothing (their index IS
 * the value), and this returns their index for them. */
int32_t     BrUiOptGetPublished(BrUiOptId id);

/* 0x100AB3D8 -- which of the two volume rows was touched last: 1 after the
 * SFX hook, 0 after the CD hook.  Read by 0x1003E950 / 0x1003EA40, the two
 * +0x04 hooks the Audio screen puts on its slider widgets. */
int32_t     BrUiOptGetVolumeSelector(void);

/* The original's own address for an option, for reports. */
const char *BrUiOptName(BrUiOptId id);
uint32_t    BrUiOptAddress(BrUiOptId id);

/* The two edit words, so a caller can show the seam did what it says. */
int32_t     BrUiOptGetEditUp(void);
int32_t     BrUiOptGetEditDown(void);

#endif /* SLICE7_80_H */
