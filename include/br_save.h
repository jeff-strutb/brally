/* br_save.h -- the ".BRF" championship-season save file.
 *
 * WHAT THIS MODULE IS
 * -------------------
 * The FORMAT, read off the two functions that define it, plus the half of the
 * reader that is portable.  Nothing here is inferred from a file image -- there
 * is no retail .BRF to infer from (the game writes saves into its own install
 * directory, and none shipped on the disc).
 *
 *   writer   0x100709A0 (D3D)  /  0x10069930 (Glide)
 *   reader   0x10070610 (D3D)  /  0x100695C0 (Glide)
 *
 * The writer is ALREADY PORTED, as slice4_52.c's `BrMenuSub100709A0`, and this
 * module does not duplicate it: BR_SEASON_BLOCK_SIZE and BR_SEASON_TAIL_SIZE
 * are reused from slice4_52.h rather than renamed here, per CONVENTIONS.md
 * ("reuse, never coin a fifth name").  `BrBrfEncode` below is the byte LAYOUT
 * factored out so the decoder can be checked against it; test_br_save.c also
 * checks it byte-for-byte against what the real 0x100709A0 transcription
 * emits, so the two cannot drift apart silently.
 *
 * The reader is NOT ported.  port/host/br_stubs.c still carries
 * `BrSub10070610` (declared in slice5_60.h as `int32_t (int32_t mode,
 * int32_t arg)`).  That function is half file parsing and half installing the
 * result into globals five other slices own; only the file half is portable,
 * and it is `BrBrfReadFile` / `BrBrfDecode` here.  INTEGRATION: when the
 * install half lands, point BrSub10070610 at these and delete the stub line.
 * This header deliberately does NOT define BrSub10070610, because br_stubs.c
 * is generated, is off-limits to this pass, and the host link would then have
 * two definitions.
 *
 * THE FILE
 * --------
 * Fixed layout, 0x29C == 668 bytes.  Every offset below is stated by BOTH the
 * writer and the reader; where they disagree it is called out.
 *
 *   0x000   4      magic "RSea".  The writer copies it out of the WRITABLE
 *                  global 0x100B5D94 (Glide 0x100B559C), not a literal;
 *                  nothing in either binary writes that global.  The reader
 *                  accepts on `strncmp(buf, "RSea", 4) == 0`, so a fifth byte
 *                  is never involved and no NUL is written.
 *   0x004   4      adler32 of the 0x200 payload ONLY, little-endian.
 *                  Seeded the way the original asks for a seed: the value of
 *                  `adler32(0, NULL, 0)`, which is 1.  Nothing else in the
 *                  file is protected, and the sum PRECEDES the data it covers.
 *   0x008   0x200  the season payload -- *0x10ACED34 (Glide *0x10AF2094).
 *   0x208   4  |   0x10AA2A08   (Glide 0x10AC5D60)
 *   0x20C   4  |   0x100AC64C   (Glide 0x100ABDEC)
 *   0x210   4  |-- five loose option dwords, in this order
 *   0x214   4  |   0x100AC654   (Glide 0x100ABDF4)   -- 0x100AC650 at 0x210
 *   0x218   4  |   0x100AC65C   (Glide 0x100ABDFC)
 *   0x21C   0x80   the save's DISPLAY NAME.  0x10AD0990 (Glide 0x10AF3CF0).
 *
 * The five dwords are five ADJACENT option indices with one deliberately
 * missing: 0x100AC658 sits between 0x100AC654 and 0x100AC65C and is NOT
 * saved.  slice2_25.h names their bounds -- 0x10AA2A08 0..1, 0x100AC64C 0..2,
 * 0x100AC650 0..2, 0x100AC654 the track 0..0x1F, 0x100AC65C 0..7.  The reader
 * stages them at 0x10AD0978..0x10AD0988, which slice2_25.c already reads back
 * as g_brAD0978..g_brAD0988; the ghost (".GRF", magic "RGho") reader fills a
 * SIXTH at 0x10AD098C, which is why that staging block has one more slot than
 * a season file has dwords.
 *
 * THE TAIL IS READ FROM THE END, NOT SEQUENTIALLY.  After validating the
 * payload the reader does
 *     fseek(END); n = ftell(); fseek(n - 0x94); read 5 dwords;
 *     fseek(END); n = ftell(); fseek(n - 0x80); read 0x80 bytes;
 * so 0x94 == 0x14 + 0x80 is the reader's own statement that the option dwords
 * and the name are the last 0x94 bytes of the file, whatever its length.
 * BrBrfReadFile reproduces that; BrBrfDecode, which works on a whole image,
 * does the same arithmetic from cbImage.
 *
 * ASYMMETRIC ERROR CHECKING, both ways round, both reproduced:
 *   - the writer checks the magic, the checksum, the payload and the name
 *     against their byte counts, and does NOT check the five dwords;
 *   - the reader checks the magic, the checksum field and the payload, and
 *     does NOT check either tail read.
 *
 * WHAT IS IN THE PAYLOAD
 * ----------------------
 * The save code treats the 0x200 block as opaque -- it fwrite()s it and
 * memcpy()s it and never looks inside -- so the layout below comes from its
 * CONSUMERS, and it is partial.  Offsets are within the payload.
 *
 *   +0x00  flags byte.  Bit 0 is tested at 0x1002F488 and rotates the stage
 *          index by +/-6 within 0..11 -- the mirrored half of the track set.
 *          (0x100BD2A8's record 13 is the one test_data.c calls Mirror Bonus.)
 *   +0x04  stage group, +0x05 event within it.  0x1002F460 computes
 *          `12 * blk[4] + blk[5]` and uses it to index the two-byte stage
 *          table at 0x100B3820: the low byte becomes 0x100B380C, which is the
 *          index into the 16 track records at 0x100BD2A8, and the high byte
 *          (0x100B3821) becomes 0x104BBE08.  Twelve events per group is the
 *          same 12 as slice2_25.h's BR_OPT_BD3E0_MAX.
 *   +0x06  6 dwords  } the three progress runs the autosave path clears, and
 *   +0x1E  12 dwords } clears ONLY when blk[4] and blk[5] are both zero --
 *   +0x50  24 dwords } i.e. only for a season that has not started.  12 is one
 *          per event in a group and 24 is twelve pairs; the 6 is not pinned.
 *   +0xF0, +0xF2, +0xF4  three 16-bit fields.  The ghost writer 0x10069840
 *          assigns them and the ghost reader 0x10070AF0 ORs into +0xF0/+0xF2,
 *          so they are bit sets, not counters.
 *   +0xF8..+0x108  five dwords, the car-equipment set.  0x10071130 mode 2
 *          (RallyConfig.dat) writes them, and the season reader's non-4 arm
 *          READS THEM BACK OUT of slot `index ^ 1` before the overwrite and
 *          puts them back afterwards -- i.e. the other player's equipment
 *          survives a load.  slice5_60.h already names the offset
 *          BR_CAR_EQUIP_OFF and the count BR_CAR_EQUIP_COUNT.
 *
 * The block is one of a PAIR.  0x10ACED34 and 0x10AD189C are 0x2B68 apart --
 * the entity/car stride -- and hold pointers to player 0's and player 1's
 * blocks; 0x1003E1D0 (slice1_06.c's BrPairBufReset) binds a null pointer to a
 * static and zeroes both.  Note the size disagreement, which is in the
 * original and not a modelling slip: everything that copies BETWEEN the two
 * moves 0x53 dwords (332 bytes), and the file carries 0x200 (512).  When the
 * pointers are on their statics those statics are adjacent -- 0x10AF9890 +
 * 332 == 0x10AF99DC -- so the writer's 512-byte fwrite reads 180 bytes of
 * player 1's buffer, and the reader installs the same 332 bytes into both.
 * A save taken in that state does not round-trip in full.
 *
 * WHO WRITES ONE
 * --------------
 *   0x10041B50 (Glide 0x1003B0B0) -- "AutoSave.brf".  Sets the filename buffer
 *       at 0x11782CD0 (Glide 0x117A6030), clears three runs inside the payload
 *       when payload[4] and payload[5] are both zero, then calls the writer.
 *       Already ported as slice2_24.c's BrMenuAutoSaveName.
 *   0x10041DF0 (Glide 0x1003B350) -- the named save.  Builds
 *       "RallySeason" + itoa(slot) + ".brf" into the same buffer, copies the
 *       typed name into both the on-screen name list slot and 0x10AD0990, then
 *       calls the writer.  NOT ported.
 *
 * INTEGRATION HAZARD (an aliased-storage instance of exactly the kind
 * CONVENTIONS.md describes): 0x11782CD0 is the mutable 0x104-byte FILENAME
 * BUFFER, whose initial contents happen to be "c:\RallySeason.dat".  The port
 * models it twice -- slice1_06.c as `const char *const g_pszBrRallySeasonDat`,
 * slice2_24.c as `BrMenuState::g1782CD0`, which BrMenuAutoSaveName writes
 * "AutoSave.brf" into.  slice4_52.c's writer opens the FORMER, so today the
 * ported autosave path renames a buffer nobody reads and writes the file under
 * the wrong name.  Both save screens are affected.  Not fixed here: the fix is
 * to merge the two models, not to add a third.
 *
 * The ghost path has the identical split one address along: 0x11782BC8 is
 * `const char *const g_pszBrRallyGhostDat` in slice1_06.c and
 * `char g_aBr1782BC8[0x104]` in slice2_25.c, and slice2_25.c's restore
 * strcpy()s "TimeAttack<n>.grf" into the array.  Two instances, one cause --
 * slice1_06.h describes both addresses correctly as "fixed buffers" and then
 * declares them as pointers to string literals.  (slice2_24.h also sizes its
 * copy [64] where slice2_25.h uses 0x104; the true size is not established by
 * either, since nothing bounds the writes.)
 *
 * THE FILE LIST BEHIND THE LOAD SCREEN
 * ------------------------------------
 * slice6_71.c's 0x1004F700 calls slot +0x04 of the 100-slot name list at the
 * phase's +0xC0 with the mask "RallySeason*.BRF".  That slot is 0x1005CF20:
 * a _findfirst/_findnext walk, capped at 100 entries, which for each match
 * opens the file, seeks to END-0x80 and reads 0x80 bytes -- the display name,
 * and nothing else; it never looks at the magic or the checksum -- then hands
 * (filename, name) to slot +0x18, 0x1005CE30.  That one takes the digits after
 * the prefix, atoi()s them, and stores the name in list slot [n].  The prefix
 * is "RallySeason" when 0x10AA2848 is set and "TimeAttack" when it is not,
 * which is the same flag slice6_71.c raises around the rescan and is why the
 * two lists (+0xC0 season, +0xC4 time attack) do not collide.
 * BrBrfReadName and BrBrfSlotIndex below are those two steps.
 *
 * PORTABILITY
 * -----------
 * Nothing here overlays a struct on a file image.  BrBrfSeason is a HOST
 * struct; every integer crossing the file boundary goes through the byte-wise
 * helpers.  The checksum field is decoded little-endian because the original
 * is 32-bit x86 and writes it with a raw fwrite of a dword; the big-endian
 * console sibling has no fopen path at all, so there is no second convention
 * to reconcile.
 */
#ifndef BR_SAVE_H
#define BR_SAVE_H

#include <stddef.h>
#include <stdint.h>

#include "slice4_52.h"   /* BR_SEASON_BLOCK_SIZE, BR_SEASON_TAIL_SIZE --
                          * CANONICAL, owned by the writer's packet.        */

#ifdef __cplusplus
extern "C" {
#endif

/* --- the pieces of the file ---------------------------------------------- */

#define BR_BRF_MAGIC        "RSea"   /* 0x100B5D94 / Glide 0x100B559C */
#define BR_BRF_MAGIC_SIZE   4
#define BR_BRF_SUM_SIZE     4
#define BR_BRF_OPT_COUNT    5        /* 0x100AC658 is the one that is skipped */

#define BR_BRF_MAGIC_OFF    0
#define BR_BRF_SUM_OFF      (BR_BRF_MAGIC_OFF + BR_BRF_MAGIC_SIZE)
#define BR_BRF_BLOCK_OFF    (BR_BRF_SUM_OFF + BR_BRF_SUM_SIZE)

/* The reader's own constant: the option dwords and the name are the last
 * 0x94 bytes, located by seeking back from the end of the file. */
#define BR_BRF_TAIL_FROM_END  (BR_BRF_OPT_COUNT * 4 + BR_SEASON_TAIL_SIZE)

/* 4 + 4 + 0x200 + 20 + 0x80 == 0x29C == 668. */
#define BR_BRF_FILE_SIZE                                                      \
    (BR_BRF_BLOCK_OFF + BR_SEASON_BLOCK_SIZE + BR_BRF_TAIL_FROM_END)

/* The strings the save/load screens build filenames out of.  The two masks
 * already reach the port as slice6_71.h's BrS71Globals::pszRallySeasonBrf /
 * ::pszAutoSaveBrf; these are the same bytes named once for this module's
 * own use, not a new model of that struct. */
#define BR_BRF_PREFIX_SEASON      "RallySeason"   /* 0x100AD328 / 0x100ACB00 */
#define BR_BRF_PREFIX_TIMEATTACK  "TimeAttack"    /* 0x100AD33C             */
#define BR_BRF_EXT                ".brf"          /* 0x100AD320 / 0x100ACAF8 */
#define BR_BRF_MASK               "RallySeason*.BRF"  /* 0x100AD348          */
#define BR_BRF_AUTOSAVE           "AutoSave.brf"      /* 0x100AD310          */

/* The scan's cap.  `inc ebx / cmp ebx,0x64 / jl` in 0x1005CF20, and the same
 * 100 as slice1_06.h's BR_NAMELIST_COUNT, which is what the names land in. */
#define BR_BRF_SCAN_MAX  100

/* --- the decoded file ----------------------------------------------------
 * A HOST struct.  Never overlay it on an image; use BrBrfDecode/BrBrfEncode.
 * `szName` is 0x80 raw bytes, not necessarily NUL-terminated -- the original
 * writes whatever 0x10AD0990 holds and the scan strcpy()s what it reads back,
 * so the terminator is the file's problem, not the format's. */
typedef struct BrBrfSeason {
    unsigned char aBlock[BR_SEASON_BLOCK_SIZE];  /* file +0x008 */
    uint32_t      aOpt[BR_BRF_OPT_COUNT];        /* file +0x208 */
    char          szName[BR_SEASON_TAIL_SIZE];   /* file +0x21C */
} BrBrfSeason;

/* --- results -------------------------------------------------------------
 * The original has no result codes: every failure is the same `return
 * (arg & 0xFF) != 0` (see BrBrfReaderFailReturn below).  These distinguish
 * them so a test can tell which check fired. */
enum {
    BR_BRF_OK        =  0,
    BR_BRF_ETRUNC    = -1,   /* image shorter than the reader can parse    */
    BR_BRF_EMAGIC    = -2,   /* the four bytes are not "RSea"              */
    BR_BRF_ECHECKSUM = -3,   /* adler32 of the payload does not match      */
    BR_BRF_EIO       = -4,   /* host: open/seek/read failed                */
    BR_BRF_EARG      = -5    /* port-only: a null or too-small buffer      */
};

/* --- the format ---------------------------------------------------------- */

/* adler32 over the payload, seeded exactly as the original seeds it: the
 * value returned by adler32(0, NULL, 0).  Thin, and deliberately so -- the
 * point is that the seed is asked for rather than assumed to be 1. */
uint32_t BrBrfChecksum(const unsigned char *pBlock, size_t cbBlock);

/* Lay a season out as the writer lays it out.  Returns the number of bytes
 * written (always BR_BRF_FILE_SIZE) or BR_BRF_EARG.  This is the byte layout,
 * not a second transcription of 0x100709A0 -- that one is slice4_52.c's
 * BrMenuSub100709A0 and stays there. */
int BrBrfEncode(unsigned char *pOut, size_t cbOut, const BrBrfSeason *pIn);

/* Parse a whole image the way the reader parses a file, INCLUDING locating
 * the tail from the end rather than sequentially.  Returns BR_BRF_OK or one
 * of the negative codes; on failure *pOut is untouched. */
int BrBrfDecode(const unsigned char *pImage, size_t cbImage,
                BrBrfSeason *pOut);

/* --- the host seam (Win32 in the original; stdio here) -------------------- */

/* fopen(pszPath, "rb") and the reader's exact sequence of reads and seeks.
 * The INSTALL half of 0x10070610 -- copying the payload into *0x10ACED34 and
 * *0x10AD189C, staging the dwords at 0x10AD0978 and mirroring the name to
 * 0x10AD34F8 -- is NOT here; it belongs to whoever owns those globals. */
int BrBrfReadFile(const char *pszPath, BrBrfSeason *pOut);

/* Write one out.  Not a decompilation of anything: the original's writer is
 * slice4_52.c's.  This exists so a caller (and the test) can make a file
 * without reaching into that packet's globals. */
int BrBrfWriteFile(const char *pszPath, const BrBrfSeason *pIn);

/* 0x1005CF20's per-match read: the LAST BR_SEASON_TAIL_SIZE bytes of the
 * file and nothing else -- no magic check, no checksum check.  This is why a
 * corrupt save still shows a name on the load screen.
 * Returns BR_BRF_OK or BR_BRF_EIO. */
int BrBrfReadName(const char *pszPath, char szName[BR_SEASON_TAIL_SIZE]);

/* 0x1005CE30's index: skip strlen(pszPrefix) characters of the FILE NAME and
 * atoi() what is left, so "RallySeason12.brf" -> 12.  The original does not
 * check that the prefix actually matches and does not bound the result; atoi
 * of a non-numeric tail is 0, which silently targets slot 0.  Both are
 * reproduced.  Returns the index; -1 only if an argument is null. */
int BrBrfSlotIndex(const char *pszFileName, const char *pszPrefix);

/* 0x10041DF0's filename: prefix + decimal index + ".brf".
 * DEVIATION: the original is _itoa into a stack buffer followed by two
 * strcat-shaped `rep movs` pairs with no bound at all.  Bounded here.
 * Returns the length that would have been written, or -1 on a null argument;
 * a return >= cbOut means it was truncated. */
int BrBrfFileName(char *pszOut, size_t cbOut, const char *pszPrefix, int slot);

/* --- the quirk that is not worth a function ------------------------------
 * Every failure inside 0x10070610 returns `(arg & 0xFF) != 0`, where `arg` is
 * its SECOND argument -- the LOW BYTE only.  Both call sites that matter pass
 * 1 (0x10071130 mode 4 forwards the caller's argument, and 0x10041BD0 passes
 * 1), so every failure reports SUCCESS and the caller's error report at
 * 0x100378C0 index 7 is unreachable in the shipped build.  Not a transcription
 * choice -- read it off 0x1006987B/0x10069919 in Glide. */
#define BrBrfReaderFailReturn(arg)  (((arg) & 0xFF) != 0)

#ifdef __cplusplus
}
#endif

#endif /* BR_SAVE_H */
