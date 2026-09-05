/* br_ctlai.c -- driving: the computer opponent's controller body.
 *
 *   0x1005D770  3858 B   BrCtlAiBody   (D3D 0x10064700, `shared`)
 *
 * One original function, transcribed from orig/BRGlide.dll.  br_ctlstep.c
 * holds the nine-byte thunk 0x1005E690 BrCtlAi that tail-calls it, and
 * include/br_ai.h carries a section-by-section reading of what every block
 * is for (the numbered rules there are cited below by number).  The helpers
 * br_ai.c ports from this function (BrAiLookahead, BrAiAdvanceTarget, ...)
 * are NOT called from here: the original has them in line, and VC5 inlines
 * nothing, so they are written out where they sit.
 *
 * Every vector call is a real call in the original (0x100342B0 cross,
 * 0x10034310 dot, 0x10034560 sub, 0x10034620 lerp, 0x100346A0 mul-add,
 * 0x100346D0 midpoint, 0x100347F0 length, 0x10034420 direction, 0x10034360
 * scale, 0x10034390 scale-in-place, 0x100344D0 guarded normalise) and every
 * one is already matched under the names used below.
 *
 * The car record is 0x2B68 bytes; only the fields this function touches are
 * modelled, at their original offsets, in the local declarations block.
 *
 * ON THE COMPARISONS.  Every clamp is the exact negation the x87 flag test
 * implies: `test ah,0x41` taken = less, equal or unordered; `test ah,1`
 * taken = less or unordered; `test ah,0x40` taken = equal or unordered.
 * Float literals are spelled as the constant pool holds them: VC5 never
 * emits `fadd [const]` -- `x + c` comes out `fsub [-c]` -- so an addition of
 * a constant is written as the subtraction of its negative, which is what
 * the pool entry actually is (-0.03f at 0x1007791C, -0.1f at 0x100778AC,
 * -1.0f at 0x100778E4, -0.2f at 0x10077918, -0.5f at 0x10077904, -3.0f at
 * 0x100778F4).
 *
 * RESIDUE MAP / DEAD PROBES: see the end of this header as they accrue.
 */
#include <stdint.h>

#include "br_vec.h"      /* BrVec3 and the vector leaves                   */
#include "br_match.h"    /* BR_THISCALL1                                   */

/* ---------------------------------------------------------------------
 * Local declarations: the pieces of the car and of the path this function
 * reads, at their original offsets.
 * ------------------------------------------------------------------- */

/* A path point: 0x28 bytes.  br_ai.h "THE NODE AND THE POINT". */
typedef struct BrAiPathPt {
    BrVec3 left;                 /* +0x00 */
    BrVec3 centre;               /* +0x0C */
    BrVec3 right;                /* +0x18 */
    float  arc;                  /* +0x24  distance still to run this lap  */
} BrAiPathPt;

/* A path node: links, count, flags, then `count`+1 points at +0x40. */
typedef struct BrAiPathNode {
    struct BrAiPathNode *pNext;  /* +0x00 */
    struct BrAiPathNode *pSib;   /* +0x04 */
    uint8_t   a08[0x14 - 0x08];
    uint16_t  count;             /* +0x14 */
    uint16_t  flags;             /* +0x16  bit 0 = skip this node          */
    uint8_t   a18[0x40 - 0x18];
    BrAiPathPt aPt[1];           /* +0x40 */
} BrAiPathNode;

/* car->p29C0: the control record the physics reads. */
typedef struct BrAiCtl {
    uint32_t flags;              /* +0x00  0x10000 / 0x20000 / 0x40000     */
    uint8_t  a04[0x20 - 0x04];
    float    steer;              /* +0x20  -1 .. +1                        */
} BrAiCtl;

/* car->pF00: the entrant/profile record. */
typedef struct BrAiProfile {
    uint8_t  a00[0x68];
    uint32_t f68;                /* +0x68  bit 0 = controller disabled     */
    uint8_t  a6C[0x74 - 0x6C];
    int32_t  f74;                /* +0x74  row into the difficulty table   */
} BrAiProfile;

/* The waypoint cursor's two fields are one-member structs so that the
 * corridor scan can take them by value as stack arguments (see BR_AI_SCAN
 * below); every other read goes through the member. */
typedef struct { uint32_t v; }       BrAiIdxArg;
typedef struct { BrAiPathNode *p; }  BrAiNodeArg;

typedef struct BrAiCar {
    BrVec3   fwd;   float f0C;   /* +0x00  frame row 0                     */
    BrVec3   right; float f1C;   /* +0x10  frame row 1                     */
    BrVec3   up;    float f2C;   /* +0x20  frame row 2                     */
    BrVec3   pos;   float f3C;   /* +0x30  frame row 3                     */
    BrVec3   v40;                /* +0x40  (unidentified, br_ai.h rule 5)  */
    uint8_t  a4C[0x1E8 - 0x4C];
    BrVec3   f1E8;               /* +0x1E8 the force this function shapes  */
    uint8_t  a1F4[0x204 - 0x1F4];
    BrVec3   f204;               /* +0x204 clamped to unit length at exit  */
    uint8_t  a210[0x524 - 0x210];
    int32_t  f524;               /* +0x524 gates that clamp                */
    uint8_t  a528[0xEA0 - 0x528];
    int32_t  cHoldFwd;           /* +0xEA0 rule 9                          */
    int32_t  cHoldRev;           /* +0xEA4                                 */
    int32_t  cRevRun;            /* +0xEA8                                 */
    int32_t  cFwdRun;            /* +0xEAC                                 */
    uint8_t  aEB0[0xF00 - 0xEB0];
    BrAiProfile *pProfile;       /* +0xF00                                 */
    uint8_t  aF04[0xF0C - 0xF04];
    BrVec3   aim;                /* +0xF0C the smoothed aim point          */
    uint8_t  aF18[0xF24 - 0xF18];
    BrVec3   tangent;            /* +0xF24 rule 1                          */
    BrVec3   lateral;            /* +0xF30                                 */
    BrVec3   pathUp;             /* +0xF3C                                 */
    float    fF48;               /* +0xF48 |line offset| after overtaking  */
    BrVec3   d;                  /* +0xF4C rule 2's led displacement       */
    float    fF58;               /* +0xF58 rival's line offset (0x1005D3C0)*/
    uint8_t  aF5C[0xF78 - 0xF5C];
    int32_t  fF78;               /* +0xF78                                 */
    uint8_t  aF7C[0xF8C - 0xF7C];
    BrAiNodeArg pNode;           /* +0xF8C the waypoint cursor: node       */
    BrAiIdxArg  iPt;             /* +0xF90                       ... point */
    uint8_t  aF94[0xFF4 - 0xF94];
    float    progress;           /* +0xFF4 cumulative distance             */
    uint8_t  aFF8[0x1024 - 0xFF8];
    BrVec3   vel;                /* +0x1024                                */
    uint8_t  a1030[0x2728 - 0x1030];
    float    aimFwd;             /* +0x2728 dot(row 0, aim direction)      */
    uint8_t  a272C[0x29C0 - 0x272C];
    BrAiCtl *pCtl;               /* +0x29C0                                */
    uint8_t  a29C4[0x2B68 - 0x29C4];
} BrAiCar;                       /* 0x2B68 */

/* The entrant table at 0x10AF0858: 0x80-byte slots, the car pointer first. */
typedef struct BrAiDriverSlot {
    BrAiCar *pCar;               /* +0x00 -- NULL for an empty slot        */
    uint8_t  a04[0x80 - 0x04];
} BrAiDriverSlot;

/* The menu record at 0x10AF2094: two option bytes pick the difficulty row. */
typedef struct BrAiMenuRec {
    uint8_t  a00[4];
    uint8_t  b4;                 /* +0x04 */
    uint8_t  b5;                 /* +0x05 */
} BrAiMenuRec;

extern int32_t       g_brRaceMode;            /* 0x100A9360                */
extern int32_t       g_brCarPhysWeather;      /* 0x104B15E8                */
extern int32_t       g_brRaceNDriver;         /* 0x100B2F00                */
extern BrAiDriverSlot g_aBrDriverSlot[];      /* 0x10AF0858                */
extern BrAiPathNode *g_pBrAiPathRoot;         /* 0x106EED48 (track hdr +0x70) */
extern BrAiMenuRec  *g_pBrMenuRec;            /* 0x10AF2094                */
extern const float   g_aBrAiDiffScale[];      /* 0x100B30A8                */
extern float         g_brAiBlend;             /* 0x106E9D8C                */

/* Written by the corridor scan 0x1005D060 (br_ai.h rule 8). */
extern int32_t       g_brAiScanN;             /* 0x10AC680C                */
extern BrVec3        g_aBrAiScanA[];          /* 0x10B1C9B8                */
extern BrVec3        g_aBrAiScanB[];          /* 0x10B1CA28                */
extern BrVec3        g_brAiScanAim;           /* 0x10B1CE88                */
extern BrVec3        g_brAiScanPt;            /* 0x10AF11F8                */

/* The requested steering bias, two globals set in matched pairs (rule 4). */
extern int32_t       g_brAiBiasPos;           /* 0x10B1CBE8                */
extern int32_t       g_brAiBiasNeg;           /* 0x10B1CF04                */

int  BrPodNop();                              /* 0x10008D60, the no-op logger */
void BrVec3NormaliseGuard(BrVec3 *pV);        /* 0x100344D0                */

#ifdef BR_MATCHING_BUILD
/* thiscall callees, reached through __fastcall (br_match.h).  The corridor
 * scan takes three stack arguments: they are typed so that none is
 * register-eligible -- a float for the constant (pushed as an immediate),
 * one-member structs for the two loads -- which leaves edx alone and has
 * the callee clean its own 12 bytes, exactly as thiscall does. */
uint32_t __fastcall BrAiScanCorridor(BrAiCar *pCar, float a, BrAiIdxArg idx,
                                     BrAiNodeArg node);   /* 0x1005D060 */
#define BR_AI_SCAN(pCar, a, idx, node) \
    BrAiScanCorridor((pCar), (float)(a), (idx), (node))
#else
uint32_t BrAiScanCorridor(BrAiCar *pCar, int32_t a, uint32_t idx,
                          BrAiPathNode *pNode);
#define BR_AI_SCAN(pCar, a, idx, node) \
    BrAiScanCorridor((pCar), (a), (idx).v, (node).p)
#endif
void BR_THISCALL1 BrVec3Predict(BrAiCar *pCar);            /* 0x10001C90 */
void BR_THISCALL1 BrCtlAiLineStep(BrAiCar *pCar);          /* 0x1005D3C0 */
void BR_THISCALL1 BrCarCtlChain_1006F170(BrAiCar *pCar);   /* 0x1006F170 */
void BR_THISCALL1 BrCtlAiRespawn(BrAiCar *pCar);           /* 0x1005C6D0 */

/* +1 above 0.1f, -1 below -0.1f, else 0 -- three times in line (rule 5). */
#define BR_AI_SIGN3(v, s) \
    if ((v) > 0.1f) (s) = 1; else if ((v) < -0.1f) (s) = -1; else (s) = 0

/* WHAT IT DOES: drive one computer-controlled car for this frame. It walks
 * the car's waypoint cursor ahead by a speed-scaled lookahead, smooths the
 * car's aim point toward that waypoint (and toward the corridor scan's own
 * suggestion when the scan finds one), builds the path frame at the
 * waypoint, and turns the car's offset from the racing line and its heading
 * error into a steering request: a bias, a magnitude, and a response curve.
 * A throttle ladder over the corridor scan's wall list decides how hard to
 * lift or brake, a four-counter latch flips the car between forward and
 * reverse when it has been stuck for thirty frames, the nearest rival
 * ahead nudges the line offset so the car moves to the far side of it, and
 * finally a force at car+0x1E8 is shaped along the path frame and scaled by
 * difficulty or weather before the shared per-car chain and respawn run. */
/* @implements 0x1005D770 glide BrCtlAiBody */
void BR_THISCALL1 BrCtlAiBody(BrAiCar *pCar)
{
    BrAiPathNode *pNode;
    uint32_t i;
    BrAiIdxArg  idx;             /* the cursor as the scan receives it     */
    BrAiNodeArg node;
    float    t;
    BrVec3   vTarget;            /* the waypoint the cursor lands on       */
    float    velFwd;             /* dot(velocity, row 0)                   */
    BrVec3   aimDir;             /* normalise(aim - pos)                   */
    BrVec3   vUp;                /* (0, 0, 1)                              */
    BrVec3   vUpCrossAim;        /* up x aimDir                            */
    BrVec3   vDead;              /* aimDir x (up x aimDir), never read     */
    float    lat;                /* dot(row 1, aimDir), then the steer     */
    float    offset;             /* rule 2                                 */
    float    heading;
    float    absOffset;
    float    mag;                /* rule 4/5's magnitude                   */
    BrVec3   vEdge;              /* left - centre                          */
    float    halfWidth;
    float    limit;
    float    scale;              /* rule 8's throttle scale                */
    int32_t  level;
    BrVec3   vA, vB, vN;         /* rule 8's triangle and its normal       */
    float    q, tq, fl;
    float    k;
    float    speed;
    int      sVel, sAux, sFwd;
    float    f;
    int32_t  iBest, iDrv;
    float    best, add, lap, d, diff;
    float    t1, t3, len;
    short    w;

    if (!(pCar->pProfile->f68 & 1)) {
        if (pCar->pCtl->flags & 0x2000000) {
            pCar->fF78 = 1;
            BrVec3Predict(pCar);
        }
        BrPodNop(0, 0xff, 0xff, 0xff, 0xff);

        /* The lookahead and the cursor walk (br_ai.h, BrAiLookahead /
         * BrAiAdvanceTarget). */
        pNode = pCar->pNode.p;
        i = pCar->iPt.v;
        t = 20.0f - BrVec3Length(&pCar->vel) * -3.0f;
        idx = pCar->iPt;
        node = pCar->pNode;
        vTarget = node.p->aPt[idx.v].centre;
        if (t > 80.0f)
            t = 80.0f;
        do {
            t -= pNode->aPt[i].arc - pNode->aPt[i + 1].arc;
            i++;
            if (i == pNode->count) {
                pNode = pNode->pNext;
                while (pNode->flags & 1)
                    pNode = pNode->pSib;
                i = 0;
            }
        } while (t >= 0.0f);
        vTarget = pNode->aPt[i].centre;

        /* The corridor scan, and its own aim suggestion. */
        if (BR_AI_SCAN(pCar, 0, idx, node) != 0) {
            BrVec3Midpoint(&g_brAiScanAim, &g_brAiScanAim, &g_brAiScanPt);
            if (pCar->aim.x == 0.0f && pCar->aim.y == 0.0f
                && pCar->aim.z == 0.0f)
                pCar->aim = g_brAiScanAim;
            else
                BrVec3Lerp(&pCar->aim, &pCar->aim, &g_brAiScanAim,
                           g_brAiBlend - -0.5f);
        }
        BrVec3Lerp(&pCar->aim, &pCar->aim, &vTarget, 0.4f);   /* rule 11 */

        velFwd = BrVec3Dot(&pCar->vel, &pCar->fwd);
        BrVec3Direction(&aimDir, &pCar->pos, &pCar->aim);
        vUp.x = 0.0f;
        vUp.y = 0.0f;
        vUp.z = 1.0f;
        BrVec3Cross(&vUpCrossAim, &vUp, &aimDir);
        BrVec3Cross(&vDead, &aimDir, &vUpCrossAim);
        lat = BrVec3Dot(&pCar->right, &aimDir);
        pCar->aimFwd = BrVec3Dot(&pCar->fwd, &aimDir);
        pCar->pCtl->flags |= 0x10000;

        /* 1. the path frame at the target */
        BrVec3Direction(&pCar->tangent, &pCar->pNode.p->aPt[pCar->iPt.v].centre,
                        &pCar->pNode.p->aPt[pCar->iPt.v + 1].centre);
        BrVec3Sub(&pCar->lateral, &pCar->pNode.p->aPt[pCar->iPt.v].left,
                  &pCar->pNode.p->aPt[pCar->iPt.v].right);
        BrVec3Cross(&pCar->pathUp, &pCar->tangent, &pCar->lateral);
        BrVec3NormaliseGuard(&pCar->pathUp);
        BrVec3Cross(&pCar->lateral, &pCar->pathUp, &pCar->tangent);
        BrVec3NormaliseGuard(&pCar->lateral);

        /* 2. the signed line offset, and the heading term */
        BrVec3Sub(&pCar->d, &pCar->pos, &pCar->pNode.p->aPt[pCar->iPt.v].centre);
        BrVec3MulAddTo(&pCar->d, &pCar->vel, 0.4f);
        offset = BrVec3Dot(&pCar->lateral, &pCar->d);
        mag = 0.0f;
        heading = BrVec3Dot(&pCar->fwd, &pCar->lateral);
        g_brAiBiasPos = 0;
        g_brAiBiasNeg = 0;
        if (offset < 0.0f)
            absOffset = -offset;
        else
            absOffset = offset;

        /* 3. the corridor */
        BrVec3Sub(&vEdge, &pCar->pNode.p->aPt[pCar->iPt.v].left,
                  &pCar->pNode.p->aPt[pCar->iPt.v].centre);
        halfWidth = BrVec3Dot(&pCar->lateral, &vEdge);
        limit = (halfWidth > 5.0f) ? halfWidth - 3.0f : halfWidth * 0.4f;

        if (absOffset > limit) {
            /* 4. the correction law */
            if (offset > 0.0f) {
                if (heading > -0.05) {
                    g_brAiBiasPos = 0;
                    g_brAiBiasNeg = 1;
                    mag = (absOffset - limit) / absOffset
                          * (heading * -0.2f) - -0.03f;
                } else if (heading < -0.15) {
                    g_brAiBiasPos = 1;
                    g_brAiBiasNeg = 0;
                    mag = (absOffset - limit) / absOffset
                          * (heading * -0.3f) - -0.1f;
                }
            } else if (heading < 0.05) {
                g_brAiBiasPos = 1;
                g_brAiBiasNeg = 0;
                mag = (absOffset - limit) / absOffset
                      * (heading * 0.2f) - -0.03f;
            } else if (heading > 0.15) {
                g_brAiBiasPos = 0;
                g_brAiBiasNeg = 1;
                mag = (absOffset - limit) / absOffset
                      * (heading * 0.3f) - -0.1f;
            }
        } else if (BrVec3Length(&pCar->vel) > 10.0f) {
            /* 5. the in-corridor arbitration */
            f = BrVec3Dot(&pCar->vel, &pCar->lateral);
            BR_AI_SIGN3(f, sVel);
            f = BrVec3Dot(&pCar->v40, &vUpCrossAim);
            BR_AI_SIGN3(f, sAux);
            f = BrVec3Dot(&pCar->fwd, &vUpCrossAim);
            BR_AI_SIGN3(f, sFwd);
            if (sVel != 0 || sFwd != 0) {
                if (sVel != 0 && sVel + sFwd == 0) {
                    if (sVel == 1 && sAux == 1) {
                        g_brAiBiasNeg = 0;
                        g_brAiBiasPos = 1;
                    } else if (sVel == -1 && sAux == -1) {
                        g_brAiBiasNeg = 1;
                        g_brAiBiasPos = 0;
                    }
                    mag = 0.1f;
                } else if (sVel != 0 && sVel == sFwd) {
                    if (sVel == 1 && sAux == 1) {
                        g_brAiBiasNeg = 1;
                        g_brAiBiasPos = 0;
                    } else if (sVel == -1 && sAux == -1) {
                        g_brAiBiasNeg = 0;
                        g_brAiBiasPos = 1;
                    }
                    mag = 0.4f;
                } else if (sAux != 0 || sFwd != 0) {
                    if (sAux != 0 || sVel != 0) {
                        if (sAux == sFwd) {
                            if (sAux == 1) {
                                g_brAiBiasNeg = 1;
                                g_brAiBiasPos = 0;
                            } else if (sAux == -1) {
                                g_brAiBiasNeg = 0;
                                g_brAiBiasPos = 1;
                            }
                            mag = 0.5f;
                        } else if (sAux != 0 && sAux + sVel == 0) {
                            goto ladder;
                        }
                    }
                    pCar->pCtl->flags &= ~0x10000;
                }
            }
        }

ladder:
        /* 8. the throttle ladder */
        scale = 1.0f;
        if (g_brAiScanN > 0) {
            level = 1;
            do {
                BrVec3Sub(&vA, &g_aBrAiScanA[level - 1], &g_aBrAiScanB[level - 1]);
                BrVec3Sub(&vB, &g_aBrAiScanB[level], &g_aBrAiScanB[level - 1]);
                BrVec3Cross(&vN, &vB, &vA);
                BrVec3NormaliseGuard(&vN);
                q = BrVec3Dot(&pCar->vel, &vN);
                tq = BrVec3Dot(&pCar->vel, &vA) * 0.03f;
                if (tq > q)
                    q = tq;
                else if (tq < -q)
                    q = -tq;
                fl = (float)level;
                if (q > fl * 6.0f) {
                    scale = 2.0f;
                    pCar->pCtl->flags &= ~0x10000;
                    pCar->pCtl->flags |= 0x40000;
                    break;
                }
                if (q > fl * 4.5f) {
                    scale = 2.0f;
                    pCar->pCtl->flags &= ~0x10000;
                    break;
                }
                if (q > fl * 3.0f) {
                    scale = 1.3f;
                    break;
                }
            } while (level++ < g_brAiScanN);
        }

        /* 6. the aim error, scaled and clamped */
        if (BrVec3Length(&pCar->vel) > 10.0f) {
            lat = scale * lat;
            if (lat > 1.0f) {
                lat = 1.0f;
                pCar->pCtl->flags &= ~0x10000;
            } else if (lat < -1.0f) {
                lat = -1.0f;
                pCar->pCtl->flags &= ~0x10000;
            }
        }

        /* 7. the response curve */
        if (mag < 0.01f)
            mag = 0.01f;
        k = 0.2f / mag;
        if (lat < 0.0f) {
            if (g_brAiBiasPos != 0) {
                if (lat < -k)
                    lat = lat - mag * lat;
                else
                    lat = lat - -0.2f;
            } else if (g_brAiBiasNeg != 0) {
                if (lat < -k)
                    lat = (mag - -1.0f) * lat;
                else
                    lat = lat - 0.2f;
            }
            lat = lat - -1.0f;
            lat = lat * lat * lat * lat - 1.0f;
        } else {
            if (g_brAiBiasPos != 0) {
                if (lat > k)
                    lat = (mag - -1.0f) * lat;
                else
                    lat = lat - -0.2f;
            } else if (g_brAiBiasNeg != 0) {
                if (lat > k)
                    lat = lat - mag * lat;
                else
                    lat = lat - 0.2f;
            }
            lat = 1.0f - lat;
            lat = 1.0f - lat * lat * lat * lat;
        }

        /* 9. the recovery timers: forward, reverse or brake */
        speed = BrVec3Length(&pCar->vel);
        if (pCar->aimFwd >= 0.0f) {
            if (pCar->cHoldRev != 0) {
                pCar->cHoldRev--;
                goto reverse;
            }
            if (velFwd < -1.0f)
                goto brake;
            goto forward;
        } else {
            if (pCar->cHoldFwd != 0) {
                pCar->cHoldFwd--;
                goto forward;
            }
            if (velFwd > 1.0f)
                goto brake;
            goto reverse;
        }
forward:
        pCar->pCtl->steer = -lat;
        pCar->cFwdRun++;
        if (pCar->cFwdRun > 30 && speed < 1.0f) {
            pCar->cHoldRev = 60;
            pCar->cRevRun = 0;
            pCar->cFwdRun = 0;
        }
        goto stepped;
brake:
        pCar->pCtl->flags |= 0x40000;
        goto stepped;
reverse:
        pCar->pCtl->flags |= 0x10000;
        if (lat < 0.0f)
            pCar->pCtl->steer = -1.0f;
        else
            pCar->pCtl->steer = 1.0f;
        pCar->pCtl->flags |= 0x20000;
        if (pCar->cRevRun > 150) {
            if (pCar->cRevRun > 270)
                pCar->cRevRun = 30;
            else
                pCar->pCtl->steer *= -1.0;
        }
        pCar->cRevRun++;
        if (pCar->cRevRun > 30 && speed < 1.0f) {
            pCar->cHoldFwd = 60;
            pCar->cRevRun = 0;
            pCar->cFwdRun = 0;
        }
stepped:

        /* 10. the overtaking pass */
        BrCtlAiLineStep(pCar);
        iBest = -1;
        best = 90.0f;
        add = 0.0f;
        if (g_brRaceNDriver > 0) {
            iDrv = 0;
            do {
                if (g_aBrDriverSlot[iDrv].pCar != 0
                    && g_aBrDriverSlot[iDrv].pCar != pCar) {
                    lap = g_pBrAiPathRoot->aPt[0].arc;
                    d = g_aBrDriverSlot[iDrv].pCar->progress - pCar->progress;
                    while (d > lap)
                        d -= lap;
                    while (d < -lap)
                        d += lap;
                    if (d > 0.0f && d < best) {
                        diff = g_aBrDriverSlot[iDrv].pCar->fF58 - offset;
                        best = d;
                        if (diff < 3.0f) {
                            add = (1.0f - d * 0.011111111f) * -10.0f;
                            if (add < -5.0f)
                                add = -5.0f;
                            iBest = iDrv;
                        } else if (diff > -3.0f) {
                            add = (1.0f - d * 0.011111111f) * 10.0f;
                            if (add > 5.0f)
                                add = 5.0f;
                            iBest = iDrv;
                        }
                    }
                }
                iDrv++;
            } while (iDrv < g_brRaceNDriver);
        }
        if (iBest != -1) {
            offset = add + offset;
            pCar->fF48 = (offset < 0.0f) ? -offset : offset;
        }

        /* The force at car+0x1E8, shaped along the path frame. */
        if (velFwd > 3.0f) {
            if (pCar->fF48 < halfWidth - -1.0f) {
                k = (velFwd - 3.0f) * 0.015625f;
                if (k < 0.0f)
                    k = k * -0.25f;
                if (k > 0.4f)
                    k = 0.4f;
                f = k * offset;
                t1 = BrVec3Dot(&pCar->f1E8, &pCar->tangent);
                BrVec3Dot(&pCar->f1E8, &pCar->lateral);
                t3 = BrVec3Dot(&pCar->f1E8, &pCar->pathUp);
                BrVec3Scale(&pCar->f1E8, &pCar->tangent, t1);
                BrVec3MulAddTo(&pCar->f1E8, &pCar->lateral, -f);
                BrVec3MulAddTo(&pCar->f1E8, &pCar->pathUp, t3);
            }
            if (g_brRaceMode == 0) {
                BrVec3ScaleBy(&pCar->f1E8,
                              g_aBrAiDiffScale[pCar->pProfile->f74
                                  + (g_pBrMenuRec->b4 * 4 + g_pBrMenuRec->b5) * 2]);
            } else if (g_brRaceMode == 1 || g_brRaceMode == 6) {
                w = (short)g_brCarPhysWeather - 1;
                if (w > 2 || w < 0)
                    w = 0;
                if (w == 2)
                    BrVec3ScaleBy(&pCar->f1E8, 0.99f);
                else
                    BrVec3ScaleBy(&pCar->f1E8, 0.999f);
            }
        }
    }

    BrPodNop(0, 0, 0x82, 0, 0xff);
    len = BrVec3Length(&pCar->f204);
    if (len > 1.0f && pCar->f524 != 0)
        BrVec3ScaleBy(&pCar->f204, 1.0f / len);
    BrCarCtlChain_1006F170(pCar);
    BrCtlAiRespawn(pCar);
}
