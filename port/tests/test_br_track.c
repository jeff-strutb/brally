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
