/* rigid.c -- the car's rigid bodies, their forces and the tyres:
 * driving/rbforce.c, driving/rbquat.c (BrRbIntegrate, BrRbBodyInit,
 * BrRbSetParams) and driving/tyre.c, transcribed.  Where the game's source
 * mixes a double constant in, the expression is kept as it was (FXD), so the
 * float build rounds where the cartridge rounds. */
#include "sim.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define ABSF(x) ((x) < FX(0.0) ? -(x) : (x))
#define SIGNF(x) ((x) == 0 ? FXD(0.0) : ((x) > FX(0.0) ? FXD(1.0) : FXD(-1.0)))
#define SIGNZ(x) ((x) == FX(0.0) ? FXD(0.0) : ((x) > 0 ? FXD(1.0) : FXD(-1.0)))

/* BrRbIntegrate: the state's velocity and spin by the body's acceleration (force and
   torque have become accelerations by now) */
void BrRbIntegrate(RbState *s, Body *r, fx dt)
{
    fx dv[3], dw[3];
    dv[0] = FMUL(r->force[0], dt);
    dv[1] = FMUL(r->force[1], dt);
    dv[2] = FMUL(r->force[2], dt);
    dw[0] = FMUL(r->torque[0], dt);
    dw[1] = FMUL(r->torque[1], dt);
    dw[2] = FMUL(r->torque[2], dt);
    s->vel[0] = dv[0] + s->vel[0];
    s->vel[1] = dv[1] + s->vel[1];
    s->vel[2] = dv[2] + s->vel[2];
    s->omega[0] = dw[0] + s->omega[0];
    s->omega[1] = dw[1] + s->omega[1];
    s->omega[2] = dw[2] + s->omega[2];
}

/* BrRbVelAtFlatPoint: the body's velocity at another body's mount taken flat, in body axes */
void BrRbVelAtFlatPoint(fx out[3], Body *b, Body *at)
{
    fx p[3], c[3], r[3];
    p[0] = at->st.pos[0];
    p[1] = at->st.pos[1];
    p[2] = FX(0.0);
    BrMat4RotateVecT(r, b->m, p);
    out[0] = b->st.vel[0];
    out[1] = b->st.vel[1];
    out[2] = b->st.vel[2];
    c[0] = FMUL(b->st.omega[1], r[2]) - FMUL(b->st.omega[2], r[1]);
    c[1] = FMUL(b->st.omega[2], r[0]) - FMUL(b->st.omega[0], r[2]);
    c[2] = FMUL(b->st.omega[0], r[1]) - FMUL(b->st.omega[1], r[0]);
    p[0] = out[0] + c[0];
    p[1] = out[1] + c[1];
    p[2] = out[2] + c[2];
    BrMat4RotateVec(out, b->m, p);
}

/* BrRbVelAtBodyPoint: the body's velocity at another body's mount (world axes) */
SIM_OWN static void BrRbVelAtBodyPoint(fx out[3], Body *b, Body *at)
{
    fx p[3], c[3], r[3];
    p[0] = at->st.pos[0];
    p[1] = at->st.pos[1];
    p[2] = at->st.pos[2];
    BrMat4RotateVecT(r, b->m, p);
    out[0] = b->st.vel[0];
    out[1] = b->st.vel[1];
    out[2] = b->st.vel[2];
    c[0] = FMUL(b->st.omega[1], r[2]) - FMUL(b->st.omega[2], r[1]);
    c[1] = FMUL(b->st.omega[2], r[0]) - FMUL(b->st.omega[0], r[2]);
    c[2] = FMUL(b->st.omega[0], r[1]) - FMUL(b->st.omega[1], r[0]);
    out[0] = out[0] + c[0];
    out[1] = out[1] + c[1];
    out[2] = out[2] + c[2];
}

/* BrRbVelAtPoint: the body's velocity at a point in its own axes (world axes) */
void BrRbVelAtPoint(fx out[3], Body *b, const fx *pt)
{
    fx p[3], c[3], r[3];
    p[0] = pt[0];
    p[1] = pt[1];
    p[2] = pt[2];
    BrMat4RotateVecT(r, b->m, p);
    out[0] = b->st.vel[0];
    out[1] = b->st.vel[1];
    out[2] = b->st.vel[2];
    c[0] = FMUL(b->st.omega[1], r[2]) - FMUL(b->st.omega[2], r[1]);
    c[1] = FMUL(b->st.omega[2], r[0]) - FMUL(b->st.omega[0], r[2]);
    c[2] = FMUL(b->st.omega[0], r[1]) - FMUL(b->st.omega[1], r[0]);
    out[0] = out[0] + c[0];
    out[1] = out[1] + c[1];
    out[2] = out[2] + c[2];
}

/* BrCarAxleGrip: the axle constraint (see driving/rbforce.c) */
void BrCarAxleGrip(Body *b, fx dt, fx *gripF, fx *gripR, uint8_t *slipFp, uint8_t *slipRp)
{
    fx tmpB[3], tmpA[3], pt[3], vB[3], vA[3], w[3], sv[3];
    fx grip, side, lo, slipF, slipR, g, hold, s, v, m4, sp, t;
    int idx, ran;
    uint8_t sA, sB, sC, sD;
    short row;

    ran = 0;
    side = FX(0.0);
    if (b->sub[0]->hit == 0)
        b->sub[0]->x1b4 = 0;
    if (b->sub[1]->hit == 0)
        b->sub[1]->x1b4 = 0;
    if (b->sub[2]->hit == 0)
        b->sub[2]->x1b4 = 0;
    if (b->sub[3]->hit == 0)
        b->sub[3]->x1b4 = 0;
    slipF = FTOF(FMUL(FMUL(-ABS(b->sub[2]->brake), SIGNF(b->sub[2]->spin)), FXD(2.0)));
    slipF = FDIV(slipF, b->sub[2]->inertia);
    slipR = FTOF(FMUL(FMUL(-ABS(b->sub[0]->brake), SIGNF(b->sub[0]->spin)), FXD(2.0)));
    slipR = FDIV(slipR, b->sub[0]->inertia);
    m4 = FDIVK(b->mass, 4);
    slipF = FDIV(slipF, m4);
    slipR = FDIV(slipR, m4);
    slipF = FMUL(slipF, FMUL(dt, dt));
    slipR = FMUL(slipR, FMUL(dt, dt));
    if (ABS(slipF) > SIM_DTK)               /* (a tick's braking: k ticks' worth in a longer one) */
        slipF = FTOF(FMUL(FMUL(SIGNF(slipF), FX(1.5f)), SIM_DTK));
    if (ABS(slipR) > SIM_DTK)
        slipR = FTOF(FMUL(FMUL((slipR == 0 ? FXD(0.0) : (slipR > 0 ? FXD(1.0) : FXD(-1.0))), FX(1.5f)), SIM_DTK));
    pt[1] = pt[2] = FX(0.0);
    pt[0] = b->sub[0]->st.pos[0];
    BrRbVelAtPoint(tmpA, b, pt);
    BrMat4RotateVec(vA, b->m, tmpA);
    sA = b->sub[0]->surface;
    sB = b->sub[1]->surface;
    sC = b->sub[2]->surface;
    sD = b->sub[3]->surface;
    row = (short)(g_world.weather - 1);
    if (row >= 3 || row < 0)
        row = 0;
    b->slide = 0;
    row <<= 3;
    if ((b->sub[0]->x1b4 != 0 || b->sub[1]->x1b4 != 0) && (b->sub[2]->x1b4 != 0 || b->sub[3]->x1b4 != 0)) {
        idx = ((sA + sB + 1) >> 1) + row;
        s = FDIV(FMUL(ABS(vA[1]), b->mass), dt) + ABS(*gripF);
        ran = 1;
        hold = FX(8000.0f);
        if (ABS(slipF) > FXD(0.0001))
            s += FMUL(FX(10000.0f), ITOF(ABS(slipR) > FXD(0.0001)));
        else
            s += FMUL(FX(100000.0f), ITOF(ABS(slipR) > FXD(0.0001)));
        if (*slipFp != 0)
            hold = FX(5600.0f);
        *slipFp = 0;
        grip = g_grip[idx];
        grip -= FMUL(FXD(0.002), ITOF(b->tyres - 1));
        lo = g_grip[48 + idx];
        v = g_grip[24 + idx];
        g = s;
        if (v < s)
            g = v;
        if (g < lo)
            g = lo;
        g = FMUL(FMUL(FDIV(lo, g), FX(20.0f)), grip);
        if (b->sub[2]->steer == 0)
            g = FTOF(FMUL(FXD(1.5), g));
        if (ABS(g) > FX(1.0f))
            g = FX(1);
        if (s < hold) {
            side = vA[1] = FX(0.0);
        } else {
            sp = FSQRT(FMUL(b->st.vel[0], b->st.vel[0]) + FMUL(b->st.vel[1], b->st.vel[1]) + FMUL(b->st.vel[2], b->st.vel[2]));
            if (sp < FX(27.0f)) {
                t = FDIVK(FMUL(FX(27.0f) - sp, FX(0.1f)), 27.0f);
                if (g < t)
                    g = t;
            }
            vA[1] = vA[1] - FMUL(vA[1], g);
            side = FMUL(vA[1], g);
            *slipFp = 1;
        }
        if (ABS(vA[0]) > FX(1.0f)) {
            if (ABS(FDIV(vA[1], vA[0])) > FX(0.25f))
                b->slide = 0x80;
        } else if (ABS(vA[1]) > FX(1.0f)) {
            b->slide = 0x80;
        } else {
            b->slide = 0;
        }
        t = vA[0];
        vA[0] = vA[0] - slipR;
        if (ABS(vA[0]) > FX(1e-05f)) {
            if (SIGNF(vA[0]) != SIGNF(t))
                vA[0] = FX(0.f);
        }
    } else {
        *slipFp = 0;
    }
    pt[0] = b->sub[2]->st.pos[0];
    BrRbVelAtPoint(tmpB, b, pt);
    BrMat4RotateVec(vB, b->m, tmpB);
    if ((b->sub[2]->x1b4 != 0 || b->sub[3]->x1b4 != 0) && (b->sub[0]->x1b4 != 0 || b->sub[1]->x1b4 != 0)) {
        fx d, save[3], lat[3];

        ran = 1;
        pt[0] = FCOS(b->sub[2]->steer);
        pt[1] = FSIN(b->sub[2]->steer);
        pt[2] = FX(0.f);
        d = FMUL(pt[0], vB[0]) + FMUL(pt[1], vB[1]) + FMUL(pt[2], vB[2]);
        save[0] = vB[0];
        save[1] = vB[1];
        save[2] = vB[2];
        vB[0] = FMUL(pt[0], d);
        vB[1] = FMUL(pt[1], d);
        vB[2] = FMUL(pt[2], d);
        lat[0] = save[0] - vB[0];
        lat[1] = save[1] - vB[1];
        lat[2] = save[2] - vB[2];
        sp = FSQRT(FMUL(lat[0], lat[0]) + FMUL(lat[1], lat[1]) + FMUL(lat[2], lat[2]));
        v = ABS(*gripR) + FDIV(FMUL(sp, b->mass), dt);
        g = FX(8000.0f);
        v += FMUL(FX(10000.0f), ITOF(ABS(slipF) > FXD(0.0001)));
        if (*slipRp != 0)
            g = FTOF(FMUL(g, FXD(0.7)));
        *slipRp = 0;
        if (g < v) {
            idx = ((sC + sD + 1) >> 1) + row;
            grip = g_grip[idx];
            grip -= FMUL(FX(0.002f), ITOF(b->tyres - 1));
            lo = g_grip[48 + idx];
            s = g_grip[24 + idx];
            g = v;
            if (s < v)
                g = s;
            if (g < lo)
                g = lo;
            g = FMUL(FMUL(FDIV(lo, g), FX(20.0f)), grip);
            if (b->sub[2]->steer == FX(0.0f))
                g = FTOF(FMUL(g, FXD(1.5)));
            if (ABS(g) > FX(1.0f))
                g = FX(1.0f);
            sp = FSQRT(FMUL(b->st.vel[0], b->st.vel[0]) + FMUL(b->st.vel[1], b->st.vel[1]) + FMUL(b->st.vel[2], b->st.vel[2]));
            if (sp < FX(27.0f)) {
                t = FDIVK(FMUL(FX(27.0f) - sp, FX(0.1f)), 27.0f);
                if (g < t)
                    g = t;
            }
            lat[0] = FMUL(lat[0], g);
            lat[1] = FMUL(lat[1], g);
            lat[2] = FMUL(lat[2], g);
            vB[0] = save[0] - lat[0];
            vB[1] = save[1] - lat[1];
            vB[2] = save[2] - lat[2];
            *slipRp = 1;
        }
    }
    if (ran != 0) {
        sv[0] = FDIVK(vB[0] + vA[0], 2);
        sv[2] = FDIV(vA[1] - vB[1], b->sub[0]->st.pos[0] - b->sub[2]->st.pos[0]);
        sv[1] = vA[1] - FMUL(b->sub[0]->st.pos[0], sv[2]);
        BrMat4RotateVec(w, b->m, b->st.omega);
        w[2] = sv[2];
        BrMat4RotateVecT(b->st.omega, b->m, w);
        BrMat4RotateVec(w, b->m, b->st.vel);
        w[0] = sv[0];
        w[1] = sv[1];
        BrMat4RotateVecT(b->st.vel, b->m, w);
    }
    if (ABS(side) > FX(0.5f))
        side = FTOF(FMUL((side == FX(0.0f) ? FXD(0.) : (side > FX(0.0f) ? FXD(1.) : FXD(-1.))), FXD(0.5)));
    side = FDIVK(side, 0.5f);
    side = FMUL(side, FX(-4.0f));
    if (ABS(b->angle - side) < FX(0.26666668f))
        b->angle = side;
    else if (b->angle < side)
        b->angle = b->angle + FX(0.26666668f);
    else
        b->angle = b->angle - FX(0.26666668f);
}

/* BrWheelTyre: one wheel's drive force along its rolling direction, and its spin */
void BrWheelTyre(Body *b, Body *w, fx *pA, uint8_t *pB, fx dt)
{
    static const fx axis[3] = { FX(0.0f), FX(1.0f), FX(0.0f) };
    fx a[3], c[3], d[3], e[3], fwd[3], side[3], v[3];
    fx dot, sn, cs, load, q, tq, h;

    if (w->hit == 0)
        return;
    if (w->hitN[2] < FXD(0.7))
        return;
    BrMat4RotateVecT(a, b->m, axis);
    c[0] = FMUL(a[1], w->hitN[2]) - FMUL(a[2], w->hitN[1]);
    c[1] = FMUL(a[2], w->hitN[0]) - FMUL(a[0], w->hitN[2]);
    c[2] = FMUL(a[0], w->hitN[1]) - FMUL(a[1], w->hitN[0]);
    d[0] = FMUL(w->hitN[1], c[2]) - FMUL(w->hitN[2], c[1]);
    d[1] = FMUL(w->hitN[2], c[0]) - FMUL(w->hitN[0], c[2]);
    d[2] = FMUL(w->hitN[0], c[1]) - FMUL(w->hitN[1], c[0]);
    cs = FCOS(w->steer);
    sn = FSIN(w->steer);
    fwd[0] = FMUL(c[0], cs);
    fwd[1] = FMUL(c[1], cs);
    fwd[2] = FMUL(c[2], cs);
    side[0] = FMUL(c[0], -sn);
    side[1] = FMUL(c[1], -sn);
    side[2] = FMUL(c[2], -sn);
    c[0] = FMUL(d[0], sn);
    c[1] = FMUL(d[1], sn);
    c[2] = FMUL(d[2], sn);
    fwd[0] = fwd[0] + c[0];
    fwd[1] = fwd[1] + c[1];
    fwd[2] = fwd[2] + c[2];
    c[0] = FMUL(d[0], cs);
    c[1] = FMUL(d[1], cs);
    c[2] = FMUL(d[2], cs);
    side[0] = side[0] + c[0];
    side[1] = side[1] + c[1];
    side[2] = side[2] + c[2];
    if (b->sub[0]->x1b4 != 0 && b->sub[2]->x1b4 != 0 && b->sub[1]->x1b4 != 0 && b->sub[3]->x1b4 != 0) {
        BrRbVelAtBodyPoint(v, b, w);
        dot = FMUL(v[0], fwd[0]) + FMUL(v[1], fwd[1]) + FMUL(v[2], fwd[2]);
        h = FTOF(w->st.pos[2] - FXD(-0.97));
        h = FMUL(h, FX(0));
        a[1] = FX(0.0f);
        a[0] = FX(0.0f);
        a[2] = FMUL(b->mass + FMUL(FX(4.0f), w->mass), FX(2.9430003f)) + h;
        load = FMUL(FMUL(a[0], w->hitN[0]) + FMUL(a[1], w->hitN[1]) + FMUL(a[2], w->hitN[2]), FX(3.5f));
        tq = w->drive;
        q = FDIV(tq, w->inertia);
        *pA = *pA + FDIVK(q, 2);
        if (*pB != 0)
            q = FTOF(FMUL(q, FXD(0.9)));
        if (ABSF(q) > ABSF(load)) {
            load = ABSF(FDIV(load, q));
            q = FTOF(FMUL(q, FMUL(load, FXD(0.1))));
        }
        e[0] = FMUL(-q, fwd[0]);
        e[1] = FMUL(-q, fwd[1]);
        e[2] = FMUL(-q, fwd[2]);
        BrMat4RotateVec(a, b->m, e);
        w->forces->f[0] = w->forces->f[0] + a[0];
        w->forces->f[1] = w->forces->f[1] + a[1];
        w->forces->f[2] = w->forces->f[2] + a[2];
        h = tq - FMUL(w->inertia, q);
        w->spin = w->spin + FMUL(h, dt);
        w->spin = FTOF(w->spin - FMUL(FMUL(w->spin, w->inertia) + dot, FXD(0.4)));
        if (ABSF(w->spin) > FX(300.0f))
            w->spin = FTOF(FMUL(SIGNZ(w->spin), FXD(300.0)));
    } else {
        tq = w->drive;
        w->spin = w->spin + FMUL(tq, dt);
        if (ABSF(w->spin) > FX(300.0f))
            w->spin = FTOF(FMUL(SIGNZ(w->spin), FXD(300.0)));
    }
    w->angle = w->angle - FMUL(FMUL(w->spin, FX(57.295776f)), dt);
    while (w->angle > FXD(360.0)) {
        w->angle = FTOF(w->angle - FXD(360.0));
        FX_STEP(w->angle);
    }
    if (w->angle < FXD(0.)) {
        do {
            w->angle = w->angle + FX(360.0f);
            FX_STEP(w->angle);
        } while (w->angle < FXD(0.));
    }
}

/* BrRbSolveAccel: the chassis' force and torque to accelerations (the wheels' forces
   on the ground axes added) */
SIM_OWN static void BrRbSolveAccel(Body *b)
{
    fx t[3], u[3], w[3];
    BrMat4RotateVec(t, b->m, b->force);
    b->force[0] = t[0];
    b->force[1] = t[1];
    b->force[2] = t[2];
    t[0] = FDIV(b->force[0] + b->sub[0]->force[0] + b->sub[1]->force[0] + b->sub[2]->force[0] + b->sub[3]->force[0], b->mass);
    t[1] = FDIV(b->force[1] + b->sub[0]->force[1] + b->sub[1]->force[1] + b->sub[2]->force[1] + b->sub[3]->force[1], b->mass);
    t[2] = FDIV(b->force[2], b->mass);
    BrMat4RotateVecT(b->force, b->m, t);
    BrMat4RotateVec(u, b->m, b->torque);
    BrMat3MulVec(w, b->Iinv, u);
    BrMat4RotateVecT(b->torque, b->m, w);
}

/* BrRbAddForces: the body's applied forces into its sums */
static void BrRbAddForces(Body *b)
{
    Force *a;
    fx f[3], r[3], t[3];
    for (a = b->forces; a != 0; a = a->next) {
        switch (a->frame) {
        case 0:
            f[0] = a->f[0];
            f[1] = a->f[1];
            f[2] = a->f[2];
            break;
        case 1:
            BrMat4RotateVecT(f, b->m, a->f);
            break;
        }
        b->force[0] = b->force[0] + f[0];
        b->force[1] = b->force[1] + f[1];
        b->force[2] = b->force[2] + f[2];
        if (b->kind != 2) {
            BrMat4RotateVecT(r, b->m, a->at);
            t[0] = FMUL(r[1], f[2]) - FMUL(r[2], f[1]);
            t[1] = FMUL(r[2], f[0]) - FMUL(r[0], f[2]);
            t[2] = FMUL(r[0], f[1]) - FMUL(r[1], f[0]);
            b->torque[0] = b->torque[0] + t[0];
            b->torque[1] = b->torque[1] + t[1];
            b->torque[2] = b->torque[2] + t[2];
        }
    }
}

/* BrRbAddWheelForces: a wheel's forces into its sum, and their flat moment about its mount
   into the chassis' torque while it is on the ground */
static void BrRbAddWheelForces(Body *b, Body *w)
{
    Force *a;
    fx g[3], flat[3], f[3], p[3], r[3], t[3];
    for (a = w->forces; a != 0; a = a->next) {
        if (a->frame == 0)
            BrMat4RotateVec(f, b->m, a->f);
        if (a->frame == 1) {
            f[0] = a->f[0];
            f[1] = a->f[1];
            f[2] = a->f[2];
        }
        flat[0] = f[0];
        flat[1] = f[1];
        flat[2] = FX(0.0f);
        BrMat4RotateVecT(g, b->m, flat);
        w->force[0] = w->force[0] + f[0];
        w->force[1] = w->force[1] + f[1];
        w->force[2] = w->force[2] + f[2];
        if (0 != w->x1b4) {
            p[0] = w->m[3][0];
            p[1] = w->m[3][1];
            p[2] = w->m[3][2];
            BrMat4RotateVecT(r, b->m, p);
            t[0] = FMUL(r[1], g[2]) - FMUL(r[2], g[1]);
            t[1] = FMUL(r[2], g[0]) - FMUL(r[0], g[2]);
            t[2] = FMUL(r[0], g[1]) - FMUL(r[1], g[0]);
            b->torque[0] = b->torque[0] + t[0];
            b->torque[1] = b->torque[1] + t[1];
            b->torque[2] = b->torque[2] + t[2];
        }
    }
}

/* BrRbForcesClear: clear and resum the chassis' and wheels' forces, then accelerations */
#ifndef FX_FLOAT
/* the fixed point's BrRbForcesClear: the same sums with the rotations taken out of them.  A
   body-axes force's moment R^T at x R^T f is R^T (at x f), so those are summed in body axes and
   turned once; a wheel's forces all act at its mount, so its flat moment is the mount crossed
   with their flat sum, the four wheels' turned once together */
SIM_OWN static void forces_sum(Body *b)
{
    Force *a;
    fx fw[3] = { 0, 0, 0 }, tw[3] = { 0, 0, 0 }, fb[3] = { 0, 0, 0 }, tb[3] = { 0, 0, 0 };
    fx r[3], t[3], c[3];
    int k, i;

    for (a = b->forces; a != 0; a = a->next) {
        if (a->frame == 0) {
            for (k = 0; k < 3; k++)
                fw[k] += a->f[k];
            if (b->kind != 2) {
                BrMat4RotateVecT(r, b->m, a->at);
                tw[0] += FMUL(r[1], a->f[2]) - FMUL(r[2], a->f[1]);
                tw[1] += FMUL(r[2], a->f[0]) - FMUL(r[0], a->f[2]);
                tw[2] += FMUL(r[0], a->f[1]) - FMUL(r[1], a->f[0]);
            }
        } else {
            for (k = 0; k < 3; k++)
                fb[k] += a->f[k];
            if (b->kind != 2) {
                tb[0] += FMUL(a->at[1], a->f[2]) - FMUL(a->at[2], a->f[1]);
                tb[1] += FMUL(a->at[2], a->f[0]) - FMUL(a->at[0], a->f[2]);
                tb[2] += FMUL(a->at[0], a->f[1]) - FMUL(a->at[1], a->f[0]);
            }
        }
    }
    for (i = 0; i < 4; i++) {                    /* the wheels: their forces in body axes */
        Body *w = b->sub[i];
        fx sf[3] = { 0, 0, 0 }, f[3];
        for (a = w->forces; a != 0; a = a->next) {
            if (a->frame == 0)
                BrMat4RotateVec(f, b->m, a->f);
            else {
                f[0] = a->f[0];
                f[1] = a->f[1];
                f[2] = a->f[2];
            }
            for (k = 0; k < 3; k++)
                sf[k] += f[k];
        }
        for (k = 0; k < 3; k++)
            w->force[k] += sf[k];
        if (w->forces != 0 && w->x1b4 != 0) {
            const fx *p = w->m[3];
            tb[0] += -FMUL(p[2], sf[1]);
            tb[1] += FMUL(p[2], sf[0]);
            tb[2] += FMUL(p[0], sf[1]) - FMUL(p[1], sf[0]);
        }
    }
    BrMat4RotateVecT(c, b->m, fb);
    BrMat4RotateVecT(t, b->m, tb);
    for (k = 0; k < 3; k++) {
        b->force[k] += fw[k] + c[k];
        b->torque[k] += tw[k] + t[k];
    }
}
#endif

void BrRbForcesClear(Body *b)
{
    b->force[2] = b->force[1] = b->force[0] = FX(0.0f);
    b->torque[2] = b->torque[1] = b->torque[0] = FX(0.0f);
    b->sub[0]->force[0] = FX(0.0f);
    b->sub[0]->force[1] = FX(0.0f);
    b->sub[0]->force[2] = FX(0.0f);
    b->sub[1]->force[0] = FX(0.0f);
    b->sub[1]->force[1] = FX(0.0f);
    b->sub[1]->force[2] = FX(0.0f);
    b->sub[2]->force[0] = FX(0.0f);
    b->sub[2]->force[1] = FX(0.0f);
    b->sub[2]->force[2] = FX(0.0f);
    b->sub[3]->force[0] = FX(0.0f);
    b->sub[3]->force[1] = FX(0.0f);
    b->sub[3]->force[2] = FX(0.0f);
#ifndef FX_FLOAT
    forces_sum(b);
#else
    BrRbAddForces(b);
    BrRbAddWheelForces(b, b->sub[0]);
    BrRbAddWheelForces(b, b->sub[1]);
    BrRbAddWheelForces(b, b->sub[2]);
    BrRbAddWheelForces(b, b->sub[3]);
#endif
    BrRbSolveAccel(b);
}

/* ---- driving/tyre.c ---- */

/* BrTyreSkidCheck: the body's drag, more on surface 4 when moving */
void BrTyreSkidCheck(Body *b, Force *f)
{
    int s0, s1, s2, s3;
    fx d[3];
    f->f[0] = FMUL(b->st.vel[0], FX(-110.0f));
    f->f[1] = FMUL(b->st.vel[1], FX(-110.0f));
    f->f[2] = FMUL(b->st.vel[2], FX(-110.0f));
    s0 = b->sub[0]->surface;
    s1 = b->sub[1]->surface;
    s2 = b->sub[2]->surface;
    s3 = b->sub[3]->surface;
    if (FSQRT(FMUL(b->st.vel[0], b->st.vel[0]) + FMUL(b->st.vel[1], b->st.vel[1]) + FMUL(b->st.vel[2], b->st.vel[2])) > FX(4.0f)
        && g_world.weather != 3 && (s0 == 4 || s1 == 4 || s2 == 4 || s3 == 4)) {
        d[0] = FMUL(b->st.vel[0], FX(-220.0f));
        d[1] = FMUL(b->st.vel[1], FX(-220.0f));
        d[2] = FMUL(b->st.vel[2], FX(-220.0f));
        f->f[0] += d[0];
        f->f[1] += d[1];
        f->f[2] += d[2];
    }
}

int BrCrTriContainsPoint(const Plane *pT, const fx *pP);
fx BrCrPlaneDist(const fx *n, fx d, const fx *p);
SimCell BrCollGridCellAcquire(fx x, fx y);

/* BrWheelGroundProbe: how far a wheel can drop before the ground (100: none) */
SIM_OWN static fx BrWheelGroundProbe(Body *b, Body *w)
{
    static const fx down[3] = { FX(0.0f), FX(0.0f), FX(-1.0f) };   /* D_802A4B94 */
    fx mount[3], world[3], dir[3], best, t, h, d, e;
    const Plane *pPl;
    SimCell cell;
    int k, npick;
    int32_t q[6];
    uint16_t pick[256];

    best = FX(100.0f);
    mount[0] = w->st.pos[0];
    mount[1] = w->st.pos[1];
    mount[2] = FX(0.0f);
    BrMat3MulVecRows(world, b->m, mount);
    BrMat4RotateVecT(dir, b->m, down);
    w->hit = 0;
    cell = BrCollGridCellAcquire(world[0], world[1]);
    sim_pick_box(q, world, FX(2.01f));  /* (a hit is within 2 of the point, on its triangle) */
    npick = sim_cell_pick(&cell, q, pick);
#ifndef FX_FLOAT
    return sim_probe_walk(w, &cell, pick, npick, world, dir);
#endif
    for (k = 0; k < npick; k++) {
        pPl = &g_track->planes[cell.tris[pick[k]]];
        {   /* (a hit within 2 of the point lies on the triangle: farther than that, none) */
            fx dx = pPl->c[0] - world[0], dy = pPl->c[1] - world[1], dz = pPl->c[2] - world[2], rr = pPl->r + FX(2.01f);
            if (FMUL(dx, dx) + FMUL(dy, dy) + FMUL(dz, dz) > FMUL(rr, rr))
                continue;
        }
        d = BrCrPlaneDist(pPl->n, pPl->d, world);
        if (d > FXD(-2.0) && d < FXD(2.0)) {
            t = FMUL(pPl->n[0], dir[0]) + FMUL(pPl->n[1], dir[1]) + FMUL(pPl->n[2], dir[2]);
            if ((t < FX(0.0f) ? -t : t) > FXD(0.001)) {
                e = FMUL(pPl->n[0], world[0]) + FMUL(pPl->n[1], world[1]) + FMUL(pPl->n[2], world[2]);
                h = FDIV(-(pPl->d + e), t);
                mount[0] = FMUL(dir[0], h);
                mount[1] = FMUL(dir[1], h);
                mount[2] = FMUL(dir[2], h);
                mount[0] = mount[0] + world[0];
                mount[1] = mount[1] + world[1];
                mount[2] = mount[2] + world[2];
                if (h > FXD(-2.0) && h < FXD(2.0) && h < best && pPl->n[2] > FXD(0.2) && BrCrTriContainsPoint(pPl, mount) != 0) {
                    w->hit = pPl;
                    w->surface = pPl->surface;
                    best = h;
                    w->hitN[0] = pPl->n[0];
                    w->hitN[1] = pPl->n[1];
                    w->hitN[2] = pPl->n[2];
                    w->hitN[3] = pPl->d;
                }
            }
        }
    }
    return best;
}

/* BrTyreSprings: the four wheels' spring loads into the chassis' first four force records */
void BrTyreSprings(Body *b)
{
    Force *w;
    Body *s;
    int i;
    short prev;
    fx d;

    w = b->forces;
    for (i = 0; i < 4; i++) {
        w->f[0] = w->f[1] = FX(0.0f);
        s = b->sub[i];
        prev = (short)s->x1b4;
        d = s->depth;
        if (s->x1b4 < 100)
            s->x1b4++;
        if (d <= FXD(-0.4 + 0.0001)) {
            s->x1b4 = 0;
            d = FTOF(FXD(-0.3));
        }
        if (FXD(0.0) < d)
            d = 0;
        d = FTOF(d - FXD(-0.3));
        if (d < FX(0.0f))
            d = FX(0.0f);
        d = FTOF(FMUL(d, FMUL(d == FX(0.0f) ? FXD(0.0) : (d > 0 ? FXD(1.0) : FXD(-1.0)), d)));
        d = FMUL(d, b->spring);
        w->f[2] = d;
        w = w->next;
        if (s->x1b4 != 0 && prev == 0)
            b->landed = 0x80;
    }
}

/* BrTyreLoads: each wheel's damper load from the chassis' velocity there */
void BrTyreLoads(Body *b)
{
    Force *w;
    Body *s;
    int i;
    fx v[3], load;

    w = b->forces;
    for (i = 0; i < 4; i++) {
        w->f[0] = w->f[1] = FX(0.0f);
        BrRbVelAtFlatPoint(v, b, b->sub[i]);
        s = b->sub[i];
        if (s->x1b4 == 0)
            load = FX(0.0f);
        else if (v[2] < FX(0.0f))
            load = FX(0.0f);
        else
            load = FMUL(v[2], b->damper);
        w->f[2] = load;
        w = w->next;
    }
}

/* BrTyreDepthAll: each wheel's drop, and its travel held to [-0.4, 0] */
void BrTyreDepthAll(Body *b)
{
    int i;
    Body *w;
    fx f;
    for (i = 0; i < 4; i++) {
        w = b->sub[i];
        f = -BrWheelGroundProbe(b, w);
        w->depth = f;
        if (f > FXD(0.0))
            f = FX(0.0f);
        if (f < FXD(-0.4))
            w->st.pos[2] = FX(-0.4f);
        else
            w->st.pos[2] = f;
    }
}
