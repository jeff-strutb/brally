/* br_collgrid.c -- the collision grid's storage and its binding to a track.
 *
 * See br_collgrid.h for what this corrects in CONVENTIONS.md and for where
 * the five source tables live in the .TRK header.
 */
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_collrespsolve.h"   /* br_globals: its objects */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice1_08.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* A key BrCollGridCellAcquire can never compute.  Its key is
 * `(int16_t)(ix + (iy << 6))` with ix and iy both derived from a float, and
 * it compares the STORED key zero-extended against the requested key
 * sign-extended (the defect slice6_73.c reproduces).  0 is a perfectly
 * reachable key, so the cache cannot be invalidated by zeroing it; the stamps
 * are what make a cell a victim, so those are zeroed instead and the keys are
 * pushed somewhere the sign-extension mismatch guarantees a miss. */
/* (port-only BrCollGridInvalidate removed) */


/* (port-only BrCollGridBind removed) */


/* (port-only BrCollGridRelease removed) */


/* (port-only BrCollGridLoaded removed) */


/* 0x100686D0 (D3D twin 0x1006F720, port body in slice6_73.c) */
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
/* WHAT IT DOES: get the collision-grid cell covering a point, reusing the
 * least recently used of the four cache slots when it is not already
 * loaded. A freshly loaded slot is filled with one plane record per triangle
 * of that grid square: the triangle's three vertex pointers, its index and
 * surface bits, its unit normal (V1-V0) x (V2-V0) and its plane constant. */
/* @implements 0x100686D0 glide BrCollGridCellAcquire */
short BrCollGridCellAcquire(float x, float y)
{
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    typedef unsigned int  (*BrGridSampleG)(float, float);
    typedef unsigned short (*BrCursorNextG)(unsigned short *);
    BrCollPlane *p;
    const uint16_t *aFace = BR_PTR32(const uint16_t *, g_brTrkHdr.aFaces);
    BrVec3 *aVtx = BR_PTR32(BrVec3 *, g_brTrkHdr.aVertices);
    const uint8_t *aKind = BR_PTR32(const uint8_t *, g_brTrkHdr.aFaceKind);
    unsigned int best, packed;
    unsigned short cur[2], tri, n;
    int key, i, victim;

    ++g_brCrPlane.cellTick;
    key = (int)x / 32 + ((int)y / 32 << 6);
    best = 0x40000000u;
    for (i = 0; i < 4; i++) {
        if (g_brCrPlane.aCellKey[i] == (short)key) {
            g_brCrPlane.aCellAge[i] = g_brCrPlane.cellTick;
            return (short)i;
        }
        if (g_brCrPlane.aCellAge[i] < best) {
            victim = i;
            best = g_brCrPlane.aCellAge[i];
        }
    }
    g_brCrPlane.aCellKey[victim] = (unsigned short)key;
    g_brCrPlane.aCellAge[victim] = g_brCrPlane.cellTick;
    n = 0;
    p = DAT_11773698[victim];
    packed = ((BrGridSampleG)BrGrid64Sample)(x, y);
    cur[0] = (unsigned short)packed;
    cur[1] = (unsigned short)(packed >> 16);
    if (packed != 0) {
        while ((tri = ((BrCursorNextG)BrU16CursorNext)(cur)) != 0) {
            BrVec3 a, b;

            p->pV0 = &aVtx[aFace[tri * 4]];
            p->pV1 = &aVtx[aFace[tri * 4 + 1]];
            p->pV2 = &aVtx[aFace[tri * 4 + 2]];
            p->flags = (uint8_t)(aKind[tri] & 7);
            p->tri = tri;
            a.x = p->pV1->x - p->pV0->x;
            a.y = p->pV1->y - p->pV0->y;
            a.z = p->pV1->z - p->pV0->z;
            b.x = p->pV2->x - p->pV0->x;
            b.y = p->pV2->y - p->pV0->y;
            b.z = p->pV2->z - p->pV0->z;
            p->nx = a.y * b.z - a.z * b.y;
            p->ny = a.z * b.x - a.x * b.z;
            p->nz = a.x * b.y - a.y * b.x;
            BrVec3Normalise((BrVec3 *)(void *)&p->nx);
            p->d = -((p->nx * p->pV0->x + p->pV0->y * p->ny) + p->pV0->z * p->nz);
            ++n;
            p++;
        }
    }
    g_brCrPlane.aCellCount[victim] = n;
    return (short)victim;
}
