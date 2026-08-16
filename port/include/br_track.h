/* br_track.h -- .TRK track loader, decompiled from BRGlide.dll.
 *
 * WHERE THE GAME DATA LIVES
 * -------------------------
 * Not in the POD. The disc's `BossRally.pod` holds one entry, MODELLIGHTS.BLOB,
 * and nothing else; it is a leftover, not the content store. The bulk data sits
 * in PLAIN DIRECTORIES on the retail CD and is played from the CD -- it is not
 * unpacked out of the installer (DATA1.CAB is 125 KB, only the setup stub):
 *
 *     TRACKS/   *.TRK  6 championship tracks + GAMEWIN + BONUS  0.9 - 2.3 MB
 *               *.HNT  ASCII texture-heap hint list
 *               *.HND  ASCII, one number
 *     CARS/     *.RCA  16 cars                    (br_rca.c already reads these)
 *     CARGFX/   skytex*.ci4 + .lut                (br_n64tex.c)
 *     IMAGES/   *.bmp  menu art          PAINT/   *.bmp  car liveries
 *     SFX/      *.wav
 *
 * `tools/extract_iso.py --list` walks the real ISO 9660 tree and prints all of
 * it. 2111 files, 116 MB, on the MODE1/2352 data track.
 *
 * THE FORMAT
 * ----------
 * A .TRK is a raw N64 memory image with a 0x230-byte header on the front, and
 * it is BIG-ENDIAN. File offset 0 corresponds to N64 address 0x80025C00, so the
 * payload (file offset 0x230) is N64 0x80025E30 -- which is exactly what every
 * shipped file stores at header +0x84. Internal references are N64 addresses,
 * relocated on load. This is the same arrangement as .rca (br_rca.c), whose
 * struct sits at file 0x8000 / N64 0x803C8000.
 *
 * The header's field widths are not inferred: they are read straight off the
 * byte-swapper at 0x10031B80, which reverses every dword in 0x00..0x7F and
 * 0x84..0x163 and leaves 0x80..0x83 (an RGBA quad) and the 0x164..0x22F tail
 * alone. Nothing here comes from a byte histogram.
 *
 * THE LOADER
 * ----------
 *   0x100311C0  BrTrackLoad(int iTrack)     the whole thing
 *   0x10031140  BrTrackLoadHints(int)       "tracks/" + name, ".hnt", 0x10031030
 *   0x10031B80  header read + byte-swap + relocate (1549 B, all unrolled)
 *   0x100189E0  the relocation itself: *p += hostBase - n64Base, 0 stays 0
 *   0x10018A10  registers the (n64Base, hostBase) pair
 *   0x100314D0  the payload passes: vertices, sections, grid, faces
 *   0x10018AD0/AF0  vertex array   stride 0x0C, three f32
 *   0x10018A70/A90  face array     stride 0x08, four u16
 *   0x10018B40/B60  section array  stride 0x24  (NOT PORTED -- see below)
 *   0x10018A50      u16 run swapper
 * All of these are `shared` in config/shared.csv except 0x100311C0 and
 * 0x10031030, which the Glide map has and the D3D map does not list at the
 * same address.
 *
 * WHAT THIS MODULE DOES NOT DO, NAMED PLAINLY
 * -------------------------------------------
 *  - The section array (header +0x1C, stride 0x24) is left BIG-ENDIAN. Its
 *    element swapper 0x10018B60 is 493 bytes, relocates three pointers, walks
 *    into sub-arrays through 0x10018D50, and indexes the heap at header +0x00.
 *    Confirmed so far: +0x00/+0x04/+0x08 are relocated pointers, +0x0C..+0x17
 *    are u16, +0x20 is a dword; +0x18..+0x1F is unconfirmed. A half-right swap
 *    would link cleanly and be wrong, so it is not written.
 *  - 0x10031660, called from 0x100314D0 between the grid and the faces, is not
 *    ported; nor is anything reached from header +0x50..+0x5C.
 *  - The display-list payload the header points at is not decoded here.
 *    br_f3d.c already knows F3DEX.
 *
 * PORTABILITY DEVIATION (deliberate, and the reason this is not a transcription)
 * The original relocates N64 addresses into HOST POINTERS in place. On LP64 a
 * pointer does not fit in the dword it came from, so this port relocates them
 * into FILE OFFSETS instead and indexes the image. The guard is the original's:
 * zero stays zero, and a value below the base (signed compare, as in
 * 0x100189E0) becomes zero. Offsets that land outside the image are reported by
 * BrTrackFieldValid rather than trusted.
 */
#ifndef BR_TRACK_H
#define BR_TRACK_H

#include <stdint.h>

#include "br_mat.h"
#include "br_vec.h"

/* Constants read out of the original, not chosen here. */
#define BR_TRK_HEADER_SIZE   0x230u     /* asserted by 0x1003147F */
#define BR_TRK_N64_BASE      0x80025C00u/* pushed at 0x100311E8 */
#define BR_TRK_MAX_FILE      0x3D0900u  /* 4,000,000; 0x10031282 */
#define BR_TRK_MAX_INSTANCES 0x800u     /* "MAX_INSTANCES"; 0x10031459 */
#define BR_TRK_GRID_STARTS   0x1001u    /* 0x1000 cells + a total */

#define BR_TRK_VERTEX_STRIDE   0x0Cu
#define BR_TRK_FACE_STRIDE     0x08u
#define BR_TRK_SECTION_STRIDE  0x24u
#define BR_TRK_INSTANCE_STRIDE 0x54u

/* Header dword offsets. Names come from the code that reads them; anything
 * still anonymous keeps its offset in the name rather than a guessed word. */
#define BR_TRK_H_HEAPOFF     0x00u  /* file offset; 0x100311C0 -> g_106B7C7C   */
#define BR_TRK_H_HEADERSIZE  0x04u  /* must be 0x230                          */
#define BR_TRK_H_CFACES      0x08u
#define BR_TRK_H_FACES       0x0Cu
#define BR_TRK_H_CVERTICES   0x10u
#define BR_TRK_H_VERTICES    0x14u
#define BR_TRK_H_CSECTIONS   0x18u
#define BR_TRK_H_SECTIONS    0x1Cu
#define BR_TRK_H_GRIDITEMS   0x20u  /* u16[], length = gridStart[0x1000]       */
#define BR_TRK_H_GRIDSTART   0x24u  /* u16[0x1001]                             */
#define BR_TRK_H_INSTANCES   0x60u  /* stride 0x54, matrix first               */
#define BR_TRK_H_CINSTANCES  0x64u  /* <= 0x800                                */
#define BR_TRK_H_RGBA        0x80u  /* four raw bytes, NOT byte-swapped        */
#define BR_TRK_H_PAYLOAD     0x84u  /* always N64 0x80025E30 == file 0x230     */
#define BR_TRK_H_U16LIST8C   0x8Cu  /* u16[], NUL-terminated                   */
#define BR_TRK_H_U16LIST90   0x90u
#define BR_TRK_H_FACESEND    0x94u  /* butts against the end of the face array */

/* The instance flag the loader sets when a transform turns out to be a pure
 * uniform scale. 0x10031431: `or byte ptr [rec+0x4D], 0x20`. */
#define BR_TRK_INST_UNIFORM  0x20u
#define BR_TRK_INST_FLAGSOFF 0x4Du
#define BR_TRK_INST_SCALEOFF 0x40u   /* float, written by the load-time pass   */

typedef struct BrTrack {
    unsigned char *pbImage;     /* whole file; payload swapped in place        */
    uint32_t       cbImage;
    unsigned char  abHdr[BR_TRK_HEADER_SIZE];  /* host order, relocated        */
    /* Everything the load-time instance pass produced, kept host-side so the
     * image stays a faithful copy of what the original would have in memory. */
    uint32_t       cInstancesUniform;
} BrTrack;

/* The name table at 0x100B78C0: 15 entries, indices 6..11 repeating 0..5. */
#define BR_TRK_NAME_COUNT 15
const char *BrTrackName(int iTrack);

/* 0x100311C0's opener, minus the engine wiring. Returns 0 on success.
 * Enforces the original's three checks: size <= 4,000,000 (0x10031282),
 * instances <= 2048 (0x10031459), header size == 0x230 (0x1003147F). */
int  BrTrackOpen(BrTrack *pTrack, const char *pszPath);
/* "tracks/" + BrTrackName(iTrack), the way 0x100311C0 builds it.
 * pszDir replaces the literal "tracks/" prefix when non-NULL. */
int  BrTrackOpenIndex(BrTrack *pTrack, const char *pszDir, int iTrack);
void BrTrackClose(BrTrack *pTrack);

/* Header accessors. All decode byte-wise from abHdr. */
uint32_t BrTrackHdrU32(const BrTrack *pTrack, unsigned off);
/* A relocated field is a file offset; 0 means "absent" (the original's NULL).
 * Returns 0 when the offset plus cbNeed does not fit inside the image. */
int      BrTrackFieldValid(const BrTrack *pTrack, unsigned off, uint32_t cbNeed);

uint32_t BrTrackVertexCount(const BrTrack *pTrack);
uint32_t BrTrackFaceCount(const BrTrack *pTrack);
uint32_t BrTrackSectionCount(const BrTrack *pTrack);
uint32_t BrTrackInstanceCount(const BrTrack *pTrack);
uint32_t BrTrackGridItemCount(const BrTrack *pTrack);   /* gridStart[0x1000] */

/* Element readers. Each returns 0 on success, non-zero if the index or the
 * underlying array is out of range. */
int BrTrackVertex(const BrTrack *pTrack, uint32_t i, BrVec3 *pOut);
int BrTrackFace(const BrTrack *pTrack, uint32_t i, uint16_t aOut[4]);
int BrTrackInstance(const BrTrack *pTrack, uint32_t i, BrMat4 *pM,
                    float *pfInvScale, uint32_t *pFlags);
int BrTrackGridStart(const BrTrack *pTrack, uint32_t iCell, uint16_t *pOut);

/* Axis-aligned bounds over the whole vertex array. 0 on success. */
int BrTrackBounds(const BrTrack *pTrack, BrVec3 *pMin, BrVec3 *pMax);

/* 0x10031030 -- the .HNT/.HND sidecar. First line is a single %u (a memory
 * budget in bytes; the default it overwrites is 0x200000). Later lines are
 * "%u %x %d %d" = id, N64 address, width, height, at most 0x100 of them. */
#define BR_TRK_MAX_HINTS 0x100u
typedef struct BrTrackHint {
    uint32_t id;
    uint32_t n64Addr;
    int32_t  cx;
    int32_t  cy;
} BrTrackHint;

typedef struct BrTrackHints {
    uint32_t     cbBudget;
    uint32_t     cHints;
    BrTrackHint  aHints[BR_TRK_MAX_HINTS];
} BrTrackHints;

int BrTrackLoadHints(BrTrackHints *pHints, const char *pszPath);

#endif /* BR_TRACK_H */
