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
#define BrSpanTestPoint BrSpanTestPoint_port
#define BrPfxReset      BrPfxReset_port
#include "slice3_41.h"   /* BrDriverCar */
#include "slice1_05.h"   /* g_aBrEntRecs */
#include "br_vec.h"
#include "slice2_21.h"
#include "br_cartypes.h"
#include "slice3_41.h"
#undef BrSpanTestPoint
#undef BrPfxReset
/* BrSpanTestPoint: prototype in br_funcs.h */
/* BrPfxReset: prototype in br_funcs.h */
/* BrSpanContains: prototype in br_funcs.h */

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


/* The four wheel records, in the order both routines index them. */
static const unsigned aWheelOff[4] = { 0x994u, 0x57Cu, 0x370u, 0x788u };



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
 * before the end-grind. */
/* @implements 0x10039200 d3d BrCarWheelFx */
/* The Glide twin (0x10032880) is __fastcall(pCar): everything the port passes
 * as pEnv->* and pSeed is a file-scope global here, and the wheel effect state
 * lives on the car object reached through ecx.  Same algorithm as the port
 * body in the #else arm below -- only the inputs move.  The truncations go
 * through MSVC's _ftol (0x10074560, a plain (int) cast), NOT BrFtolTrunc
 * (0x1007C8A0), and the RNG is BrRandom (0x100353D0), not BrDPlayRandStep. */
/* Glide's globals: 0x106EED38 the track face records (84 bytes each),
 * 0x106ED6B0/B4 two words in the pointer-free span the inventory gives
 * g_aBrEntRecs, 0x100B3014 the selection, 0x106E9D8C the frame step. */
#define DAT_106eed38 ((unsigned char *)g_BrDrawTrackFlags)
#define DAT_106ed6b0 (*(int32_t *)((char *)&g_aBrEntRecs + 0x80))
#define DAT_106ed6b4 (*(int32_t *)((char *)&g_aBrEntRecs + 0x84))
#define DAT_100b3014 g_Br0B380C
#define DAT_106e9d8c g_brRaceFlyStep
#define WHEEL_SURF(w) (*(signed char *)&((BrCarBody *)(w))->rb.f01A0)
#define WHEEL_LIVE(w) (*(int32_t *)&((BrCarBody *)(w))->rb.f1B4)

void __fastcall BrCarWheelFx(struct BrDriverCar *pCar)
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

    if ((*(int32_t *)&((BrDriverCar *)(pCar))->gotHit) != 0) {
        unsigned idx = (*(uint16_t *)&((BrDriverCar *)(pCar))->aNearIds[0]);
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
    apW[0] = ((unsigned char *)&((BrDriverCar *)(pCar))->aBody[4]);
    apW[1] = ((unsigned char *)&((BrDriverCar *)(pCar))->aBody[2]);
    apW[2] = ((unsigned char *)&((BrDriverCar *)(pCar))->aBody[1]);
    apW[3] = ((unsigned char *)&((BrDriverCar *)(pCar))->aBody[3]);

    /* (1 - 50/(speed + 50)) * 3 */
    k1 = (K_1 - 50.0f / ((*(float *)&((BrDriverCar *)(pCar))->f1030) - -50.0f)) * 3.0f;

    dot = BrVec3Dot(((BrVec3 *)&((BrDriverCar *)(pCar))->fwd), ((BrVec3 *)&((BrDriverCar *)(pCar))->f1024));
    if (dot < K_0)
        dot = -dot;
    /* (1 - 25/(25 - (-2.24 * |dot|))) * 3 */
    k2 = (K_1 - 25.0f / (25.0f - dot * -2.240000009536743f)) * 3.0f;

    /* Induction pointers: the original strength-reduces every per-wheel array
     * access to a base pointer walked by a fixed byte stride each iteration. */
    ppW   = apW;
    pSoa  = ((unsigned char *)&((BrDriverCar *)(pCar))->a10AC[0]);
    pWpos = ((BrVec3 *)&((BrDriverCar *)(pCar))->aWheel[0].m[3][0]);
    pPrev = ((BrVec3 *)&((BrDriverCar *)(pCar))->aWheelPrev[0]);
    pS16  = ((int16_t *)&((BrDriverCar *)(pCar))->aWHist[1][0]);
    pTag  = &(*(uint16_t *)&((BrDriverCar *)(pCar))->a2680[0]);
    pF    = &(*(float *)&((BrDriverCar *)(pCar))->aHist[1][0]);

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
            BrVec3Scale(&vecA, ((BrVec3 *)&((BrDriverCar *)(pCar))->up), s);
            if (kind != 3) {
                BrVec3MulAddTo(&vecA, ((BrVec3 *)&((BrDriverCar *)(pCar))->right),
                               (i != 0 && i < 3) ? -s : s);
                BrVec3ScaleBy(&vecA, k1);
                BrVec3MulAddTo(&vecA, ((BrVec3 *)&((BrDriverCar *)(pCar))->f1024),
                               0.30000001192092896f);
                dv = BrVec3Dot(((BrVec3 *)&((BrDriverCar *)(pCar))->right), ((BrVec3 *)&((BrDriverCar *)(pCar))->f1024)) * 0.5f;
                BrVec3MulAddTo(&vecA, ((BrVec3 *)&((BrDriverCar *)(pCar))->right), dv);
            } else {
                BrVec3ScaleBy(&vecA, k2);
                if (k2 != K_0) {
                    if (i >= 2)
                        vecA.z = vecA.z + vecA.z;
                    dv = BrVec3Dot(((BrVec3 *)&((BrDriverCar *)(pCar))->right), ((BrVec3 *)&((BrDriverCar *)(pCar))->f1024))
                       * 0.30000001192092896f;
                    BrVec3MulAddTo(&vecA, ((BrVec3 *)&((BrDriverCar *)(pCar))->right), dv);
                }
            }
        }

        BrVec3MulAdd(&vecB, pWpos, ((BrVec3 *)&((BrDriverCar *)(pCar))->up), -0.25f);

        if (!(kind == 3 && i >= 2)) {
            float s3 = (i == 0) ? 0.15000000596046448f
                     : (i < 3)  ? -0.15000000596046448f
                                : 0.15000000596046448f;
            BrVec3MulAddTo(&vecB, ((BrVec3 *)&((BrDriverCar *)(pCar))->right), s3);
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

        pF[0] = (float)(int16_t)((vecB.x - (*(float *)&((BrDriverCar *)(pCar))->f26C8)) * 127.0f);
        pF[1] = (float)(int16_t)((vecB.y - (*(float *)&((BrDriverCar *)(pCar))->f26CC)) * 127.0f);
        pF[2] = (float)(int16_t)((vecB.z - (*(float *)&((BrDriverCar *)(pCar))->f26D0)) * 127.0f);
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
