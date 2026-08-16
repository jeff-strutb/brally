/* slice8_88.h -- seven more of the control hooks slice6_71.c / slice6_72.c /
 * slice6_73.c install, transcribed over br_ui.h's canonical `BrUiCtl_`.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS
 *
 * The builders in slice6_71.c / slice6_72.c / slice6_73.c copy function
 * pointers out of their hook tables into each control's +0x04 / +0x08 slot.
 * A NULL slot is a control that does nothing.  slice7_80.c, slice7_81.c,
 * slice8_84.c, slice8_85.c and the host already fill part of those tables;
 * this module fills seven more slots, and it is scoped to the caption/text
 * setters the slice2_24 packet owns plus one slice2_25 toggle.
 *
 * Every pairing below cites the builder line that proves it.  Nothing here is
 * inferred from a name.
 *
 *   addr        builder line                          slot it lands in
 *   ----------  -----------------------------------   -------------------
 *   0x10040730  slice6_72.c:886, :1070                control pfn04
 *   0x100407E0  slice6_72.c:870, :1054                control pfn04
 *   0x10040950  slice6_72.c:1510                      control pfn04
 *   0x10040990  slice6_72.c:1528                      control pfn04
 *   0x100413B0  slice6_72.c:1153 / slice6_73.c:890    control pfn04
 *   0x100414B0  slice6_72.c:1123                      control pfn04
 *   0x10042B00  slice6_71.c:634                       control pfn08
 *
 * ---------------------------------------------------------------------------
 * PRE-FLIGHT, AND WHERE THE BRIEF THIS PASS WAS GIVEN WAS WRONG
 *
 * tools/whereis.py was run on every address before a line was written.  The
 * brief called all eight "unported"; five of them already had bodies:
 *
 *   0x10040730 0x100407E0 0x10040950 0x10040990  slice2_24.c, as
 *       BrMenuCap0730 / BrMenuCap07E0 / BrMenuCap0950 / BrMenuCap0990.
 *   0x10042B00  slice2_25.c, as BrOptToggle2F7C_C.
 *   0x100436B0  slice2_25.c, as BrOptCycleAA2A24 -- AND ALREADY INSTALLED,
 *       into BrUi72Hooks::p100436B0 by slice7_80.c's BrUiOptInstall72
 *       (slice7_80.c:127, asserted by test_slice7_80.c:438).  This module
 *       therefore does NOT touch it: two installers writing one slot is the
 *       exact fight test_slice8_84.c:340 exists to catch.
 *
 * Only TWO of the eight had no body anywhere: 0x100413B0 and 0x100414B0,
 * which slice8_84.h:154-155 and slice8_85.h both list as holes.
 *
 * The brief also called them all `pfn08` "control action hooks".  Six of the
 * seven land in `pfn04` -- they are the per-frame caption/text setters, not
 * the enter/action hooks.  Only 0x10042B00 is a pfn08.
 *
 * ---------------------------------------------------------------------------
 * WHY THE FIVE EXISTING BODIES ARE TRANSCRIBED AGAIN RATHER THAN WIRED
 *
 * This is slice8_85.h's answer, restated because it is the whole reason this
 * module exists rather than a seven-line installer:
 *
 *     a byte-offset body cannot be pointed at a struct whose fields have
 *     moved under LP64.
 *
 * slice2_24.c types its four over `BrMenuItem`, a THREE-FIELD compression
 * `{ f1C, f1E20C, text }` -- so `BrMenuCap0730((BrMenuItem *)pCtl)` links
 * cleanly and writes 4 bytes into the control's +0x04, not its +0x1E20C.
 * slice2_25.c types 0x10042B00 over a `BrGameObj` byte image, same problem.
 * slice8_85.c already re-transcribed 0x10040930 (BrMenuCap0930's twin) and
 * 0x10042AC0 (0x10042B00's byte-identical twin) for exactly this reason.
 *
 * Each body below therefore names its twin, so neither looks like a duplicate
 * nobody noticed.  Where the two disagree, one of them is wrong and the diff
 * is a diff of behaviour.
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Every body was read from orig/BRGlide.dll, the project reference, at the
 * Glide address config/shared.csv pairs with the D3D address the builders
 * name, and then cross-checked against orig/BRD3D.dll at the D3D address.
 * The pairs, all confirmed structurally identical:
 *
 *     D3D         Glide       bytes
 *     0x10040730  0x10039C70   98
 *     0x100407E0  0x10039D20  129
 *     0x10040950  0x10039E90   64
 *     0x10040990  0x10039ED0   30
 *     0x100413B0  0x1003A910  245
 *     0x100414B0  0x1003AA10  236 (D3D) / 238 (Glide -- tail padding only)
 *     0x10042B00  0x1003C050   48
 *
 * NOT A CONFLICT, but it looks like one and cost time here: br_span.h names
 * 0x1003A910 `BrSpanTest`.  That is correct -- br_span.h reads 0x1003A910 as
 * a **D3D** address, where it is a 55-byte point-in-span test.  In the GLIDE
 * image the same number is this module's 245-byte text setter.  The two
 * readings are of different builds and neither is wrong.  whereis.py flags
 * this class of collision as AMBIGUOUS; heed it.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, stated address by address
 *
 * REACHED, never duplicated -- the point of this list is that six of the nine
 * globals these hooks read already have exactly one owner in the tree, and
 * this module adds no second storage for any of them:
 *
 *   0x100AA010  session kind          slice6_73.h  g_br73.n0AA010
 *   0x100AC648  vehicle cursor        slice6_73.h  g_br73.n0AC648
 *   0x10AA28A4  stage column A        slice6_73.h  g_br73.nAA28A4
 *   0x10AA28AC  stage column B        slice6_73.h  g_br73.nAA28AC
 *   0x10AA28B8  stage index (BYTE)    slice6_73.h  g_br73.bAA28B8
 *   0x10AA289C  "have a time" flag    slice6_73.h  g_br73.nAA289C
 *   0x10AA28A0  lap/stage counter     slice6_73.h  g_br73.nAA28A0
 *   0x10AA2A00  0x100AC590's index    slice6_73.h  g_br73.nAA2A00
 *   0x10AA2518  the "%d" scratch      slice6_73.h  g_br73.szAA2518
 *   0x10AA26F0  the 0x53-dword block  slice6_73.h  g_br73.aAA26F0
 *   0x10AA28D8  the "edit is live" latch  slice6_73.h  g_brAA28D8
 *   0x10AA2904  the CURRENT phase     br_uinav.h   BrUiNav::pAA2904
 *   0x10AA28E8                        slice2_25.h  g_brAA28E8
 *   0x10AA2A1C  force feedback        slice2_25.h  g_brAA2A1C
 *   0x10AA2A28  skid marks            slice2_25.h  g_brAA2A28
 *   0x118ABDBC  FFB device present    slice3_45.h  g_br18ABDBC
 *
 * OWNED HERE (BrUiHook88Ctx), because nothing outside slice2_24.c's private
 * state block models them.  Each is ALSO a field of slice2_24.c's `g_menu`;
 * if a host ever drives both modules, exactly one of the two must keep the
 * storage and the other must alias into it.  Stated, not hidden -- this is
 * the same disclosure slice8_85.h makes for its four:
 *
 *   0x10AA28A8  the BYTE that chooses between 0x10AA28A4 and 0x10AA28AC
 *   0x10AA2964  the phase 0x10AA2904 is compared against
 *   0x100B3810  the stage record array (stride 0x18)
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND, reported rather than silently resolved
 *
 * 1. 0x10AA28B8 is read `movsx byte`, i.e. SIGNED, by 0x10040730, 0x100407E0
 *    and 0x100414B0.  slice6_73.h types it `uint8_t bAA28B8`; slice5_63.h and
 *    slice2_23.h both type the same address `int8_t`.  Two of three votes say
 *    signed and the instruction settles it.  This module casts `(int8_t)` at
 *    each of the three reads; slice6_73.h should adopt the signed type, and
 *    then exactly those three casts delete.  It is NOT cosmetic: a stage byte
 *    >= 0x80 indexes BACKWARDS off the front of both tables, and slice5_63.c
 *    already reproduces that on purpose for the same global.
 *
 * 2. 0x100AD278 (D3D) / 0x100ACA50 (Glide) is a DWORD TABLE -- 0x30, 0x2F,
 *    0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, ... -- and 0x100414B0 `strcpy`s from
 *    it.  Read as a C string that address is the single byte 0x30 followed by
 *    the first dword's zero padding, i.e. "0".  So the flag-clear arm of
 *    0x100414B0 always displays "0", never the table.  Preserved as a bug,
 *    not repaired; see k_AD278 in the .c.  This is the "data misclassified"
 *    trap running the other way -- the ORIGINAL is the one that mistook a
 *    record array for a string.
 *
 * 3. 0x100414B0 has an early `return 0` when the formatted string is EMPTY
 *    (`dec ecx / jne`, with eax already zeroed by the `xor eax,eax` that set
 *    up the scan).  Every other hook in this family returns 1 unconditionally.
 *    The path is UNREACHABLE in the original -- the clear arm yields "0" and
 *    _itoa never yields "" -- so it is transcribed and NOT claimed as tested.
 *    It is reachable in this port only if a host wires an empty k_AD278, which
 *    it cannot: the constant is file-static.
 *
 * ---------------------------------------------------------------------------
 * NOT INSTALLED, DELIBERATELY
 *
 *   0x100436B0  slice7_80.c's BrUiOptInstall72 owns BrUi72Hooks::p100436B0.
 *               Body: slice2_25.c BrOptCycleAA2A24.  See PRE-FLIGHT above.
 */
#ifndef SLICE8_88_H
#define SLICE8_88_H

#include <stddef.h>
#include <stdint.h>

#include "slice6_71.h"    /* BrS71Hooks -- 0x10042B00's table                */
#include "slice6_72.h"    /* BrUi72Hooks -- the six pfn04 slots              */
#include "slice6_73.h"    /* BrUi73Hooks, g_br73, g_brAA28D8, BrUiCtl_,
                           * BrTextBox / BrTextBoxVtbl, BrStrGet             */
#include "slice2_24.h"    /* BrMenuStage -- the 0x100B3810 record.  The TYPE
                           * only: none of that module's bodies is called,
                           * for the LP64 reason stated at the top.          */
#include "br_uinav.h"     /* g_pBrUiNav -- 0x10AA2904                        */

/* ==========================================================================
 * The three globals this module owns
 * ========================================================================== */

typedef struct BrUiHook88Ctx {
    /* 0x100B3810.  Stride 0x18, `uint16_t f10[4]` at +0x10; 0x10040730 and
     * 0x100407E0 read the LOW and HIGH byte of f10[k] respectively.  A
     * pointer, not an array, because the extent is not determinable -- the
     * same call slice2_24.h made. */
    const BrMenuStage *pStages;

    /* 0x10AA2964.  Only ever COMPARED, against 0x10AA2904. */
    const BrPhase_ *pAA2964;

    /* 0x10AA28A8.  A BYTE, tested against zero: non-zero selects 0x10AA28AC
     * as the stage column, zero selects 0x10AA28A4. */
    uint8_t bAA28A8;
} BrUiHook88Ctx;

/* The single instance, zero-initialised.  BrUiHook88Reset puts it back. */
extern BrUiHook88Ctx g_brHook88;

void BrUiHook88Reset(void);

/* ==========================================================================
 * The hooks.  All are `int32_t (BrUiCtl_ *)` -- br_ui.h's BrUiCtlHookFn_.
 * ========================================================================== */

/* 0x10040730 (98 B).  Caption setter: look a small integer up in the 16-entry
 * word table 0x100AC550 and store it at control +0x1E20C.  Returns 1.
 *
 * The index is 0x100AC648 while 0x100AA010 is set, and a byte out of the stage
 * record otherwise.
 *
 * GOTCHA: 0x100AC550 is a WORD table (`mov cx, word ptr [eax*2 + ...]`) while
 * every neighbour in the family indexes a BYTE table with `movsx`.  The stride
 * is the observable half of that difference and is tested; the sign-extension
 * half is NOT observable, because every byte of 0x100AC590 and 0x100AC630 is
 * below 0x80 in both shipped images, so `movsx` and `movzx` cannot disagree on
 * any value the game can produce.  Transcribed as signed anyway, because the
 * instruction says so and a host that reskins the tables would see it.
 *
 * Twin: slice2_24.c BrMenuCap0730. */
int32_t BrUiHook88_10040730(BrUiCtl_ *pCtl);

/* 0x100407E0 (129 B).  Same shape as 0x10040730 with four differences:
 *   - it is guarded, and answers -2 ("leave this item alone") when idle;
 *   - it takes the HIGH byte of the stage word (0x100B3821, not 0x100B3820);
 *   - the alternate index is 0x10AA2A00, not 0x100AC648;
 *   - the table 0x100AC590 is BYTES and IS sign-extended (unobservably -- see
 *     the GOTCHA on 0x10040730).
 *
 * -2 is reserved: nothing else in the family returns it.
 *
 * Twin: slice2_24.c BrMenuCap07E0. */
int32_t BrUiHook88_100407E0(BrUiCtl_ *pCtl);

/* 0x10040950 (64 B).  Caption setter over the 4-byte table 0x100AC630.
 *
 * GOTCHA: when 0x118ABDBC is CLEAR the entry is hard-wired to 0x100AC631 --
 * element ONE of the same table, not element zero.  Returns 1 on both arms.
 *
 * Twin: slice2_24.c BrMenuCap0950. */
int32_t BrUiHook88_10040950(BrUiCtl_ *pCtl);

/* 0x10040990 (30 B).  Caption setter over 0x100AC640.
 *
 * GOTCHA: the table is DWORDS (`[eax*4 + ...]`) but only the LOW WORD is
 * taken, and it is NOT sign-extended.  Returns 1.
 *
 * Twin: slice2_24.c BrMenuCap0990. */
int32_t BrUiHook88_10040990(BrUiCtl_ *pCtl);

/* 0x100413B0 (245 B).  TEXT setter, the caption shape: format, then the text
 * box's vtable +0x04 followed by +0x10.  Returns 1.
 *
 *   sprintf(0x10AA2518, "%d", 0x10AA28A0 + 1);
 *   id = 0x10AA28A0 == 0 ? 0xB3 : == 1 ? 0xB4 : == 2 ? 0xB5 : 0xB6;
 *   sprintf(local, "%s%s", 0x10AA2518, BrStrGet(id));
 *   strcpy(ctl + 0x2B65, local);
 *
 * GOTCHA: 0x10AA28A0 is read TWICE -- once for the number, once for the id --
 * with the sprintf in between.  Nothing changes it, but the second read is the
 * original's and is kept so the two cannot drift apart.
 *
 * GOTCHA: the +0x10 call is guarded by `test ebx,ebx` where ebx is the text
 * buffer's ADDRESS, which is never null, so it always runs.  Same shape as
 * slice8_85.c's Br85TextNumber.
 *
 * No twin: this address has no body anywhere else in the tree. */
int32_t BrUiHook88_100413B0(BrUiCtl_ *pCtl);

/* 0x100414B0 (236 B).  TEXT setter, the VALUE shape: format, then the text
 * box's vtable +0x08 followed by +0x2C.  Returns 1 (but see CONFLICT 3).
 *
 * When 0x10AA289C is clear it copies the "string" at 0x100AD278 -- which is
 * "0", see CONFLICT 2.  Otherwise it sums FOUR unsigned halfwords at
 * 0x10AA270E + 8 * (signed)0x10AA28B8 and _itoa()s the total in base 10.  The
 * result is upper-cased before it is stored.
 *
 * The upper-casing is DEAD with the shipped data and is transcribed anyway:
 * both arms can only ever produce digits ("0", or _itoa of a sum), so _strupr
 * has nothing to change.  It is not testable through this module's interface
 * and is not claimed to be.
 *
 * The four-halfword sum is the SAME table, base, stride and term count that
 * slice5_63.c's BrExt_1005FBC0 folds into 0x10AA28C4 -- 0x10AA26F0 + 0x1E,
 * stride 8, four terms, signed byte index.  That is one producer and one
 * consumer of the same record, and it is why the signed index in CONFLICT 1
 * is worth keeping.
 *
 * No twin: this address has no body anywhere else in the tree. */
int32_t BrUiHook88_100414B0(BrUiCtl_ *pCtl);

/* 0x10042B00 (48 B).  Toggle the dword at control +0x2F7C, once.
 *
 * GOTCHA: 0x10AA28D8 is a LATCH, not a debounce -- nothing in the packet ever
 * clears it, so across all three byte-identical copies (0x10042A90 /
 * 0x10042AC0 / 0x10042B00) the field is toggled at most once per clear of that
 * global.  Returns 1 either way.
 *
 * The stored value is `sete cl` on a 32-bit test, so it is exactly 0 or 1 --
 * never the complement of a wider value.
 *
 * Twin: slice2_25.c BrOptToggle2F7C_C over a BrGameObj byte image, and
 * slice8_85.c BrUiHook85_10042AC0 over this same control.  The three bodies
 * are byte-identical in the image. */
int32_t BrUiHook88_10042B00(BrUiCtl_ *pCtl);

/* ==========================================================================
 * Installation.  Each writes ONLY the slots listed above and tolerates NULL,
 * as BrUiOptInstall73 / BrUiHook81Install / BrUiHook85Install all do.
 * ========================================================================== */

void BrUiHook88Install71(BrS71Hooks  *pHooks);   /* 0x10042B00              */
void BrUiHook88Install72(BrUi72Hooks *pHooks);   /* the six pfn04 setters   */
void BrUiHook88Install73(BrUi73Hooks *pHooks);   /* 0x100413B0 only         */

#endif /* SLICE8_88_H */
