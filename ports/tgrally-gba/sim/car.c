/* car.c -- a car's physics frame: racing/cartick.c BrCarPhysTick and BrGroundRay,
 * driving/carphys.c (BrCarPhysStep, BrCarDriveInput, BrCarBuildMatrices,
 * BrCarPhysInit) and geometry/tri.c BrTriContainsPoint, transcribed.
 *
 * The car's wheel bodies sit in memory at 0x350, 0x558, 0x760 and 0x968 (wb[0..3]);
 * the game's wheel[0..3] (and the chassis' sub[]) are wb[0], wb[2], wb[1], wb[3];
 * its wheels[0..3] (the draw view) are wb[0..3] in memory order. */
#include "vec.h"

#define WHEEL(car, i) ((car)->body.sub[i])
#define FORCE(car, off) (&(car)->force[((off) - 0xB70) / 0x20])

const Track *g_track;
World g_world;
fx g_grip[72];
void (*sim_camera)(Car *car);           /* the viewed car's chase camera (BrCamChaseStep) */

void BrRbIntegrate(RbState *s, Body *r, fx dt);
void BrCarAxleGrip(Body *b, fx dt, fx *gripF, fx *gripR, uint8_t *slipFp, uint8_t *slipRp);
void BrWheelTyre(Body *b, Body *w, fx *pA, uint8_t *pB, fx dt);
void BrRbForcesClear(Body *b);
void BrTyreSkidCheck(Body *b, Force *f);
void BrTyreSprings(Body *b);
void BrTyreLoads(Body *b);
void BrTyreDepthAll(Body *b);
void BrCarPhysAdvance(Body *b);
SimCell BrCollGridCellAcquire(fx x, fx y);

/* ---- geometry/tri.c ---- */
int BrTriContainsPointV(const fx *pt, const fx *a, const fx *b, const fx *c, const fx *ref)
{
    fx toA[3], toB[3], e1[3], e2[3], e3[3], n[3];
    BrVec3Sub(e1, b, a);
    BrVec3Sub(toB, pt, b);
    BrVec3Cross(n, e1, toB);
    if (BrVec3Dot(n, ref) < FX(0.0f))
        return 0;
    BrVec3Sub(e2, c, b);
    BrVec3Cross(n, e2, toB);
    if (BrVec3Dot(n, ref) < FX(0.0f))
        return 0;
    BrVec3Sub(e3, a, c);
    BrVec3Sub(toA, pt, a);
    BrVec3Cross(n, e3, toA);
    if (BrVec3Dot(n, ref) < FX(0.0f))
        return 0;
    return 1;
}

/* ---- racing/cartick.c BrGroundRay: a ray straight down through the cell under the eye:
   the nearest upward-facing triangle at or below 1.5 above it gives the ground (height,
   normal, triangle, distance), each one hit adding its surface and triggers to the lists;
   failing any, the nearest just above (within 1) ---- */
int BrGroundRay(fx *pPosOut, fx *pNormOut, const fx *pEye, uint16_t *pNearIds, int *pGotHit, uint16_t *pFarIds,
                int *pFarCount, fx *pDistOut, int *pFaceOut)
{
    int hitCount, nearCount, farCount, ci, i;
    fx dt, dir[3], bestFarHitZ, bestNearHitZ, bestNearDist, bestFarDist, tmpV[3], t, dn, hitPt[3], origin[3], dist;
    fx bestFarNorm[3], bestNearNorm[3], x, y;
    uint16_t farFaceVal, farFaceIdx;
    const Plane *pP;
    SimCell cell;
    const Track *T = g_track;

    for (i = 0; i < 3; i++) {
        bestFarNorm[i] = T->defNormFar[i];
        bestNearNorm[i] = T->defNormNear[i];
    }
    bestFarHitZ = pEye[2];
    bestNearHitZ = pEye[2];
    dt = T->x3c - T->x38;
    bestNearDist = bestFarDist = FMUL(dt, dt) + FX(1.0f);
    farFaceVal = 0;
    farFaceIdx = 0;
    dir[0] = FX(0.0f);
    dir[1] = FX(0.0f);
    dir[2] = FX(1.f);
    x = pEye[0];
    y = pEye[1];
    origin[1] = y;
    origin[0] = x;
    origin[2] = FTOF(FXD(1.0));
    hitCount = 0;
    *pFaceOut = 0;
    nearCount = 0;
    farCount = 0;
    cell = BrCollGridCellAcquire(origin[0], origin[1]);
    for (i = 0; i < cell.n; i++) {
        pP = &T->planes[cell.tris[i]];
        if (x < pP->xmin || x > pP->xmax || y < pP->ymin || y > pP->ymax)
            continue;                   /* (the vertical ray misses its box: it misses it) */
        if (pP->n[2] < FX(0.0f))
            continue;
        dn = BrVec3Dot(dir, pP->n);
        if (dn == 0)
            continue;
        BrVec3Sub(tmpV, pP->v0, origin);
        t = FDIV(BrVec3Dot(tmpV, pP->n), dn);
        hitPt[0] = FMUL(dir[0], t) + origin[0];
        hitPt[1] = FMUL(dir[1], t) + origin[1];
        hitPt[2] = FMUL(dir[2], t) + origin[2];
        if (!BrTriContainsPointV(hitPt, pP->v0, pP->v1, pP->v2, pP->n))
            continue;
        dist = pEye[2] + FX(1.5f) - hitPt[2];
        if (dist >= FX(0.0f)) {
            hitCount++;
            if (dist < bestNearDist) {
                bestNearDist = dist;
                *pFaceOut = pP->tri;
                bestNearHitZ = hitPt[2];
                if (pP->n[2] < FX(0.0f)) {
                    bestNearNorm[0] = -pP->n[0];
                    bestNearNorm[1] = -pP->n[1];
                    bestNearNorm[2] = -pP->n[2];
                } else {
                    bestNearNorm[0] = pP->n[0];
                    bestNearNorm[1] = pP->n[1];
                    bestNearNorm[2] = pP->n[2];
                }
                if (nearCount < 32) {
                    pNearIds[nearCount] = pNearIds[0];
                    nearCount++;
                }
                pNearIds[0] = (uint16_t)(T->tris[pP->tri][3] + 1);
                if (dist < FX(5.0f)) {
                    ci = T->triTrigger[pP->tri];
                    if (T->triggers[ci] != 0) {
                        do {
                            if (farCount < 32) {
                                pFarIds[farCount] = pFarIds[0];
                                farCount++;
                            }
                            pFarIds[0] = T->triggers[ci];
                            ci++;
                        } while (T->triggers[ci] != 0);
                    }
                }
            } else if (nearCount < 32) {
                pNearIds[nearCount++] = (uint16_t)(T->tris[pP->tri][3] + 1);
                if (dist < FX(5.0f)) {
                    ci = T->triTrigger[pP->tri];
                    if (T->triggers[ci] != 0) {
                        do {
                            if (farCount < 32)
                                pFarIds[farCount++] = T->triggers[ci];
                            ci++;
                        } while (T->triggers[ci] != 0);
                    }
                }
            }
        }
        dist -= FX(1.5f);
        if (!(dist <= FX(0.0f) && dist < bestFarDist))
            continue;
        farFaceVal = (uint16_t)(T->tris[pP->tri][3] + 1);
        if (FX(-1.0f) < dist) {
            bestFarDist = dist;
            farFaceIdx = pP->tri;
            bestFarHitZ = hitPt[2];
            if (pP->n[2] < FX(0.0f)) {
                bestFarNorm[0] = -pP->n[0];
                bestFarNorm[1] = -pP->n[1];
                bestFarNorm[2] = -pP->n[2];
            } else {
                bestFarNorm[0] = pP->n[0];
                bestFarNorm[1] = pP->n[1];
                bestFarNorm[2] = pP->n[2];
            }
        }
    }
    if (hitCount != 0) {
        if (pPosOut != 0) {
            pPosOut[2] = bestNearHitZ;
            *pDistOut = bestNearDist - FX(1.5f);
        }
        if (pNormOut != 0) {
            pNormOut[0] = bestNearNorm[0];
            pNormOut[1] = bestNearNorm[1];
            pNormOut[2] = bestNearNorm[2];
        }
    } else {
        ci = T->triTrigger[farFaceIdx];
        while (T->triggers[ci] != 0) {
            if (farCount < 32) {
                pFarIds[farCount++] = T->triggers[ci];
            } else {
                pFarIds[31] = T->triggers[ci];
                break;
            }
            ci++;
        }
        if (nearCount < 32)
            pNearIds[nearCount] = farFaceVal;
        else
            pNearIds[31] = farFaceVal;
        if (pPosOut != 0)
            pPosOut[2] = bestFarHitZ;
        if (pNormOut != 0) {
            pNormOut[0] = bestFarNorm[0];
            pNormOut[1] = bestFarNorm[1];
            pNormOut[2] = bestFarNorm[2];
        }
    }
    *pGotHit = 1;
    *pFarCount = farCount;
    return hitCount;
}

/* ---- driving/carphys.c ---- */

/* BrCarPhysStep: one rigid-body frame: the springs, the tyres (not on the first frame),
   the velocities and spin from the forces, the axle grip, then the drag and dampers into
   a second step whose sign changes damp the first; the position physics; the suspension;
   the wheels' matrices */
void BrCarPhysStep(Car *car)
{
    int i;
    Body *b = &car->body;

    b->forces = FORCE(car, 0xB70);
    WHEEL(car, 0)->forces = FORCE(car, 0xCF0);
    WHEEL(car, 1)->forces = FORCE(car, 0xD30);
    WHEEL(car, 2)->forces = FORCE(car, 0xD10);
    WHEEL(car, 3)->forces = FORCE(car, 0xD50);
    for (i = 0; i < 4; i++) {
        WHEEL(car, i)->forces->f[0] = FX(0.0f);
        WHEEL(car, i)->forces->f[1] = FX(0.0f);
        WHEEL(car, i)->forces->f[2] = FX(0.0f);
    }
    BrTyreSprings(b);
    if (car->firstFrame == 0) {
        car->gripR = FX(0.0f);
        car->gripF = FX(0.0f);
        car->slipF = 0;
        BrWheelTyre(b, WHEEL(car, 0), &car->gripF, &car->slipF, FX(0.033333335f));
        BrWheelTyre(b, WHEEL(car, 1), &car->gripF, &car->slipF, FX(0.033333335f));
        BrWheelTyre(b, WHEEL(car, 2), &car->gripR, &car->slipR, FX(0.033333335f));
        BrWheelTyre(b, WHEEL(car, 3), &car->gripR, &car->slipR, FX(0.033333335f));
    }
    car->firstFrame = 0;
    for (i = 0; i < 3; i++)
        b->force[i] = b->torque[i] = FX(0.0f);
    BrRbForcesClear(b);
    BrRbIntegrate(&b->st, b, FX(0.033333335f));
    BrCarAxleGrip(b, FX(0.033333335f), &car->gripF, &car->gripR, &car->slipF, &car->slipR);
    BrRbQuatDerivative(&b->st);
    b->forces = FORCE(car, 0xBF0);
    WHEEL(car, 0)->forces = 0;
    WHEEL(car, 2)->forces = 0;
    WHEEL(car, 1)->forces = 0;
    WHEEL(car, 3)->forces = 0;
    BrTyreSkidCheck(b, FORCE(car, 0xCD0));
    BrTyreLoads(b);
    for (i = 0; i < 3; i++)
        b->force[i] = b->torque[i] = FX(0.0f);
    BrRbForcesClear(b);
    b->stB = b->st;
    BrRbIntegrate(&b->stB, b, FX(0.033333335f));
    for (i = 0; i < 3; i++) {
        fx a = b->st.omega[i], c = b->stB.omega[i];
        if ((a == FX(0.0f) ? 0 : (a > FX(0.0f) ? 1 : -1)) != (c == FX(0.0f) ? 0 : (c > FX(0.0f) ? 1 : -1)))
            b->st.omega[i] = FX(0.0f);
        else
            b->st.omega[i] = c;
        a = b->st.vel[i];
        c = b->stB.vel[i];
        if ((a == FX(0.0f) ? 0 : (a > FX(0.0f) ? 1 : -1)) != (c == FX(0.0f) ? 0 : (c > FX(0.0f) ? 1 : -1)))
            b->st.vel[i] = FX(0.0f);
        else
            b->st.vel[i] = c;
    }
    b->stA = b->st;
    BrRbQuatDerivative(&b->stA);
    BrRbQuatDerivative(&b->stA);
    BrCarPhysAdvance(b);
    b->st = b->stB;
    BrTyreDepthAll(b);
    for (i = 0; i < 4; i++)
        BrQuatToMat(WHEEL(car, i)->m, &WHEEL(car, i)->st);
}

#define SGN(x) ((x) == FX(0.0f) ? FX(0.0f) : ((x) > FX(0.0f) ? FX(1.0f) : FX(-1.0f)))

/* BrCarDriveInput: the pad to the steering angle (slewed toward a target inside a lock
   that narrows with speed), the gearbox, the engine force from the torque curve, the
   engine speed from the driven wheel, the brake */
void BrCarDriveInput(Car *car)
{
    static fx lockMax, slewRate, lockCentre, lockFall;
    fx rate, s, t, a, sp, lim, v[3], x = FX(0.0f), lo, tgt, old;
    uint32_t flags;
    int k, g, ctlt;
    Pad *pad = car->pad;

    s = pad->steer;
    if (pad->kind == 4) {
        a = s < FX(0.0f) ? -s : s;
        car->xdf0 = FDIVK(FMUL(FMUL(-FMUL(SGN(s), FSQRT(FMUL(FSQRT(a), FSQRT(FMUL(FSQRT(a), a))))), FX(10.0f)), FX(3.1415927f)), 180.0f);
    } else {
        if (FX(0.0f) < s) {
            s = s - FX(0.07f);
            if (s < FX(0.0f))
                s = 0;
        } else {
            s = s + FX(0.07f);
            if (FX(0.0f) < s)
                s = 0;
        }
        sp = car->speedMph;
        BrMat4RotateVec(v, car->body.m, car->body.st.vel);
        t = v[0];
        if (t < FX(10.0f))
            t = FX(10.0f);
        if (FX(70.0f) < t)
            t = FX(70.0f);
        if (sp < FX(140.0f))
            sp = FX(140.0f);
        if (FX(340.0f) < sp)
            sp = FX(340.0f);
        sp -= FX(140.0f);
        k = 1;
        switch (car->handling) {
        case 0:
            lockMax = FX(14.0f); slewRate = FX(0.01f); lockCentre = FX(6.0f); lockFall = FX(10.0f);
            k = 0;
            x = lockCentre;
            break;
        case 1:
            lockMax = FX(14.0f); slewRate = FX(0.01f); lockCentre = FX(6.0f); lockFall = FX(10.0f);
            k = 0;
            x = FX(3.0f);
            break;
        case 2:
            lockMax = FX(17.0f); slewRate = FX(0.1f); lockCentre = FX(16.0f); lockFall = FX(14.0f);
            k = 0;
            x = lockCentre;
            break;
        }
        rate = slewRate;
        lim = lockMax - FMUL(FDIVK(t, 90.0f), lockFall);
        if ((s < FX(0.0f) ? -s : s) < FX(0.001f)) {
            tgt = FX(0.0f);
            t = FDIVK(FMUL(lim, FX(3.1415927f)), 180.0f);
            lo = FDIVK(FMUL(-lim, FX(3.1415927f)), 180.0f);
        } else if (s < FX(-0.75f)) {
            t = FDIVK(FMUL(lim, FX(3.1415927f)), 180.0f);
            lo = FDIVK(FMUL(-lim, FX(3.1415927f)), 180.0f);
            tgt = t;
        } else if (FX(0.75f) < s) {
            lo = FDIVK(FMUL(-lim, FX(3.1415927f)), 180.0f);
            t = FDIVK(FMUL(lim, FX(3.1415927f)), 180.0f);
            tgt = lo;
        } else {
            x = x - FMUL(FMUL(ITOF(k), FDIVK(x, 2)), FDIVK(sp, 200.0f));
            tgt = FMUL(-s, FDIVK(FMUL(x, FX(3.1415927f)), 180.0f));
            t = FDIVK(FMUL(lim, FX(3.1415927f)), 180.0f);
            lo = FDIVK(FMUL(-lim, FX(3.1415927f)), 180.0f);
        }
        if (t < tgt)
            tgt = t;
        if (tgt < lo)
            tgt = lo;
        k = SGN(tgt) != SGN(car->xdf0) || (FX(0.0f) < car->xdf0 && tgt < car->xdf0) || (car->xdf0 < FX(0.0f) && car->xdf0 < tgt);
        if (car->xdf0 == FX(0.0f)) {
            k = 0;
            car->slew = 0;
        }
        g = tgt < car->xdf0;
        ctlt = car->xdf0 < tgt;
        if (g && car->slew < 0)
            k = 1;
        if (ctlt && car->slew > 0)
            k = 1;
        if (k) {
            car->slew = g ? -1 : 1;
            rate = FX(1.0f);
            if (SGN(car->xdf0) != SGN(tgt))
                tgt = FX(0.0f);
        } else {
            car->slew = 0;
        }
        if ((car->xdf0 < tgt ? -(car->xdf0 - tgt) : car->xdf0 - tgt) < rate)
            car->xdf0 = tgt;
        else if (tgt < car->xdf0)
            car->xdf0 = car->xdf0 - rate;
        else
            car->xdf0 = car->xdf0 + rate;
    }
    old = car->xdf4;
    if (car->xdf4 < FX(800.0f))
        car->xdf4 = FX(800.0f);
    if (car->automatic != 0) {
        g = car->gear;
        if (g >= 2 && car->xdf4 < FX(4000.0f))
            car->gear = g - 1;
        else if (g < car->gears && FX(6000.0f) < car->xdf4 && !(pad->flags & 0x20000))
            car->gear = g + 1;
        flags = pad->flags;
    } else if (car->gear > 0 && (pad->flags & 0x200000)) {
        pad->flags &= ~0x200000u;                    /* BrPadConsume */
        car->gear--;
        flags = pad->flags;
    } else {
        flags = pad->flags;
        if (car->gear < car->gears && (flags & 0x100000) && !(flags & 0x20000)) {
            pad->flags &= ~0x100000u;
            car->gear++;
            flags = pad->flags;
        }
    }
    lo = FX(0.0f);
    if (g_world.mode == 1 && car->xfac == 1) {
        Car *o = g_world.cars[car->slot ^ 1];
        a = FMUL(o->xfa8 < car->xfa8 ? -(o->xfa8 - car->xfa8) : o->xfa8 - car->xfa8, FX(0.5f));
        lo = a - FX(18.0f);
        if (a < FX(18.0f))
            lo = FX(0.0f);
        else if (FX(30.0f) < lo)
            lo = FX(30.0f);
    }
#ifdef FX_FLOAT
    t = FMUL(FMUL(FMUL(car->torque[0], car->xdf4), car->xdf4), car->xdf4) + FMUL(FMUL(car->torque[1], car->xdf4), car->xdf4) +
        FMUL(car->torque[2], car->xdf4) + car->torque[3] + lo;
#else
    {   /* the curve's cubic term is some 1e-9: in thousands of rpm, so 32.32 keeps it */
        fx r = FMUL(car->xdf4, FX(0.001));
        t = FMUL(FMUL(FMUL(car->torqueK[0], r) + car->torqueK[1], r) + car->torqueK[2], r) + car->torqueK[3] + lo;
    }
#endif
    if (car->automatic == 0)
        t = FTOF(FMUL(t, FXD(1.03)));
    if (!(flags & 0x10000))
        t = FX(0.0f);
    g = 0;
    car->xe38 = FMUL(t, FX(7.0f));
    if (car->linkFlags & 1) {
        car->gear = 0;
    } else {
        g = car->gear;
        if (g == 0) {
            car->gear = 1;
            g = 1;
        }
    }
    if (g != 0) {
        car->xdf4 = FMUL(FMUL(FMUL(-FDIVK(car->wb[1].spin, 6.2831855f), FX(60.0f)), car->xe24), car->ratio[g]);
    } else {
        if (pad->flags & 0x10000)
            car->xdf4 += FX(300.0f);
        else
            car->xdf4 -= FX(200.0f);
        car->xe38 = FX(0.0f);
    }
    if (car->xdf4 < FX(0.0f))
        car->xdf4 = -car->xdf4;
    if (car->xdf4 < FX(900.0f))
        car->xdf4 = FX(900.0f);
    if (FX(8000.0f) < car->xdf4)
        car->xdf4 = FX(8000.0f);
    lim = car->xdf4 - old;
    if (FX(400.0f) < (lim < FX(0.0f) ? -lim : lim))
        lim = FMUL(SGN(lim), FX(400.0f));
    car->xdf4 = old + lim;
    car->xe3c = 0;
    if (pad->flags & 0x40000)
        car->xe3c = FX(-140000.0f);
}

#undef SGN

/* BrCarBuildMatrices: the body's draw matrix (rolled by its visual roll) and each wheel's
   (spun about its axle, the front two steered at twice the angle) at its mount, lowered
   0.254 */
void BrCarBuildMatrices(Car *car)
{
    fx spin[4][4], m[4][4], steer[4][4];
    int k;

    guRotateF(m, car->body.angle, FX(1.0f), FX(0.0f), FX(0.0f));
    guMtxCatF(m, car->body.m, car->mtx0);
    for (k = 0; k < 4; k++)
        car->wb[k].m[3][2] += FX(0.254f);
    guRotateF(m, car->wb[0].angle, FX(0.0f), FX(1.0f), FX(0.0f));
    guMtxCatF(m, car->body.m, car->wheelMtx[2]);
    BrMat3MulVecRows(car->wheelMtx[2][3], car->body.m, car->wb[0].m[3]);
    guRotateF(m, car->wb[2].angle, FX(0.0f), FX(1.0f), FX(0.0f));
    guMtxCatF(m, car->body.m, car->wheelMtx[3]);
    BrMat3MulVecRows(car->wheelMtx[3][3], car->body.m, car->wb[2].m[3]);
    guRotateF(spin, car->wb[1].angle, FX(0.0f), FX(1.0f), FX(0.0f));
    guRotateF(steer, FMUL(car->wb[1].steer, FX(114.59155f)), FX(0.0f), FX(0.0f), FX(1.0f));
    guMtxCatF(spin, steer, m);
    guMtxCatF(m, car->body.m, car->wheelMtx[1]);
    BrMat3MulVecRows(car->wheelMtx[1][3], car->body.m, car->wb[1].m[3]);
    guRotateF(spin, car->wb[3].angle, FX(0.0f), FX(1.0f), FX(0.0f));
    guRotateF(steer, FMUL(car->wb[3].steer, FX(114.59155f)), FX(0.0f), FX(0.0f), FX(1.0f));
    guMtxCatF(spin, steer, m);
    guMtxCatF(m, car->body.m, car->wheelMtx[0]);
    BrMat3MulVecRows(car->wheelMtx[0][3], car->body.m, car->wb[3].m[3]);
    for (k = 0; k < 4; k++)
        car->wb[k].m[3][2] -= FX(0.254f);
}

/* ---- racing/cartick.c BrCarPhysTick: one physics frame for a car: the ground under it,
   the wheels' drive and brake from the engine and pad, the body stepped; the draw
   matrices, the speed for the gauges, the viewed car's camera ---- */
void BrCarPhysTick(Car *car)
{
    int i;

    car->groundHits = BrGroundRay(car->mtx0[3], car->mtx0[2], car->mtx0[3], car->nearIds, &car->gotHit, car->farIds,
                                  &car->farCount, &car->groundDist, &car->groundFace);
    WHEEL(car, 0)->steer = WHEEL(car, 1)->steer = FX(0.0f);
    BrCarDriveInput(car);
    WHEEL(car, 2)->steer = WHEEL(car, 3)->steer = car->xdf0;
    for (i = 0; i < 4; i++) {
        WHEEL(car, i)->drive = FX(0.0f);
        WHEEL(car, i)->brake = FX(0.0f);
    }
    if (car->xe38 > FX(0.0f)) {
        if ((car->pad->flags & 0x20000) && car->gear == 1)
            car->xe38 = -car->xe38;
        if (car->frontDrive != 0) {
            WHEEL(car, 0)->drive = FMUL(-car->xe38, FX(2));
            WHEEL(car, 1)->drive = FMUL(-car->xe38, FX(2));
            WHEEL(car, 2)->drive = FMUL(-car->xe38, FX(0));
            WHEEL(car, 3)->drive = FMUL(-car->xe38, FX(0));
        } else {
            for (i = 0; i < 4; i++)
                WHEEL(car, i)->drive = -car->xe38;
        }
    }
    if (car->xe3c < FX(0.0f)) {
        WHEEL(car, 0)->brake = -car->xe3c;
        WHEEL(car, 1)->brake = -car->xe3c;
        WHEEL(car, 0)->drive = FX(0.0f);
        WHEEL(car, 1)->drive = FX(0.0f);
        if ((WHEEL(car, 2)->drive < FX(0.0f) ? -WHEEL(car, 2)->drive : WHEEL(car, 2)->drive) < FX(0.0001f)) {
            WHEEL(car, 2)->brake = -car->xe3c;
            WHEEL(car, 3)->brake = -car->xe3c;
        }
    }
    BrCarPhysStep(car);
    BrCarBuildMatrices(car);
    if (car->wb[1].x1b4 != 0) {
        fx *v = car->body.st.vel;
        car->speedMph = FMUL(FSQRT(FMUL(v[2], v[2]) + (FMUL(v[0], v[0]) + FMUL(v[1], v[1]))), FX(2.24f));
    }
    if (sim_camera && car->slot == g_world.viewCar)
        sim_camera(car);
}

/* a plane's bounds (sim.h): the sphere about its corners' box centre, the box of its x and y,
   each 0.01 generous */
void sim_plane_bounds(Plane *p)
{
    const fx *v[3] = { p->v0, p->v1, p->v2 };
    fx lo, hi, r2 = 0, d[3], q;
    int i, k;
    for (k = 0; k < 3; k++) {
        lo = hi = v[0][k];
        for (i = 1; i < 3; i++) {
            if (v[i][k] < lo) lo = v[i][k];
            if (v[i][k] > hi) hi = v[i][k];
        }
        p->c[k] = lo + (hi - lo) / 2;
        if (k == 0) { p->xmin = lo - FX(0.01f); p->xmax = hi + FX(0.01f); }
        if (k == 1) { p->ymin = lo - FX(0.01f); p->ymax = hi + FX(0.01f); }
    }
    for (i = 0; i < 3; i++) {
        for (k = 0; k < 3; k++)
            d[k] = v[i][k] - p->c[k];
        q = FMUL(d[0], d[0]) + FMUL(d[1], d[1]) + FMUL(d[2], d[2]);
        if (q > r2)
            r2 = q;
    }
    p->r = FSQRT(r2) + FX(0.01f);
}

/* the pointers BrCarPhysInit sets up: the chassis' wheels, the force records' links */
void sim_car_link(Car *car)
{
    static const short links[][2] = {   /* record, next (game offsets; 0: none) */
        { 0xB70, 0xBB0 }, { 0xBB0, 0xB90 }, { 0xB90, 0xBD0 }, { 0xBD0, 0xC70 }, { 0xC70, 0 },
        { 0xCB0, 0 }, { 0xC90, 0 }, { 0xCF0, 0xCB0 }, { 0xD30, 0xCB0 }, { 0xD10, 0xC90 }, { 0xD50, 0xC90 },
        { 0xBF0, 0xC30 }, { 0xC30, 0xC10 }, { 0xC10, 0xC50 }, { 0xC50, 0xCD0 }, { 0xCD0, 0 },
    };
    int i;
    car->body.sub[0] = &car->wb[0];
    car->body.sub[1] = &car->wb[2];
    car->body.sub[2] = &car->wb[1];
    car->body.sub[3] = &car->wb[3];
    for (i = 0; i < 16; i++)
        FORCE(car, links[i][0])->next = links[i][1] ? FORCE(car, links[i][1]) : 0;
    car->body.forces = FORCE(car, 0xBF0);
    for (i = 0; i < 4; i++)
        car->wb[i].forces = 0;
}
