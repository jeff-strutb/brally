/* br_data.c -- REAL definitions for the game's cross-module data objects.
 *
 * WHAT THIS REPLACES
 *
 * port/host/br_stubs.c used to carry 64 "provisional" 1 MiB zeroed blocks --
 * one per `extern` array or object whose owning module is not ported yet.
 * Those blocks link, but they are silently wrong wherever the original's
 * definition carries a non-zero initialiser, because the port then reads 0.
 *
 * Every symbol below has been read back out of orig/BRD3D.dll and is recorded
 * here with its ORIGINAL ADDRESS, its RECOVERED SIZE, and -- the part that
 * matters -- whether the image actually holds initialised bytes for it.
 *
 * HOW ".data WITH INITIALISERS" IS TOLD FROM ".bss"
 *
 * BRD3D.dll's `.data` section is  rva 0x00094000  vsize 25,270,924  raw 185,344.
 * Only the first 185,344 bytes exist in the file; everything from
 *   0x10094000 + 0x2D420 == 0x100C1420
 * up to 0x118ADFCC is section slack the loader zero-fills -- the linker's .bss.
 * So the test is purely arithmetic:
 *
 *     VA <  0x100C1420   ->  the image holds its bytes; read them (done below)
 *     VA >= 0x100C1420   ->  genuinely zero at process start, in the ORIGINAL
 *                            too.  Zero here is CORRECT, not a placeholder.
 *
 * That second case is a real result, not a gap: it converts a "we do not know
 * what this started as" hazard into "we do know, and it started as zero".
 * Each such symbol is marked `.bss (VERIFIED zero)` below.
 *
 * SIZES ARE EVIDENCE-BASED, NOT ROUND NUMBERS
 *
 * Where the size is pinned by something checkable -- a NULL terminator that is
 * NOT in the relocation table, the address of the next referenced global, or
 * the stride the indexing arithmetic uses -- the evidence is written at the
 * definition.  Where it is not pinned, the definition says so in as many words
 * rather than implying a precision that was never established.
 */

#include <stddef.h>
#include <stdint.h>

#include "br_vec.h"
#include "slice1_02.h"    /* BrCarState (40 floats == the original's 0xA0)   */
#include "slice2_11.h"    /* CD / net / camera globals, BrCollPlane          */
#include "slice2_20.h"    /* BrPoolNode                                      */
#include "slice2_24.h"    /* g_pBrMenuACED34                                 */
#include "slice2_25.h"    /* BrRec2A8, the 0x100AC3xx option tables          */
#include "slice5_61.h"    /* g_aBr0B3820, g_br0AB3F4, g_brPAA29D0, ...       */

/* ==========================================================================
 * PART 1 -- .data.  These carry initialisers, and the initialisers matter.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * 0x100AC300  int, == 1
 *
 * slice2_20.c gates on `g_i0AC300 == 0` and slice6_71.c on it being non-zero.
 * The provisional block made it 0, which is the OPPOSITE of the shipped state
 * at both sites.  This is the clearest example of why a zeroed placeholder is
 * not a neutral choice.
 * -------------------------------------------------------------------------- */
int g_i0AC300 = 1;

/* --------------------------------------------------------------------------
 * 0x100AC308 .. 0x100AC54C -- fifteen contiguous option tables.
 *
 * One run of int32 in the image; the fifteen names below are the fifteen
 * offsets the code takes the address of.  Extents are exactly those already
 * documented in slice2_25.h (recovered independently from the indexing), and
 * the recovered VALUES agree with that reading everywhere: each table either
 * ends on a 0 sentinel or butts against the next named base.
 *
 * The two dwords at 0x100AC3A8 (117, 118) and the eight at 0x100AC400
 * (115,116 repeated four times) sit inside this run but are not the base of
 * any referenced table; they are left out deliberately rather than folded
 * into a neighbour to make the arithmetic tidy.
 * -------------------------------------------------------------------------- */

/* 0x100AC308, 24 x int32 -- string ids, four 0-terminated groups.
 * Indexed by the table VALUE (see the note in slice2_25.c), not by an
 * option index, which is why it is longer than its selector. */
const int32_t g_aBrAC308[24] = {
    119, 120, 121, 122, 123, 124,
    119, 120, 121, 122, 123, 124, 0,
    125, 125, 0,
    126, 127, 128, 0,
    129, 127, 130, 0
};

/* 0x100AC368, 16 x int32 -- string ids 141..156, index = track & 0xF.
 * Dense and unterminated: the run continues into unrelated dwords at
 * 0x100AC3A8, so the mask IS the bound. */
const int32_t g_aBrAC368[16] = {
    141, 142, 143, 144, 145, 146, 147, 148,
    149, 150, 151, 152, 153, 154, 155, 156
};

/* 0x100AC3B0, 6 x int32 -- string ids 131..135 then the 0 sentinel. */
const int32_t g_aBrAC3B0[6] = { 131, 132, 133, 134, 135, 0 };

/* 0x100AC3C8, 6 x int32 -- string ids 157..161 then the 0 sentinel. */
const int32_t g_aBrAC3C8[6] = { 157, 158, 159, 160, 161, 0 };

/* 0x100AC420, 32 x int32 -- the identity map 0..31.  It is indexed by a
 * cycler whose bound is BR_OPT_TRACK_MAX (0x1F), so all 32 are live.
 * An identity table is not a no-op here: the port read 0 for every index. */
const int32_t g_aBrAC420[32] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

const int32_t g_aBrAC4A0[4]  = { 0, 1, 2, 0 };            /* 0x100AC4A0 */
const int32_t g_aBrAC4B0[4]  = { 0, 1, 2, 0 };            /* 0x100AC4B0 */
const int32_t g_aBrAC4C0[6]  = { 0, 1, 2, 3, 4, 0 };      /* 0x100AC4C0 */

/* 0x100AC4D8, 16 x int32 -- 0..14 then the 0 sentinel.  Fifteen live values,
 * and g_apszTrackFiles below holds exactly fifteen names.  That agreement is
 * how both extents were confirmed. */
const int32_t g_aBrAC4D8[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0
};

/* Five two-entry toggles.  Index 0 yields 1 and index 1 yields 0 -- i.e. the
 * FIRST menu position is the enabled one.  A zeroed placeholder makes both
 * positions read "off", which is exactly the failure this file exists to
 * remove. */
const int32_t g_aBrAC518[2] = { 1, 0 };                   /* 0x100AC518 */
const int32_t g_aBrAC530[2] = { 1, 0 };                   /* 0x100AC530 */
const int32_t g_aBrAC538[2] = { 1, 0 };                   /* 0x100AC538 */
const int32_t g_aBrAC540[2] = { 1, 0 };                   /* 0x100AC540 */
const int32_t g_aBrAC548[2] = { 1, 0 };                   /* 0x100AC548 */

/* 0x100AC520, 4 x int32 -- identity 0..3, indexed by a cycler bounded at
 * BR_OPT_AA2A0C_MAX (3).  slice6_72.h carries `BR72_AC520_MAX 8` for the same
 * address; at 8 the read would run off this table into g_aBrAC530.  The image
 * gives four entries before the next referenced base, so 4 is what is defined
 * and the 8 is flagged rather than accommodated. */
const int32_t g_aBrAC520[4] = { 0, 1, 2, 3 };

/* --------------------------------------------------------------------------
 * 0x100B3820, 256 bytes -- the stage/session byte-pair table.
 *
 * Verbatim from the image.  Only the first four 2-byte pairs are plausible
 * table entries; from 0x100B3828 the region is 0xE0/0xE1/0xE2/0xE3/0xE4
 * records followed by a run of floats just under 1.0.  slice5_61.h reached
 * the same conclusion from the indexing and says so.
 *
 * ALIAS NOTE: slice5_62.c holds a byte-identical private copy of this same
 * 256 bytes as `g_brB3820`.  Both are const and neither is ever written, so
 * the two cannot drift; it is duplication, not the aliasing bug.  Recorded so
 * the next person does not have to re-establish that.
 * -------------------------------------------------------------------------- */
const uint8_t g_aBr0B3820[256] = {
    0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x07, 0x50, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x04, 0x02, 0x00, 0x00, 0xE1, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x04, 0x06, 0x0C, 0x00,
    0x04, 0x01, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, 0xE2, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x02, 0x20, 0x00,
    0x04, 0x03, 0x01, 0x01, 0x02, 0x04, 0x00, 0x02, 0xE3, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x05, 0x20, 0x00,
    0x02, 0x03, 0x00, 0x04, 0x0A, 0x03, 0x01, 0x02, 0xE4, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x05, 0x05, 0x20, 0x00,
    0x06, 0x01, 0x02, 0x02, 0x01, 0x04, 0x0A, 0x02, 0x17, 0xD9, 0x7E, 0x3F,
    0xEE, 0x7C, 0x7F, 0x3F, 0x17, 0xD9, 0x7E, 0x3F, 0xEE, 0x7C, 0x7F, 0x3F,
    0x17, 0xD9, 0x7E, 0x3F, 0xEE, 0x7C, 0x7F, 0x3F, 0x17, 0xD9, 0x7E, 0x3F,
    0xEE, 0x7C, 0x7F, 0x3F, 0x49, 0x2E, 0x7F, 0x3F, 0x84, 0x0D, 0x7F, 0x3F,
    0xA0, 0x1A, 0x7F, 0x3F, 0x49, 0x2E, 0x7F, 0x3F, 0xA0, 0x1A, 0x7F, 0x3F,
    0x17, 0xD9, 0x7E, 0x3F, 0x17, 0xD9, 0x7E, 0x3F, 0xB2, 0x9D, 0x7F, 0x3F,
    0x52, 0xB8, 0x7E, 0x3F, 0x9B, 0x55, 0x7F, 0x3F, 0xE0, 0xBE, 0x7E, 0x3F,
    0x44, 0x69, 0x7F, 0x3F, 0x04, 0x56, 0x7E, 0x3F, 0x64, 0x3B, 0x7F, 0x3F,
    0x04, 0x56, 0x7E, 0x3F, 0xD7, 0x34, 0x7F, 0x3F, 0x3F, 0x35, 0x7E, 0x3F,
    0x89, 0xD2, 0x7E, 0x3F, 0xC9, 0x76, 0x7E, 0x3F, 0xBB, 0x27, 0x7F, 0x3F,
    0x52, 0xB8, 0x7E, 0x3F, 0xD2, 0x6F, 0x7F, 0x3F, 0xAD, 0x69, 0x7E, 0x3F,
    0x64, 0x3B, 0x7F, 0x3F
};

/* --------------------------------------------------------------------------
 * 0x100B84F8, 16 pointers -- the car file stems.
 *
 * BOUNDED, not guessed: the sixteenth entry is the last relocated dword in
 * the run (0x100B8538, the next dword, is NOT in the base-relocation table
 * and holds the literal "sfx/").  Sixteen also matches the sixteen race
 * entrants at 0x10ACDEA8.
 *
 * The image stores these in DESCENDING target order -- entry 0 points at the
 * highest string.  Reconstructing the list from the string block's address
 * order would silently reverse it.
 * -------------------------------------------------------------------------- */
const char *const g_apszCarFiles[16] = {
    "ce", "es", "ns", "rs", "sp", "ps", "m3", "ip",
    "ld", "hm", "mt", "cu", "bb", "pj", "tr", "mn"
};

/* --------------------------------------------------------------------------
 * 0x100B80B8, 15 pointers + a NULL -- the track files.
 *
 * The trailing dword at 0x100B80F4 is zero AND absent from the relocation
 * table, so it is a real terminator rather than a pointer the fixup missed.
 *
 * Entries 6..11 repeat entries 0..5 verbatim: the mirror variants load the
 * SAME .trk file and are distinguished only by the flag byte in
 * g_aBrBD2A8 below.  That repetition is the data, not a transcription slip.
 * -------------------------------------------------------------------------- */
const char *const g_apszTrackFiles[16] = {
    "desert.trk", "mountain.trk", "coast.trk",
    "mine.trk",   "amazon.trk",   "race.trk",
    "desert.trk", "mountain.trk", "coast.trk",   /* mirror 0..5 */
    "mine.trk",   "amazon.trk",   "race.trk",
    "gamewin.trk", "bonus.trk",   "bonus.trk",
    NULL
};

/* --------------------------------------------------------------------------
 * 0x100BD2A8, 16 pointers + a NULL -> records of stride 0x17C at 0x100BBAE8.
 *
 * Count pinned three ways: the dword after the sixteenth entry is a
 * non-relocated 0; the sixteenth record's own base (0x100BD12C + 0x17C) is
 * 0x100BD2A8, i.e. the table starts immediately after the last record; and
 * g_aBrAC4D8 offers exactly fifteen selectable indices with a sixteenth
 * reserved row.
 *
 * FINDING WORTH RECORDING: the record NAMES recovered from +0x00 are TRACK
 * names -- "Desert", "Mirror Mountain", "Bonus Track", "Reset To First Year"
 * -- and they line up index-for-index with g_apszTrackFiles.  slice2_25.c
 * reaches this table down the path it labels BR_OPT_STR_CAR (0x10042EE0).
 * On this evidence that selector is the TRACK selector and the name is wrong.
 * Flagged, not renamed: renaming belongs to the naming registry, not here.
 *
 * The only field the port reads is +0x04 bit 0x10, which appends the "locked"
 * suffix.  It is set on the six mirror tracks and on Mirror Bonus Track --
 * seven of sixteen.  With the provisional zeroed block every row read as
 * unlocked, so the distinction the byte exists to make disappeared entirely.
 *
 * DEVIATION: +0x00 is a `char *` in the original, into the string pool at
 * 0x100BD2EC.  slice2_25.h models it as int32_t, and a 32-bit VA is not a
 * host pointer, so it is left 0 here and the recovered name is written in the
 * comment instead of being smuggled in as an integer nobody can dereference.
 * The remaining 0x174 bytes of each record are outside slice2_25.h's model.
 * -------------------------------------------------------------------------- */
static const BrRec2A8 s_aBrBBAE8[16] = {
    { 0, 0x00, 0, 0, 0 },   /*  0  0x100BBAE8  "Desert"              */
    { 0, 0x00, 0, 0, 0 },   /*  1  0x100BBC64  "Mountain"            */
    { 0, 0x00, 0, 0, 0 },   /*  2  0x100BBDE0  "Coastline"           */
    { 0, 0x00, 0, 0, 0 },   /*  3  0x100BBF5C  "Strip Mine"          */
    { 0, 0x00, 0, 0, 0 },   /*  4  0x100BC0D8  "Jungle"              */
    { 0, 0x00, 0, 0, 0 },   /*  5  0x100BC254  "Race Track"          */
    { 0, 0x10, 0, 0, 0 },   /*  6  0x100BC3D0  "Mirror Desert"       */
    { 0, 0x10, 0, 0, 0 },   /*  7  0x100BC54C  "Mirror Mountain"     */
    { 0, 0x10, 0, 0, 0 },   /*  8  0x100BC6C8  "Mirror Coastline"    */
    { 0, 0x10, 0, 0, 0 },   /*  9  0x100BC844  "Mirror Strip Mine"   */
    { 0, 0x10, 0, 0, 0 },   /* 10  0x100BC9C0  "Mirror Jungle"       */
    { 0, 0x10, 0, 0, 0 },   /* 11  0x100BCB3C  "Mirror Race Track"   */
    { 0, 0x02, 0, 0, 0 },   /* 12  0x100BCCB8  "Load and Gamewin"    */
    { 0, 0x10, 0, 0, 0 },   /* 13  0x100BCE34  "Mirror Bonus Track"  */
    { 0, 0x00, 0, 0, 0 },   /* 14  0x100BCFB0  "Bonus Track"         */
    { 0, 0x02, 0, 0, 0 }    /* 15  0x100BD12C  "Reset To First Year" */
};

const BrRec2A8 *const g_aBrBD2A8[17] = {
    &s_aBrBBAE8[ 0], &s_aBrBBAE8[ 1], &s_aBrBBAE8[ 2], &s_aBrBBAE8[ 3],
    &s_aBrBBAE8[ 4], &s_aBrBBAE8[ 5], &s_aBrBBAE8[ 6], &s_aBrBBAE8[ 7],
    &s_aBrBBAE8[ 8], &s_aBrBBAE8[ 9], &s_aBrBBAE8[10], &s_aBrBBAE8[11],
    &s_aBrBBAE8[12], &s_aBrBBAE8[13], &s_aBrBBAE8[14], &s_aBrBBAE8[15],
    NULL
};

/* --------------------------------------------------------------------------
 * 0x100A73C4 -- "%d"
 *
 * NOT a pointer in the image: the dword at 0x100A73C4 is absent from the
 * base-relocation table and its bytes are `25 64 00 00`, i.e. the literal
 * two-character format string.  slice5_63.h and slice6_70.h both model it as
 * `const char *`, which is a fair model of "the address of that string" -- but
 * the provisional block made it a NULL pointer, and every use is
 * `BrSprintf(dst, g_pszBr0A73C4, n)`.  Passing NULL as a printf format is
 * undefined behaviour and crashes here, so this was not merely a wrong value.
 * -------------------------------------------------------------------------- */
const char *g_pszBr0A73C4 = "%d";

/* --------------------------------------------------------------------------
 * Scalars, all read straight out of the image.  Each of these is a case where
 * "zero" and "what shipped" pick different branches.
 * -------------------------------------------------------------------------- */

/* 0x100AB3F4 == -1.  Documented by slice3_31.h/slice6_72.h as "set to -1 by
 * every name reset"; it is ALSO -1 before anything runs, and slice5_61 uses it
 * as an unchecked index into the 0x10AA29D0 record array.  0 would be a valid
 * record; -1 is the no-selection state. */
int32_t g_br0AB3F4 = -1;

/* 0x100AA010 == 1.  Compared against literal 5 (short-circuits three camera
 * routines) and against 0 in slice5_63/slice2_24.  Ships as 1, so the
 * `== 0` arms were being taken here and are not taken in the original. */
int g_brMode0AA010 = 1;

/* 0x100AA8B4 == 1.  slice2_11.c: `(g_brMode0AA8B4 == 1) ? -11.0f : -19.8f`.
 * The shipped value selects -11.0f; the provisional 0 selected -19.8f, so the
 * camera sat 8.8 units further back on frame one for no reason at all.
 * (The neighbouring dword 0x100AA8B0 is the float 400.0f -- the pan/volume
 * neutral point -- which is what you hit if the address is read four bytes
 * low.) */
int g_brMode0AA8B4 = 1;

/* 0x100940A4 == 2.  This is the compiled-in `PlayMusic=` default, and 2 is
 * the EAR driver rather than Redbook CD (README records the same conclusion
 * from the dispatcher at 0x100027C0).  slice2_11.c gates all three CD track
 * routines on `g_brCdEnabled != 0`, so 0 disabled music outright. */
int g_brCdEnabled = 2;

/* 0x10094298 == -1.  slice2_11.c does `n = g_brNetSendCount + 1;` and then
 * sends a full packet on `n % 4 == 0`.  Starting at -1 makes the first tick
 * n == 0 and therefore a FULL packet; starting at 0 makes it n == 1 and the
 * first full packet slips to the fourth tick. */
int g_brNetSendCount = -1;

/* 0x100B8C90 == 1.  Saved and restored around the non-preview .rca load in
 * slice2_20.c, which only forces it to 1 when it reads as 0. */
int g_i0B8C90 = 1;

/* 0x100C129C == 0.  In the image's initialised range, and the bytes there
 * really are zero -- so this one is a placeholder that happened to be right.
 * Stated explicitly because "0 because nobody checked" and "0 because the
 * image says 0" are indistinguishable at the C level. */
int g_brCamCollided = 0;

/* ==========================================================================
 * PART 2 -- .bss.  Zero here is the ORIGINAL's value, not a stand-in.
 *
 * Everything below sits at or above 0x100C1420, past the end of .data's raw
 * bytes, so the PE loader zero-fills it.  These were the symbols most at risk
 * of being "fixed" with invented initialisers; they need none.
 * ========================================================================== */

/* 0x10A9D078 -- name scratch, strcpy'd into by slice5_61/slice6_73.
 * .bss (VERIFIED zero).  SIZE NOT PINNED: the only spacing evidence is the
 * 0x60 gap to 0x10A9D018, and the next global anyone references is
 * 0x10A9D5C0 (0x548 further on).  BR63_TEXT_MAX (0x104) is used here because
 * that is the size slice5_63 gives the other name buffers; it is inside the
 * 0x548 bound but is a convention, not a measurement. */
char g_aBrA9D078[0x104];

/* 0x10B4FBE8 -- passed BY ADDRESS to 0x1006A4A0 (still a stub).
 * .bss (VERIFIED zero).  SIZE NOT PINNED: it is the element just past
 * slice2_23.h's 134-entry table (0x10B4E910..0x10B4FBE8, stride 0x24), and
 * the next referenced global is 0x10B4FFE8, so it is at most 0x400 bytes.
 * That upper bound is what is allocated -- over-allocating a .bss block is
 * safe here, under-allocating would not be. */
unsigned char g_aBrB4FBE8[0x400];

/* 0x10A99BB8 -- 256 pool nodes of 0x20 bytes.
 * .bss (VERIFIED zero).  COUNT PINNED: the next referenced global is
 * 0x10A9BBB8, exactly 0x2000 bytes on, and 0x2000 / 0x20 == 256 -- which is
 * also br_pool.h's BR_POOL_SLOTS_USED.  BrPoolNode is still 32 bytes on LP64
 * (six floats, one float, u16, u8, u8), so the stride survives the host
 * change; that was checked rather than assumed. */
BrPoolNode g_aPoolNodes[256];

/* 0x10A99BA8 / 0x10A99BB0 -- the free-list head and the emit-list head.
 * .bss (VERIFIED zero).  Both are u16 in the image (globals.csv agrees) and
 * both start 0, so slot 0 is the first node handed out. */
uint16_t g_uPoolFree;
uint16_t g_uPoolHead;

/* 0x10220E60, 7 floats -- per-field network peak hold.  .bss (VERIFIED zero),
 * and zero is load-bearing: slice2_11.c resets the whole array to 0.0f every
 * third tick, so the boot state and the steady state agree. */
float g_abrNetPeak[7];

/* 0x10220CF0 -- BrCarState, the last full network snapshot.
 * .bss (VERIFIED zero).  See the ALIAS note in the header changes: the float
 * the port used to call g_brNet220D68 is this record's f78. */
BrCarState g_brNetLastFull;

/* 0x1022AF40 -- keep-alive tick counter.  .bss (VERIFIED zero). */
int g_brNetTickCount;

/* CD bookkeeping, all .bss (VERIFIED zero).  Note the asymmetry with
 * g_brCdEnabled above, which is in .data and ships as 2: the ENABLE flag is
 * compiled in, the track cursors are not. */
int g_brCdPlaying;      /* 0x10220CD0 */
int g_brCdTrackCur;     /* 0x10220CD4 */
int g_brCdTrackFirst;   /* 0x10220C44 */
int g_brCdTrackLast;    /* 0x10220C38 */

/* 0x106909E0 -- camera mode flag.  .bss (VERIFIED zero). */
int g_brFlag6909E0;

/* Scalars in .bss (VERIFIED zero), each declared by slice2_20.c's XSLICE
 * block.  Re-declared here to match, since no header exposes them. */
/* XSLICE 0x106C2CFC */ float g_f6C2CFC;
/* XSLICE 0x10AA3444 */ int   g_i10AA3444;
/* XSLICE 0x10AA3460 */ int   g_i10AA3460;
/* XSLICE 0x104BBE08 */ int   g_i4BBE08;
/* XSLICE 0x106C661C */ int   g_i6C661C;
/* XSLICE 0x106C6624 */ int   g_i6C6624;
/* XSLICE 0x106C7C3C */ void *g_p6C7C3C;

/* 0x10AA26F4 / 0x10AA26F5 -- see the ALIAS resolution in slice5_61.h.  They
 * are byte 0 and byte 1 of ONE dword that slice5_63.c already owns as
 * g_aBrAA26F4[4], so nothing is defined here.  .bss (VERIFIED zero). */

/* 0x10AA29D0 -- pointer to the record array slice5_61/slice6_73 index.
 * .bss (VERIFIED zero), i.e. NULL, and slice6_73.c's `== NULL` guard is
 * therefore reachable and correct at boot.
 *
 * The stub generator emitted this one as a FUNCTION (`long g_brPAA29D0(void)`)
 * rather than as data.  A function symbol is never NULL, so the guard could
 * not fire and the code went on to dereference the stub's own machine code as
 * a record array.  That is a live crash, not a wrong initial value. */
unsigned char *g_brPAA29D0;

/* 0x10ACED34 -- the record BrMenuAutoSaveName clears.  .bss (VERIFIED zero);
 * slice2_24.h's `0x10ACED34 != 0` test is a genuine "not loaded yet" check. */
uint8_t *g_pBrMenuACED34;

/* Track-geometry pointers, all filled in by the (unported) track loader.
 * .bss (VERIFIED zero) -- NULL at boot is the original's state too.
 * Declared in slice6_73.h; repeated here rather than including that header,
 * which is being restructured for the page/control models. */
/* XSLICE 0x106C7C54 */ const uint16_t *g_pBrCollTriIdx;
/* XSLICE 0x106C7C5C */ BrVec3         *g_pBrCollVerts;
/* XSLICE 0x106C7CDC */ const uint8_t  *g_pBrCollTriFlags;

/* 0x106C7CB0 -- u16 payload table for BrU16QueuePop.  .bss (VERIFIED zero). */
const uint16_t *g_pBrU16QueueTable;

/* Renderer entry points, .bss (VERIFIED zero).  These are function pointers
 * the platform layer installs at init; NULL at boot is correct, and calling
 * one before init crashes in the original too. */
uint32_t (*g_pfn18AA084)(uint32_t hCtx, uint32_t hSrc, void *pDesc); /* 0x118AA084 */
void     (*g_pfn18AA0C4)(void *pv);                                  /* 0x118AA0C4 */
void     (*g_pfn18AA0C8)(void *pRec, int flag);                      /* 0x118AA0C8 */
void     (*g_pfn18AA0CC)(void *pTable, int cRecords);                /* 0x118AA0CC */

/* --------------------------------------------------------------------------
 * 0x11750338 / 0x117554A0 -- the collision grid base and its count array.
 *
 * .bss in the image, but NULL here is NOT equivalent to the original, and
 * this is the one symbol pair in this file that is knowingly left wrong.
 *
 * In BRD3D these are not pointers at all: the addresses are baked into the
 * addressing arithmetic (`cell * 4800 + 0x11750338`, count `<< 5`), so the
 * arrays are always present.  slice2_11.h turned them into extern pointers so
 * the module could link and be tested -- a representation-only DEVIATION,
 * documented there.
 *
 * They cannot simply be pointed at storage, because the SAME bytes are
 * already owned by slice3_42.c as `BrFxRecord g_BrFx1750338[600]` (0x20 each,
 * 600 == 4 cells x 150 planes -- the two views agree on the shape, which is
 * how the alias was spotted).  On a 32-bit host they would be one object.  On
 * THIS host they cannot be: BrFxRecord is still 32 bytes, but BrCollPlane
 * holds three BrVec3 POINTERS and so widens under LP64.  Giving the collision
 * view its own array would create exactly the two-objects-one-address bug
 * CONVENTIONS.md warns about, and would do it silently.
 *
 * So they stay NULL, which every consumer already guards (`if (g_pBrCollGrid
 * == NULL) return;`).  The fix is to make BrCollPlane store vertex INDICES
 * instead of pointers so it is 32 bytes again and can alias g_BrFx1750338 --
 * a change to slice2_11/slice6_73's types, not to this file.
 * -------------------------------------------------------------------------- */
BrCollPlane    *g_pBrCollGrid;
const uint16_t *g_pBrCollGridCount;
