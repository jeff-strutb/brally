/* br_snapinterp.c -- drawing: the frame interpolator between physics
 * snapshots.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * The physics side writes complete game-state snapshots (every driver record,
 * every car record, and a tail of scene state) into a ring of five 0x2E0F0-
 * byte slots based at 0x10396F48; 0x10013F20 picks the slot for the next one
 * and rotates the cur/prev pair at 0x104AB4EC / 0x104AB500.  The function
 * here is the render side of that arrangement: it blends the two newest
 * snapshots into a SIXTH slot (index 5, at 0x1047D3F8) by the wall-clock
 * fraction elapsed since the last blend, then hands slot 5 to the frame
 * driver 0x10011FA0.  The `Interpolate=` key of BossRally.ini (0x100A5EAC)
 * switches the blend off, in which case every frame simply shows the newest
 * snapshot.
 *
 * Transcribed from build/ghidra_decomp/0x100131e0.c against the annotated
 * disassembly of build/match/orig/0x100131E0.bin.  Facts read off the bytes
 * rather than the draft:
 *   - 0x1007727C / 0x10077280 / 0x100772A4 / 0x100772A8 are 0.0f, 1.0f,
 *     100.0f and 0.03f in .rdata -- the literal pool of the original TU that
 *     also holds br_fps.c's 0.0f seed, so they are written as literals here.
 *   - the blend parameter `t` is never stored: it lives on the x87 stack from
 *     its first `fld` to the `fstp st(0)` before the frame-driver call (and
 *     on the not-drawn path, before the return).
 *   - the return value is a memory-homed local zeroed in the prologue
 *     (`mov [esp+0x1c], edx`), reloaded on the not-drawn path and set to 1
 *     (through edx) on the drawn path -- `ret = 0; ...; ret = 1; return ret`.
 *   - `(float)(unsigned)(now - t0)` is the `fild qword` with a zeroed high
 *     dword; the stamp delta is subtracted as a signed int (`fisub dword`).
 *   - the driver copy walks two pointers (both spilled) with the from-slot's
 *     base hoisted before the loop; the car copy is index form, and re-reads
 *     the to/from slot indices and the car count on every pass.
 *
 * Neighbours by address (0x10011D20 .. 0x10013FC0) are all drawing: the FPS
 * readout, the frame driver, the HUD scene, the per-frame flags in br_clear.c.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

/* One of the six 0x44-byte matrix records hung off a car: a 4x4 plus one
 * trailing float.  Two pointers in the car record (+0x2734, +0x2738) each
 * point at one of them, which is why the copy has to re-target them. */
typedef struct BrSnapMtx {
    float m[4][4];
    float f40;
} BrSnapMtx;                                /* 0x44 */

/* The 0x2B68-byte car record as this function sees it.  Only the fields it
 * blends or re-targets are named; the rest travels through the struct copy. */
typedef struct BrSnapCar {
    float      m[4][4];                     /* +0x0000  body matrix          */
    float      wheel[4][4][4];              /* +0x0040  four wheel matrices  */
    uint8_t    pad140[0x2734 - 0x140];
    BrSnapMtx *pMatA;                       /* +0x2734                       */
    BrSnapMtx *pMatB;                       /* +0x2738                       */
    BrSnapMtx  rec[6];                      /* +0x273C .. +0x28D4            */
    uint8_t    pad28D4[0x2B68 - 0x28D4];
} BrSnapCar;                                /* 0x2B68 */

/* The 0x80-byte driver record: the only field touched is its car pointer. */
typedef struct BrSnapDrv {
    uint8_t    pad00[0x60];
    BrSnapCar *pCar;                        /* +0x60 */
    uint8_t    pad64[0x1C];
} BrSnapDrv;                                /* 0x80 */

typedef struct BrSnapTailA { int32_t a[0x16]; }  BrSnapTailA;   /* 0x58   */
typedef struct BrSnapTailC { int32_t a[0x802]; } BrSnapTailC;   /* 0x2008 */

/* One snapshot slot.  Slots 0..4 are the ring the physics writes; slot 5 is
 * the blend this function builds and draws. */
typedef struct BrSnap {
    int32_t     stamp;                      /* +0x00000  sequence number    */
    BrSnapDrv   drv[20];                    /* +0x00004                     */
    int32_t     fA04;                       /* +0x00A04                     */
    BrSnapCar   car[16];                    /* +0x00A08 .. +0x2C088         */
    BrSnapTailA tailA;                      /* +0x2C088                     */
    int32_t     tailB;                      /* +0x2C0E0                     */
    BrSnapTailC tailC;                      /* +0x2C0E4 .. +0x2E0EC         */
    int32_t     f2E0EC;                     /* +0x2E0EC                     */
} BrSnap;                                   /* 0x2E0F0 */

typedef char br_assert_snapmtx[(sizeof(BrSnapMtx) == 0x44) ? 1 : -1];
typedef char br_assert_snapcar[(sizeof(BrSnapCar) == 0x2B68) ? 1 : -1];
typedef char br_assert_snapdrv[(sizeof(BrSnapDrv) == 0x80) ? 1 : -1];
typedef char br_assert_snap[(sizeof(BrSnap) == 0x2E0F0) ? 1 : -1];

extern int32_t g_brSnapCur;                 /* 0x104AB4EC  newest complete slot   */
extern int32_t g_brSnapPrev;                /* 0x104AB500  the one before it      */
extern int32_t g_brSnapTo;                  /* 0x10396F44  blend target slot      */
extern int32_t g_brSnapFrom;                /* 0x104AB4FC  blend source slot      */
extern int32_t g_aBrSnapLocked[5];          /* 0x10396F10  slots the blend holds  */
extern float   g_brSnapOrigin;              /* 0x10396F24  t at the last reset    */
extern int32_t g_brSnapT0;                  /* 0x104AB4F4  tick at the last reset */
extern int32_t g_brSnapFrames;              /* 0x104AB4F0  frames drawn           */
extern BrSnap  g_aBrSnap[6];                /* 0x10396F48  five slots + the blend */
extern int32_t g_brCfgInterpolate;          /* 0x100A5EAC  Interpolate=           */
extern int32_t g_brRaceNDriver;             /* 0x100B2F00                         */
extern int32_t g_brRaceNCar;                /* 0x100B2F04                         */

int32_t BrSub10075020(void);                /* 0x1006E280  milliseconds           */
void    BrFrameDrawView(int32_t iView);     /* 0x10011FA0  the frame driver       */

#define BR_SNAP_BLEND   5                   /* the slot the frame driver draws */

#define FROMCAR  g_aBrSnap[g_brSnapFrom].car[i]
#define TOCAR    g_aBrSnap[g_brSnapTo].car[i]
#define OUTCAR   g_aBrSnap[BR_SNAP_BLEND].car[i]
#define LERP(f)  OUTCAR.f = (TOCAR.f - FROMCAR.f) * t + FROMCAR.f

/* WHAT IT DOES: the render-side frame interpolator.  The physics writes whole
 * game-state snapshots into a five-slot ring; this blends the two newest of
 * them (from -> to, by the wall-clock fraction elapsed since the previous
 * blend, at 0.03 per millisecond) into the sixth slot and hands that slot to
 * the frame driver, so the picture moves smoothly between physics ticks.
 * Every car's body matrix, the translation of its four wheel matrices and
 * five of its six attached matrix records are blended; driver records and
 * the scene tail are copied from the source slot with their car and matrix
 * pointers re-targeted into the blend slot.  The blend is skipped in favour
 * of the newest snapshot when the caller forces it, when Interpolate= is
 * off, or when car 0's camera matrix jumped more than 10 units between the
 * two snapshots.  Returns 1 when it drew a frame; 0 when nothing new has
 * arrived and the blend already sits on the newest snapshot (the caller
 * then has nothing to show), or when no snapshot pair exists yet. */
/* @implements 0x100131E0 glide BrSnapInterpDraw */
int32_t BrSnapInterpDraw(int32_t force)
{
    int32_t ret = 0;
    int32_t delta;
    int32_t now;
    int32_t jumped;
    int32_t reset;
    int32_t n;
    int32_t j;
    int32_t i;
    float dx;
    float dy;
    float dz;
    float t;
    BrSnapMtx *pA;
    BrSnapMtx *pB;
    BrSnapMtx *p;
    BrSnapDrv *pSrc;
    BrSnapDrv *pDst;

    if (g_brSnapCur >= 0 && g_brSnapPrev >= 0) {
        if (g_brSnapTo >= 0) {
            delta = g_aBrSnap[g_brSnapCur].stamp - g_aBrSnap[g_brSnapTo].stamp;
        } else {
            delta = 0;
        }
        g_brSnapTo = g_brSnapCur;
        g_brSnapFrom = g_brSnapPrev;

        /* How far car 0's camera matrix moved between the two snapshots. */
        pA = g_aBrSnap[g_brSnapCur].car[0].pMatA;
        pB = g_aBrSnap[g_brSnapPrev].car[0].pMatA;
        dx = pA->m[3][0] - pB->m[3][0];
        dy = pA->m[3][1] - pB->m[3][1];
        dz = pA->m[3][2] - pB->m[3][2];
        jumped = (dx * dx + dy * dy + dz * dz > 100.0f);
        reset = 1;
        now = BrSub10075020();

        if (jumped) {
            t = 1.0f;
        } else if (force != 0) {
            t = 1.0f;
        } else if (g_brSnapT0 == 0) {
            t = 0.0f;
        } else {
            t = (float)(uint32_t)(now - g_brSnapT0) * 0.03f + g_brSnapOrigin - delta;
            if (delta == 0 && t > 1.0f) {
                reset = 0;
            }
            if (t < 0.0f) {
                t = 0.0f;
            }
            if (t > 1.0f) {
                t = 1.0f;
            }
        }
        if (g_brCfgInterpolate == 0) {
            t = 1.0f;
            reset = 1;
        }

        if (reset) {
            g_aBrSnapLocked[0] = 0;
            g_aBrSnapLocked[1] = 0;
            g_aBrSnapLocked[2] = 0;
            g_aBrSnapLocked[3] = 0;
            g_aBrSnapLocked[4] = 0;
            g_brSnapOrigin = t;
            g_brSnapT0 = now;
            g_aBrSnapLocked[g_brSnapTo] = 1;
            g_aBrSnapLocked[g_brSnapFrom] = 1;

            /* Driver records: copied, with the car pointer re-targeted. */
            n = g_brRaceNDriver;
            pSrc = g_aBrSnap[g_brSnapFrom].drv;
            pDst = g_aBrSnap[BR_SNAP_BLEND].drv;
            for (j = 0; j < n; j++) {
                *pDst = *pSrc;
                if (pSrc->pCar != NULL) {
                    pDst->pCar = &g_aBrSnap[BR_SNAP_BLEND].car[pSrc->pCar - g_aBrSnap[g_brSnapFrom].car];
                }
                pSrc++;
                pDst++;
            }

            /* Car records: copied, pointers re-targeted, matrices blended. */
            for (i = 0; i < (g_brRaceNCar ? g_brRaceNCar : 1); i++) {
                OUTCAR = FROMCAR;

                p = FROMCAR.pMatA;
                if (p != NULL) {
                    if (p == &FROMCAR.rec[0]) {
                        OUTCAR.pMatA = &OUTCAR.rec[0];
                    } else if (p == &FROMCAR.rec[5]) {
                        OUTCAR.pMatA = &OUTCAR.rec[5];
                    } else if (p == &FROMCAR.rec[1]) {
                        OUTCAR.pMatA = &OUTCAR.rec[1];
                    } else if (p == &FROMCAR.rec[2]) {
                        OUTCAR.pMatA = &OUTCAR.rec[2];
                    } else if (p == &FROMCAR.rec[3]) {
                        OUTCAR.pMatA = &OUTCAR.rec[3];
                    } else {
                        OUTCAR.pMatA = NULL;
                    }
                }
                p = FROMCAR.pMatB;
                if (p != NULL) {
                    if (p == &FROMCAR.rec[0]) {
                        OUTCAR.pMatB = &OUTCAR.rec[0];
                    } else if (p == &FROMCAR.rec[5]) {
                        OUTCAR.pMatB = &OUTCAR.rec[5];
                    } else if (p == &FROMCAR.rec[1]) {
                        OUTCAR.pMatB = &OUTCAR.rec[1];
                    } else if (p == &FROMCAR.rec[2]) {
                        OUTCAR.pMatB = &OUTCAR.rec[2];
                    } else if (p == &FROMCAR.rec[3]) {
                        OUTCAR.pMatB = &OUTCAR.rec[3];
                    } else {
                        OUTCAR.pMatB = NULL;
                    }
                }

                /* The body matrix: every row's x, y, z. */
                LERP(m[0][0]); LERP(m[0][1]); LERP(m[0][2]);
                LERP(m[1][0]); LERP(m[1][1]); LERP(m[1][2]);
                LERP(m[2][0]); LERP(m[2][1]); LERP(m[2][2]);
                LERP(m[3][0]); LERP(m[3][1]); LERP(m[3][2]);

                /* The wheels: translation only. */
                LERP(wheel[0][3][0]); LERP(wheel[0][3][1]); LERP(wheel[0][3][2]);
                LERP(wheel[1][3][0]); LERP(wheel[1][3][1]); LERP(wheel[1][3][2]);
                LERP(wheel[2][3][0]); LERP(wheel[2][3][1]); LERP(wheel[2][3][2]);
                LERP(wheel[3][3][0]); LERP(wheel[3][3][1]); LERP(wheel[3][3][2]);

                /* The attached matrix records: rows blended, the last column
                 * rebuilt as 0,0,0,1, the trailing float blended. */
#define LERPREC(k) \
                LERP(rec[k].m[0][0]); LERP(rec[k].m[0][1]); LERP(rec[k].m[0][2]); \
                OUTCAR.rec[k].m[0][3] = 0.0f; \
                LERP(rec[k].m[1][0]); LERP(rec[k].m[1][1]); LERP(rec[k].m[1][2]); \
                OUTCAR.rec[k].m[1][3] = 0.0f; \
                LERP(rec[k].m[2][0]); LERP(rec[k].m[2][1]); LERP(rec[k].m[2][2]); \
                OUTCAR.rec[k].m[2][3] = 0.0f; \
                LERP(rec[k].m[3][0]); LERP(rec[k].m[3][1]); LERP(rec[k].m[3][2]); \
                OUTCAR.rec[k].m[3][3] = 1.0f; \
                LERP(rec[k].f40)

                LERPREC(0);
                LERPREC(1);
                LERPREC(2);
                LERPREC(3);
                LERPREC(5);
#undef LERPREC
            }

            /* The scene tail travels unblended. */
            g_aBrSnap[BR_SNAP_BLEND].tailA = g_aBrSnap[g_brSnapFrom].tailA;
            g_aBrSnap[BR_SNAP_BLEND].tailB = g_aBrSnap[g_brSnapFrom].tailB;
            g_aBrSnap[BR_SNAP_BLEND].tailC = g_aBrSnap[g_brSnapFrom].tailC;

            BrFrameDrawView(BR_SNAP_BLEND);
            g_brSnapFrames++;
            ret = 1;
        }
    }
    return ret;
}

#undef LERP
#undef OUTCAR
#undef TOCAR
#undef FROMCAR

#endif /* BR_MATCHING_BUILD */
