/* br_ctlname.h -- RESPONSIBILITY: reading what the player is doing.  This is
 * the VOCABULARY half of it: the human-readable name of every key, mouse
 * control and joystick control the binding screen can show.
 *
 * WHAT THIS IS
 *
 * slice2_23.h already models the consumer.  `BrCfgLookupIndex` (D3D
 * 0x10040040 == Glide 0x10039580) is a linear search of one of three tables
 * of `BrCfgRec { uint32_t key; char szText[32]; }`, and `BrUiText100400E0`
 * copies the matching szText into a menu item.  Both take a `BrCfgTables *`
 * that names the three tables, and NOBODY HAS EVER FILLED ONE -- the only
 * instance in the tree is a set of blank arrays in test_slice2_23.c.
 *
 * This module is the missing half: it owns the three tables and builds two of
 * them.  `BrCtlNameTables()` is the `BrCfgTables` the lookup wants.
 *
 * THE THREE TABLES, with both builds' addresses
 *
 *   kind 0     keyboard   120 records   Glide 0x100B3B40   D3D 0x100B4338
 *   kind 1,2   joystick   134 records   Glide 0x10B71C70   D3D 0x10B4E910
 *   kind 3     mouse       10 records   Glide 0x10B71B08   D3D 0x10B4E7A8
 *
 * Kinds 1 and 2 select the SAME table -- slice2_23.h records that too, and it
 * is why two joysticks share one name table.
 *
 * The keyboard table is static .data in the image and is transcribed below.
 * The other two are built at boot by Glide 0x10058AF0 (D3D 0x1005FDB0), the
 * third call state 0 makes before the state advances to 4.
 *
 * WHY THE EXTENTS ARE FACTS AND NOT ESTIMATES
 *
 * The three tables ABUT EXACTLY, and the last one abuts the next object:
 *
 *   0x10B71B08 + 10 * 0x24 == 0x10B71C70    mouse -> joystick
 *   0x10B71C70 + 134 * 0x24 == 0x10B72F48   joystick -> the config path
 *                                           RallyMain strcpy's at 0x1001CCC8
 *
 * and every one of the six loop bounds in 0x10058AF0 and 0x10039580 lands on
 * one of those three addresses.  Nothing here is inferred from a stride.
 *
 * WHAT 0x10058AF0 ACTUALLY BUILDS
 *
 * Both tables are "N buttons, then six axis directions", built out of seven
 * string resources.  The ids are literals in the code and the text is from
 * BRString.dll (see br_strres.h), so the names below are quoted from the
 * shipped resource rather than guessed:
 *
 *   0xC3  "BUTTON %d"   the only one with a format argument
 *   0xC4  "Left"        0xC5  "Right"      0xC6  "Forward"
 *   0xC7  "Backward"    0xC8  "Z Negative" 0xC9  "Z Positive"
 *
 *   mouse     records 0..3    key = 0..3       "BUTTON 0".."BUTTON 3"
 *             records 4..9    key = 0x86..0x8B the six axis names
 *   joystick  records 0..127  key = 0..127     "BUTTON 0".."BUTTON 127"
 *             records 128..133 key = 0x80..0x85 the six axis names
 *
 * THE AXIS CODE BASES DIFFER BY SIX AND THAT IS WHAT THE CODE SAYS.  The
 * joystick arm computes `key = (unsigned char)i` over i = 128..133; the mouse
 * arm computes `key = (unsigned char)(i - 0x7E)` over i = 4..9, which is
 * 0x86..0x8B, not 0x80..0x85.  Both then select the string with an UNSIGNED
 * `key - base <= 5`, each against its own base, so both land on 0xC4..0xC9
 * and nothing misbehaves.  It is recorded, and asserted by the test, because
 * "tidying" the two arms into one is the obvious change and it would silently
 * renumber every mouse axis binding a player has saved.
 *
 * THE TWO CLEARS ARE SHORTER THAN THE TABLES THEY CLEAR, also preserved.
 * 0x10058AF4 is `mov ecx,0x37 / rep stosd` -- 0x37 DWORDS, 220 bytes, of a
 * 360-byte table -- and 0x10058B02 is 0x56 DWORDS, 344 bytes, of a 4824-byte
 * one.  Every record's key and name are written by the loops regardless, so
 * the only bytes affected are the padding after each name's terminator; on a
 * cold boot the whole of .data is zero anyway and the clears change nothing.
 * The counts are DWORD counts: read as byte counts they would be 55 and 86,
 * which is how a buffer got sized four times too small in this tree before.
 *
 * A NULL STRING IS NOT GUARDED, because the original does not guard it.
 * BrStrGet returns NULL for an id that never loaded, and 0x10058AF0 hands
 * whatever it gets straight to sprintf as the FORMAT.  Boot order makes that
 * unreachable -- RallyMain runs 0x1006D1A0 long before state 0 runs this --
 * but a caller that skips br_strres.c will crash exactly where the original
 * would.  The sprintf is likewise unbounded into a 32-byte field, as it is in
 * the original; the shipped strings are at most ten characters.
 */
#ifndef BR_CTLNAME_H
#define BR_CTLNAME_H

#include <stdint.h>

#include "slice2_23.h"   /* BrCfgRec, BrCfgTables, BR_CFG_T*_COUNT.  The
                          * record type is REUSED, never redefined. */

/* The record counts are slice2_23.h's, restated here only as names for what
 * each one is.  BR_CFG_T0_COUNT == 120, T1 == 134, T3 == 10. */
#define BR_CTLNAME_KEY_COUNT    BR_CFG_T0_COUNT
#define BR_CTLNAME_JOY_COUNT    BR_CFG_T1_COUNT
#define BR_CTLNAME_MOUSE_COUNT  BR_CFG_T3_COUNT

/* Where each table stops being buttons and starts being axes: the loop
 * bounds at 0x10058B1B (0x10B71B9C) and 0x10058BB0 (0x10B72E74). */
#define BR_CTLNAME_MOUSE_BUTTONS  4
#define BR_CTLNAME_JOY_BUTTONS    128

/* The two axis code bases -- see the banner.  They are NOT the same number. */
#define BR_CTLNAME_MOUSE_AXIS_BASE  0x86
#define BR_CTLNAME_JOY_AXIS_BASE    0x80

/* The seven string ids, as literals in 0x10058AF0. */
#define BR_CTLNAME_STR_BUTTON     0xC3   /* "BUTTON %d" */
#define BR_CTLNAME_STR_AXIS_FIRST 0xC4   /* "Left", and the five after it */
#define BR_CTLNAME_STR_AXIS_COUNT 6

/* The two `rep stosd` byte counts, exactly as the original computes them.
 * Both are deliberately smaller than sizeof the table -- see the banner. */
#define BR_CTLNAME_MOUSE_CLEAR  (0x37 * 4)   /* 220 of 360   */
#define BR_CTLNAME_JOY_CLEAR    (0x56 * 4)   /* 344 of 4824  */

/* Glide 0x100B3B40 / D3D 0x100B4338 -- DirectInput scan-code names, static in
 * the image.  Keys ascend strictly from 0x01 (ESCAPE) to 0xDD (APP MENU). */
extern const BrCfgRec g_aBrCtlNameKey[BR_CTLNAME_KEY_COUNT];

/* Glide 0x10B71C70 / D3D 0x10B4E910 -- built by BrCtlNameInit. */
extern BrCfgRec g_aBrCtlNameJoy[BR_CTLNAME_JOY_COUNT];

/* Glide 0x10B71B08 / D3D 0x10B4E7A8 -- built by BrCtlNameInit. */
extern BrCfgRec g_aBrCtlNameMouse[BR_CTLNAME_MOUSE_COUNT];

/* Glide 0x10058AF0 / D3D 0x1005FDB0.  Builds the joystick and mouse tables
 * out of BrStrGet.  Requires the string table to be loaded; see the banner. */
void BrCtlNameInit(void);

/* Not in the original, which hardcodes the three addresses in 0x10039580's
 * jump table.  This is the same three tables as the argument slice2_23.c's
 * BrCfgLookupIndex and BrUiText100400E0 already take. */
const BrCfgTables *BrCtlNameTables(void);

#endif /* BR_CTLNAME_H */
