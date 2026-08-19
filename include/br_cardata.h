/* br_cardata.h -- the CAR-DATA RECORD: the .rca image the physics reads.
 *
 * REFERENCE IS orig/BRGlide.dll.  Every address below was checked with
 * tools/whereis.py before a line was written; none of the four functions this
 * module ports is ported anywhere else in this tree under either build's
 * number.
 *
 * ======================================================================
 * WHY THIS MODULE EXISTS, AND THE MISREADING IT CORRECTS
 * ======================================================================
 * br_collresp.h and br_carphys.h both stated, as established fact, that the
 * car's collision-box extents at body+0x1DC..+0x1E8 are
 *
 *   - written by the car constructor (Glide 0x1005BCC0) at 0x1005BD40 /
 *     0x1005BD42 / 0x1005BD48 / 0x1005BD4E as (0, 0, 2.0f, 0), and
 *   - replaced by 0x10059A80 out of a car-data record's +0x10..+0x1C, which
 *     0x10063B80 fills from 0x10B73668 + 24 * n.
 *
 * ALL THREE CLAIMS ARE WRONG, and they are wrong in ONE way: they confuse
 * car+0x1DC with body+0x1DC.  The body is car+0x164 -- 0x1005BD05 pushes
 * `esi+0x164` into the body initialiser 0x1006DAD0, and br_carphys.h's own
 * offsets agree (body+0x114 == car+0x278, body+0x158 == car+0x2BC, and
 * 0x164 + 0x114 == 0x278).  So:
 *
 *      car +0x1DC  is the live BrRbState  (body+0x78)
 *      car +0x340  is the collision box   (body+0x1DC)
 *
 * and the two are 0x164 bytes apart.  Read that way, the three claims say
 * something quite different and quite ordinary:
 *
 *   - 0x1005BD40..0x1005BD5A writes esi+0x1DC..+0x1F4 with ebx == 0,
 *     edi == 0x40000000 and ebp == 0x3F800000.  That is the car's INITIAL
 *     STATE -- pos (0, 0, 2.0), vel 0, quat (1, 0, 0, 0), angVel 0 -- which
 *     br_carphys.c's BrCarPhysInit already writes as exactly that.  The
 *     constructor NEVER TOUCHES car+0x340; grepping every store in the image
 *     for a displacement of 0x340 finds none inside 0x1005BCC0.
 *   - 0x10059A80 is slice8_83.h's BrCarRecordFromState: it copies a
 *     BrCarState (the 0xA0-byte NETWORK WIRE FORMAT) into car+0x1DC and then
 *     mirrors the 0x44-byte state into car+0x278 and car+0x2BC.  Its
 *     `mov [ebx+0x1e0], edx` at 0x10059ABF is state.pos.y, not a box extent.
 *   - 0x10063B80 is the GHOST/REPLAY playback: it copies a 24-byte sample out
 *     of a per-stream ring, feeds it to 0x10059A80, and then estimates the
 *     velocity as (nextSample.pos - pos) * 0x10077A70.  Its `esi+0x1dc` at
 *     0x10063C4B is the position it is differencing.  (It is also not indexed
 *     the way that paragraph says: the record index is `(n << 16) + cursor`
 *     with the cursor at 0x11773670+4n and the sample count at 0x10B73648+4n,
 *     and the two functions' arguments are CROSSED relative to the reading
 *     that produced the claim -- 0x10063B89's `[esp+0xb4]` and 0x10063BB3's
 *     `[esp+0xb8]` are read with ESP EIGHT BYTES APART, because the `add
 *     esp,8` that balances the first call sits at 0x10063BBA, AFTER the
 *     second read.  arg1 is the car and arg2 is the stream index.)
 *
 * The consequence for the port is the opposite of the one that was drawn:
 * the constructor does not leave the box degenerate, it leaves it UNWRITTEN.
 * The only writer of car+0x340..+0x34C anywhere in either image is
 *
 *      0x1006FEBF / 0x1006FEC5 / 0x1006FED1 / 0x1006FEDD, inside 0x1006FD90
 *
 * and the source is `[eax + 0xC8 .. 0xD4]` with `eax == [car + 0x29C4]` --
 * the CAR-DATA OBJECT.  That is what this module supplies.
 *
 * ======================================================================
 * THE CHAIN, END TO END, ALL FOUR LINKS READ OFF THE BYTES
 * ======================================================================
 *   0x1002EBD1  LoadCar(slot, iCar, flag):
 *                   dst = 0x100BCDD0 + slot * 0x15F88
 *                   0x10030DE0(dst, iCar)
 *               0x15F88 == 89992 comes out of the `lea` chain at
 *               0x1006FCB4..0x1006FCC4 (x5, x2+1, <<6, -1, <<4, +1, x8) and
 *               is one byte-image slot per entrant -- larger than the largest
 *               .rca on the disc (ce.rca, 87256 B).
 *
 *   0x10030DE0  build "cars/" + name[iCar] + ".rca" (the literals at
 *               0x100B7900 and 0x100AA310), read the whole file into dst
 *               (0x10030F50), `strncmp(dst, "RCar", 4)` -- 0x100AA308 -- and
 *               complain "not a car file: %s" if it fails, then 0x10030770.
 *               name[] is the 16-entry table at 0x100B7D00; see
 *               BrCarDataName.
 *
 *   0x10030770  the byte-swap / relocation pass, 1641 B.  It registers the
 *               pair (N64 0x803C8000, host dst+0x8000) with 0x10018A10 -- the
 *               same relocator br_track.c uses -- clears dst+0x7C, and then
 *               touches NOTHING BELOW dst+0x8000.  That is checked, not
 *               assumed: every effective address in the function is either
 *               [esi+0x7c] or [esi+0x8000] and up.
 *
 *               THIS IS WHY THE BOX NEEDS NO BYTE SWAP.  The .rca is two
 *               formats in one file: a LITTLE-ENDIAN PC parameter header at
 *               +0x00..~+0xE4, and a BIG-ENDIAN N64 memory image from
 *               +0x8000.  The physics block lives in the first, so it is
 *               read straight.  The suspension mounts the constructor reads
 *               at +0x80EC..+0x80FC live in the second and are big-endian in
 *               the file; see BrCarDataMount and the DEVIATION below.
 *
 *   0x1006FCB0  car->f29C4 = the slot's image;  0x1006FC70 -> 0x1005BCC0.
 *   0x1006FD90  __thiscall on the car, 368 B.  Resets six matrices, then
 *               copies the physics block out of car->f29C4 and CLEARS the
 *               pointer (0x1006FEF1).  The four stores this module exists for
 *               are its last four.
 *
 * ======================================================================
 * THE PHYSICS BLOCK, as 0x1006FD90 reads it
 * ======================================================================
 *   +0x96  byte, movsx  -> car+0xE5C
 *   +0x97  byte, movsx  -> car+0xE64
 *   +0x98  7 dwords     -> car+0xE28..+0xE40   (a gear array; index 0 is 0.0
 *                         in every shipped car and 1..6 are the six ratios
 *                         br_rca.h already found at +0x9C)
 *   +0xB4  -> car+0xE44     +0xB8 -> car+0xE48     +0xBC -> car+0xE4C
 *   +0xC0  -> car+0xE50     +0xC4 -> car+0xE54
 *   +0xD8  byte, movsx  -> car+0xE58
 *   +0xC8  -> car+0x340 == body+0x1DC   THE COLLISION BOX, X extent
 *   +0xCC  -> car+0x344 == body+0x1E0                        Y extent
 *   +0xD0  -> car+0x348 == body+0x1E4                        Z extent
 *   +0xD4  -> car+0x34C == body+0x1E8                        Z offset
 *
 * The four box words are full extents, not half extents: 0x10067C30
 * reciprocates the first three into the box matrix's scale and 0x10066260
 * classifies the transformed vertices against +-0.5 (0x10077B48 / 0x10077B50,
 * both DOUBLES).  0x10066D70 halves them again for its corner.  Every shipped
 * car agrees with that reading -- ce.rca is (3.5, 2.0, 0.8, 0.7) and the
 * constructor's own inertia dimensions are (3.5, 2.0, 1.5) -- and the two
 * outliers are the two joke cars, the Milk Truck at (4.5, 3.3) and the Beach
 * Ball at (4.5, 2.5).
 *
 * ======================================================================
 * DEVIATIONS
 * ======================================================================
 *  - The original's car-data object is the whole file image and every field
 *    is a byte offset into it.  This module DECODES the block it uses into a
 *    struct instead of overlaying one, because the file is a mixed-endian
 *    N64 image and an overlay would be correct on exactly one host.
 *  - 0x10030770's swap pass is NOT ported.  Nothing this module returns
 *    depends on it: the physics block is below +0x8000.  The four mount
 *    words at +0x80EC..+0x80FC ARE above it, so BrCarDataMount byte-swaps
 *    them itself and says so; that is the only place this module makes an
 *    endianness assumption, and it is checkable -- the four values come out
 *    as plausible metres for every shipped car when swapped and as 1e30-scale
 *    garbage when not.
 *  - The file is found by SEARCH rather than by the original's fixed
 *    "cars/" prefix, because this port is run from the tree root and from
 *    build/ alike.  See BrCarDataDir.
 */
#ifndef BR_CARDATA_H
#define BR_CARDATA_H

#include <stddef.h>
#include <stdint.h>

/* 0x10030EDB's `strncmp(buf, "RCar", 4)`, the literal at 0x100AA308. */
#define BR_CARDATA_MAGIC     "RCar"

/* The name table at 0x100B7D00.  Sixteen entries; index 16 is the first
 * unrelated dword after it, so the table's extent is pinned by its content
 * rather than by a count the code carries. */
#define BR_CARDATA_CARS      16

/* 0x1006FD90's 7-dword copy at +0x98.  br_rca.h's `gears[6]` is entries
 * 1..6 of this array. */
#define BR_CARDATA_GEARS     7

/* The offsets 0x1006FD90 reads.  Named so a reader can check them against
 * the disassembly without counting struct members. */
#define BR_CARDATA_O_B96     0x96u
#define BR_CARDATA_O_B97     0x97u
#define BR_CARDATA_O_GEARS   0x98u
#define BR_CARDATA_O_PARAMS  0xB4u   /* five floats, +0xB4..+0xC4  */
#define BR_CARDATA_O_BOX     0xC8u   /* four floats, +0xC8..+0xD4  */
#define BR_CARDATA_O_D8      0xD8u
/* The four suspension mount words 0x1005BE0B..0x1005BF98 read, in the
 * BIG-ENDIAN half of the file. */
#define BR_CARDATA_O_MOUNT   0x80ECu

/* Everything 0x1006FD90 and 0x1005BCC0 take out of the image, decoded.
 * Fields carry the offset they came from, not a guessed name: naming
 * `param[3] == 174.375` "top speed" would be inventing knowledge. */
typedef struct BrCarData {
    char     szName[64];                  /* +0x04, NUL-terminated          */

    /* THE COLLISION BOX -- the whole point of this module.
     * +0xC8 / +0xCC / +0xD0 / +0xD4, straight into body+0x1DC..+0x1E8. */
    float    boxX, boxY, boxZ, boxOffZ;

    float    gears[BR_CARDATA_GEARS];     /* +0x98, 7 dwords                */
    float    param[5];                    /* +0xB4 .. +0xC4                 */
    int8_t   b96, b97, bD8;               /* the three `movsx` bytes        */

    /* +0x80EC..+0x80FC, byte-swapped out of the N64 half.  The constructor
     * uses (mount[0], mount[1]) as the FRONT pair's (x, y) and
     * (mount[2], mount[3]) as the REAR pair's, mirroring y for wheels 1
     * and 3.  NOT applied by BrCarPhysInit -- see br_carphys.h. */
    float    mount[4];
    int      fMountValid;                 /* 0 when the file stopped short  */

    size_t   cbFile;
} BrCarData;

/* The 16 names at 0x100B7D00, in the original's order: index 0 is "ce".
 * Returns NULL outside 0..15. */
const char *BrCarDataName(int iCar);

/* 0x10030DE0's tail: check the "RCar" magic and pull the block out.
 * Returns 0 on success, non-zero when the buffer is short or unmagicked. */
int BrCarDataDecode(BrCarData *pData, const void *pvFile, size_t cbFile);

/* 0x10030DE0 whole, minus the fixed "cars/" prefix: read pszPath and decode
 * it.  Returns 0 on success. */
int BrCarDataLoadFile(BrCarData *pData, const char *pszPath);

/* 0x10030DE0's name build: pszDir + "/" + BrCarDataName(iCar) + ".rca".
 * pszDir == NULL means BrCarDataDir(). */
int BrCarDataLoadIndex(BrCarData *pData, const char *pszDir, int iCar);

/* Where the CARS/ directory is.  The original hardcodes "cars/" relative to
 * the CD's root; this port is run from the tree root, so the answer is
 * searched for once and cached:
 *
 *      $BR_CARS_DIR, then "testdata/cars", "cars", "../testdata/cars"
 *
 * Returns NULL when none of them holds the default car's file, and a NULL
 * answer is a MEASUREMENT, not an error: BrCarPhysInit then leaves the box
 * unwritten and BrCollRespBoxDegenerate reports it, exactly as the asset
 * policy requires. */
const char *BrCarDataDir(void);

/* Car 0 ("ce", TYPE-CE), loaded once from BrCarDataDir() and cached.  NULL
 * when the directory or the file is absent.  This is the record
 * BrCarPhysInit applies when nothing else has been supplied. */
const BrCarData *BrCarDataDefault(void);

#endif /* BR_CARDATA_H */
