/* br_chasestep.c -- drawing: the chase camera's per-frame step.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * One function, 0x10001CF0, the caller of the br_chasecam.c helpers.  It
 * lives in its own file so that the three byte-exact helpers keep their
 * surroundings.
 */
#include <stdint.h>
#include "br_vec.h"
#include "slice3_41.h"   /* BrDriverCar, the canonical record */


/* BrVec3: br_vec.h */
/* BrMat4: br_mat.h */

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

/* 64-bit core: declared once, in br_globals.h or its struct's header */                       /* 0x105CCB88 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                          /* 0x100BCDCC */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                          /* 0x10B1CF14 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10B1CF10 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x100AA03C */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrVec3Length: prototype in br_funcs.h */
/* BrVec3MulAddTo: prototype in br_funcs.h */
/* BrVec3Negate: prototype in br_funcs.h */
/* br_dl_normalise: prototype in br_funcs.h */                                       /* 0x100344D0 */
/* BrVec3Cross: prototype in br_funcs.h */
/* BrVec3Add: prototype in br_funcs.h */
/* BrVec3SubFrom: prototype in br_funcs.h */
/* BrVec3DivBy: prototype in br_funcs.h */
/* BrCosF: prototype in br_funcs.h */

/* The helpers are thiscall: `this` in ecx, everything else on the stack.
 * A struct-typed argument is never register-eligible, so wrapping each
 * stack argument keeps edx out of the call (br_match.h's per-site recipe). */
typedef struct BrPtrArg { void *p; } BrPtrArg;

/* FUN_100018f0: prototype in br_funcs.h */
/* BrCamChaseZoomStep: prototype in br_funcs.h */
/* FUN_10001bb0: prototype in br_funcs.h */
/* FUN_10001510: prototype in br_funcs.h */

/* WHAT IT DOES: advances a car's chase camera by one frame.  Smooths the
 * car's speed and spin into the camera's own damped copies, lifts the
 * camera by how far the smoothed spin overshoots, runs the zoom and the
 * placement helpers, drops the camera height while the car is settled (or
 * snaps it while a demo is running), and rebuilds the camera frame: either
 * a copy of the car's own matrix when the view is locked, or a frame looking
 * back along the car's forward axis from twenty units behind.  Finally it
 * refreshes the eye/look/right/up basis, the lens factor from the distance
 * to the eye, and the negated axis rows the renderer reads. */
/* Residue (see the T3 certification below): seven regions, ALL x87 scheduling -- the
 * speed/spin blends' operand side, the lift then-arm pop/push, the height
 * updates (fcom;fstp vs fst;fcomp), the second frame-row product order, the
 * prologue prev copy, and the final axis-negation DAG (the 357 B "never
 * compared" is a resync artifact of dense fxch/fsubp, not missing code;
 * +6 insns total).  Dead (do not re-run): double temps, blend ternaries,
 * named product temps, compound-assign forms, /G3-/G6, VC4.2, /Op (adds
 * reloads).  Matched: 16-byte camera vectors, struct-wrapped thiscall args
 * (no edx), `k` ternary, tail-merged lift call sites, the frame copy in the
 * arg expression, demo branch polarity, pAxZ/pUp/pLook locals, the dead 0.02
 * height store. */
/* @t4-pass 0x10001CF0 2 2026-09-20 probes 14 bytes 1578 insns 408 regions 7 rows 43 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10001CF0 3 2026-09-20 probes 12 bytes 1578 insns 408 regions 7 rows 43 census yes  (tools/brally/crank.py) */
/* @t3 0x10001CF0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1578/1567 insns 408/403 rows 19+24 regions 7 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 2 3
 * Residue is the pure x87 SCHEDULING wall the dossier above describes (seven
 * regions of fxch / fcom-vs-fcomp / fst-vs-fstp stack-depth / fsubp index /
 * a*b+c*d fld-side commutation) -- nothing is missing (the 357 B "never
 * compared" is a resync artifact, +6 insns total).  The A5 behavioural oracle
 * RUNS the whole step on a seeded car object -- speed/spin blends, the lift and
 * placement helpers, the frame rebuild and basis refresh -- and returns
 * EQUIVALENT over 48 seeds, with a negative control on a frame term proving
 * teeth; that supersedes the byte gates.  Do not reopen
 * before the end-grind. */
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

    prev.x = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).x;
    prev.y = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).y;
    prev.z = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).z;
    len1 = BrVec3Length(&(*(struct BrVec3 *)&((BrDriverCar *)(car))->aBody[0].rb.st.angVel.x));
    len2 = BrVec3Length(&(*(struct BrVec3 *)&((BrDriverCar *)(car))->aBody[0].rb.st.vel.x));
    spin = len2 * _DAT_10077040;
    speed = (len1 > _DAT_10077044) ? len1 - _DAT_10077044 : _DAT_10077000;
    if (speed > _DAT_10077048)
        speed = _DAT_10077048;
    if (spin > _DAT_10077048)
        spin = 31.415928f;
    if (speed <= (*(float *)&((BrDriverCar *)(car))->f28D4))
        speed = (*(float *)&((BrDriverCar *)(car))->f28D4) * _DAT_10077050 + speed * _DAT_1007704c;
    (*(float *)&((BrDriverCar *)(car))->f28D4) = speed;
    (*(float *)&((BrDriverCar *)(car))->f28D8) = (*(float *)&((BrDriverCar *)(car))->f28D8) * _DAT_10077050 + spin * _DAT_1007704c;
    (*(float *)&((BrDriverCar *)(car))->f28D8) = (_DAT_10077008 - (*(float *)&((BrDriverCar *)(car))->f28F8) * _DAT_10077054) * (*(float *)&((BrDriverCar *)(car))->f28D8);
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).x = (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).x;
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).y = (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).y;
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).z = (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).z;
    k    = (((DAT_105ccb68[8]) != 0) ? _DAT_10077058 : _DAT_1007705c) * _DAT_10077064;
    lift = k * (*(float *)&((BrDriverCar *)(car))->f28D8);
    s60  = (*(float *)&((BrDriverCar *)(car))->f28D4) * _DAT_10077060;
    if ((s60 - lift) + (*(float *)&((BrDriverCar *)(car))->f28F8) > _DAT_10077068) {
        camArg.p = (*(unsigned char (*)[48])&((BrDriverCar *)(car))->aSnap[1].m[0][0]);
        FUN_100018f0((BrDriverCar *)car, camArg.p, 0.0f);
    } else {
        camArg.p = (*(unsigned char (*)[48])&((BrDriverCar *)(car))->aSnap[1].m[0][0]);
        FUN_100018f0((BrDriverCar *)car, camArg.p, lift - (((*(float *)&((BrDriverCar *)(car))->f28F8) + s60) - _DAT_10077068));
    }
    BrCamChaseZoomStep((char *)car);
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).x = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).x;
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).y = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).y;
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->f28EC[0]).z = (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[1].m[3][0]).z;
    if ((*(int *)&((BrDriverCar *)(car))->fF7C) == 0) {
        prevArg.p = &prev;
        FUN_10001510((BrDriverCar *)car, camArg.p, prevArg.p);
        if (g_BrCamDemo != 0) {
            if ((DAT_105ccb68[8]) != 0) {
                g_BrCamHold = 0x1E;
                if ((*(void * *)&((BrDriverCar *)(car))->pMatA) == (*(unsigned char (*)[48])&((BrDriverCar *)(car))->aSnap[1].m[0][0])) {
                    (*(void * *)&((BrDriverCar *)(car))->pMatA) = &(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]);
                    (*(int *)&((BrDriverCar *)(car))->fF78)    = 2;
                    g_BrCamHold2 = 0x3C;
                }
            }
            if ((*(float *)&((BrDriverCar *)(car))->f28F8) < _DAT_1007706c) {
                pH  = &(*(float *)&((BrDriverCar *)(car))->f28F8);
                *pH = 0.02f;
                (*(float *)&((BrDriverCar *)(car))->f28F8) = 0.05f;
            } else {
                if (((*(float *)&((BrDriverCar *)(car))->f28F8) = (*(float *)&((BrDriverCar *)(car))->f28F8) - _DAT_10077070) > _DAT_10077074)
                    (*(float *)&((BrDriverCar *)(car))->f28F8) = 0.055f;
                (*(float *)&((BrDriverCar *)(car))->f28F8) = 0.05f;
            }
        } else {
            if (g_BrCamHold != 0)
                g_BrCamHold = g_BrCamHold - 1;
            if (((*(float *)&((BrDriverCar *)(car))->f28F8) = (*(float *)&((BrDriverCar *)(car))->f28F8) - _DAT_10077078) < _DAT_10077000)
                (*(float *)&((BrDriverCar *)(car))->f28F8) = 0.0f;
        }
    }
    FUN_10001bb0((BrDriverCar *)car, (float *)camArg.p);
    if ((*(int *)&((BrDriverCar *)(car))->fF7C) != 0) {
        (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]) = *(BrCamFrame *)car;
    } else {
        (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).m[3][0] = (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B8 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2][0] + (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B0 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][0] + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][0];
        (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).m[3][1] = (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B8 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2][1] + (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B0 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][1] + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][1];
        (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).m[3][2] = (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B8 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2][2] + (*(BrCamView * *)&((BrDriverCar *)(car))->pModel)->f80B0 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][2] + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][2];
        BrVec3MulAddTo((BrVec3 *)&(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]), (BrVec3 *)car, -20.0f);
        BrVec3Negate((BrVec3 *)&(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]), (BrVec3 *)&(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]));
        br_dl_normalise((BrVec3 *)&(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]));
        pRow = (BrVec3 *)(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).m[1];
        pRow->x = (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][0];
        pRow->y = (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][1];
        pRow->z = (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][2];
        BrVec3Cross((BrVec3 *)(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).m[2], (BrVec3 *)&(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]), pRow);
    }
    BrVec3MulAddTo(((*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[2].m[0][0]) = (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]), (BrVec3 *)(*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[2].m[0][0]).m[3]), (BrVec3 *)car, 0.6f);
    pUp   = &(*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[3].m[2][0]);
    pLook = &(*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[3].m[0][0]);
    pUp->x = 0.0f;
    pUp->y = 0.0f;
    pUp->z = 1.0f;
    pAxZ = (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2];
    BrVec3Add(pLook, (BrVec3 *)(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3], (BrVec3 *)pAxZ);
    BrVec3SubFrom(pLook, &(*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[3].m[3][0]));
    dist = BrVec3Length(pLook);
    BrVec3DivBy(pLook, dist);
    BrVec3Cross(&(*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[3].m[1][0]), pUp, pLook);
    BrVec3Cross(pUp, pLook, &(*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[3].m[1][0]));
    if (dist <= _DAT_10077008) {
        dist = 0.0f;
    } else if (dist >= _DAT_1007707c) {
        dist = 100.0f;
    } else {
        dist = dist - _DAT_10077008;
    }
    (*(float *)&((BrDriverCar *)(car))->aSnap[3].f40) = (float)((BrCosF((dist - _DAT_10077008) * _DAT_10077084) * _DAT_1007703c
                          - (_DAT_10077088 - dist) * _DAT_1007708c) - _DAT_10077090) * g_BrCamScale;
    (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[2].m[0][0]).w  = g_BrCamScale;
    (*(float *)&((BrDriverCar *)(car))->aSnap[1].f40)     = g_BrCamScale;
    (*(struct BrCamFrame *)&((BrDriverCar *)(car))->aSnap[0].m[0][0]).w   = g_BrCamScale;
    (*(float *)&((BrDriverCar *)(car))->aSnap[5].f40)     = g_BrCamScale;
    pv = (*(BrCamView * *)&((BrDriverCar *)(car))->pModel);
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[3][0]).x = (pAxZ[0] * pv->f80B8 - (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][0] * pv->f80B4 * _DAT_10077094) + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][0];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[3][0]).y = ((*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2][1] * pv->f80B8 - pv->f80B4 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][1] * _DAT_10077094) + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][1];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[0][0]).x    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][0];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[0][0]).y    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][1];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[0][0]).z    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][2];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[1][0]).x    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][0];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[1][0]).y    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][1];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[1][0]).z    = -(*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[1][2];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[3][0]).z = ((*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[2][2] * pv->f80B8 - pv->f80B4 * (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[0][2] * _DAT_10077094) + (*(float (*)[4][4])&((BrDriverCar *)(car))->fwd.x)[3][2];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[2][0]).x     = pAxZ[0];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[2][0]).y     = pAxZ[1];
    (*(struct BrVec3 *)&((BrDriverCar *)(car))->aSnap[5].m[2][0]).z     = pAxZ[2];
    (*(void * *)&((BrDriverCar *)(car))->pMatB) = (*(void * *)&((BrDriverCar *)(car))->pMatA);
}

