/* br_chasestep.c -- drawing: the chase camera's per-frame step.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * One function, 0x10001CF0, the caller of the br_chasecam.c helpers.  It
 * lives in its own file so that the three byte-exact helpers keep their
 * surroundings.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

typedef struct BrVec3 { float x, y, z; } BrVec3;
typedef struct BrMat4 { float m[4][4]; } BrMat4;

/* A camera frame: a 4x4 matrix and one trailing scalar, 0x44 bytes. */
typedef struct BrCamFrame {
    float m[4][4];
    float w;
} BrCamFrame;

/* The view record the car points at through +0x29C4. */
typedef struct BrCamView {
    unsigned char pad[0x80B0];
    float         f80B0;
    float         f80B4;
    float         f80B8;
} BrCamView;

/* The car record, the parts the chase camera step touches. */
typedef struct BrCamCar {
    float          m[4][4];       /* +0x0000 */
    unsigned char  pad0[0x1E8 - 0x40];
    BrVec3         vel;           /* +0x01E8 */
    unsigned char  pad1[0x204 - 0x1F4];
    BrVec3         v204;          /* +0x0204 */
    unsigned char  pad2[0xF78 - 0x210];
    int            mode;          /* +0x0F78 */
    int            locked;        /* +0x0F7C */
    unsigned char  pad3[0x2734 - 0xF80];
    void          *pTarget;       /* +0x2734 */
    void          *pTargetPrev;   /* +0x2738 */
    BrCamFrame     frame;         /* +0x273C */
    unsigned char  cam[0x30];     /* +0x2780 */
    BrVec3         prev;          /* +0x27B0 */
    float          f27BC;
    float          f27C0;
    BrCamFrame     frame2;        /* +0x27C4 */
    BrVec3         look;          /* +0x2808 */
    float          lookW;
    BrVec3         right;         /* +0x2818 */
    float          rightW;
    BrVec3         up;            /* +0x2828 */
    float          upW;
    BrVec3         eye;           /* +0x2838 */
    float          eyeW;
    float          f2848;
    unsigned char  pad4[0x2890 - 0x284C];
    BrVec3         negX;          /* +0x2890 */
    float          f289C;
    BrVec3         negY;          /* +0x28A0 */
    float          f28AC;
    BrVec3         axZ;           /* +0x28B0 */
    float          f28BC;
    BrVec3         shifted;       /* +0x28C0 */
    float          f28CC;
    float          f28D0;
    float          speed;         /* +0x28D4 */
    float          spin;          /* +0x28D8 */
    unsigned char  pad5[0x28EC - 0x28DC];
    BrVec3         last;          /* +0x28EC */
    float          height;        /* +0x28F8 */
    unsigned char  pad6[0x29C4 - 0x28FC];
    BrCamView     *pView;         /* +0x29C4 */
} BrCamCar;

extern int32_t g_brRaceReplay;                       /* 0x105CCB88 */
extern int32_t g_BrCamDemo;                          /* 0x100BCDCC */
extern int32_t g_BrCamHold;                          /* 0x10B1CF14 */
extern int32_t g_BrCamHold2;                         /* 0x10B1CF10 */
extern float   g_BrCamScale;                         /* 0x100AA03C */

extern float _DAT_10077000;
extern float _DAT_10077008;
extern float _DAT_1007703c;
extern float _DAT_10077040;
extern float _DAT_10077044;
extern float _DAT_10077048;
extern float _DAT_1007704c;
extern float _DAT_10077050;
extern float _DAT_10077054;
extern float _DAT_10077058;
extern float _DAT_1007705c;
extern float _DAT_10077060;
extern float _DAT_10077064;
extern float _DAT_10077068;
extern float _DAT_1007706c;
extern float _DAT_10077070;
extern float _DAT_10077074;
extern float _DAT_10077078;
extern float _DAT_1007707c;
extern float _DAT_10077084;
extern float _DAT_10077088;
extern float _DAT_1007708c;
extern float _DAT_10077090;
extern float _DAT_10077094;

float  BrVec3Length(const BrVec3 *pV);                                    /* 0x100347F0 */
void   BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s);            /* 0x100346A0 */
void   BrVec3Negate(BrVec3 *pOut, const BrVec3 *pV);                     /* 0x10034340 */
void   br_dl_normalise(BrVec3 *pV);                                       /* 0x100344D0 */
void   BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);   /* 0x100342B0 */
void   BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);     /* 0x100345C0 */
void   BrVec3SubFrom(BrVec3 *pA, const BrVec3 *pB);                      /* 0x10034590 */
void   BrVec3DivBy(BrVec3 *pV, float s);                                  /* 0x100343F0 */
double BrCosF(float a);                                                   /* 0x100023E0 */

/* The helpers are thiscall: `this` in ecx, everything else on the stack.
 * A struct-typed argument is never register-eligible, so wrapping each
 * stack argument keeps edx out of the call (br_match.h's per-site recipe). */
typedef struct BrPtrArg { void *p; } BrPtrArg;

void __fastcall FUN_100018f0(BrCamCar *car, BrPtrArg cam, float lift);   /* 0x100018F0 */
void __fastcall BrCamChaseZoomStep(char *pCam);                           /* 0x10001A80 */
void __fastcall FUN_10001bb0(BrCamCar *car, BrPtrArg cam);               /* 0x10001BB0 */
void __fastcall FUN_10001510(BrCamCar *car, BrPtrArg cam, BrPtrArg prev);/* 0x10001510 */

/* WHAT IT DOES: advances a car's chase camera by one frame.  Smooths the
 * car's speed and spin into the camera's own damped copies, lifts the
 * camera by how far the smoothed spin overshoots, runs the zoom and the
 * placement helpers, drops the camera height while the car is settled (or
 * snaps it while a demo is running), and rebuilds the camera frame: either
 * a copy of the car's own matrix when the view is locked, or a frame looking
 * back along the car's forward axis from twenty units behind.  Finally it
 * refreshes the eye/look/right/up basis, the lens factor from the distance
 * to the eye, and the negated axis rows the renderer reads. */
/* T2 residue (2026-09-13): 1578/1567 B, 408/403 insns, seven regions, all
 * x87 operand handling: the speed/spin blends (original multiplies the
 * stack-resident value first, this build the reloaded field), the lift
 * call's then-arm pops and push placement, the height updates (original
 * compares before it stores, `fcom; fstp`, this build `fst; fcomp`), the
 * second frame-row product order, the prologue copy of the previous
 * position, and the final axis-negation DAG (357 bytes the aligner cannot
 * resync).  Under /Op the operand orders match but /Op adds reloads.
 * Dead: double-typed temps, ternaries for the blend, named product temps,
 * compound assignment forms, /G3-/G6, VC4.2.  Matched: the four camera
 * vectors are 16 bytes, the thiscall helpers take struct-wrapped stack
 * arguments (no edx set-up), `k` as a ternary, two call sites for the lift
 * (tail-merged), the frame copy inside the argument expression, the demo
 * branch polarity, pAxZ/pUp/pLook pointer locals, the first height store
 * through a pointer (keeps the dead 0.02 store the original has).
 * @t4-pass 0x10001CF0 r7 2026-09-13 probes 35 bytes 11 insns 5 regions 7 rows 15+10 census no
 * T3 verdict (2026-09-15, CORRECTED): x87 SCHEDULING wall, NOT missing code.
 * Recorded variant is O2 (frame-correct here).  The A4 "357 B never compared"
 * is a RESYNC ARTIFACT, not an absent block: the whole-function register-blind
 * msetdiff is only 19+24 = 43 rows and the function is +6 insns total, so
 * nothing is missing -- dense fxch/fsubp permutation just defeats the 8-insn
 * resync key.  The 43 rows are entirely x87: fxch st(1/2/4/5/6), fcom vs
 * fcomp, fst vs fstp (stack-depth), fsubp with varying stack indices, and the
 * a*b+c*d fld-side commutation (region #6).  Same class as carcol/cartrail:
 * respelling-dead, co-filing NULL. Parks as T2 (A2 43 vs 10.1). NOT a
 * transcription target -- the earlier "missing code" reading was wrong.
 */
/* @t4-pass 0x10001CF0 2 2026-09-20 probes 14 bytes 1578 insns 408 regions 7 rows 43 census yes  (tools/crank.py) */
/* @implements 0x10001CF0 glide BrCamChaseStep */
void __fastcall BrCamChaseStep(BrCamCar *car)
{
    BrVec3  prev;
    float   len1, len2, speed, spin, lift, dist;
    float   k, s60;
    BrVec3 *pRow;
    BrPtrArg camArg, prevArg;
    float  *pAxZ;
    float  *pH;
    BrVec3 *pUp, *pLook;
    BrCamView *pv;

    prev.x = car->prev.x;
    prev.y = car->prev.y;
    prev.z = car->prev.z;
    len1 = BrVec3Length(&car->v204);
    len2 = BrVec3Length(&car->vel);
    spin = len2 * _DAT_10077040;
    speed = (len1 > _DAT_10077044) ? len1 - _DAT_10077044 : _DAT_10077000;
    if (speed > _DAT_10077048)
        speed = _DAT_10077048;
    if (spin > _DAT_10077048)
        spin = 31.415928f;
    if (speed <= car->speed)
        speed = car->speed * _DAT_10077050 + speed * _DAT_1007704c;
    car->speed = speed;
    car->spin = car->spin * _DAT_10077050 + spin * _DAT_1007704c;
    car->spin = (_DAT_10077008 - car->height * _DAT_10077054) * car->spin;
    car->prev.x = car->last.x;
    car->prev.y = car->last.y;
    car->prev.z = car->last.z;
    k    = ((g_brRaceReplay != 0) ? _DAT_10077058 : _DAT_1007705c) * _DAT_10077064;
    lift = k * car->spin;
    s60  = car->speed * _DAT_10077060;
    if ((s60 - lift) + car->height > _DAT_10077068) {
        camArg.p = car->cam;
        FUN_100018f0(car, camArg, 0.0f);
    } else {
        camArg.p = car->cam;
        FUN_100018f0(car, camArg, lift - ((car->height + s60) - _DAT_10077068));
    }
    BrCamChaseZoomStep((char *)car);
    car->last.x = car->prev.x;
    car->last.y = car->prev.y;
    car->last.z = car->prev.z;
    if (car->locked == 0) {
        prevArg.p = &prev;
        FUN_10001510(car, camArg, prevArg);
        if (g_BrCamDemo != 0) {
            if (g_brRaceReplay != 0) {
                g_BrCamHold = 0x1E;
                if (car->pTarget == car->cam) {
                    car->pTarget = &car->frame;
                    car->mode    = 2;
                    g_BrCamHold2 = 0x3C;
                }
            }
            if (car->height < _DAT_1007706c) {
                pH  = &car->height;
                *pH = 0.02f;
                car->height = 0.05f;
            } else {
                if ((car->height = car->height - _DAT_10077070) > _DAT_10077074)
                    car->height = 0.055f;
                car->height = 0.05f;
            }
        } else {
            if (g_BrCamHold != 0)
                g_BrCamHold = g_BrCamHold - 1;
            if ((car->height = car->height - _DAT_10077078) < _DAT_10077000)
                car->height = 0.0f;
        }
    }
    FUN_10001bb0(car, camArg);
    if (car->locked != 0) {
        car->frame = *(BrCamFrame *)car;
    } else {
        car->frame.m[3][0] = car->pView->f80B8 * car->m[2][0] + car->pView->f80B0 * car->m[0][0] + car->m[3][0];
        car->frame.m[3][1] = car->pView->f80B8 * car->m[2][1] + car->pView->f80B0 * car->m[0][1] + car->m[3][1];
        car->frame.m[3][2] = car->pView->f80B8 * car->m[2][2] + car->pView->f80B0 * car->m[0][2] + car->m[3][2];
        BrVec3MulAddTo((BrVec3 *)&car->frame, (BrVec3 *)car, -20.0f);
        BrVec3Negate((BrVec3 *)&car->frame, (BrVec3 *)&car->frame);
        br_dl_normalise((BrVec3 *)&car->frame);
        pRow = (BrVec3 *)car->frame.m[1];
        pRow->x = car->m[1][0];
        pRow->y = car->m[1][1];
        pRow->z = car->m[1][2];
        BrVec3Cross((BrVec3 *)car->frame.m[2], (BrVec3 *)&car->frame, pRow);
    }
    BrVec3MulAddTo((car->frame2 = car->frame, (BrVec3 *)car->frame2.m[3]), (BrVec3 *)car, 0.6f);
    pUp   = &car->up;
    pLook = &car->look;
    pUp->x = 0.0f;
    pUp->y = 0.0f;
    pUp->z = 1.0f;
    pAxZ = car->m[2];
    BrVec3Add(pLook, (BrVec3 *)car->m[3], (BrVec3 *)pAxZ);
    BrVec3SubFrom(pLook, &car->eye);
    dist = BrVec3Length(pLook);
    BrVec3DivBy(pLook, dist);
    BrVec3Cross(&car->right, pUp, pLook);
    BrVec3Cross(pUp, pLook, &car->right);
    if (dist <= _DAT_10077008) {
        dist = 0.0f;
    } else if (dist >= _DAT_1007707c) {
        dist = 100.0f;
    } else {
        dist = dist - _DAT_10077008;
    }
    car->f2848 = (float)((BrCosF((dist - _DAT_10077008) * _DAT_10077084) * _DAT_1007703c
                          - (_DAT_10077088 - dist) * _DAT_1007708c) - _DAT_10077090) * g_BrCamScale;
    car->frame2.w  = g_BrCamScale;
    car->f27C0     = g_BrCamScale;
    car->frame.w   = g_BrCamScale;
    car->f28D0     = g_BrCamScale;
    pv = car->pView;
    car->shifted.x = (pAxZ[0] * pv->f80B8 - car->m[0][0] * pv->f80B4 * _DAT_10077094) + car->m[3][0];
    car->shifted.y = (car->m[2][1] * pv->f80B8 - pv->f80B4 * car->m[0][1] * _DAT_10077094) + car->m[3][1];
    car->negX.x    = -car->m[0][0];
    car->negX.y    = -car->m[0][1];
    car->negX.z    = -car->m[0][2];
    car->negY.x    = -car->m[1][0];
    car->negY.y    = -car->m[1][1];
    car->negY.z    = -car->m[1][2];
    car->shifted.z = (car->m[2][2] * pv->f80B8 - pv->f80B4 * car->m[0][2] * _DAT_10077094) + car->m[3][2];
    car->axZ.x     = pAxZ[0];
    car->axZ.y     = pAxZ[1];
    car->axZ.z     = pAxZ[2];
    car->pTargetPrev = car->pTarget;
}

#endif /* BR_MATCHING_BUILD */
