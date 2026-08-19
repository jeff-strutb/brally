/* br_track.c -- .TRK track loader, decompiled from BRGlide.dll.
 *
 * See br_track.h for where the data lives, what the format is, and which parts
 * of the original are deliberately not ported.
 *
 * Everything is decoded byte-wise. The payload is BIG-ENDIAN N64 data and the
 * host is little-endian, so a struct overlay would be wrong in the one way
 * that still compiles.
 */
#include "br_track.h"

/* BrSwapU16Array -- 0x10018A50, shared with slice2_16.c's 0x1002B9E0. */
#include "br_bits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- byte-wise primitives ------------------------------------------------ */

static uint32_t rd_u32be(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16le(const unsigned char *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static void wr_u32le(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

/* A dword byte-reversal in place: what 0x10031B80 and 0x10018AF0 do to every
 * 32-bit field, and what 0x10018A90 does to none of them (that one is u16s). */
static void swap_u32(unsigned char *p)
{
    unsigned char t;
    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

/* 0x10018A50 -- reverse a run of consecutive u16s.  This file used to carry
 * its own copy under BRGlide's address while slice2_16.c carried the same 29
 * bytes as BrSwapU16Array under BRD3D's 0x1002B9E0.  There is now one, in
 * br_bits.c, and it takes a SIGNED count as the original does -- the copy
 * here took an unsigned one, which would have run away on a negative rather
 * than doing nothing. */
static void swap_u16_run(unsigned char *p, uint32_t c)
{
    BrSwapU16Array(p, (int)c);
}

/* Reinterpret a host-order dword as a float without aliasing through a
 * pointer cast. memcpy is the portable spelling and the compiler folds it. */
static float bits_to_float(uint32_t u)
{
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static uint32_t float_to_bits(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

/* --- the name table at 0x100B78C0 ---------------------------------------- */

/* Fifteen entries. 6..11 repeat 0..5, which is how the original addresses the
 * same six tracks under a second set of indices; 12..14 are the two
 * non-championship tracks, with bonus.trk appearing twice. Reproduced as it
 * is, duplicates included -- an index is not a name here. */
static const char *const g_apszBrTrackNames[BR_TRK_NAME_COUNT] = {
    "desert.trk", "mountain.trk", "coast.trk", "mine.trk", "amazon.trk",
    "race.trk",
    "desert.trk", "mountain.trk", "coast.trk", "mine.trk", "amazon.trk",
    "race.trk",
    "gamewin.trk", "bonus.trk", "bonus.trk"
};

const char *BrTrackName(int iTrack)
{
    if (iTrack < 0 || iTrack >= BR_TRK_NAME_COUNT)
        return NULL;
    return g_apszBrTrackNames[iTrack];
}

/* --- relocation ---------------------------------------------------------- */

/* 0x100189E0. The original turns an N64 address into a host pointer:
 *
 *      if (*p == 0)                  leave it
 *      else if ((int32)*p < (int32)n64Base)  *p = 0
 *      else                          *p += hostBase - n64Base
 *
 * DEVIATION: this port produces a FILE OFFSET rather than a host pointer,
 * because on LP64 a host pointer does not fit in the dword being rewritten.
 * The comparison stays SIGNED, as in the original -- 0x80025C00 is negative as
 * an int32, so a positive junk value passes the test there and passes it here,
 * and the resulting nonsense offset is caught by BrTrackFieldValid instead of
 * being silently trusted. Reproducing the polarity matters: the unsigned form
 * would reject exactly the values the original accepts.
 */
static uint32_t br_reloc(uint32_t n64va)
{
    if (n64va == 0)
        return 0;
    if ((int32_t)n64va < (int32_t)BR_TRK_N64_BASE)
        return 0;
    return n64va - BR_TRK_N64_BASE;
}

/* The nineteen header fields 0x10031B80 passes to 0x100189E0, in its order.
 * Derived from the function's `lea`/stack-slot bookkeeping, not guessed: the
 * saved slots are 0x50->+0x0C, 0x10->+0x14, 0x14->+0x1C, 0x18->+0x20,
 * 0x1C->+0x24, 0x20->+0x5C, 0x24->+0x60, 0x28->+0x68, 0x2C->+0x6C, 0x30->+0x70,
 * 0x34->+0x74, 0x38->+0x78, 0x3C->+0x84, 0x40->+0x8C, 0x44->+0x90, 0x48->+0x94,
 * plus edi/ebx/ebp holding +0x50/+0x54/+0x58. */
static const unsigned g_aBrTrackPtrFields[] = {
    0x0C, 0x14, 0x1C, 0x20, 0x24,
    0x50, 0x54, 0x58, 0x5C, 0x60,
    0x68, 0x6C, 0x70, 0x74, 0x78,
    0x84, 0x8C, 0x90, 0x94
};
#define BR_TRK_PTR_FIELDS \
    (sizeof g_aBrTrackPtrFields / sizeof g_aBrTrackPtrFields[0])

/* --- header decode (0x10031B80) ------------------------------------------ */

/* The swapper reverses every dword in [0x00,0x80) and [0x84,0x164), and leaves
 * 0x80..0x83 and 0x164..0x22F untouched. Verified by replaying every memory
 * write in the disassembly and taking the union of the byte offsets covered;
 * the only holes below 0x164 are 0x80..0x83, which is the RGBA quad -- four
 * bytes whose meaning does not depend on byte order, which is exactly why the
 * original skips it. */
static void br_track_swap_header(unsigned char *h)
{
    unsigned off;
    for (off = 0x00; off < 0x80; off += 4)
        swap_u32(h + off);
    for (off = 0x84; off < 0x164; off += 4)
        swap_u32(h + off);
}

uint32_t BrTrackHdrU32(const BrTrack *pTrack, unsigned off)
{
    if (off + 4 > BR_TRK_HEADER_SIZE)
        return 0;
    return rd_u32le(pTrack->abHdr + off);
}

int BrTrackFieldValid(const BrTrack *pTrack, unsigned off, uint32_t cbNeed)
{
    uint32_t o = BrTrackHdrU32(pTrack, off);
    if (o == 0)
        return 0;
    if (o >= pTrack->cbImage)
        return 0;
    return (pTrack->cbImage - o) >= cbNeed;
}

uint32_t BrTrackVertexCount(const BrTrack *p)  { return BrTrackHdrU32(p, BR_TRK_H_CVERTICES); }
uint32_t BrTrackFaceCount(const BrTrack *p)    { return BrTrackHdrU32(p, BR_TRK_H_CFACES); }
uint32_t BrTrackSectionCount(const BrTrack *p) { return BrTrackHdrU32(p, BR_TRK_H_CSECTIONS); }
uint32_t BrTrackInstanceCount(const BrTrack *p){ return BrTrackHdrU32(p, BR_TRK_H_CINSTANCES); }

uint32_t BrTrackGridItemCount(const BrTrack *pTrack)
{
    uint32_t o = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDSTART);
    if (!BrTrackFieldValid(pTrack, BR_TRK_H_GRIDSTART, BR_TRK_GRID_STARTS * 2))
        return 0;
    /* 0x1003150F reads the LAST of the 0x1001 entries as the total. */
    return rd_u16le(pTrack->pbImage + o + (BR_TRK_GRID_STARTS - 1) * 2);
}

/* --- payload passes (0x100314D0) ----------------------------------------- */

/* Every pass below is bounds-checked against the image before it runs. The
 * original is not: it writes through relocated pointers unconditionally. A
 * truncated file would walk off the end there and is refused here. */
static int br_in_range(const BrTrack *pTrack, uint32_t off, uint32_t cb)
{
    if (off == 0 || off >= pTrack->cbImage)
        return 0;
    return (pTrack->cbImage - off) >= cb;
}

static void br_track_swap_payload(BrTrack *pTrack)
{
    uint32_t off, c, i, n, iMax, iMax2;

    /* 1. vertices -- 0x10018AD0 over 0x10018AF0, stride 0x0C, three dwords. */
    off = BrTrackHdrU32(pTrack, BR_TRK_H_VERTICES);
    c   = BrTrackVertexCount(pTrack);
    if (br_in_range(pTrack, off, c * BR_TRK_VERTEX_STRIDE)) {
        for (i = 0; i < c; i++) {
            unsigned char *e = pTrack->pbImage + off + i * BR_TRK_VERTEX_STRIDE;
            swap_u32(e + 0); swap_u32(e + 4); swap_u32(e + 8);
        }
    }

    /* 2. sections -- 0x10018B40 over 0x10018B60, stride 0x24. NOT PORTED.
     *    The element swapper relocates three pointers and recurses; a partial
     *    version would be wrong in a way that links. Left big-endian; the
     *    header still reports the count. */

    /* 3. the grid start table -- 0x100314F9: swap(gridStart, 0x1001). */
    off = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDSTART);
    if (br_in_range(pTrack, off, BR_TRK_GRID_STARTS * 2))
        swap_u16_run(pTrack->pbImage + off, BR_TRK_GRID_STARTS);

    /* 4. the grid items -- length is the table's LAST entry, read after (3). */
    n   = BrTrackGridItemCount(pTrack);
    off = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDITEMS);
    if (br_in_range(pTrack, off, n * 2))
        swap_u16_run(pTrack->pbImage + off, n);

    /* 5. 0x10031525: scan gridItems[1 .. n-1] for its maximum, then swap that
     *    many + 1 entries of the +0x90 list. The original walks DOWNWARD from
     *    n-1 and skips index 0; both quirks are preserved -- index 0 is not
     *    considered, so a maximum stored there is invisible. */
    iMax = 0;
    if (n >= 2 && br_in_range(pTrack, BrTrackHdrU32(pTrack, BR_TRK_H_GRIDITEMS), n * 2)) {
        const unsigned char *g = pTrack->pbImage + BrTrackHdrU32(pTrack, BR_TRK_H_GRIDITEMS);
        for (i = n - 1; i >= 1; i--) {
            uint32_t v = rd_u16le(g + i * 2);
            if (v > iMax)
                iMax = v;
        }
    }
    off = BrTrackHdrU32(pTrack, BR_TRK_H_U16LIST90);
    if (br_in_range(pTrack, off, (iMax + 1) * 2))
        swap_u16_run(pTrack->pbImage + off, iMax + 1);

    /* 6. same shape again: maximum over list90[1 .. iMax]. */
    iMax2 = 0;
    if (iMax >= 1 && br_in_range(pTrack, off, (iMax + 1) * 2)) {
        const unsigned char *g = pTrack->pbImage + off;
        for (i = iMax; i >= 1; i--) {
            uint32_t v = rd_u16le(g + i * 2);
            if (v > iMax2)
                iMax2 = v;
        }
    }

    /* 7. walk the +0x8C list from iMax2 to its first zero, then swap [0, k).
     *    The zero test runs on unswapped data in the original too, which is
     *    harmless: a zero u16 reads as zero either way. */
    off = BrTrackHdrU32(pTrack, BR_TRK_H_U16LIST8C);
    if (off != 0 && off < pTrack->cbImage) {
        uint32_t k = iMax2;
        while ((off + (k + 1) * 2) <= pTrack->cbImage
               && rd_u16le(pTrack->pbImage + off + k * 2) != 0)
            k++;
        if (br_in_range(pTrack, off, k * 2))
            swap_u16_run(pTrack->pbImage + off, k);
    }

    /* 8. 0x10031660 sits here in the original. Not ported. */

    /* 9. faces -- 0x10018A70 over 0x10018A90, stride 0x08, four u16. */
    off = BrTrackHdrU32(pTrack, BR_TRK_H_FACES);
    c   = BrTrackFaceCount(pTrack);
    if (br_in_range(pTrack, off, c * BR_TRK_FACE_STRIDE)) {
        for (i = 0; i < c; i++)
            swap_u16_run(pTrack->pbImage + off + i * BR_TRK_FACE_STRIDE, 4);
    }
}

/* --- the instance pass (0x1003139E .. 0x10031459) ------------------------ */

/* For each 0x54-byte instance record the original:
 *
 *   v = (1,0,0) * M                        via 0x10034A70, a ROW-vector
 *                                          transform, so v is column 0 of M
 *   if (|v| != 0):
 *       s = 1.0f / |v|
 *       if (s*m00 == 1.0f && s*m11 == 1.0f && s*m22 == 1.0f)
 *           rec[0x4D] |= 0x20
 *       rec[0x40] = s
 *
 * The three equality tests are exact float compares in the original and stay
 * exact here. They are only ever true for a matrix that is a pure uniform
 * scale, which is what the 0x20 flag records.
 *
 * The comparison polarity is preserved: `fcom` + `test ah,0x40` + `jne` takes
 * the discard path when the length COMPARES EQUAL to 0.0f, and an unordered
 * result sets C3 too -- so a NaN length also discards. Written below as the
 * negated form for that reason, per CONVENTIONS.
 */
static void br_track_instance_pass(BrTrack *pTrack)
{
    uint32_t off = BrTrackHdrU32(pTrack, BR_TRK_H_INSTANCES);
    uint32_t c   = BrTrackInstanceCount(pTrack);
    uint32_t i, j;

    pTrack->cInstancesUniform = 0;
    if (!br_in_range(pTrack, off, c * BR_TRK_INSTANCE_STRIDE))
        return;

    for (i = 0; i < c; i++) {
        unsigned char *rec = pTrack->pbImage + off + i * BR_TRK_INSTANCE_STRIDE;
        BrMat4 m;
        BrVec3 vIn, vOut;
        float len, s;

        /* The instance record is big-endian on disc and the section pass above
         * does not touch it, so decode rather than swap in place: the record
         * also carries bytes (the +0x4D flags) that must not move. */
        for (j = 0; j < 16; j++)
            m.m[j / 4][j % 4] = bits_to_float(rd_u32be(rec + j * 4));

        vIn.x = 1.0f; vIn.y = 0.0f; vIn.z = 0.0f;
        BrMat4MulVec3Transposed(&vOut, &m, &vIn);
        len = BrVec3Length(&vOut);

        /* NOT (len != 0): NaN must take the discard side, as fcom/test ah,0x40
         * makes it in the original. */
        if (!(len != 0.0f))
            continue;

        s = 1.0f / len;
        if (s * m.m[0][0] == 1.0f && s * m.m[1][1] == 1.0f
                                  && s * m.m[2][2] == 1.0f) {
            rec[BR_TRK_INST_FLAGSOFF] = (unsigned char)
                (rec[BR_TRK_INST_FLAGSOFF] | BR_TRK_INST_UNIFORM);
            pTrack->cInstancesUniform++;
        }
        /* The original stores s in HOST order into the loaded image; so does
         * this, which is why BrTrackInstance reads +0x40 little-endian while
         * reading the matrix big-endian. Ugly, and it is what the code does. */
        wr_u32le(rec + BR_TRK_INST_SCALEOFF, float_to_bits(s));
    }
}

/* --- open / close -------------------------------------------------------- */

int BrTrackOpen(BrTrack *pTrack, const char *pszPath)
{
    FILE *pFile;
    long cb;
    uint32_t i, cInst;

    memset(pTrack, 0, sizeof(*pTrack));

    pFile = fopen(pszPath, "rb");
    if (pFile == NULL)
        return 1;
    if (fseek(pFile, 0, SEEK_END) != 0)
        goto fail;
    cb = ftell(pFile);
    if (cb < (long)BR_TRK_HEADER_SIZE)
        goto fail;
    /* 0x10031282: "Track %d is too big (%d vs. %d)" then exit(1). Refused
     * rather than fatal, because a library has no business calling exit. */
    if ((unsigned long)cb > BR_TRK_MAX_FILE)
        goto fail;
    if (fseek(pFile, 0, SEEK_SET) != 0)
        goto fail;

    pTrack->cbImage = (uint32_t)cb;
    pTrack->pbImage = (unsigned char *)malloc(pTrack->cbImage);
    if (pTrack->pbImage == NULL)
        goto fail;
    if (fread(pTrack->pbImage, 1, pTrack->cbImage, pFile) != pTrack->cbImage)
        goto fail;
    fclose(pFile);
    pFile = NULL;

    /* 0x10031B80: header into its own buffer, byte-swapped, then relocated. */
    memcpy(pTrack->abHdr, pTrack->pbImage, BR_TRK_HEADER_SIZE);
    br_track_swap_header(pTrack->abHdr);

    /* 0x1003147F: "Error: Track header size mismatch(%d != %d)". The original
     * checks this LAST, after it has already used the header; checking it here
     * costs nothing and cannot change the outcome for a valid file. */
    if (rd_u32le(pTrack->abHdr + BR_TRK_H_HEADERSIZE) != BR_TRK_HEADER_SIZE)
        goto fail;

    for (i = 0; i < BR_TRK_PTR_FIELDS; i++) {
        unsigned o = g_aBrTrackPtrFields[i];
        wr_u32le(pTrack->abHdr + o, br_reloc(rd_u32le(pTrack->abHdr + o)));
    }

    /* 0x10031459: "ERROR: instances (%d) > MAX_INSTANCES (%d)". */
    cInst = rd_u32le(pTrack->abHdr + BR_TRK_H_CINSTANCES);
    if (cInst > BR_TRK_MAX_INSTANCES)
        goto fail;

    br_track_swap_payload(pTrack);
    br_track_instance_pass(pTrack);
    return 0;

fail:
    if (pFile != NULL)
        fclose(pFile);
    BrTrackClose(pTrack);
    return 1;
}

int BrTrackOpenIndex(BrTrack *pTrack, const char *pszDir, int iTrack)
{
    /* 0x1003115D builds it with sprintf("%s%s", "tracks/", name). */
    const char *pszName = BrTrackName(iTrack);
    char szPath[512];
    size_t cchDir, cchName;

    if (pszName == NULL) {
        memset(pTrack, 0, sizeof(*pTrack));
        return 1;
    }
    if (pszDir == NULL)
        pszDir = "tracks/";
    cchDir  = strlen(pszDir);
    cchName = strlen(pszName);
    if (cchDir + cchName + 1 > sizeof szPath) {
        memset(pTrack, 0, sizeof(*pTrack));
        return 1;
    }
    memcpy(szPath, pszDir, cchDir);
    memcpy(szPath + cchDir, pszName, cchName + 1);
    return BrTrackOpen(pTrack, szPath);
}

void BrTrackClose(BrTrack *pTrack)
{
    free(pTrack->pbImage);
    memset(pTrack, 0, sizeof(*pTrack));
}

/* --- element readers ----------------------------------------------------- */

int BrTrackVertex(const BrTrack *pTrack, uint32_t i, BrVec3 *pOut)
{
    uint32_t off = BrTrackHdrU32(pTrack, BR_TRK_H_VERTICES);
    const unsigned char *e;

    if (i >= BrTrackVertexCount(pTrack))
        return 1;
    if (!br_in_range(pTrack, off, (i + 1) * BR_TRK_VERTEX_STRIDE))
        return 1;
    e = pTrack->pbImage + off + i * BR_TRK_VERTEX_STRIDE;
    /* swapped in place by pass 1, so read host order */
    pOut->x = bits_to_float(rd_u32le(e + 0));
    pOut->y = bits_to_float(rd_u32le(e + 4));
    pOut->z = bits_to_float(rd_u32le(e + 8));
    return 0;
}

int BrTrackFace(const BrTrack *pTrack, uint32_t i, uint16_t aOut[4])
{
    uint32_t off = BrTrackHdrU32(pTrack, BR_TRK_H_FACES);
    const unsigned char *e;
    int j;

    if (i >= BrTrackFaceCount(pTrack))
        return 1;
    if (!br_in_range(pTrack, off, (i + 1) * BR_TRK_FACE_STRIDE))
        return 1;
    e = pTrack->pbImage + off + i * BR_TRK_FACE_STRIDE;
    for (j = 0; j < 4; j++)
        aOut[j] = rd_u16le(e + j * 2);
    return 0;
}

int BrTrackInstance(const BrTrack *pTrack, uint32_t i, BrMat4 *pM,
                    float *pfInvScale, uint32_t *pFlags)
{
    uint32_t off = BrTrackHdrU32(pTrack, BR_TRK_H_INSTANCES);
    const unsigned char *rec;
    uint32_t j;

    if (i >= BrTrackInstanceCount(pTrack))
        return 1;
    if (!br_in_range(pTrack, off, (i + 1) * BR_TRK_INSTANCE_STRIDE))
        return 1;
    rec = pTrack->pbImage + off + i * BR_TRK_INSTANCE_STRIDE;

    if (pM != NULL) {
        for (j = 0; j < 16; j++)
            pM->m[j / 4][j % 4] = bits_to_float(rd_u32be(rec + j * 4));
    }
    if (pfInvScale != NULL)
        *pfInvScale = bits_to_float(rd_u32le(rec + BR_TRK_INST_SCALEOFF));
    if (pFlags != NULL)
        *pFlags = rec[BR_TRK_INST_FLAGSOFF];
    return 0;
}

int BrTrackGridStart(const BrTrack *pTrack, uint32_t iCell, uint16_t *pOut)
{
    uint32_t off = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDSTART);

    if (iCell >= BR_TRK_GRID_STARTS)
        return 1;
    if (!br_in_range(pTrack, off, BR_TRK_GRID_STARTS * 2))
        return 1;
    *pOut = rd_u16le(pTrack->pbImage + off + iCell * 2);
    return 0;
}

int BrTrackBounds(const BrTrack *pTrack, BrVec3 *pMin, BrVec3 *pMax)
{
    uint32_t c = BrTrackVertexCount(pTrack);
    uint32_t i;
    BrVec3 v;

    if (c == 0 || BrTrackVertex(pTrack, 0, &v) != 0)
        return 1;
    *pMin = v;
    *pMax = v;
    for (i = 1; i < c; i++) {
        if (BrTrackVertex(pTrack, i, &v) != 0)
            return 1;
        if (v.x < pMin->x) pMin->x = v.x;
        if (v.y < pMin->y) pMin->y = v.y;
        if (v.z < pMin->z) pMin->z = v.z;
        if (v.x > pMax->x) pMax->x = v.x;
        if (v.y > pMax->y) pMax->y = v.y;
        if (v.z > pMax->z) pMax->z = v.z;
    }
    return 0;
}

/* --- the .HNT/.HND sidecar (0x10031030) ---------------------------------- */

int BrTrackLoadHints(BrTrackHints *pHints, const char *pszPath)
{
    FILE *pFile;
    char szLine[1024];

    memset(pHints, 0, sizeof(*pHints));
    /* 0x10031040 seeds the budget with 0x200000 before reading anything, so a
     * file whose first line does not parse leaves that default in place. */
    pHints->cbBudget = 0x200000u;

    pFile = fopen(pszPath, "rb");
    if (pFile == NULL)
        return 1;

    if (fgets(szLine, (int)sizeof szLine, pFile) == NULL) {
        fclose(pFile);
        return 1;
    }
    /* 0x100310A8: sscanf(line, "%u", &budget) */
    if (sscanf(szLine, "%u", &pHints->cbBudget) != 1)
        pHints->cbBudget = 0x200000u;

    /* 0x100310EC: sscanf(line, "%u %x %d %d", &id, &addr, &cx, &cy), at most
     * 0x100 records -- 0x100310FD stops when the count reaches 0x100. */
    while (pHints->cHints < BR_TRK_MAX_HINTS
           && fgets(szLine, (int)sizeof szLine, pFile) != NULL) {
        BrTrackHint *ph = &pHints->aHints[pHints->cHints];
        unsigned uId = 0, uAddr = 0;
        int cx = 0, cy = 0;
        if (sscanf(szLine, "%u %x %d %d", &uId, &uAddr, &cx, &cy) != 4)
            continue;
        ph->id      = uId;
        ph->n64Addr = uAddr;
        ph->cx      = cx;
        ph->cy      = cy;
        pHints->cHints++;
    }
    fclose(pFile);
    return 0;
}
