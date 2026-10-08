/* camera.c -- the car's cameras: racing/carcam.c (BrCamChaseStep and the chase
 * camera's placement, target, wall push and aim), transcribed.  cams[1] is the
 * chase camera, cams[0] the race view, cams[2] its copy, cams[3] a fixed-point
 * view of the car, cam4 the rear view. */
#include "vec.h"

int BrTriContainsPointV(const fx *pt, const fx *a, const fx *b, const fx *c, const fx *ref);
Cell BrCollGridCellAcquire(fx x, fx y);

/* BrCarCamWallPush: keep the chase camera out of walls (a ray from the target to the
   camera against the triangles of both ends' cells; on a hit, the wall it came through
   from its last place, 0.1 in front of it, no nearer the target than it was) */
static void BrCarCamWallPush(Car *car, fx *pos, const fx *prev)
{
    fx dir[3], hit[3], toV0[3], hitOut[3] = { 0, 0, 0 }, tBest, denom, t, len, dist;
    Cell cells[2];
    const Plane *pBest;
    int c, i, nCells;

    g_world.camPushed = 0;
    cells[0] = BrCollGridCellAcquire(car->camTarget[0], car->camTarget[1]);
    cells[1] = BrCollGridCellAcquire(pos[0], pos[1]);
    nCells = cells[0].key == cells[1].key ? 1 : 2;
    BrVec3Sub(dir, pos, car->camTarget);
    pBest = 0;
    len = BrVec3Length(dir);
    if (len != 0)
        tBest = FDIV(len + FX(0.1f), len);
    else
        tBest = FX(1.0f);
    for (c = 0; c < nCells; c++) {
        for (i = 0; i < cells[c].n; i++) {
            const Plane *pP = &g_track->planes[cells[c].tris[i]];
            denom = BrVec3Dot(dir, pP->n);
            if (denom < FX(0.0f)) {
                BrVec3Sub(toV0, pP->v0, car->camTarget);
                t = FDIV(BrVec3Dot(toV0, pP->n), denom);
                if (t > FX(0.0f) && t < tBest) {
                    BrVec3MulAdd(hit, car->camTarget, dir, t);
                    if (BrTriContainsPointV(hit, pP->v0, pP->v1, pP->v2, pP->n)) {
                        tBest = t;
                        pBest = pP;
                        hitOut[0] = hit[0];
                        hitOut[1] = hit[1];
                        hitOut[2] = hit[2];
                    }
                }
            }
        }
    }
    if (pBest == 0)
        return;
    BrVec3Sub(dir, pos, prev);
    pBest = 0;
    tBest = FX(1.0f);
    for (c = 0; c < nCells; c++) {
        for (i = 0; i < cells[c].n; i++) {
            const Plane *pP = &g_track->planes[cells[c].tris[i]];
            denom = BrVec3Dot(dir, pP->n);
            if (denom < FX(0.0f)) {
                BrVec3Sub(toV0, pP->v0, prev);
                t = FDIV(BrVec3Dot(toV0, pP->n), denom);
                if (t > FX(0.0f) && t < tBest) {
                    BrVec3MulAdd(hit, prev, dir, t);
                    if (BrTriContainsPointV(hit, pP->v0, pP->v1, pP->v2, pP->n)) {
                        tBest = t;
                        pBest = pP;
                        hitOut[0] = hit[0];
                        hitOut[1] = hit[1];
                        hitOut[2] = hit[2];
                    }
                }
            }
        }
    }
    if (pBest == 0)
        return;
    dist = BrVec3Dist(pos, car->camTarget);
    BrVec3MulAdd(pos, hitOut, pBest->n, FX(0.1f));
    {
        fx v[3], l;
        BrVec3Sub(v, pos, car->camTarget);
        l = BrVec3Length(v);
        if (dist < l && l != FX(0.0f)) {
            BrVec3ScaleBy(v, FDIV(dist, l));
            BrVec3Add(pos, car->camTarget, v);
        }
    }
    g_world.camPushed = 1;
}

/* BrCarCamPlaceBehind: the chase camera 11 behind the car (blended by t toward its old
   offset scaled to 19.8), lifted 2.4 */
void BrCarCamPlaceBehind(Car *car, fx (*mtx)[4], fx t)
{
    fx d[3], len, *pos = mtx[3];

    if (car->fly != 0) {
        BrVec3MulAdd(pos, car->mtx0[3], car->mtx0[2], FX(2.4f));
        BrVec3MulAdd(pos, pos, car->mtx0[0], g_world.views == 1 ? FX(-11.0f) : FX(-19.8f));
        return;
    }
    mtx[3][2] -= FX(2.4f);
    BrVec3Sub(d, pos, car->mtx0[3]);
    len = BrVec3Length(d);
    if (len != FX(0.0f)) {
        if (g_world.views == 1)
            BrVec3ScaleBy(d, FDIV(FX(11.0f), len));
        else
            BrVec3ScaleBy(d, FDIV(FX(19.8f), len));
    }
    if (g_world.replay != 0) {
        BrVec3Scale(pos, car->mtx0[0], FX(11.0f));
    } else if (g_world.mode == 5) {
        BrVec3Scale(pos, car->mtx0[1], FX(-11.0f));
        BrVec3MulAddTo(pos, car->mtx0[0], FX(-13.0f));
    } else {
        BrVec3Scale(pos, car->mtx0[0], FX(-11.0f));
    }
    len = BrVec3Length(pos);
    if (len != FX(0.0f))
        BrVec3ScaleBy(pos, FDIV(FX(11.0f), len));
    BrVec3Lerp(pos, pos, d, t);
    BrVec3AddTo(pos, car->mtx0[3]);
    mtx[3][2] += FX(2.4f);
}

/* BrCarCamTargetStep: the target above the car, led ahead while it turns slowly */
static void BrCarCamTargetStep(Car *car)
{
    fx spin, want;

    if (car->fly != 0) {
        BrVec3MulAdd(car->camTarget, car->mtx0[3], car->mtx0[2], FX(1.1f));
        return;
    }
    car->camTarget[0] = car->mtx0[3][0];
    car->camTarget[1] = car->mtx0[3][1];
    car->camTarget[2] = car->mtx0[3][2] + FX(0.66f);
    spin = BrVec3Length(car->body.st.omega);
    if (g_world.mode != 5) {
        want = FX(0.0f);
        if (spin < FX(3.5f))
            want = FX(2.0f);
        else if (spin < FX(7.0f))
            want = FX(4.0f) - FMUL(spin, FX(0.5714286f));
        if (car->x1f90 < want) {
            car->x1f90 += FX(0.1f);
            if (car->x1f90 > want)
                car->x1f90 = want;
        } else if (car->x1f90 > want) {
            car->x1f90 -= FX(0.1f);
            if (car->x1f90 < want)
                car->x1f90 = want;
        }
        BrVec3MulAddTo(car->camTarget, car->mtx0[0], car->x1f90);
    }
}

/* BrCarCamLookAt: aim a camera at the car's camera target */
static void BrCarCamLookAt(Car *car, fx (*mtx)[4])
{
    fx v[3], len;

    BrVec3Sub(v, car->camTarget, mtx[3]);
    len = BrVec3Length(v);
    if (len != FX(0.0f)) {
        BrVec3Div(mtx[0], v, len);
    } else if (BrVec3Length(mtx[0]) == FX(0.0f)) {
        mtx[0][0] = car->mtx0[0][0];
        mtx[0][1] = car->mtx0[0][1];
        mtx[0][2] = car->mtx0[0][2];
    }
    if (car->fly != 0) {
        BrVec3Cross(mtx[1], car->mtx0[2], mtx[0]);
    } else {
        v[0] = FX(0.0f);
        v[1] = FX(0.0f);
        v[2] = FX(1.0f);
        BrVec3Cross(mtx[1], v, mtx[0]);
    }
    BrVec3Cross(mtx[2], mtx[0], mtx[1]);
}

/* BrCamChaseStep: the car's cameras for a frame */
void BrCamChaseStep(Car *car)
{
    fx prev[3], len2, spin, k, speed, dist;
    fx (*cam)[4];
    fx *pRow, *pUp, *pLook;
    int i;

    prev[0] = car->cams[1].mtx[3][0];
    prev[1] = car->cams[1].mtx[3][1];
    prev[2] = car->cams[1].mtx[3][2];
    speed = BrVec3Length(car->body.st.omega);
    len2 = BrVec3Length(car->body.st.vel);
    spin = FMUL(len2, FX(0.3f));
    if (speed > FX(2.5f))
        speed = speed - FX(2.5f);
    else
        speed = FX(0.0f);
    if (speed > FX(31.415928f))
        speed = FX(31.415928f);
    if (spin > FX(31.415928f))
        spin = FX(31.415928f);
    if (car->camSpeed < speed)
        car->camSpeed = speed;
    else
        car->camSpeed = FMUL(car->camSpeed, FX(0.95f)) + FMUL(speed, FX(0.05f));
    car->camSpin = FMUL(car->camSpin, FX(0.95f)) + FMUL(spin, FX(0.05f));
    car->camSpin = FMUL(car->camSpin, FX(1.0f) - FMUL(car->x1fac, FX(18.181818f)));
    car->cams[1].mtx[3][0] = car->camPosA[0];
    car->cams[1].mtx[3][1] = car->camPosA[1];
    car->cams[1].mtx[3][2] = car->camPosA[2];
    k = g_world.replay != 0 ? FX(0.5f) : FX(0.15f);
    cam = car->cams[1].mtx;
    if (car->x1fac + (FMUL(car->camSpeed, FX(0.009549296f)) - FMUL(car->camSpin, FDIV(k, FX(31.415928f)))) > FX(0.07f))
        BrCarCamPlaceBehind(car, cam, FX(0));
    else
        BrCarCamPlaceBehind(car, cam, FX(0.07f) - car->x1fac - FMUL(car->camSpeed, FX(0.009549296f)) + FMUL(car->camSpin, FDIV(k, FX(31.415928f))));
    BrCarCamTargetStep(car);
    car->camPosA[0] = car->cams[1].mtx[3][0];
    car->camPosA[1] = car->cams[1].mtx[3][1];
    car->camPosA[2] = car->cams[1].mtx[3][2];
    if (car->fly == 0) {
        BrCarCamWallPush(car, cam[3], prev);
        if (g_world.camPushed != 0) {
            if (g_world.replay != 0) {
                g_world.camHold2 = 30;
                if (car->cam == 1) {
                    car->cam = 0;
                    car->camMode = 2;
                    g_world.camHold = 60;
                }
            }
            if (car->x1fac < FX(0.02f)) {
                car->x1fac = FX(0.02f);
            } else {
                car->x1fac = car->x1fac + FX(0.01f);
                if (car->x1fac > FX(0.055f))
                    car->x1fac = FX(0.055f);
            }
            car->x1fac = FX(0.05f);
        } else {
            if (g_world.camHold2 != 0)
                g_world.camHold2--;
            car->x1fac = car->x1fac - FX(0.005f);
            if (car->x1fac < FX(0.0f))
                car->x1fac = FX(0.0f);
        }
    }
    BrCarCamLookAt(car, cam);
    if (car->fly != 0) {
        for (i = 0; i < 16; i++)
            (&car->cams[0].mtx[0][0])[i] = (&car->mtx0[0][0])[i];
        car->cams[0].fov = car->wheelMtx[0][0][0];
    } else {
        fx *v = car->camView;
        for (i = 0; i < 3; i++)
            car->cams[0].mtx[3][i] = FMUL(v[2], car->mtx0[2][i]) + (car->mtx0[3][i] + FMUL(car->mtx0[0][i], v[0]));
        BrVec3MulAddTo(car->cams[0].mtx[0], car->mtx0[0], FX(-20.0f));
        BrVec3Negate(car->cams[0].mtx[0], car->cams[0].mtx[0]);
        BrVec3Normalise(car->cams[0].mtx[0]);
        pRow = car->cams[0].mtx[1];
        pRow[0] = car->mtx0[1][0];
        pRow[1] = car->mtx0[1][1];
        pRow[2] = car->mtx0[1][2];
        BrVec3Cross(car->cams[0].mtx[2], car->cams[0].mtx[0], pRow);
    }
    car->cams[2] = car->cams[0];
    BrVec3MulAddTo(car->cams[2].mtx[3], car->mtx0[0], FX(0.5f));
    pUp = car->cams[3].mtx[2];
    pLook = car->cams[3].mtx[0];
    pUp[0] = FX(0.0f);
    pUp[1] = FX(0.0f);
    pUp[2] = FX(1.0f);
    BrVec3Add(pLook, car->mtx0[3], car->mtx0[2]);
    BrVec3SubFrom(pLook, car->cams[3].mtx[3]);
    dist = BrVec3Length(pLook);
    BrVec3DivBy(pLook, dist);
    BrVec3Cross(car->cams[3].mtx[1], pUp, pLook);
    BrVec3Cross(pUp, pLook, car->cams[3].mtx[1]);
    if (dist <= FX(1.0f))
        dist = FX(0.0f);
    else if (dist >= FX(101.0f))
        dist = FX(100.0f);
    else
        dist = dist - FX(1.0f);
    car->cams[3].fov = FMUL(FMUL(FCOS(FMUL(dist - FX(1.0f), FX(0.041887902f))), FX(0.1f)) + FX(0.7f) + FMUL(FX(51.0f) - dist, FX(0.004f)), g_world.lens);
    car->cams[2].fov = g_world.lens;
    car->cams[1].fov = g_world.lens;
    car->cams[0].fov = g_world.lens;
    car->cam4.fov = g_world.lens;
    for (i = 0; i < 3; i++) {
        fx *v = car->camView;
        car->cam4.mtx[3][i] = FMUL(v[2], car->mtx0[2][i]) + (car->mtx0[3][i] + FMUL(FMUL(car->mtx0[0][i], v[1]), FX(2.0f)));
        car->cam4.mtx[0][i] = -car->mtx0[0][i];
        car->cam4.mtx[1][i] = -car->mtx0[1][i];
        car->cam4.mtx[2][i] = car->mtx0[2][i];
    }
}
