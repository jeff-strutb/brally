/* br_collgrid.c -- the collision grid's storage and its binding to a track.
 *
 * See br_collgrid.h for what this corrects in CONVENTIONS.md and for where
 * the five source tables live in the .TRK header.
 */
#include <string.h>

#include "br_collgrid.h"

/* 0x11750338 and 0x117554A0.  DEVIATION, and it is the one this module does
 * NOT fix: in the original these ARE g_BrFx1750338 and g_BrX17554A0, i.e. one
 * address with two uses.  On LP64 a BrCollPlane is 40 bytes and a BrFxRecord
 * is 32, so 600 records cannot carry both views and the two objects have to
 * be separate here.  Nothing in this port runs the fx system and the
 * collision grid at once, so the divergence is currently unobservable -- but
 * it is a divergence, not a resolution.  The fix remains the one
 * CONVENTIONS.md names: make BrCollPlane store vertex INDICES, at which point
 * this array can simply BE g_BrFx1750338. */
static BrCollPlane s_aPlane[BR73_COLL_CELLS * BR_COLL_CELL_PLANES];
static uint16_t    s_aCount[BR73_COLL_CELLS];

BrCollPlane    *g_pBrCollGrid      = NULL;
const uint16_t *g_pBrCollGridCount = NULL;

/* A key BrCollGridCellAcquire can never compute.  Its key is
 * `(int16_t)(ix + (iy << 6))` with ix and iy both derived from a float, and
 * it compares the STORED key zero-extended against the requested key
 * sign-extended (the defect slice6_73.c reproduces).  0 is a perfectly
 * reachable key, so the cache cannot be invalidated by zeroing it; the stamps
 * are what make a cell a victim, so those are zeroed instead and the keys are
 * pushed somewhere the sign-extension mismatch guarantees a miss. */
static void BrCollGridInvalidate(void)
{
    int i;
    for (i = 0; i < BR73_COLL_CELLS; ++i) {
        g_aBrCollGridKey[i]   = (int16_t)0x8000;
        g_aBrCollGridStamp[i] = 0u;
        s_aCount[i]           = 0u;
    }
    memset(s_aPlane, 0, sizeof s_aPlane);
}

int BrCollGridBind(const BrTrack *pTrack)
{
    uint32_t offFaces, offVerts, offFlags, offItems, offStart;
    uint32_t cFaces, cVerts;

    BrCollGridRelease();

    if (pTrack == NULL || pTrack->pbImage == NULL) {
        return 1;
    }

    cFaces = BrTrackFaceCount(pTrack);
    cVerts = BrTrackVertexCount(pTrack);

    offFaces = BrTrackHdrU32(pTrack, BR_TRK_H_FACES);
    offVerts = BrTrackHdrU32(pTrack, BR_TRK_H_VERTICES);
    offFlags = BrTrackHdrU32(pTrack, BR_TRK_H_FACESEND);
    offItems = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDITEMS);
    offStart = BrTrackHdrU32(pTrack, BR_TRK_H_GRIDSTART);

    /* Every one must address the image.  The face and vertex arrays are
     * checked at their full extent; the grid start table is 0x1001 u16 (64x64
     * cells plus the closing total, BR_TRK_GRID_STARTS); the item table and
     * the flag array are checked for one element, because their lengths are
     * data-dependent (the item count is gridStart[0x1000], and the flag array
     * is indexed by triangle). */
    if (!BrTrackFieldValid(pTrack, BR_TRK_H_FACES,
                           cFaces * BR_TRK_FACE_STRIDE) ||
        !BrTrackFieldValid(pTrack, BR_TRK_H_VERTICES,
                           cVerts * BR_TRK_VERTEX_STRIDE) ||
        !BrTrackFieldValid(pTrack, BR_TRK_H_FACESEND, cFaces) ||
        !BrTrackFieldValid(pTrack, BR_TRK_H_GRIDITEMS, 2u) ||
        !BrTrackFieldValid(pTrack, BR_TRK_H_GRIDSTART,
                           2u * BR_TRK_GRID_STARTS)) {
        return 1;
    }

    /* The vertex array is three host-order f32 on a 12-byte stride after
     * br_track.c's pass 1, which is exactly BrVec3.  The cast is a rename,
     * not a reinterpretation -- but the image is `unsigned char *`, so the
     * alignment has to hold: BR_TRK_H_VERTICES is a relocated N64 address and
     * every shipped track puts the array on a 4-byte boundary.  A misaligned
     * one is rejected rather than trusted. */
    if ((offVerts & 3u) != 0u || (offFaces & 1u) != 0u ||
        (offItems & 1u) != 0u || (offStart & 1u) != 0u) {
        return 1;
    }

    g_pBrCollTriIdx   = (const uint16_t *)(const void *)
                        (pTrack->pbImage + offFaces);
    g_pBrCollVerts    = (BrVec3 *)(void *)(pTrack->pbImage + offVerts);
    g_pBrCollTriFlags = (const uint8_t *)(pTrack->pbImage + offFlags);
    g_pBrTriTable     = (const uint16_t *)(const void *)
                        (pTrack->pbImage + offItems);
    g_pBrGrid64       = (const uint16_t *)(const void *)
                        (pTrack->pbImage + offStart);

    g_pBrCollGrid      = s_aPlane;
    g_pBrCollGridCount = s_aCount;

    BrCollGridInvalidate();
    return 0;
}

void BrCollGridRelease(void)
{
    g_pBrCollTriIdx   = NULL;
    g_pBrCollVerts    = NULL;
    g_pBrCollTriFlags = NULL;
    g_pBrTriTable     = NULL;
    g_pBrGrid64       = NULL;

    g_pBrCollGrid      = NULL;
    g_pBrCollGridCount = NULL;

    BrCollGridInvalidate();
}

int BrCollGridLoaded(int *pCells, int *pPlanes)
{
    int i, cells = 0, planes = 0;

    for (i = 0; i < BR73_COLL_CELLS; ++i) {
        if (s_aCount[i] != 0u) {
            ++cells;
            planes += (int)s_aCount[i];
        }
    }
    if (pCells  != NULL) *pCells  = cells;
    if (pPlanes != NULL) *pPlanes = planes;
    return (g_pBrCollGrid != NULL);
}

#ifdef BR_MATCHING_BUILD
/* 0x100686D0 (D3D twin 0x1006F720, port body in slice6_73.c) */
/* WHAT IT DOES: get the collision-grid cell covering a point, reusing the
 * least recently used of the four cache slots when it is not already
 * loaded. A freshly loaded slot is filled with one plane record per triangle
 * of that grid square: the triangle's three vertex pointers, its index and
 * surface bits, its unit normal (V1-V0) x (V2-V0) and its plane constant. */
/* Transcribed from the Glide bytes: four slots, keys (u16) at 0x11778838,
 * stamps at 0x11778828, the clock at 0x11778840 bumped in place, 150 plane
 * records of 0x20 per slot at 0x11773698 and the plane counts at
 * 0x11778800. The stored key is compared zero-extended against the new key
 * sign-extended, so a negative key never hits (the original's defect). The
 * victim is uninitialised if no stamp is below 0x40000000, as in the
 * original. BrGrid64Sample and BrU16CursorNext take (x, y) and (&cursor);
 * the triangle table, vertex array and flag bytes are behind the pointers
 * at 0x106EECE4, 0x106EECEC and 0x106EED6C.
 * The cross product's operands are two BrVec3 locals (the original's
 * 0x10 frame is the victim plus one of them; the other lives in registers),
 * the vertices are read back through the record fields rather than copied
 * to locals, and the surface bits are stored before the triangle index. */
/* @implements 0x100686D0 glide BrCollGridCellAcquire */
short BrCollGridCellAcquire(float x, float y)
{
    extern unsigned short DAT_11778838[4];
    extern unsigned int   DAT_11778828[4];
    extern unsigned int   DAT_11778840;
    extern unsigned short DAT_11778800[4];
    extern unsigned char  DAT_11773698[];
    extern unsigned short *DAT_106eece4;
    extern unsigned char  *DAT_106eecec;
    extern unsigned char  *DAT_106eed6c;
    typedef unsigned int  (*BrGridSampleG)(float, float);
    typedef unsigned short (*BrCursorNextG)(unsigned short *);
    float *p;
    unsigned int best, packed;
    unsigned short cur[2], tri, n;
    int key, i, victim;

    ++DAT_11778840;
    key = (int)x / 32 + ((int)y / 32 << 6);
    best = 0x40000000u;
    for (i = 0; i < 4; i++) {
        if (DAT_11778838[i] == (short)key) {
            DAT_11778828[i] = DAT_11778840;
            return (short)i;
        }
        if (DAT_11778828[i] < best) {
            victim = i;
            best = DAT_11778828[i];
        }
    }
    DAT_11778838[victim] = (unsigned short)key;
    DAT_11778828[victim] = DAT_11778840;
    n = 0;
    p = (float *)(DAT_11773698 + victim * 0x12C0);
    packed = ((BrGridSampleG)BrGrid64Sample)(x, y);
    cur[0] = (unsigned short)packed;
    cur[1] = (unsigned short)(packed >> 16);
    if (packed != 0) {
        while ((tri = ((BrCursorNextG)BrU16CursorNext)(cur)) != 0) {
            BrVec3 a, b;

            *(float **)(p + 4) = (float *)(DAT_106eecec + DAT_106eece4[tri * 4] * 12);
            *(float **)(p + 5) = (float *)(DAT_106eecec + DAT_106eece4[tri * 4 + 1] * 12);
            *(float **)(p + 6) = (float *)(DAT_106eecec + DAT_106eece4[tri * 4 + 2] * 12);
            *((unsigned char *)(p + 7) + 2) = (unsigned char)(DAT_106eed6c[tri] & 7);
            *(unsigned short *)(p + 7) = tri;
            a.x = (*(float **)(p + 5))[0] - (*(float **)(p + 4))[0];
            a.y = (*(float **)(p + 5))[1] - (*(float **)(p + 4))[1];
            a.z = (*(float **)(p + 5))[2] - (*(float **)(p + 4))[2];
            b.x = (*(float **)(p + 6))[0] - (*(float **)(p + 4))[0];
            b.y = (*(float **)(p + 6))[1] - (*(float **)(p + 4))[1];
            b.z = (*(float **)(p + 6))[2] - (*(float **)(p + 4))[2];
            p[0] = a.y * b.z - a.z * b.y;
            p[1] = a.z * b.x - a.x * b.z;
            p[2] = a.x * b.y - a.y * b.x;
            BrVec3Normalise((BrVec3 *)(void *)p);
            p[3] = -((p[0] * (*(float **)(p + 4))[0] + (*(float **)(p + 4))[1] * p[1]) + (*(float **)(p + 4))[2] * p[2]);
            ++n;
            p += 8;
        }
    }
    DAT_11778800[victim] = n;
    return (short)victim;
}
#endif /* BR_MATCHING_BUILD */
