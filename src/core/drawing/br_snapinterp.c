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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stddef.h>
#include <stdint.h>
#include <string.h>


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
/* @t4-pass 0x100131E0 1 2026-09-07 probes 102 bytes 3532 insns 824 regions 17 rows 340 census yes  (tools/crank.py) */
/* @t4-pass 0x100131E0 2 2026-09-07 probes 102 bytes 3532 insns 824 regions 17 rows 340 census yes  (tools/crank.py) */
/* @t4-pass 0x100131E0 3 2026-09-19 probes 11 bytes 3501 insns 817 regions 13 rows 457 census no  (x87/addressing grind: the orig keeps the blend fraction t RESIDENT in an x87 register (fmul st(1)) across every LERP and reaches car fields through hoisted base pointers (lea once, then [reg+imm]); our compile reloads t per component (fmul [t]) and addresses each field by absolute global ([reg+abs]) -- the +bytes. Pointer-macro and row-batch variants shift the shape but VC5 will not reproduce the t-in-register retention from C. Numbers held.) */
/* @t4-pass 0x100131E0 4 2026-09-19 probes 10 bytes 3501 insns 817 regions 13 rows 457 census yes  (write-slot census (tools/slotcensus.py) + variant sweep confirm the residue is x87 register retention / pointer-vs-index addressing, not missing/wrong code; the A5 oracle proves same-in/same-out across the blend math, the wheels, the integer lock/counter state (exact_regions) and the return, with the timer and frame driver black-boxed identically; numbers unmoved.) */
/* @t3 0x100131E0 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3501/3217 insns 817/856 rows 248+209 regions 13 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue is x87/register scheduling: the orig keeps the blend fraction t in an
 * x87 register (fmul st(1)) across every per-component LERP and reaches the
 * snapshot car fields through hoisted base pointers (lea once, [reg+imm]); our
 * compile reloads t each component and addresses fields by absolute global
 * ([reg+abs]).  VC5 will not reproduce the t-in-register retention from C
 * (crank tried 102x).  A5 EQUIVALENT with teeth: negative-controlled on the
 * blend math, the wheels, the integer lock/counter state and the return.  The
 * timer (0x1006E280) and frame driver (0x10011FA0) are black-boxed by the
 * oracle -- both sides call them identically.  Colouring/scheduling wall; do
 * not reopen before the end-grind (CLAUDE.md rule 12). */
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
            for (j = 0; j < n; j++) {
                g_aBrSnap[BR_SNAP_BLEND].drv[j] = g_aBrSnap[g_brSnapFrom].drv[j];
                if (g_aBrSnap[g_brSnapFrom].drv[j].pCar != NULL) {
                    g_aBrSnap[BR_SNAP_BLEND].drv[j].pCar =
                        &g_aBrSnap[BR_SNAP_BLEND].car[g_aBrSnap[g_brSnapFrom].drv[j].pCar
                                                      - g_aBrSnap[g_brSnapFrom].car];
                }
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
#define LERPROW(k, r) \
                { \
                    float *o = OUTCAR.rec[k].m[r]; \
                    float *a = TOCAR.rec[k].m[r]; \
                    float *b = FROMCAR.rec[k].m[r]; \
                    o[0] = (a[0] - b[0]) * t + b[0]; \
                    o[1] = (a[1] - b[1]) * t + b[1]; \
                    o[2] = (a[2] - b[2]) * t + b[2]; \
                }
#define LERPREC(k) \
                LERPROW(k, 0); \
                OUTCAR.rec[k].m[0][3] = 0.0f; \
                LERPROW(k, 1); \
                OUTCAR.rec[k].m[1][3] = 0.0f; \
                LERPROW(k, 2); \
                OUTCAR.rec[k].m[2][3] = 0.0f; \
                LERPROW(k, 3); \
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

/* ------------------------------------------------------------------ */
/* 0x10013E80                                                         */
/* ------------------------------------------------------------------ */

/* 0x10013E80 -- one-time reset of the snapshot ring and race-begin bookkeeping.
 *
 * RESIDUE (T3a, 4 bytes): size-exact 128/128, 30/30 instructions, REGNORM 0+0
 * -- the two streams are the SAME instruction multiset and differ only in
 * WHERE ONE STORE SITS.  The original puts the memset's fifth store AFTER the
 * loop's pointer init:
 *      59  mov [0x104AB4F0], ecx
 *      5f  mov eax, 0x10396F48        <- loop IV init
 *      64  mov [0x10396F20], esi      <- memset store #5, filling the AGI slot
 *      6a  mov [eax], ecx             <- loop body
 * and every spelling tried emits store #5 at 0x4d instead, leaving `mov eax`
 * back-to-back with `mov [eax]`.
 *
 * â¼ WHAT DID MOVE THE NEEDLE (keep it): `g_brRaceBeginLimitOn = 1;` must be
 * the FIRST statement in the block.  That is what forces `mov edx,1` into the
 * prologue, which in turn denies edx to the memset's zero temp and buys the
 * `push esi` / `pop esi` pair the original has.  Without it the function is
 * 121-126 bytes and REGNORM 0+2 (missing push/pop).
 *
 * DEAD PROBES for the remaining 4 bytes -- do not re-run (all plateau at 4,
 * or regress):
 *   - memset at every one of the six source positions among the five -1
 *     stores, crossed with limitOn first / second / last (18 builds): only
 *     limitOn-first survives, and all survivors read 4.
 *   - zero chain as three separate statements; chain written 6F24-last;
 *     chain sourced from g_aBrRbPerCar[0]; chain after the loop.
 *   - loop spellings: do/while on the index, do/while on a pointer, while on
 *     a pointer, `for (p = ...; p != end; p++)`, hoisted `p =` before the
 *     chain, separate counter + pointer, `(float)0` vs `0.0f`, int field
 *     instead of float.  Pointer forms cost a REGNORM row; the rest read 4.
 *   - array split 4+1 with 0x10396F20 as its own global, stored before the
 *     chain, after the chain, or via a second one-word memset: the store
 *     lands in the right REGION but takes ecx, not the memset's esi (8).
 *   - memset(...,20) vs sizeof; six-element array absorbing 0x10396F24.
 *   - guard as `!g_brRbInited`; guard against a const 0.
 *   - COMPILER FLAGS ARE NOT THE LEVER: /G5, /O1, /Ox, /Oxs, /O2 /Op,
 *     /O2 /Oy-, /O2 /Gr and /Og /Oi /Ot /Oy /Ob1 /Gs all emit store #5 at
 *     0x4d (the two /O1-family builds also lose the whole shape).
 */
extern int   g_brRbInited;          /* 0x104AB504 */
extern int   g_aBrRbPerCar[5];      /* 0x10396F10 .. 0x10396F20 -- one per car */
extern int   g_brRb6F24;            /* 0x10396F24 */
extern int   g_brRb6F44;            /* 0x10396F44 */
extern int   g_brRbB4E8;            /* 0x104AB4E8 */
extern int   g_brRbB4EC;            /* 0x104AB4EC */
extern int   g_brRbB4F0;            /* 0x104AB4F0 */
extern int   g_brRbB4F4;            /* 0x104AB4F4 */
extern int   g_brRbB4FC;            /* 0x104AB4FC */
extern int   g_brRbB500;            /* 0x104AB500 */
extern int   g_brRaceBeginLimitOn;  /* 0x100A5EA8 */

/* Five car records of 0x2E0F0 bytes starting at 0x10396F48; the loop
 * bound 0x1047D3F8 is the end of the fifth. */
typedef struct {
    float f0;
    char  pad[0x2E0F0 - 4];
} BrRbCar;
extern BrRbCar g_aBrRbCar[5];       /* 0x10396F48 */

/* WHAT IT DOES: the first-time reset of the race-begin state -- the per-car
 * int array is cleared (an inlined memset: five stores from one zeroed
 * register), the position/lap slots go to -1, the limit flag comes on, three
 * float accumulators go to 0 and each car's first field is cleared.  Guarded
 * by a done flag so it only ever runs once. */
/* @t4-pass 0x10013E80 1 2026-09-07 probes 22 bytes 128 insns 30 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10013E80 2 2026-09-07 probes 22 bytes 128 insns 30 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10013E80 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 128/128 insns 30/30 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 2 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10013E80 glide BrRaceBeginResetOnce */
void BrRaceBeginResetOnce(void)
{
    int i;

    if (g_brRbInited == 0) {
        g_brRaceBeginLimitOn = 1;
        g_brRbB4FC = -1;
        memset(g_aBrRbPerCar, 0, sizeof(g_aBrRbPerCar));
        g_brRb6F44 = -1;
        g_brRbB4E8 = -1;
        g_brRbB4EC = -1;
        g_brRbB500 = -1;
        g_brRbB4F0 = g_brRbB4F4 = g_brRb6F24 = 0;
        for (i = 0; i < 5; i++)
            g_aBrRbCar[i].f0 = 0.0f;
        g_brRbInited = 1;
    }
}

/* ------------------------------------------------------------------ */
/* 0x10013F20                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10396f10;
extern int DAT_10396f48;
extern int DAT_104ab4e8;
extern int DAT_104ab4ec;
extern int DAT_104ab500;

/* WHAT IT DOES: picks the snapshot-ring slot for the next physics snapshot:
 * the free slot (flag clear) with the lowest use count, never the one being
 * shown, then rotates the current/previous pair and bumps the chosen slot's
 * use count.  Returns the slot; BrRaceStep writes the snapshot into it.
 *
 * Both branch residues were spellings, not codegen.  `*cost <= best` compiles
 * the compare with cost on the left and skips on `ja`; the original compares
 * the other way round and skips on `jb`, which is `best >= *cost` -- the same
 * condition with the operands swapped.  And the original falls through to the
 * indexed load and jumps away on `jl`, so its guard is `cur >= 0` with the
 * zero in the else arm, not `cur < 0` with the zero first.  Twelve probes;
 * only the two swaps together move it (register-blind rows 2 -> 0).
 *
 * RESIDUE: register-blind rows 0+0 at 157/157 B -- an eax/ebx transposition
 * plus one scheduling window in the tail, where the original sinks the
 * chosen*0x2E0F0 lea chain below the DAT_104AB500 store and VC5 hoists it. */
/* @t4-pass 0x10013F20 1 2026-09-07 probes 92 bytes 157 insns 56 regions 3 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x10013F20 2 2026-09-07 probes 94 bytes 157 insns 56 regions 3 rows 2 census yes  (tools/crank.py) */
/* DEAD 2026-09-13 (fn.py, 3 probes): the head assignments in the orders
 * chosen/best/i/flag/cost/cur, cur first, and the declarations reordered to
 * match -- the eax/ebx transposition is unchanged (157/157, 30+30 raw). */
/* @t4-pass 0x10013F20 3 2026-09-20 probes 84 bytes 157 insns 56 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10013F20 4 2026-09-20 probes 84 bytes 157 insns 56 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10013F20 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 157/157 insns 56/56 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * residue after tools/crank.py: 84 compiles this pass, levers accepted: mut:reorder_stmts;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10013F20 glide BrSnapPickSlot */
int BrSnapPickSlot(void)
{
    int chosen;
    unsigned int best;
    int i;
    unsigned int *cost;
    int *flag;
    int cur;
    int prev;

    chosen = -1;
    best = 0xffffffffu;
    i = 0;
    cost = (unsigned int *)&DAT_10396f48;
    flag = &DAT_10396f10;
    cur = DAT_104ab4e8;
    do {
        if (*flag == 0 && i != cur && best >= *cost) {
            chosen = i;
            best = *cost;
        }
        flag++;
        i++;
        cost += 0xb83c;
    } while ((int)flag < 0x10396f24);
    if (cur >= 0) {
        prev = *(int *)((char *)&DAT_10396f48 + cur * 0x2e0f0);
    } else {
        prev = 0;
    }
    DAT_104ab500 = DAT_104ab4ec;
    DAT_104ab4ec = cur;
    DAT_104ab4e8 = chosen;
    *(int *)((char *)&DAT_10396f48 + chosen * 0x2e0f0) = prev + 1;
    return chosen;   /* original returns the picked slot in eax; callers (BrRaceStep) use it */
}


