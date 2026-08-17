/* test_br_track.c -- verify the .TRK loader against real shipped tracks.
 *
 * The assertions here are INVARIANTS OF THE FORMAT, not thresholds. Each one
 * could fail if the decode were wrong and cannot fail merely because a
 * particular track is small, sparse or oddly shaped:
 *
 *   - the header's self-declared size is 0x230;
 *   - the payload pointer is exactly the N64 address of file offset 0x230,
 *     which is an arithmetic identity between two independently stored
 *     numbers (base 0x80025C00 from the code, +0x84 from the file);
 *   - every face index addresses a vertex that exists;
 *   - the vertex array and the face array butt against their neighbours the
 *     way a linearly laid-out N64 image must;
 *   - the grid start table is monotonic and its last entry is the item total;
 *   - re-opening the same file twice produces identical results.
 *
 * The "counts are plausible" kind of assertion is deliberately absent. Three
 * such assertions have already been wrong in this project while the code was
 * right.
 */
#include "br_track.h"
#include "br_testdata.h"
#include "br_tmpfile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *pszWhat)
{
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", pszWhat);
    if (!cond)
        g_fail = 1;
}

static void report(const char *pszPath)
{
    BrTrack trk;
    BrVec3 vMin, vMax;
    uint32_t cV, cF, cS, cI, cGrid, i;
    uint32_t cBadIndex = 0, cUniform = 0;
    uint16_t aFace[4];
    int fMonotonic = 1;

    printf("\n=== %s ===\n", pszPath);
    if (BrTrackOpen(&trk, pszPath) != 0) {
        printf("  [FAIL] BrTrackOpen\n");
        g_fail = 1;
        return;
    }
    check(1, "BrTrackOpen succeeded");

    cV    = BrTrackVertexCount(&trk);
    cF    = BrTrackFaceCount(&trk);
    cS    = BrTrackSectionCount(&trk);
    cI    = BrTrackInstanceCount(&trk);
    cGrid = BrTrackGridItemCount(&trk);

    printf("  file            %u bytes\n", trk.cbImage);
    printf("  vertices        %u   (stride %u, at file 0x%X)\n",
           cV, BR_TRK_VERTEX_STRIDE, BrTrackHdrU32(&trk, BR_TRK_H_VERTICES));
    printf("  faces           %u   (stride %u, at file 0x%X)\n",
           cF, BR_TRK_FACE_STRIDE, BrTrackHdrU32(&trk, BR_TRK_H_FACES));
    printf("  sections        %u   (stride %u, at file 0x%X, still big-endian)\n",
           cS, BR_TRK_SECTION_STRIDE, BrTrackHdrU32(&trk, BR_TRK_H_SECTIONS));
    printf("  instances       %u   (of max %u; %u pure uniform scale)\n",
           cI, BR_TRK_MAX_INSTANCES, trk.cInstancesUniform);
    printf("  grid            %u cells, %u items\n",
           BR_TRK_GRID_STARTS - 1, cGrid);
    printf("  heap at file    0x%X\n", BrTrackHdrU32(&trk, BR_TRK_H_HEAPOFF));
    printf("  rgba            %02X %02X %02X %02X\n",
           trk.abHdr[BR_TRK_H_RGBA + 0], trk.abHdr[BR_TRK_H_RGBA + 1],
           trk.abHdr[BR_TRK_H_RGBA + 2], trk.abHdr[BR_TRK_H_RGBA + 3]);

    check(BrTrackHdrU32(&trk, BR_TRK_H_HEADERSIZE) == BR_TRK_HEADER_SIZE,
          "header declares its own size as 0x230");

    /* The payload pointer relocates to exactly the header size. Two numbers
     * stored in different places -- the base in the code, the pointer in the
     * file -- agreeing to the byte. */
    check(BrTrackHdrU32(&trk, BR_TRK_H_PAYLOAD) == BR_TRK_HEADER_SIZE,
          "payload pointer relocates to file offset 0x230");

    check(BrTrackFieldValid(&trk, BR_TRK_H_VERTICES, cV * BR_TRK_VERTEX_STRIDE),
          "vertex array lies inside the file");
    check(BrTrackFieldValid(&trk, BR_TRK_H_FACES, cF * BR_TRK_FACE_STRIDE),
          "face array lies inside the file");
    check(BrTrackFieldValid(&trk, BR_TRK_H_GRIDSTART, BR_TRK_GRID_STARTS * 2),
          "grid start table lies inside the file");

    /* The image is laid out linearly, so the vertex array must end at or just
     * before the face array, and the face array at or just before +0x94. */
    {
        uint32_t offV = BrTrackHdrU32(&trk, BR_TRK_H_VERTICES);
        uint32_t offF = BrTrackHdrU32(&trk, BR_TRK_H_FACES);
        uint32_t offE = BrTrackHdrU32(&trk, BR_TRK_H_FACESEND);
        uint32_t endV = offV + cV * BR_TRK_VERTEX_STRIDE;
        uint32_t endF = offF + cF * BR_TRK_FACE_STRIDE;
        printf("  vertices end 0x%X, faces start 0x%X (gap %d)\n",
               endV, offF, (int)(offF - endV));
        printf("  faces    end 0x%X, +0x94 is  0x%X (gap %d)\n",
               endF, offE, (int)(offE - endV ? offE - endF : 0));
        check(endV <= offF && (offF - endV) < 0x100,
              "vertex array abuts the face array");
        check(endF <= offE && (offE - endF) < 0x100,
              "face array abuts the object at header +0x94");
    }

    /* Every face index must name a vertex that exists. This is the strongest
     * single check available without rendering: a wrong stride, a wrong
     * element width or a missed byte-swap all break it immediately. */
    for (i = 0; i < cF; i++) {
        int j;
        if (BrTrackFace(&trk, i, aFace) != 0) {
            cBadIndex++;
            continue;
        }
        for (j = 0; j < 3; j++) {
            if (aFace[j] >= cV)
                cBadIndex++;
        }
    }
    printf("  face indices out of range: %u of %u\n", cBadIndex, cF * 3);
    check(cF > 0 && cBadIndex == 0,
          "every face's first three u16 index an existing vertex");

    /* Grid start table: monotonic non-decreasing, last entry is the total. */
    {
        uint16_t prev = 0, cur = 0;
        for (i = 0; i < BR_TRK_GRID_STARTS; i++) {
            if (BrTrackGridStart(&trk, i, &cur) != 0) {
                fMonotonic = 0;
                break;
            }
            if (i > 0 && cur < prev)
                fMonotonic = 0;
            prev = cur;
        }
        check(fMonotonic, "grid start table is non-decreasing");
        check(cur == cGrid, "last grid start entry is the item total");
    }

    if (BrTrackBounds(&trk, &vMin, &vMax) == 0) {
        printf("  extents  x [%g .. %g]  y [%g .. %g]  z [%g .. %g]\n",
               (double)vMin.x, (double)vMax.x,
               (double)vMin.y, (double)vMax.y,
               (double)vMin.z, (double)vMax.z);
        check(vMin.x <= vMax.x && vMin.y <= vMax.y && vMin.z <= vMax.z,
              "bounds are ordered");
        /* Every coordinate must be a finite number. A byte-swap that was
         * missed turns floats into denormals and NaNs, so this is a real
         * detector rather than a truism. */
        check(isfinite(vMin.x) && isfinite(vMax.x)
           && isfinite(vMin.y) && isfinite(vMax.y)
           && isfinite(vMin.z) && isfinite(vMax.z),
              "all vertex coordinates are finite");
    } else {
        check(0, "BrTrackBounds succeeded");
    }

    /* Instances. The load-time pass ORs 0x20 in, so the bit being set does NOT
     * mean the pass set it -- some records ship with it already on (desert has
     * four). The invariant that does hold, and the one worth asserting, is the
     * implication: whenever the matrix satisfies the uniform-scale identity,
     * the bit is set afterwards. Checked by recomputing the identity from the
     * matrix, i.e. from the file image, against the flag byte, i.e. from what
     * the pass wrote. Also verifies the stored 1/|column 0| round-trips.
     *
     * The earlier version of this test asserted `flags-set == pass-count` and
     * failed on desert.trk while the loader was right -- the exact "expectation
     * rather than property" mistake CONVENTIONS warns about. Kept as a comment
     * because it is the third time this project has made it. */
    {
        uint32_t cIdentity = 0, cImplicationBroken = 0;
        for (i = 0; i < cI; i++) {
            BrMat4 m;
            BrVec3 vIn, vOut;
            float s = 0.0f, len;
            uint32_t flags = 0;
            int fIdentity;

            if (BrTrackInstance(&trk, i, &m, &s, &flags) != 0)
                continue;
            vIn.x = 1.0f; vIn.y = 0.0f; vIn.z = 0.0f;
            BrMat4MulVec3Transposed(&vOut, &m, &vIn);
            len = BrVec3Length(&vOut);

            fIdentity = (len != 0.0f)
                     && (1.0f / len) * m.m[0][0] == 1.0f
                     && (1.0f / len) * m.m[1][1] == 1.0f
                     && (1.0f / len) * m.m[2][2] == 1.0f;
            if (fIdentity) {
                cIdentity++;
                if (!(flags & BR_TRK_INST_UNIFORM))
                    cImplicationBroken++;
                /* the scale the pass stored must be the one recomputed here */
                if (s != 1.0f / len)
                    cImplicationBroken++;
            }
            if (flags & BR_TRK_INST_UNIFORM)
                cUniform++;
        }
        printf("  instances: %u carry 0x20, %u satisfy the identity, "
               "%u gained it on load\n",
               cUniform, cIdentity, trk.cInstancesUniform);
        check(cImplicationBroken == 0,
              "every uniform-scale matrix ends up flagged, with its 1/|scale|");
        check(cIdentity == trk.cInstancesUniform,
              "the pass flagged exactly the matrices that satisfy the identity");
        check(cUniform >= trk.cInstancesUniform,
              "the flag is OR-ed in, so bits already in the file survive");
    }

    BrTrackClose(&trk);
    check(trk.pbImage == NULL && trk.cbImage == 0, "close releases the image");
}

/* ====================================================================== *
 * LITERALS FROM race.trk's OWN BYTES
 *
 * Everything in report() above is a relation between two numbers this
 * suite reads through the SAME loader -- which means a decoder that is
 * uniformly wrong satisfies all of them.  The instance block was the
 * clearest case: it recomputed the uniform-scale identity from the matrix
 * and compared it against the flag and the scale the loader had computed
 * from the same matrix, and BOTH sides come out of rd_u32be.  Make
 * rd_u32be return 0 for every read and every matrix becomes the zero
 * matrix, every length becomes zero, the pass skips every record and the
 * test compares 0 against 0, in the green.
 *
 * These literals were read out of the file with xxd and a byte-wise
 * decode script, not taken from any comment in the tree.  race.trk is a
 * big-endian N64 image; file offset 0 is N64 0x80025C00, so a header
 * pointer's file offset is the stored address minus that base.
 *
 *   +0x08 cFaces      0x0A2E =  2606        +0x0C faces      0x80087D38
 *   +0x10 cVertices   0x0924 =  2340        +0x14 vertices   0x80080F88
 *   +0x18 cSections   0x0095 =   149        +0x60 instances  0x800C3A68
 *   +0x64 cInstances  0x0182 =   386
 *
 * so the instance array is at file 0x0009DE68 and the vertex array at
 * file 0x0005B388.
 *
 * Floats are compared as BIT PATTERNS, because that is what is actually
 * in the file and a decimal literal invites a rounding argument.
 * ====================================================================== */

#define TRK_RACE "testdata/tracks/race.trk"

static uint32_t f2b(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

/* Instance 1 of race.trk, sixteen big-endian dwords at file 0x0009DEBC:
 * a uniform 1/64 scale with the translation (856, 804, 4) in the LAST row.
 * The last row is what proves the matrix is read in file order rather than
 * transposed, and 0x3C800000 == 0.015625f is what proves rd_u32be assembles
 * its four bytes the right way round. */
static const uint32_t g_aRaceInst1[16] = {
    0x3C800000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x3C800000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x3C800000u, 0x00000000u,
    0x44560000u, 0x44490000u, 0x40800000u, 0x3F800000u
};

static void race_literals(void)
{
    BrTrack  trk;
    BrMat4   m;
    BrVec3   v;
    uint16_t aFace[4];
    float    s;
    uint32_t flags, i, cWrong;

    printf("\n=== %s: literals ===\n", TRK_RACE);
    if (BrTrackOpen(&trk, TRK_RACE) != 0) {
        printf("SKIP literals: %s not extracted\n", TRK_RACE);
        return;
    }

    /* --- header counts and offsets ---------------------------------- */
    check(BrTrackFaceCount(&trk)     == 2606u, "race.trk has 2606 faces");
    check(BrTrackVertexCount(&trk)   == 2340u, "race.trk has 2340 vertices");
    check(BrTrackSectionCount(&trk)  ==  149u, "race.trk has 149 sections");
    check(BrTrackInstanceCount(&trk) ==  386u, "race.trk has 386 instances");
    check(BrTrackGridItemCount(&trk) == 6953u, "race.trk grid holds 6953 items");
    check(BrTrackHdrU32(&trk, BR_TRK_H_VERTICES)  == 0x0005B388u,
          "vertex array relocates to file 0x5B388");
    check(BrTrackHdrU32(&trk, BR_TRK_H_FACES)     == 0x00062138u,
          "face array relocates to file 0x62138");
    check(BrTrackHdrU32(&trk, BR_TRK_H_INSTANCES) == 0x0009DE68u,
          "instance array relocates to file 0x9DE68");
    /* The RGBA quad the swapper skips: 7F 7F 7F 00 on disc, and it must
     * come back in THAT order, not reversed. */
    check(trk.abHdr[BR_TRK_H_RGBA + 0] == 0x7F
       && trk.abHdr[BR_TRK_H_RGBA + 1] == 0x7F
       && trk.abHdr[BR_TRK_H_RGBA + 2] == 0x7F
       && trk.abHdr[BR_TRK_H_RGBA + 3] == 0x00,
          "the +0x80 RGBA quad is 7F 7F 7F 00, unswapped");

    /* --- one vertex, one face --------------------------------------- */
    check(BrTrackVertex(&trk, 0, &v) == 0, "vertex 0 reads");
    check(f2b(v.x) == 0x44455CA9u && f2b(v.y) == 0x44523DD4u
       && f2b(v.z) == 0x3B31FEEBu,
          "vertex 0 is 44455CA9 44523DD4 3B31FEEB (789.448, 840.966, 0.002716)");
    check(BrTrackVertex(&trk, 1, &v) == 0, "vertex 1 reads");
    check(f2b(v.z) == 0x42A0292Eu, "vertex 1 z is 42A0292E (80.0804)");
    check(BrTrackFace(&trk, 1, aFace) == 0, "face 1 reads");
    check(aFace[0] == 3 && aFace[1] == 2 && aFace[2] == 1 && aFace[3] == 3,
          "face 1 is (3, 2, 1, 3)");

    /* --- the instance block, element by element ---------------------- */
    check(BrTrackInstance(&trk, 1, &m, &s, &flags) == 0, "instance 1 reads");
    cWrong = 0;
    for (i = 0; i < 16; i++) {
        if (f2b(m.m[i / 4][i % 4]) != g_aRaceInst1[i])
            cWrong++;
    }
    check(cWrong == 0, "instance 1's sixteen floats match the file's dwords");
    check(m.m[0][0] == 0.015625f && m.m[1][1] == 0.015625f
       && m.m[2][2] == 0.015625f,
          "instance 1's diagonal is exactly 1/64");
    check(m.m[3][0] == 856.0f && m.m[3][1] == 804.0f && m.m[3][2] == 4.0f
       && m.m[3][3] == 1.0f,
          "instance 1's translation row is (856, 804, 4, 1)");
    /* 1/|column 0| for a 1/64 diagonal is 64, and the pass must have
     * written that dword into the image at record +0x40. */
    check(f2b(s) == 0x42800000u, "instance 1's stored 1/|scale| is 64.0f");
    /* Disc flags byte is 0x44; the pass ORs 0x20 in, so it reads 0x64. */
    check(flags == 0x64u, "instance 1's flag byte is 0x44 | 0x20 == 0x64");

    /* Instance 0's matrix is ALL ZERO on disc, so its column-0 length is
     * zero and the pass takes the discard path: no flag, no scale write.
     * The `if (!(len != 0.0f)) continue;` has no other test. */
    check(BrTrackInstance(&trk, 0, &m, &s, &flags) == 0, "instance 0 reads");
    check(f2b(m.m[0][0]) == 0u && f2b(m.m[3][3]) == 0u,
          "instance 0's matrix is entirely zero on disc");
    check(flags == 0x00u, "instance 0 gains no flag, because it is discarded");

    /* Instance 2: disc flags 0x40, same 1/64 scale, translation (788, 840, 1). */
    check(BrTrackInstance(&trk, 2, &m, &s, &flags) == 0, "instance 2 reads");
    check(m.m[3][0] == 788.0f && m.m[3][1] == 840.0f && m.m[3][2] == 1.0f,
          "instance 2's translation row is (788, 840, 1)");
    check(flags == 0x60u, "instance 2's flag byte is 0x40 | 0x20 == 0x60");

    check(trk.cInstancesUniform == 385u,
          "385 of race.trk's 386 instances are pure uniform scales");

    BrTrackClose(&trk);
}

/* ====================================================================== *
 * THE SCALE WRITE, ON A DOCTORED COPY
 *
 * wr_u32le writes four bytes and only one call site's result is ever read
 * back: the 1/|scale| the instance pass stores at record +0x40.  On every
 * shipped track that value is 64.0f == 0x42800000, whose low byte is zero
 * -- and the four bytes already in the file there are zero too, so
 * DELETING `p[0] = v & 0xFF` changes nothing observable.  The relocation
 * call site cannot catch it either: the N64 base 0x80025C00 has a zero low
 * byte, so subtracting it never changes byte 0 of any pointer.
 *
 * So the byte has to be made to matter.  This loads a byte-for-byte copy
 * of race.trk with a non-zero sentinel poked into two records' +0x40 and
 * asserts what the pass does with it:
 *
 *   instance 1  -- has a length, so the pass MUST overwrite all four
 *                  bytes with 0x42800000;
 *   instance 0  -- zero matrix, discarded, so the sentinel MUST survive
 *                  intact.
 * ====================================================================== */

#define TRK_INST_ARRAY   0x0009DE68u   /* header +0x60, relocated */
#define TRK_SENTINEL     0x11223344u

static void race_scale_write(void)
{
    const char    *pszTmp = BrTmpPath(0, "test_br_track_poked");
    unsigned char *pb;
    long           cb;
    size_t         off;
    FILE          *f;
    BrTrack        trk;
    float          s;
    int            fWrote = 0;

    printf("\n=== %s: the +0x40 scale write ===\n", TRK_RACE);

    f = fopen(TRK_RACE, "rb");
    if (f == NULL) {
        printf("SKIP scale write: %s not extracted\n", TRK_RACE);
        return;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); check(0, "seek"); return; }
    cb = ftell(f);
    rewind(f);
    pb = (unsigned char *)malloc((size_t)cb);
    if (pb == NULL || fread(pb, 1, (size_t)cb, f) != (size_t)cb) {
        free(pb); fclose(f); check(0, "read race.trk"); return;
    }
    fclose(f);

    /* The four bytes are zero on disc; that is exactly why they hide a
     * dropped write, and exactly why they are safe to overwrite here. */
    off = TRK_INST_ARRAY + 0 * BR_TRK_INSTANCE_STRIDE + BR_TRK_INST_SCALEOFF;
    check(pb[off] == 0 && pb[off + 1] == 0 && pb[off + 2] == 0
       && pb[off + 3] == 0, "instance 0's +0x40 is four zero bytes on disc");
    for (fWrote = 0; fWrote < 2; fWrote++) {
        off = TRK_INST_ARRAY + (size_t)fWrote * BR_TRK_INSTANCE_STRIDE
            + BR_TRK_INST_SCALEOFF;
        pb[off + 0] = (unsigned char)(TRK_SENTINEL & 0xFFu);
        pb[off + 1] = (unsigned char)((TRK_SENTINEL >> 8) & 0xFFu);
        pb[off + 2] = (unsigned char)((TRK_SENTINEL >> 16) & 0xFFu);
        pb[off + 3] = (unsigned char)((TRK_SENTINEL >> 24) & 0xFFu);
    }

    f = fopen(pszTmp, "wb");
    if (f == NULL) { free(pb); check(0, "open scratch copy"); return; }
    fWrote = (fwrite(pb, 1, (size_t)cb, f) == (size_t)cb);
    fclose(f);
    free(pb);
    if (!fWrote) { remove(pszTmp); check(0, "write scratch copy"); return; }

    if (BrTrackOpen(&trk, pszTmp) != 0) {
        remove(pszTmp);
        check(0, "the doctored copy still loads");
        return;
    }
    s = 0.0f;
    check(BrTrackInstance(&trk, 1, NULL, &s, NULL) == 0, "poked instance 1 reads");
    check(f2b(s) == 0x42800000u,
          "the pass overwrites ALL FOUR bytes of +0x40 with 64.0f");
    s = 0.0f;
    check(BrTrackInstance(&trk, 0, NULL, &s, NULL) == 0, "poked instance 0 reads");
    check(f2b(s) == TRK_SENTINEL,
          "a discarded instance's +0x40 is left exactly as it was");
    BrTrackClose(&trk);
    remove(pszTmp);
}

int main(int argc, char **argv)
{
    const char *pszPath = (argc > 1) ? argv[1] : "testdata/tracks/race.trk";
    BrTrackHints hints;

    BR_REQUIRE_TESTDATA(pszPath, "br_track");

    /* the name table at 0x100B78C0, duplicates and all */
    check(strcmp(BrTrackName(0), "desert.trk") == 0, "name[0] is desert.trk");
    check(strcmp(BrTrackName(5), "race.trk") == 0,   "name[5] is race.trk");
    check(strcmp(BrTrackName(6), BrTrackName(0)) == 0,
          "indices 6..11 repeat 0..5, as the original table does");
    check(strcmp(BrTrackName(13), BrTrackName(14)) == 0,
          "bonus.trk appears twice, at 13 and 14");
    check(BrTrackName(BR_TRK_NAME_COUNT) == NULL, "name table is bounded");

    report(pszPath);
    if (argc > 2)
        report(argv[2]);

    /* These two are keyed to race.trk's own bytes, so they run against
     * that file regardless of what was passed on the command line. */
    race_literals();
    race_scale_write();

    /* The .HNT sidecar, if it was extracted. Its absence is not a failure --
     * a missing asset skips, per the asset policy. */
    if (BrTrackLoadHints(&hints, "testdata/tracks/desert.hnt") == 0) {
        printf("\n=== testdata/tracks/desert.hnt ===\n");
        printf("  budget %u bytes, %u texture hints\n",
               hints.cbBudget, hints.cHints);
        check(hints.cbBudget != 0, "hint file supplies a memory budget");
    } else {
        printf("\nSKIP hints: testdata/tracks/desert.hnt not extracted\n");
    }
    if (BrTrackLoadHints(&hints, "testdata/tracks/coast.hnt") == 0) {
        uint32_t i, cBad = 0;
        printf("=== testdata/tracks/coast.hnt ===\n");
        printf("  budget %u bytes, %u texture hints\n",
               hints.cbBudget, hints.cHints);
        for (i = 0; i < hints.cHints; i++) {
            if ((hints.aHints[i].n64Addr & 0x80000000u) == 0)
                cBad++;
        }
        check(hints.cHints > 0 && cBad == 0,
              "every hint's second column is a KSEG0 address");
    }

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
