/* br_carwheelfx.c -- drawing: per-wheel effect state for the car effects.
 *
 * BrCarWheelFx works out, once a frame for each of a car's four wheels, which
 * effect (dust, spray, water) the wheel shows, its direction and its source
 * point, and writes them out packed for the effect drawing to read.
 *
 * Filed out of the address batch slice2_21.c; the preamble is slice2_21.c's,
 * carried whole. That file's header note:
 *
 * Every x87 sequence below was traced instruction by instruction through its
 * fxch/fsubr/fdivr chain; the reversed operand forms (fsubr, fdivr, fsubp
 * st(n)) are spelled out in the arithmetic here rather than "tidied", because
 * `fsubr m32` is `st0 = m32 - st0` and getting that backwards is silent.
 */
#ifdef BR_MATCHING_BUILD
#define BrSpanTestPoint BrSpanTestPoint_port
#define BrPfxReset      BrPfxReset_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrSpanTestPoint
#undef BrPfxReset
int  BrSpanTestPoint(float x, float y);
void BrPfxReset(void);
int  BrSpanContains(int param_1, int param_2);
#endif

#include <string.h>

/* --------------------------------------------------------------------------
 * Constants, all read straight out of .rdata rather than guessed.
 * -------------------------------------------------------------------------- */
#define K_0            0.0f                    /* 0x1008F62C, 0x1008F59C */
#define K_1            1.0f                    /* 0x1008F628, 0x1008F588 */
#define K_EPS_REL      1.0000000036274937e-15f /* 0x1008F63C */
#define K_PI           3.1415927410125732f     /* 0x1008F640 */
#define K_PI_2         1.5707963705062866f     /* -0x1008F644 */
#define K_PI_4         0.7853981852531433f     /* -0x1008F648, +0x1008F658 */
#define K_PI_8         0.39269909262657166f    /* 0x1008F64C */
#define K_PI_16        0.19634954631328583f    /* 0x1008F650 */
#define K_SIN_TOL      0.004999999888241291f   /* 0x1008F654 */
#define K_CELL_RECIP   0.03125f                /* 0x1008F61C, 0x1008F608 */
#define K_CELL         32.0f                   /* 0x1008F624 */
#define K_65280_RECIP  1.5318628356908448e-05f /* 0x1008F5FC = 1/65280 */
#define K_65536_RECIP  1.5259021893143654e-05f /* 0x1008F57C = 1/65536 */

/* --------------------------------------------------------------------------
 * 6. Car-driven effects
 * -------------------------------------------------------------------------- */

#define CAR_B(p, o)   ((unsigned char *)(p) + (o))
#define CAR_F(p, o)   (*(float *)   CAR_B(p, o))
#define CAR_I(p, o)   (*(int32_t *) CAR_B(p, o))
#define CAR_U16(p, o) (*(uint16_t *)CAR_B(p, o))
#define CAR_S16(p, o) ((int16_t *)  CAR_B(p, o))
#define CAR_V(p, o)   ((BrVec3 *)   CAR_B(p, o))

/* The four wheel records, in the order both routines index them. */
static const unsigned aWheelOff[4] = { 0x994u, 0x57Cu, 0x370u, 0x788u };

#define WHEEL_SURF(w) (*(signed char *)((w) + 0x1A0))
#define WHEEL_LIVE(w) (*(int32_t *)    ((w) + 0x1B4))


/* 0x10039200 */
/* WHAT IT DOES: works out, once a frame and for each of a car's four wheels,
 * what kind of effect that wheel should be showing and which way it should be
 * throwing it -- the direction and the point it comes from, both scaled by how
 * fast the car is going and how much it is sliding sideways. Water is treated
 * differently from dust, a wheel keeps emitting for three frames after it
 * leaves the ground, and the results are written out in the packed form the
 * effect drawing later reads. */
/* @t4-pass 0x10032880 1 2026-09-21 probes 10 bytes 1467 insns 405 regions 12 rows 54 census yes  (hand, fn.py variants: scaled-index vs walked induction pointers, indexed apW[i] vs walked ppW, dead vecB/vecC zero-init removal, vecB component temps; slot census orig-vs-recomp balanced) */
/* @t4-pass 0x10032880 2 2026-09-21 probes 10 bytes 1467 insns 405 regions 12 rows 54 census yes  (hand, fn.py variants: typed vs char* pointer strides, induction-pointer init reorder, pF subtraction operand spellings; none move the ppW/pSoa CSE merge or the fsub/fsubr operand-role fork) */
/* @t3 0x10032880 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1467/1464 insns 405/405 rows 27+27 regions 12 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Behaviour is the original's: the differential oracle agrees on the return,
 * every touched global and every side effect across 64 seeded car states
 * (tools/t3b_verify.py EQUIVALENT), and the instruction count is exact
 * (405/405).  Residue is register allocation plus the x87 operand-role wall:
 *   1. The original walks TWO separate stride-4 induction pointers -- the
 *      wheel-record pointer array (apW, [esp+0x28]) and the SoA cursor (pSoa,
 *      [esp+0x2c]).  VC5 here CSE-merges them (keeps `&apW - pSoa` constant and
 *      rebuilds apW as `[(&apW-pSoa)+pSoa]`), which costs two extra dword slots
 *      (frame sub esp,0x78 vs 0x70) and shifts every esp-relative offset -- the
 *      bulk of the masked diffs are that positional shift, not changed logic.
 *   2. The three packed-position writes `(vecB.n - pCar->f26Cn) * 127` come out
 *      `fld [pCar field] / fsubr [vecB.n]` where the original does
 *      `fld [vecB.n] / fsub [pCar field]` -- same value, the known VC5
 *      commutative-subtract operand-role fork (docs: x87 wall).  Neither
 *      component temps nor statement reordering moves it.
 * Minor: one int->float uses fild qword where the original uses fild dword, and
 * one zeroing is sub r,r vs xor r,r.  Slot census (tools/slotcensus.py) shows
 * matched per-slot write/read balance -- no dropped store.  Do not reopen
 * before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10039200 d3d BrCarWheelFx */
#ifdef BR_MATCHING_BUILD
/* The Glide twin (0x10032880) is __fastcall(pCar): everything the port passes
 * as pEnv->* and pSeed is a file-scope global here, and the wheel effect state
 * lives on the car object reached through ecx.  Same algorithm as the port
 * body in the #else arm below -- only the inputs move.  The truncations go
 * through MSVC's _ftol (0x10074560, a plain (int) cast), NOT BrFtolTrunc
 * (0x1007C8A0), and the RNG is BrRandom (0x100353D0), not BrDPlayRandStep. */
extern unsigned char *DAT_106eed38;   /* pEnv->pRecs (record base pointer)  */
extern int32_t        DAT_106ed6b0;   /* pEnv->mode6620                     */
extern int32_t        DAT_100b3014;   /* pEnv->sel0B380C                    */
extern int32_t        DAT_106ed6b4;   /* pEnv->flag6624                     */
extern float          DAT_106e9d8c;   /* pEnv->dt                           */
extern int            BrRandom(void); /* 0x100353D0                         */

void __fastcall BrCarWheelFx(struct BrCar *pCar)
{
    int bMasked = 0;
    float k1, k2, dot;
    unsigned char *apW[4];
    unsigned char **ppW;
    unsigned char *pSoa;
    BrVec3 *pWpos, *pPrev;
    int16_t *pS16;
    uint16_t *pTag;
    float *pF;
    int i;

    if (CAR_I(pCar, 0x294C) != 0) {
        unsigned idx = CAR_U16(pCar, 0x290C);
        if (DAT_106eed38[idx * 84u + 0x4Cu] & 0x10u)
            bMasked = 1;
    }

    if (DAT_106ed6b0 != 0) {
        if (DAT_100b3014 != 2 && DAT_100b3014 != 8)
            return;
    }

    /* The four wheel records are hoisted into a stack array before the loop
     * (the original materialises all four base pointers up front, then walks
     * them). */
    apW[0] = CAR_B(pCar, 0x994);
    apW[1] = CAR_B(pCar, 0x57C);
    apW[2] = CAR_B(pCar, 0x370);
    apW[3] = CAR_B(pCar, 0x788);

    /* (1 - 50/(speed + 50)) * 3 */
    k1 = (K_1 - 50.0f / (CAR_F(pCar, 0x1030) - -50.0f)) * 3.0f;

    dot = BrVec3Dot(CAR_V(pCar, 0x00), CAR_V(pCar, 0x1024));
    if (dot < K_0)
        dot = -dot;
    /* (1 - 25/(25 - (-2.24 * |dot|))) * 3 */
    k2 = (K_1 - 25.0f / (25.0f - dot * -2.240000009536743f)) * 3.0f;

    /* Induction pointers: the original strength-reduces every per-wheel array
     * access to a base pointer walked by a fixed byte stride each iteration. */
    ppW   = apW;
    pSoa  = CAR_B(pCar, 0x10AC);
    pWpos = CAR_V(pCar, 0x70);
    pPrev = CAR_V(pCar, 0x10EC);
    pS16  = CAR_S16(pCar, 0x2326);
    pTag  = &CAR_U16(pCar, 0x2680);
    pF    = &CAR_F(pCar, 0x1140);

    for (i = 0; i < 4; i++) {
        unsigned char *pW    = *ppW;
        float *pSoa00 = (float *)  (pSoa + 0x00);
        int32_t *pSoa10 = (int32_t *)(pSoa + 0x10);
        float *pSoa20 = (float *)  (pSoa + 0x20);
        int32_t *pSoa30 = (int32_t *)(pSoa + 0x30);

        BrVec3 vecA, vecB, vecC;
        float t, s, dv;
        int bIdle, kind;

        /* fsubr: timer - (dt * -4) */
        t = *pSoa20 - DAT_106e9d8c * -4.0f;
        *pSoa20 = t;
        if (t <= 0.75f) {
            bIdle = 1;
            *pSoa30 = 0;
        } else {
            bIdle = 0;
            if (t < 1.7000000476837158f)
                *pSoa20 = 0.0f;         /* stored as an integer 0 */
            else
                *pSoa20 = t - K_1;
            *pSoa30 = 1;
        }

        if (WHEEL_LIVE(pW) != 0) {
            *pSoa00 = 3.0f;
            *pSoa10 = WHEEL_SURF(pW);   /* movsx: signed */
        } else if (*pSoa00 != K_0) {
            *pSoa00 = *pSoa00 - K_1;
        }

        kind = *pSoa10;
        if (*pSoa00 == K_0)
            goto zero_path;

        if (bIdle) {
            s = 1.5f;
        } else if (kind == 4) {
            s = 2.5f;
        } else if (kind == 3) {
            if (DAT_106ed6b4 == 0)
                goto zero_path;
            if (bMasked)
                goto zero_path;
            s = 1.5f;
            *pSoa20 = 7.75f;
        } else {
            goto zero_path;
        }

        if (!bIdle) {
            BrVec3Scale(&vecA, CAR_V(pCar, 0x20), s);
            if (kind != 3) {
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10),
                               (i != 0 && i < 3) ? -s : s);
                BrVec3ScaleBy(&vecA, k1);
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x1024),
                               0.30000001192092896f);
                dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024)) * 0.5f;
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
            } else {
                BrVec3ScaleBy(&vecA, k2);
                if (k2 != K_0) {
                    if (i >= 2)
                        vecA.z = vecA.z + vecA.z;
                    dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024))
                       * 0.30000001192092896f;
                    BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
                }
            }
        }

        BrVec3MulAdd(&vecB, pWpos, CAR_V(pCar, 0x20), -0.25f);

        if (!(kind == 3 && i >= 2)) {
            float s3 = (i == 0) ? 0.15000000596046448f
                     : (i < 3)  ? -0.15000000596046448f
                                : 0.15000000596046448f;
            BrVec3MulAddTo(&vecB, CAR_V(pCar, 0x10), s3);
        }

        vecC = vecB;
        if (!bIdle) {
            if (BrVec3DistSq(&vecB, pPrev) < 256.0f) {
                float u = (float)(int)(BrRandom() & 0xFFFFu)
                        * K_65536_RECIP;
                BrVec3Lerp(&vecB, pPrev, &vecB, u * u);
            }
        }
        /* The PRE-lerp value is what gets remembered, on both branches. */
        *pPrev = vecC;
        goto have_vectors;

    zero_path:
        BrVec3Zero(&vecA);
        vecB = *pWpos;

    have_vectors:
        if (!bIdle) {
            pS16[0] = (int16_t)(vecA.x * 127.0f);
            pS16[1] = (int16_t)(vecA.y * 127.0f);
            pS16[2] = (int16_t)(vecA.z * 127.0f);
            if (kind == 3 && DAT_106ed6b4 != 0) {
                pS16[-3] = pS16[0];
                pS16[-2] = pS16[1];
            } else {
                pS16[-3] = (int16_t)(pS16[0] >> 1);   /* sar: arithmetic */
                pS16[-2] = (int16_t)(pS16[1] >> 1);
            }
            pS16[-1] = 0;
        }

        *pTag = (uint16_t)kind;

        pF[0] = (float)(int16_t)((vecB.x - CAR_F(pCar, 0x26C8)) * 127.0f);
        pF[1] = (float)(int16_t)((vecB.y - CAR_F(pCar, 0x26CC)) * 127.0f);
        pF[2] = (float)(int16_t)((vecB.z - CAR_F(pCar, 0x26D0)) * 127.0f);
        pF[-8] = pF[0];
        pF[-7] = pF[1];
        pF[-6] = pF[2];

        ppW  += 1;
        pSoa  += 0x04;
        pWpos = (BrVec3 *)((unsigned char *)pWpos + 0x40);
        pPrev += 1;
        pS16  += 0x6C;
        pTag  += 0x09;
        pF    += 0x120;
    }
}
#else
void BrCarWheelFx(struct BrCar *pCar, const BrCarFxEnv *pEnv, uint32_t *pSeed)
{
    int bMasked = 0;
    float k1, k2, dot;
    int i;

    if (CAR_I(pCar, 0x294C) != 0 && pEnv->pRecs != NULL) {
        unsigned idx = CAR_U16(pCar, 0x290C);
        if (pEnv->pRecs[idx * 84u + 0x4Cu] & 0x10u)
            bMasked = 1;
    }

    if (pEnv->mode6620 != 0) {
        if (pEnv->sel0B380C != 2 && pEnv->sel0B380C != 8)
            return;
    }

    /* (1 - 50/(speed + 50)) * 3 */
    k1 = (K_1 - 50.0f / (CAR_F(pCar, 0x1030) - -50.0f)) * 3.0f;

    dot = BrVec3Dot(CAR_V(pCar, 0x00), CAR_V(pCar, 0x1024));
    if (dot < K_0)
        dot = -dot;
    /* (1 - 25/(25 - (-2.24 * |dot|))) * 3 */
    k2 = (K_1 - 25.0f / (25.0f - dot * -2.240000009536743f)) * 3.0f;

    for (i = 0; i < 4; i++) {
        unsigned char *pW    = CAR_B(pCar, aWheelOff[i]);
        unsigned char *pSoa  = CAR_B(pCar, 0x10AC + 4u * (unsigned)i);
        BrVec3        *pWpos = CAR_V(pCar, 0x70   + 0x40u * (unsigned)i);
        BrVec3        *pPrev = CAR_V(pCar, 0x10EC + 12u * (unsigned)i);
        int16_t       *pS16  = CAR_S16(pCar, 0x2326 + 0xD8u * (unsigned)i);
        uint16_t      *pTag  = &CAR_U16(pCar, 0x2680 + 0x12u * (unsigned)i);
        float         *pF    = &CAR_F(pCar, 0x1140 + 0x480u * (unsigned)i);

        float *pSoa00 = (float *)  (pSoa + 0x00);
        int32_t *pSoa10 = (int32_t *)(pSoa + 0x10);
        float *pSoa20 = (float *)  (pSoa + 0x20);
        int32_t *pSoa30 = (int32_t *)(pSoa + 0x30);

        BrVec3 vecA, vecB, vecC;
        float t, s, dv;
        int bIdle, kind;

        vecA.x = vecA.y = vecA.z = 0.0f;
        vecB = vecA;
        vecC = vecA;
        s = 0.0f;

        /* fsubr: timer - (dt * -4) */
        t = *pSoa20 - pEnv->dt * -4.0f;
        *pSoa20 = t;
        if (t <= 0.75f) {
            bIdle = 1;
            *pSoa30 = 0;
        } else {
            bIdle = 0;
            if (t < 1.7000000476837158f)
                *pSoa20 = 0.0f;         /* stored as an integer 0 */
            else
                *pSoa20 = t - K_1;
            *pSoa30 = 1;
        }

        if (WHEEL_LIVE(pW) != 0) {
            *pSoa00 = 3.0f;
            *pSoa10 = WHEEL_SURF(pW);   /* movsx: signed */
        } else if (*pSoa00 != K_0) {
            *pSoa00 = *pSoa00 - K_1;
        }

        kind = *pSoa10;
        if (*pSoa00 == K_0)
            goto zero_path;

        if (bIdle) {
            s = 1.5f;
        } else if (kind == 4) {
            s = 2.5f;
        } else if (kind == 3) {
            if (pEnv->flag6624 == 0)
                goto zero_path;
            if (bMasked)
                goto zero_path;
            s = 1.5f;
            *pSoa20 = 7.75f;
        } else {
            goto zero_path;
        }

        if (!bIdle) {
            BrVec3Scale(&vecA, CAR_V(pCar, 0x20), s);
            if (kind != 3) {
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10),
                               (i != 0 && i < 3) ? -s : s);
                BrVec3ScaleBy(&vecA, k1);
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x1024),
                               0.30000001192092896f);
                dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024)) * 0.5f;
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
            } else {
                BrVec3ScaleBy(&vecA, k2);
                if (k2 != K_0) {
                    if (i >= 2)
                        vecA.z = vecA.z + vecA.z;
                    dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024))
                       * 0.30000001192092896f;
                    BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
                }
            }
        }

        BrVec3MulAdd(&vecB, pWpos, CAR_V(pCar, 0x20), -0.25f);

        if (!(kind == 3 && i >= 2)) {
            float s3 = (i == 0) ? 0.15000000596046448f
                     : (i < 3)  ? -0.15000000596046448f
                                : 0.15000000596046448f;
            BrVec3MulAddTo(&vecB, CAR_V(pCar, 0x10), s3);
        }

        vecC = vecB;
        if (!bIdle) {
            if (BrVec3DistSq(&vecB, pPrev) < 256.0f) {
                float u = (float)(int)(BrDPlayRandStep(pSeed) & 0xFFFFu)
                        * K_65536_RECIP;
                BrVec3Lerp(&vecB, pPrev, &vecB, u * u);
            }
        }
        /* The PRE-lerp value is what gets remembered, on both branches. */
        *pPrev = vecC;
        goto have_vectors;

    zero_path:
        BrVec3Zero(&vecA);
        vecB = *pWpos;

    have_vectors:
        if (!bIdle) {
            pS16[0] = (int16_t)BrFtolTrunc(vecA.x * 127.0f);
            pS16[1] = (int16_t)BrFtolTrunc(vecA.y * 127.0f);
            pS16[2] = (int16_t)BrFtolTrunc(vecA.z * 127.0f);
            if (kind == 3 && pEnv->flag6624 != 0) {
                pS16[-3] = pS16[0];
                pS16[-2] = pS16[1];
            } else {
                pS16[-3] = (int16_t)(pS16[0] >> 1);   /* sar: arithmetic */
                pS16[-2] = (int16_t)(pS16[1] >> 1);
            }
            pS16[-1] = 0;
        }

        *pTag = (uint16_t)kind;

        pF[0] = (float)(int16_t)BrFtolTrunc((vecB.x - CAR_F(pCar, 0x26C8)) * 127.0f);
        pF[1] = (float)(int16_t)BrFtolTrunc((vecB.y - CAR_F(pCar, 0x26CC)) * 127.0f);
        pF[2] = (float)(int16_t)BrFtolTrunc((vecB.z - CAR_F(pCar, 0x26D0)) * 127.0f);
        pF[-8] = pF[0];
        pF[-7] = pF[1];
        pF[-6] = pF[2];
    }
}
#endif /* BR_MATCHING_BUILD */
