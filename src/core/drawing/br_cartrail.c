/* br_cartrail.c -- drawing: the per-car trail ribbons (tyre marks / exhaust
 * history) stepped once per frame.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * One function, 0x10032E40.  Each car carries four ribbons of nine vertex
 * records; every frame the ribbon origin follows the car, the records shift
 * back one slot, the head record takes a fresh colour keyed by the ribbon's
 * kind, and every record drifts by the frame's displacement.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

typedef struct BrVec3 { float x, y, z; } BrVec3;
typedef struct BrMat4 { float m[4][4]; } BrMat4;

void BrMat4Translate(BrMat4 *pM, float dx, float dy, float dz);   /* 0x1002A7F0 */
void BrMat4Scale(BrMat4 *pM, float sx, float sy, float sz);       /* 0x1002A7A0 */
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut); /* 0x10029D70 */

/* One entrant slot of the 0x80-byte driver table: the car record pointer
 * first, the rest is not read here. */
typedef struct BrDriverSlot {
    unsigned char *pCar;
    unsigned char  pad[0x7C];
} BrDriverSlot;
extern BrDriverSlot g_BrDriverSlots[];               /* 0x10AF0858 */

extern int32_t g_brRaceNDriver;                      /* 0x100B2F00 */
extern int32_t g_brRacePaused;                       /* 0x105CCB5C */
extern int32_t g_brRaceReplay;                       /* 0x105CCB88 */
extern int32_t g_BrTrackKind;                        /* 0x100B3014 */
extern int32_t g_BrDrawWheelAlt;                     /* 0x106ED6B0 */
extern int32_t g_BrTrailNight;                       /* 0x106ED6B4 */
extern int32_t g_BrTrailDim;                         /* 0x106ED6AC */
extern float   g_BrFrameDelta;                       /* 0x106E9D8C */

extern float g_BrTrailScaleK;                        /* 0x1007759C */
extern float g_BrTrailStepK;                         /* 0x100775A0 */
extern float g_BrTrailFixK;                          /* 0x10077598 fixed-point scale */
extern float g_BrTrailUnfixK;                        /* 0x100775A4 its reciprocal */
extern float g_BrTrailDimK;                          /* 0x10077584 */
extern float g_BrTrailNightMulK;                     /* 0x100775A8 */
extern float g_BrTrailNightBaseK;                    /* 0x100775AC */

/* A ribbon vertex record, 0x80 bytes: two vertex pairs are touched here. */
typedef struct BrTrailVert {
    BrVec3 p;                     /* +0x00 */
    float  w;                     /* +0x0C */
    float  u;                     /* +0x10 */
    BrVec3 c;                     /* +0x14 */
} BrTrailVert;                    /* 0x20 */

typedef struct BrTrailRec {
    BrTrailVert v[4];
} BrTrailRec;                     /* 0x80 */

typedef struct BrTrailOfs {
    short s[12];
} BrTrailOfs;                     /* 0x18 */

/* Car record, the parts this function touches. */
typedef struct BrTrailCar {
    unsigned char pad0[0x30];
    BrVec3        pos;            /* +0x30 */
    unsigned char pad1[0x10DC - 0x3C];
    int           active[4];      /* +0x10DC one per ribbon */
    unsigned char pad2[0x1120 - 0x10EC];
    BrTrailRec    rec[36];        /* +0x1120 nine per ribbon */
    BrTrailOfs    ofs[36];        /* +0x2320 */
    short         kind[36];       /* +0x2680 */
    BrVec3        origin;         /* +0x26C8 */
    BrMat4        mat;            /* +0x26D4 */
} BrTrailCar;

/* WHAT IT DOES: steps every entrant's four trail ribbons for one frame.
 * Skipped while paused or in replay mode.  Per car: the ribbon origin is
 * pulled toward the car position by a fixed-point step, a translate-by-
 * origin matrix scaled by 1/127 is rebuilt, then for each ribbon whose flag
 * is set the head record is recoloured by its kind (0: dull grey; 3: bright
 * at night only; 4: track-kind colours, dimmed or night-inverted; anything
 * else copies the next record and retires the kind to -1) and copied one
 * slot down.  Every record then shifts back one slot (only the last slot
 * when the ribbon is idle), gets its width pair and u coordinate from its
 * slot number, and drifts: the head slots by the frame displacement, the
 * rest by their per-record fixed-point offsets, sinking the z offset while
 * the record stays above the head. */
/* T2 residue (2026-09-13): 1875/1881 B, 438/454 insns, frame 0x88 exact,
 * regnorm 20+36.  What is left is all inside the ribbon loop:
 *  - the four colour triples (night / dim, two vertices each): the original
 *    issues the three loads, then the three multiplies, then the subtracts,
 *    then the stores (a latency-pipelined x87 schedule); this build stores
 *    each component as soon as it is ready.  Load temporaries, a struct
 *    pointer, a float pointer, and both pointer/struct role assignments for
 *    the two vertices were tried; the pipelined shape never appears.
 *  - the second vertex's colour is addressed off the same register as the
 *    first ([edx+0x20]) in the original; every spelling here gives it its
 *    own induction register.
 *  - `kk + 0x1340` (the short index base) is a per-ribbon temporary in the
 *    original and becomes a loop induction variable here.
 *  - the fixed-point multiplies: the original alternates register and
 *    memory forms for iScale (edi vs [esp+0x1c]); the operand order in the
 *    source does not move them (canonicalised).
 * Matched: the origin update with named nx/ny/nz (the fst/reload pairs),
 * the (float)dx casts as CSE'd expressions (fst right after fild; local-
 * first x87 add order in the drift block), the switch laid out 4, 3,
 * default, 0 with case 3 falling into default, the shorts as short locals
 * widened at the fild, idx = kk + j as the one index expression of the
 * record loop.
 * @t4-pass 0x10032E40 v17 2026-09-13 probes 20 bytes -6 insns -16 regions 9 rows 20+36 census no
 */
/* @implements 0x10032E40 glide BrCarTrailStep */
void BrCarTrailStep(void)
{
    int          iScale, step, i, kk, j, idx, t;
    short        dx, dy, dz;
    float        nx, ny, nz, v, x, y, z;
    BrTrailCar  *car;
    int         *pl;
    short       *ps;
    BrTrailOfs  *pq;
    float       *c0;
    BrMat4       m;
    BrTrailRec  *r;
    BrTrailOfs  *o;

    iScale = (int)(g_BrFrameDelta * g_BrTrailScaleK);
    step   = (int)(g_BrFrameDelta * g_BrTrailStepK * g_BrTrailFixK);
    if (g_brRacePaused != 0 || g_brRaceReplay == 2)
        return;
    for (i = 0; i < g_brRaceNDriver; i++) {
        car = (BrTrailCar *)g_BrDriverSlots[i].pCar;
        if (car == 0)
            continue;
        dx = (short)(int)((car->origin.x - car->pos.x) * g_BrTrailFixK);
        dy = (short)(int)((car->origin.y - car->pos.y) * g_BrTrailFixK);
        dz = (short)(int)((car->origin.z - car->pos.z) * g_BrTrailFixK);
        nx = car->origin.x - (float)dx * g_BrTrailUnfixK;
        car->origin.x = nx;
        ny = car->origin.y - (float)dy * g_BrTrailUnfixK;
        car->origin.y = ny;
        nz = car->origin.z - (float)dz * g_BrTrailUnfixK;
        car->origin.z = nz;
        BrMat4Translate(&car->mat, nx, ny, nz);
        BrMat4Scale(&m, 1.0f / 127.0f, 1.0f / 127.0f, 1.0f / 127.0f);
        BrMat4Mul(&m, &car->mat, &car->mat);

        pq = &car->ofs[1];
        pl = car->active;
        ps = car->kind;
        c0 = &car->rec[0].v[0].c.x;
        for (kk = 0; kk < 36; kk += 9) {
            if (*pl != 0) {
                switch (*ps) {
                case 4:
                    if (g_BrTrackKind == 2 || g_BrTrackKind == 8) {
                        if (g_BrDrawWheelAlt == 0) {
                            car->rec[kk].v[0].c.x = 50.0f;
                            car->rec[kk].v[0].c.y = 100.0f;
                            car->rec[kk].v[0].c.z = 95.0f;
                            c0[8] = 210.0f;
                            c0[9] = 240.0f;
                            c0[10] = 190.0f;
                        } else {
                            car->rec[kk].v[0].c.x = 70.0f;
                            car->rec[kk].v[0].c.y = 100.0f;
                            car->rec[kk].v[0].c.z = 100.0f;
                            c0[8] = 215.0f;
                            c0[9] = 235.0f;
                            c0[10] = 195.0f;
                        }
                    } else if (g_BrTrackKind == 3 || g_BrTrackKind == 9) {
                        car->rec[kk].v[0].c.x = 16.0f;
                        car->rec[kk].v[0].c.y = 16.0f;
                        car->rec[kk].v[0].c.z = 0.0f;
                        c0[8] = 64.0f;
                        c0[9] = 64.0f;
                        c0[10] = 16.0f;
                    } else {
                        car->rec[kk].v[0].c.x = 48.0f;
                        car->rec[kk].v[0].c.y = 24.0f;
                        car->rec[kk].v[0].c.z = 8.0f;
                        c0[8] = 128.0f;
                        c0[9] = 96.0f;
                        c0[10] = 64.0f;
                    }
                    if (g_BrTrailNight != 0) {
                        x = car->rec[kk].v[0].c.x; y = car->rec[kk].v[0].c.y; z = car->rec[kk].v[0].c.z;
                        car->rec[kk].v[0].c.x = g_BrTrailNightBaseK - x * g_BrTrailNightMulK;
                        car->rec[kk].v[0].c.y = g_BrTrailNightBaseK - y * g_BrTrailNightMulK;
                        car->rec[kk].v[0].c.z = g_BrTrailNightBaseK - z * g_BrTrailNightMulK;
                        x = c0[8]; y = c0[9]; z = c0[10];
                        c0[8]  = g_BrTrailNightBaseK - x * g_BrTrailNightMulK;
                        c0[9]  = g_BrTrailNightBaseK - y * g_BrTrailNightMulK;
                        c0[10] = g_BrTrailNightBaseK - z * g_BrTrailNightMulK;
                    } else if (g_BrTrailDim != 0) {
                        x = car->rec[kk].v[0].c.x; y = car->rec[kk].v[0].c.y; z = car->rec[kk].v[0].c.z;
                        car->rec[kk].v[0].c.x = x * g_BrTrailDimK;
                        car->rec[kk].v[0].c.y = y * g_BrTrailDimK;
                        car->rec[kk].v[0].c.z = z * g_BrTrailDimK;
                        x = c0[8]; y = c0[9]; z = c0[10];
                        c0[8]  = x * g_BrTrailDimK;
                        c0[9]  = y * g_BrTrailDimK;
                        c0[10] = z * g_BrTrailDimK;
                    }
                    break;
                case 3:
                    if (g_BrTrailNight != 0) {
                        car->rec[kk].v[0].c.x = 100.0f;
                        car->rec[kk].v[0].c.y = 104.0f;
                        car->rec[kk].v[0].c.z = 108.0f;
                        c0[8] = 160.0f;
                        c0[9] = 168.0f;
                        c0[10] = 176.0f;
                        break;
                    }
                    /* fallthrough */
                default:
                    *ps   = -1;
                    c0[8] = c0[0x28];
                    c0[9] = c0[0x29];
                    c0[10] = c0[0x2A];
                    car->rec[kk].v[0].c.x = car->rec[kk + 1].v[0].c.x;
                    car->rec[kk].v[0].c.y = car->rec[kk + 1].v[0].c.y;
                    car->rec[kk].v[0].c.z = car->rec[kk + 1].v[0].c.z;
                    break;
                case 0:
                    c0[8] = 48.0f;
                    car->rec[kk].v[0].c.x = 48.0f;
                    c0[9] = 32.0f;
                    car->rec[kk].v[0].c.y = 32.0f;
                    c0[10] = 16.0f;
                    car->rec[kk].v[0].c.z = 16.0f;
                    break;
                }
                *(BrTrailRec *)(c0 + 0x1B) = *(BrTrailRec *)(c0 - 5);
                pq[0]  = pq[-1];
                ps[1]  = ps[0];
            }
            for (j = 8; j > 0; j--) {
                idx = kk + j;
                if (j == 1 || *pl != 0) {
                    car->rec[idx] = car->rec[idx - 1];
                    car->ofs[idx] = car->ofs[idx - 1];
                    car->kind[kk + j] = car->kind[kk + j - 1];
                }
                t = car->kind[kk + j];
                if (t == 3) {
                    car->rec[idx].v[0].w = 1040.0f;
                    car->rec[idx].v[1].w = 16.0f;
                } else {
                    car->rec[idx].v[0].w = 16.0f;
                    car->rec[idx].v[1].w = 1040.0f;
                }
                car->rec[idx].v[0].u = (float)(j << 11);
                car->rec[idx].v[1].u = (float)(j << 11);
                if (t == -1 || j == 1 || (j == 2 && *pl != 0)) {
                    car->rec[idx].v[0].p.x = car->rec[idx].v[0].p.x + (float)dx;
                    car->rec[idx].v[0].p.y = (float)dy + car->rec[idx].v[0].p.y;
                    car->rec[idx].v[0].p.z = (float)dz + car->rec[idx].v[0].p.z;
                    car->rec[idx].v[1].p.x = car->rec[idx].v[1].p.x + (float)dx;
                    car->rec[idx].v[1].p.y = (float)dy + car->rec[idx].v[1].p.y;
                    car->rec[idx].v[1].p.z = (float)dz + car->rec[idx].v[1].p.z;
                } else if (j != 8) {
                    car->rec[idx].v[0].p.x = (float)((iScale * car->ofs[idx].s[0] >> 12) + dx) + car->rec[idx].v[0].p.x;
                    car->rec[idx].v[0].p.y = (float)((car->ofs[idx].s[1] * iScale >> 12) + dy) + car->rec[idx].v[0].p.y;
                    car->rec[idx].v[0].p.z = (float)dz + car->rec[idx].v[0].p.z;
                    car->rec[idx].v[1].p.x = (float)((iScale * car->ofs[idx].s[3] >> 12) + dx) + car->rec[idx].v[1].p.x;
                    car->rec[idx].v[1].p.y = (float)((car->ofs[idx].s[4] * iScale >> 12) + dy) + car->rec[idx].v[1].p.y;
                    v = (float)((iScale * car->ofs[idx].s[5] >> 12) + dz) + car->rec[idx].v[1].p.z;
                    car->rec[idx].v[1].p.z = v;
                    if (v < car->rec[idx].v[0].p.z)
                        car->rec[idx].v[1].p.z = car->rec[idx].v[0].p.z;
                    else
                        car->ofs[idx].s[5] -= step;
                }
            }
            pq += 9;
            c0 += 9 * 0x20;
            ps += 9;
            pl += 1;
        }
    }
}

#endif /* BR_MATCHING_BUILD */
