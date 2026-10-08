/* coll.c -- collision: driving/collresp.c (the contact planes, the impulse,
 * the substepped advance, car against car) and driving/obb.c (triangle
 * against the unit cube), transcribed.  The track's grid cells come straight
 * from the track (sim.h SimCell) in place of the game's four-slot cache. */
#include "sim.h"

#define ABS(x) ((x) < FX(0.0f) ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) == FX(0.0f) ? FXD(0.0) : ((x) > 0 ? FXD(1.0) : FXD(-1.0)))

void BrRbVelAtPoint(fx out[3], Body *b, const fx *pt);

/* x % 3 for x in 0 .. 4, from a table (Thumb has no divide, and the compiler would call one) */
static const signed char s_mod3[5] = { 0, 1, 2, 0, 1 };
#define MOD3(x) s_mod3[x]

/* the contact list (D_802A4A20, newest first) and the shared contact values */
static const Plane *s_contact[160];
static int s_ncontact;
static fx D_8037EAA8[4];                /* the contact point, then its normal */
static fx D_8037EAC8[3];                /* the push-out */
static int D_802A4A28;                  /* the contact's case (game mode 4 only) */
static const Plane *D_802A4A2C;         /* the contact's plane */

/* BrCollGridCellAcquire: the triangles of the 32-unit cell under (x, y), as
   BrGridCellRangeAt and BrU16QueuePop list them (a 0 ends the list) */
SimCell BrCollGridCellAcquire(fx x, fx y)
{
    SimCell c;
    int i, first, n;
    c.tris = 0;
    c.n = 0;
    c.key = (FTOI(y) / 32 << 6) + FTOI(x) / 32;
    if (x < FX(0.0f) || x >= FX(2048.0f) || y < FX(0.0f) || y >= FX(2048.0f))
        return c;
    i = (uint8_t)FTOI(FDIVK(x, 32.0f)) + (uint8_t)FTOI(FDIVK(y, 32.0f)) * 64;
    first = g_track->cellStart[i];
    n = g_track->cellStart[i + 1] - first;
    c.tris = &g_track->cellTris[first];
    for (i = 0; i < n && c.tris[i] != 0; i++)
        ;
    c.n = i;
    return c;
}

/* BrCrTriContainsPoint: the point inside the plane's triangle, flattened on its two
   least-aligned axes (Badouel's test cut down to one triangle) */
int BrCrTriContainsPoint(const Plane *pT, const fx *pP)
{
    fx a0, a1, d1, b1, d2, b2, c1, c2, u, v;
    int c, i1, i2, r;

    a0 = ABS(pT->n[0]);
    a1 = ABS(pT->n[1]);
    if (a1 < a0) {
        a0 = ABS(pT->n[0]);
        a1 = ABS(pT->n[2]);
        c = a1 < a0 ? 0 : 2;
    } else {
        a0 = ABS(pT->n[1]);
        a1 = ABS(pT->n[2]);
        c = a1 < a0 ? 1 : 2;
    }
    i1 = MOD3(c + 1);
    i2 = MOD3(c + 2);
    d1 = pP[i1] - pT->v0[i1];
    d2 = pP[i2] - pT->v0[i2];
    r = 0;
    b1 = pT->v1[i1] - pT->v0[i1];
    b2 = pT->v1[i2] - pT->v0[i2];
    c1 = pT->v2[i1] - pT->v0[i1];
    c2 = pT->v2[i2] - pT->v0[i2];
    if (b1 == FX(0.0f)) {
        u = FDIV(d1, c1);
        if (u >= FX(0.0f) && u <= FX(1.0f)) {
            v = FDIV(d2 - FMUL(u, c2), b2);
            r = v >= FX(0.0f) && v + u <= FX(1.0f);
        }
    } else {
        u = FDIV(FMUL(d2, b1) - FMUL(d1, b2), FMUL(c2, b1) - FMUL(c1, b2));
        if (u >= FX(0.0f) && u <= FX(1.0f)) {
            v = FDIV(d1 - FMUL(u, c1), b1);
            r = v >= FX(0.0f) && v + u <= FX(1.0f);
        }
    }
    return (short)r;
}

fx BrCrPlaneDist(const fx *n, fx d, const fx *p)
{
    return FMUL(n[0], p[0]) + FMUL(n[1], p[1]) + FMUL(n[2], p[2]) + d;
}

/* ---- driving/obb.c: Voorhies' triangle against the unit cube ---- */

#define PAIR(cum, out, expr, bp, bn, lim) \
    if ((cum) & ((bp) | (bn))) { \
        s = (expr); \
        if (((cum) & (bp)) && s > (lim)) (out) |= (bp); \
        else if (((cum) & (bn)) && s < -(lim)) (out) |= (bn); \
    }

static int BrTriCubeTrivial(fx verts[3][3])
{
    int cum_and, i, bits;
    fx s;

    cum_and = ~0;
    for (i = 0; i < 3; i++) {
        int face_bits = 0;
        fx *p = verts[i];
        PAIR(~0, face_bits, p[0], 0x01, 0x02, FXD(.5))
        PAIR(~0, face_bits, p[1], 0x04, 0x08, FXD(.5))
        PAIR(~0, face_bits, p[2], 0x10, 0x20, FXD(.5))
        if (face_bits == 0)
            return 1;
        cum_and &= face_bits;
    }
    if (cum_and != 0)
        return 0;
    cum_and = ~0;
    for (i = 0; i < 3; i++) {
        bits = 0;
        PAIR(cum_and, bits, verts[i][0] + verts[i][1], 0x001, 0x002, FXD(1.0))
        PAIR(cum_and, bits, verts[i][0] - verts[i][1], 0x004, 0x008, FXD(1.0))
        PAIR(cum_and, bits, verts[i][0] + verts[i][2], 0x010, 0x020, FXD(1.0))
        PAIR(cum_and, bits, verts[i][0] - verts[i][2], 0x040, 0x080, FXD(1.0))
        PAIR(cum_and, bits, verts[i][1] + verts[i][2], 0x100, 0x200, FXD(1.0))
        PAIR(cum_and, bits, verts[i][1] - verts[i][2], 0x400, 0x800, FXD(1.0))
        cum_and = bits;
        if (cum_and == 0)
            break;
    }
    if (cum_and != 0)
        return 0;
    cum_and = ~0;
    for (i = 0; i < 3; i++) {
        bits = 0;
        PAIR(cum_and, bits, verts[i][0] + verts[i][1] + verts[i][2], 0x01, 0x02, FXD(1.5))
        PAIR(cum_and, bits, verts[i][0] + verts[i][1] - verts[i][2], 0x04, 0x08, FXD(1.5))
        PAIR(cum_and, bits, verts[i][0] - verts[i][1] + verts[i][2], 0x10, 0x20, FXD(1.5))
        PAIR(cum_and, bits, verts[i][0] - verts[i][1] - verts[i][2], 0x40, 0x80, FXD(1.5))
        cum_and = bits;
        if (cum_and == 0)
            break;
    }
    if (cum_and != 0)
        return 0;
    return -1;
}

#define DOT3(a, b) (FMUL((a)[0], (b)[0]) + FMUL((a)[1], (b)[1]) + FMUL((a)[2], (b)[2]))
#define SIGN_NONZERO(x) ((x) < 0 ? -1 : 1)
#define MAXINDEX2(a) ((a)[0] > (a)[1] ? 0 : 1)
#define MAXINDEX3(a) ((a)[0] > (a)[2] ? MAXINDEX2(a) : 1 + MAXINDEX2((a) + 1))
#define seg_contains_point(a, b, x) (((b) > (x)) - ((a) > (x)))

static int BrPolyContainsPoint3d(fx verts[][3], const fx polynormal[3], const fx point[3])
{
    fx abspolynormal[3];
    int zaxis, xaxis, yaxis, i, count, xdirection;
    fx *v, *w;

    for (i = 0; i < 3; i++)
        abspolynormal[i] = ABS(polynormal[i]);
    zaxis = MAXINDEX3(abspolynormal);
    if (polynormal[zaxis] < 0) {
        xaxis = MOD3(zaxis + 2);
        yaxis = MOD3(zaxis + 1);
    } else {
        xaxis = MOD3(zaxis + 1);
        yaxis = MOD3(zaxis + 2);
    }
    count = 0;
    for (i = 0; i < 3; i++) {
        v = verts[i];
        w = verts[MOD3(i + 1)];
        if ((xdirection = seg_contains_point(v[xaxis], w[xaxis], point[xaxis]))) {
            if (seg_contains_point(v[yaxis], w[yaxis], point[yaxis])) {
                if (FMUL(ITOF(xdirection), FMUL(point[xaxis] - v[xaxis], w[yaxis] - v[yaxis])) <=
                    FMUL(ITOF(xdirection), FMUL(point[yaxis] - v[yaxis], w[xaxis] - v[xaxis])))
                    count += xdirection;
            } else {
                if (v[yaxis] <= point[yaxis])
                    count += xdirection;
            }
        }
    }
    return count;
}

static int BrSegIntersectsCube(const fx v0[3], const fx v1[3])
{
    int i, iplus1, iplus2, edgevec_signs[3];
    fx edgevec[3];

    edgevec[0] = v1[0] - v0[0];
    edgevec[1] = v1[1] - v0[1];
    edgevec[2] = v1[2] - v0[2];
    for (i = 0; i < 3; i++)
        edgevec_signs[i] = SIGN_NONZERO(edgevec[i]);
    for (i = 0; i < 3; i++) {
        if (FMUL(v0[i], ITOF(edgevec_signs[i])) > FXD(.5))
            return 0;
        if (FMUL(v1[i], ITOF(edgevec_signs[i])) < FXD(-.5))
            return 0;
    }
    for (i = 0; i < 3; i++) {
        fx rhomb_normal_dot_v0;
        fx rhomb_normal_dot_cubedge;
        iplus1 = MOD3(i + 1);
        iplus2 = MOD3(i + 2);
        rhomb_normal_dot_v0 = FMUL(edgevec[iplus2], v0[iplus1]) - FMUL(edgevec[iplus1], v0[iplus2]);
        rhomb_normal_dot_cubedge = FTOF(FMUL(FXD(.5), FMUL(edgevec[iplus2], ITOF(edgevec_signs[iplus1])) +
                                                     FMUL(edgevec[iplus1], ITOF(edgevec_signs[iplus2]))));
        if (FMUL(rhomb_normal_dot_v0, rhomb_normal_dot_v0) > FMUL(rhomb_normal_dot_cubedge, rhomb_normal_dot_cubedge))
            return 0;
    }
    return 1;
}

static int BrPolyIntersectsCube(fx verts[3][3], const fx polynormal[3])
{
    int i, best_diagonal[3];
    fx p[3], t, bd[3];

    for (i = 0; i < 3; ++i)
        if (BrSegIntersectsCube(verts[i], verts[MOD3(i + 1)]))
            return 1;
    for (i = 0; i < 3; i++)
        best_diagonal[i] = SIGN_NONZERO(polynormal[i]);
    for (i = 0; i < 3; i++)
        bd[i] = ITOF(best_diagonal[i]);
    t = FDIV(DOT3(polynormal, verts[0]), DOT3(polynormal, bd));
    if (!(FMUL(t - FXD(-.5), t - FXD(.5)) <= 0))
        return 0;
    p[0] = FMUL(t, bd[0]);
    p[1] = FMUL(t, bd[1]);
    p[2] = FMUL(t, bd[2]);
    return BrPolyContainsPoint3d(verts, polynormal, p);
}

static int BrTriCubeTest(fx tri[3][3], const fx *norm)
{
    int r = BrTriCubeTrivial(tri);
    if (r == -1)
        return BrPolyIntersectsCube(tri, norm);
    return r;
}

/* ---- driving/collresp.c ---- */

/* BrCrContactKick: bounce the velocity off a contact normal (game mode 4's edge case) */
static int BrCrContactKick(Body *b, const fx *pN, int dampFlag, int spinFlag)
{
    fx t[3], v[3], M[4][4], d, k;
    RbState *cur = &b->stB;

    d = FMUL(pN[0], cur->vel[0]) + FMUL(pN[1], cur->vel[1]) + FMUL(pN[2], cur->vel[2]);
    if (d >= FX(0.0f))
        return 0;
    t[0] = FTOF(FMUL(pN[0], FMUL(d, FXD(1.05))));
    t[1] = FTOF(FMUL(pN[1], FMUL(d, FXD(1.05))));
    t[2] = FTOF(FMUL(pN[2], FMUL(d, FXD(1.05))));
    cur->vel[0] = cur->vel[0] - t[0];
    cur->vel[1] = cur->vel[1] - t[1];
    cur->vel[2] = cur->vel[2] - t[2];
    if (b->idle < 10) {
        k = FX(0.9f);
    } else {
        k = d < FX(0.0f) ? -d : d;
        if (k > FX(27.0f))
            k = FX(27.0f);
        b->hitSize = (uint8_t)FTOI(k);
        if (b->hitSize < 10) {
            b->hitSize = 0;
        } else {
            b->hitN2[0] = D_8037EAA8[0];
            b->hitN2[1] = D_8037EAA8[1];
            b->hitN2[2] = D_8037EAA8[2];
        }
        {
            uint8_t peak = (uint8_t)FTOI(FDIVK(FMUL(k, FX(127.0f)), 27.0f) + FX(128.0f));
            b->hitPeak = b->hitPeak < peak ? peak : b->hitPeak;
        }
        k = FX(0.9f);
        cur->vel[0] = FMUL(cur->vel[0], k);
        cur->vel[1] = FMUL(cur->vel[1], k);
        cur->vel[2] = FMUL(cur->vel[2], k);
    }
    if (dampFlag) {
        cur->vel[0] = FMUL(cur->vel[0], k);
        cur->vel[1] = FMUL(cur->vel[1], k);
        cur->vel[2] = FMUL(cur->vel[2], k);
    }
    if (spinFlag == 0)
        return 1;
    t[0] = pN[1];
    t[1] = pN[2];
    t[2] = pN[0];
    M[1][0] = FMUL(pN[1], t[2]) - FMUL(pN[2], t[1]);
    M[1][1] = FMUL(pN[2], t[0]) - FMUL(pN[0], t[2]);
    M[1][2] = FMUL(pN[0], t[1]) - FMUL(pN[1], t[0]);
    M[2][0] = FMUL(pN[1], M[1][2]) - FMUL(pN[2], M[1][1]);
    M[2][1] = FMUL(pN[2], M[1][0]) - FMUL(pN[0], M[1][2]);
    M[2][2] = FMUL(pN[0], M[1][1]) - FMUL(pN[1], M[1][0]);
    M[0][0] = pN[0];
    M[0][1] = pN[1];
    M[0][2] = pN[2];
    BrMat4RotateVecT(t, M, pN);
    BrMat4RotateVecT(v, M, cur->omega);
    v[0] = FMUL(v[0], t[0]);
    v[1] = FMUL(v[1], t[1]);
    v[2] = FMUL(v[2], t[2]);
    BrMat4RotateVec(cur->omega, M, v);
    return 1;
}

/* BrCrImpulseSolve: the impulse at one contact, through the contact's effective mass */
int BrCrImpulseSolve(Body *b, const fx *pN, const fx *pDir, int flag, fx rest)
{
    fx Rt[3][3], R[3][3], skew[3][3], D[3][3], K[3][3], tmp[3][3], W[3][3];
    fx vc[3], J[3], nb[3], dw[3], jm[3], tt[3], tn[3], dd, r;
    RbState *cur = &b->stB;
    int i, j;

    vc[0] = pN[0];
    vc[1] = pN[1];
    vc[2] = pN[2];
    BrMat3Transpose(Rt, R, b->m);
    BrMat3MulVec(nb, Rt, vc);
    vc[0] = FMUL(cur->omega[1], nb[2]) - FMUL(cur->omega[2], nb[1]);
    vc[1] = FMUL(cur->omega[2], nb[0]) - FMUL(cur->omega[0], nb[2]);
    vc[2] = FMUL(cur->omega[0], nb[1]) - FMUL(cur->omega[1], nb[0]);
    vc[0] = vc[0] + cur->vel[0];
    vc[1] = vc[1] + cur->vel[1];
    vc[2] = vc[2] + cur->vel[2];
    if (FMUL(pDir[0], vc[0]) + FMUL(pDir[1], vc[1]) + FMUL(pDir[2], vc[2]) >= FX(0.0f))
        return 0;
    BrMat3Skew(skew, nb);
    BrMat3Mul(tmp, b->Iinv, R);
    BrMat3Mul(W, Rt, tmp);
    BrMat3Mul(K, W, skew);
    BrMat3Mul(tmp, skew, K);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            D[i][j] = i == j ? FX(1.0f) : FX(0.0f);
    D[0][0] = D[1][1] = D[2][2] = FDIV(FX(1.0f), b->mass);
    BrMat3Sub(K, D, tmp);
    dd = FMUL(pDir[0], vc[0]) + FMUL(pDir[1], vc[1]) + FMUL(pDir[2], vc[2]);
    tn[0] = FMUL(pDir[0], dd);
    tn[1] = FMUL(pDir[1], dd);
    tn[2] = FMUL(pDir[2], dd);
    if (dd < FX(0.0f))
        dd = -dd;
    if (dd > FX(27.0f))
        dd = FX(27.0f);
    b->hitSize = (uint8_t)FTOI(dd);
    if (b->hitSize < 10) {
        b->hitSize = 0;
    } else {
        b->hitN2[0] = D_8037EAA8[0];
        b->hitN2[1] = D_8037EAA8[1];
        b->hitN2[2] = D_8037EAA8[2];
    }
    if (b->idle > 10 && rest < FX(1e-4f)) {
        uint8_t peak = (uint8_t)FTOI(FDIVK(FMUL(FX(127.0f), dd), 27.0f) + FX(128.0f));
        b->hitPeak = b->hitPeak < peak ? peak : b->hitPeak;
        tn[0] = FMUL(tn[0], FX(0.9f));
        tn[1] = FMUL(tn[1], FX(0.9f));
        tn[2] = FMUL(tn[2], FX(0.9f));
        vc[0] = FMUL(vc[0], FX(0.9f));
        vc[1] = FMUL(vc[1], FX(0.9f));
        vc[2] = FMUL(vc[2], FX(0.9f));
    }
    tt[0] = vc[0] - tn[0];
    tt[1] = vc[1] - tn[1];
    tt[2] = vc[2] - tn[2];
    if (flag) {
        tt[0] = FMUL(tt[0], FX(0.2f));
        tt[1] = FMUL(tt[1], FX(0.2f));
        tt[2] = FMUL(tt[2], FX(0.2f));
    } else {
        tt[0] = FX(0.0f);
        tt[1] = FX(0.0f);
        tt[2] = FX(0.0f);
    }
    vc[0] = tt[0] + tn[0];
    vc[1] = tt[1] + tn[1];
    vc[2] = tt[2] + tn[2];
    BrMat3Solve(J, K, vc);
    jm[0] = FMUL(J[0], FDIV(FX(1.0f), b->mass));
    jm[1] = FMUL(J[1], FDIV(FX(1.0f), b->mass));
    jm[2] = FMUL(J[2], FDIV(FX(1.0f), b->mass));
    vc[0] = FMUL(nb[1], J[2]) - FMUL(nb[2], J[1]);
    vc[1] = FMUL(nb[2], J[0]) - FMUL(nb[0], J[2]);
    vc[2] = FMUL(nb[0], J[1]) - FMUL(nb[1], J[0]);
    BrMat3MulVec(dw, W, vc);
    r = FX(1.05f) + rest;
    jm[0] = FMUL(jm[0], r);
    jm[1] = FMUL(jm[1], r);
    jm[2] = FMUL(jm[2], r);
    dw[0] = FMUL(dw[0], r);
    dw[1] = FMUL(dw[1], r);
    dw[2] = FMUL(dw[2], r);
    cur->vel[0] = cur->vel[0] - jm[0];
    cur->vel[1] = cur->vel[1] - jm[1];
    cur->vel[2] = cur->vel[2] - jm[2];
    cur->omega[0] = cur->omega[0] - dw[0];
    cur->omega[1] = cur->omega[1] - dw[1];
    cur->omega[2] = cur->omega[2] - dw[2];
    return 1;
}

/* a triangle's corners into the box's frame, its edges' normal */
static void tri_in_box(fx v[3][3], fx nrm[3], fx m[4][4], const Plane *pP)
{
    fx e1[3], e2[3];
    BrMat3MulVecRows(v[0], m, pP->v0);
    BrMat3MulVecRows(v[1], m, pP->v1);
    BrMat3MulVecRows(v[2], m, pP->v2);
    e1[0] = v[1][0] - v[0][0];
    e1[1] = v[1][1] - v[0][1];
    e1[2] = v[1][2] - v[0][2];
    e2[0] = v[2][0] - v[0][0];
    e2[1] = v[2][1] - v[0][1];
    e2[2] = v[2][2] - v[0][2];
    nrm[0] = FMUL(e1[1], e2[2]) - FMUL(e1[2], e2[1]);
    nrm[1] = FMUL(e1[2], e2[0]) - FMUL(e1[0], e2[2]);
    nrm[2] = FMUL(e1[0], e2[1]) - FMUL(e1[1], e2[0]);
}

/* BrCollRespBroadPhase: the cell's triangles touching the body's box, onto the contact list
   (the cell walked backwards on alternate frames) */
static int BrCollRespBroadPhase(Body *b, fx m[4][4])
{
    fx v[3][3], nrm[3];
    SimCell cell;
    int i, n = 0;

    cell = BrCollGridCellAcquire(b->m[3][0], b->m[3][1]);
    for (i = 0; i < cell.n; i++) {
        const Plane *pP = &g_track->planes[cell.tris[g_world.walkBack ? cell.n - 1 - i : i]];
        fx dx = pP->c[0] - b->m[3][0], dy = pP->c[1] - b->m[3][1], dz = pP->c[2] - b->m[3][2], rr = pP->r + FX(8.7f);
        if (FMUL(dx, dx) + FMUL(dy, dy) + FMUL(dz, dz) > FMUL(rr, rr))
            continue;                   /* (beyond the unit cube's reach: 5 x sqrt 3 units at the box's 0.1) */
        tri_in_box(v, nrm, m, pP);
        if (BrTriCubeTest(v, nrm) != 0) {
            if (s_ncontact < (int)(sizeof s_contact / sizeof s_contact[0]))
                s_contact[s_ncontact++] = pP;
            n++;
        }
    }
    return n;
}

/* BrCollRespTipKick: a car standing on its nose, nudged over */
static int BrCollRespTipKick(Body *b)
{
    Body *pW = 0;
    fx p[3], w[3], t, best;
    int count, k;

    BrQuatToMat(b->m, &b->stA);
    best = FX(100.0f);
    count = 0;
    for (k = 0; k < 4; k++) {
        if (b->sub[k]->x1b4 != 0) {
            count++;
            pW = b->sub[k];
            p[0] = FTOF(FMUL(SIGN(b->sub[k]->st.pos[0]), FMUL(b->box[0], FX(0.5f))));
            p[1] = FTOF(FMUL(SIGN(pW->st.pos[1]), FMUL(b->box[1], FX(0.5f))));
            p[2] = FMUL(-b->box[2], FX(0.5f)) + b->box[3];
            BrMat3MulVecRows(w, b->m, p);
            t = BrCrPlaneDist(pW->hitN, pW->hitN[3], w);
            t = ABS(t);
            best = MIN(best, t);
        }
    }
    if (count > 2 || count < 1)
        return 0;
    if (best > FXD(0.6))
        return 0;
    BrRbVelAtPoint(w, b, p);
    t = FMUL(w[0], pW->hitN[0]) + FMUL(w[1], pW->hitN[1]) + FMUL(w[2], pW->hitN[2]);
    if (ABS(t) > FX(0.1f))
        return 0;
    t = FMUL(b->m[0][0], b->hitN[0]) + FMUL(b->m[0][1], b->hitN[1]) + FMUL(b->m[0][2], b->hitN[2]);
    if (t > FX(0.0f)) {
        p[1] = FX(0.1f);
        p[0] = FX(0.0f);
        p[2] = FX(0.0f);
    } else {
        p[0] = FX(0.0f);
        p[1] = FX(-0.1f);
        p[2] = FX(0.0f);
    }
    BrMat4RotateVecT(w, b->m, p);
    b->stA.omega[0] = b->stA.omega[0] + w[0];
    b->stA.omega[1] = b->stA.omega[1] + w[1];
    b->stA.omega[2] = b->stA.omega[2] + w[2];
    b->stA.omega[0] = b->stA.omega[0] + w[0];
    b->stA.omega[1] = b->stA.omega[1] + w[1];
    b->stA.omega[2] = b->stA.omega[2] + w[2];
    BrRbQuatDerivative(&b->stA);
    return 1;
}

/* BrCrPlaneResolve: a candidate contact's push-out */
static void BrCrPlaneResolve(Body *b, const fx *pA, fx planeD, const fx *pEdgeN, fx v[3][3])
{
    fx c[3], s;
    int sgn, k;

    if (D_802A4A28 != 2) {
        s = FMUL(pA[0], pEdgeN[0]) + FMUL(pA[1], pEdgeN[1]) + FMUL(pA[2], pEdgeN[2]) - planeD;
        D_8037EAC8[0] = FMUL(pA[0], -s);
        D_8037EAC8[1] = FMUL(pA[1], -s);
        D_8037EAC8[2] = FMUL(pA[2], -s);
    } else {
        for (k = 0; k < 3; k++) {
            c[k] = v[0][k] + v[1][k] + v[2][k];
            c[k] = FDIVK(c[k], 3.0f);
        }
        if (ABS(c[0]) < ABS(c[1])) {
            if (ABS(c[0]) < ABS(c[2])) {
                D_8037EAA8[1] = D_8037EAA8[2] = FX(0.0f);
                sgn = c[0] < 0 ? -1 : 1;
                D_8037EAA8[0] = FMUL(ITOF(sgn), FX(0.5f));
            } else {
                goto three;
            }
        } else if (ABS(c[1]) < ABS(c[2])) {
            D_8037EAA8[0] = D_8037EAA8[2] = FX(0.0f);
            sgn = c[0] < 0 ? -1 : 1;
            D_8037EAA8[1] = FMUL(ITOF(sgn), FX(0.5f));
        } else {
        three:
            D_8037EAA8[0] = D_8037EAA8[1] = FX(0.0f);
            sgn = c[0] < 0 ? -1 : 1;
            D_8037EAA8[2] = FMUL(ITOF(sgn), FX(0.5f));
        }
        s = FMUL(pA[0], D_8037EAA8[0]) + FMUL(pA[1], D_8037EAA8[1]) + FMUL(pA[2], D_8037EAA8[2]) - planeD;
        D_8037EAA8[0] = FMUL(D_8037EAA8[0], b->box[0]);
        D_8037EAA8[0] = FMUL(D_8037EAA8[0], b->box[1]);
        D_8037EAA8[3] = FMUL(D_8037EAA8[3], b->box[2]);
        D_8037EAC8[0] = FMUL(pA[0], -s);
        D_8037EAC8[1] = FMUL(pA[1], -s);
        D_8037EAC8[2] = FMUL(pA[2], -s);
    }
}

/* BrCrRespWalk: this frame's contacts against the stepped state, each answered */
static int BrCrRespWalk(Body *b, fx m[4][4])
{
    fx v[3][3], nrm[3], e1[3], sign[3], planeD, d, dp[3];
    const Plane *pP;
    int ret, k, flag, spin = 0, sgn, r;
    short cnt;

    ret = 0;
    cnt = 0;
    for (k = s_ncontact - 1; k >= 0; k--) {
        pP = s_contact[k];
        tri_in_box(v, nrm, m, pP);
        if (BrTriCubeTest(v, nrm) == 0)
            continue;
        BrVec3NormaliseF(nrm);
        flag = 1;
        D_802A4A28 = 0;
        cnt++;
        planeD = (FMUL(nrm[0], v[0][0]) + FMUL(nrm[1], v[0][1])) + FMUL(nrm[2], v[0][2]);
        if (g_world.mode == 4) {
            if (ABS(nrm[0]) > FX(0.999f) || ABS(nrm[1]) > FX(0.999f) || ABS(nrm[2]) > FX(0.999f)) {
                D_802A4A28 = 1;
                spin = 1;
            } else if (ABS(planeD) < FX(0.5f)) {
                spin = 0;
                D_802A4A28 = 2;
            }
        }
        e1[0] = FMUL(nrm[0], planeD);
        e1[1] = FMUL(nrm[1], planeD);
        e1[2] = FMUL(nrm[2], planeD);
        sgn = e1[0] < 0 ? -1 : 1;
        sign[0] = FMUL(ITOF(sgn), FX(0.5f));
        sgn = e1[1] < 0 ? -1 : 1;
        sign[1] = FMUL(ITOF(sgn), FX(0.5f));
        sgn = e1[2] < 0 ? -1 : 1;
        sign[2] = FMUL(ITOF(sgn), FX(0.5f));
        D_8037EAA8[0] = FMUL(sign[0], b->box[0]);
        D_8037EAA8[1] = FMUL(sign[1], b->box[1]);
        D_8037EAA8[2] = FMUL(sign[2], b->box[2]) + b->box[3];
        D_802A4A2C = pP;
        BrCrPlaneResolve(b, nrm, planeD, sign, v);
        if (b->m[2][2] > FX(0.5f))
            flag = 0;
        if (D_802A4A28 != 1) {
            r = BrCrImpulseSolve(b, D_8037EAA8, D_802A4A2C->n, flag, FX(0.0f));
        } else {
            r = BrCrContactKick(b, D_802A4A2C->n, flag, spin);
        }
        if (r != 0) {
            ret = 1;
            dp[0] = b->stB.pos[0] - b->stA.pos[0];
            dp[1] = b->stB.pos[1] - b->stA.pos[1];
            dp[2] = b->stB.pos[2] - b->stA.pos[2];
            d = FTOF(FMUL((FMUL(dp[0], pP->n[0]) + FMUL(dp[1], pP->n[1])) + FMUL(dp[2], pP->n[2]), FXD(1.1)));
            dp[0] = FMUL(pP->n[0], d);
            dp[1] = FMUL(pP->n[1], d);
            dp[2] = FMUL(pP->n[2], d);
            b->stB.pos[0] = b->stB.pos[0] - dp[0];
            b->stB.pos[1] = b->stB.pos[1] - dp[1];
            b->stB.pos[2] = b->stB.pos[2] - dp[2];
            b->stB.q[0] = b->stA.q[0];         /* (three of the four, as the game copies them) */
            b->stB.q[1] = b->stA.q[1];
            b->stB.q[2] = b->stA.q[2];
            BrRbQuatDerivative(&b->stB);
            BrQuatToMat(b->m, &b->stB);
        }
    }
    if (cnt == 0) {
        if (b->idle < 40)
            b->idle++;
    } else {
        b->idle = 0;
    }
    return ret;
}

static void BrCarCarCollide(void);

/* BrCarPhysAdvance: the body's position physics in 1/120 s substeps: the nearby track
   gathered once, then car against car, a step, and the collision response, until 1/30 s
   is used; the stuck count while the body is not upright */
void BrCarPhysAdvance(Body *b)
{
    fx m[4][4], s[3], t, dt;

    s_ncontact = 0;
    D_8037EAA8[0] = FX(0.0f);
    D_8037EAA8[1] = FX(0.0f);
    D_8037EAA8[2] = FX(0.0f);
    D_8037EAC8[0] = FX(0.0f);
    D_8037EAC8[1] = FX(0.0f);
    D_8037EAC8[2] = FX(0.0f);
    D_802A4A28 = 0;
    D_802A4A2C = 0;
    s[1] = FX(0.1f);
    s[0] = FX(0.1f);
    s[2] = FX(0.1f);
    BrMat4InvertScaled(b->m, m, s);
    BrCollRespBroadPhase(b, m);
    s[0] = FDIV(FX(1.0f), b->box[0]);
    t = FX(0.033333335f);
    s[1] = FDIV(FX(1.0f), b->box[1]);
    s[2] = FDIV(FX(1.0f), b->box[2]);
    dt = FDIVK(t, 4);
    while (t > FX(0.002f)) {
        BrCollRespTipKick(b);
        BrCarCarCollide();
        BrRbStateStep(&b->stB, &b->stA, dt);
        BrQuatToMat(b->m, &b->stB);
        BrMat4InvertScaled(b->m, m, s);
        m[3][2] -= b->box[3];
        if (BrCrRespWalk(b, m) != 0) {
            BrRbQuatDerivative(&b->stB);
            BrQuatToMat(b->m, &b->stB);
        }
        b->stA = b->stB;
        t -= dt;
    }
    BrQuatToMat(b->m, &b->stB);
    b->stA = b->stB;
    if (b->m[2][2] < FX(0.5f)) {
        if (b->stuck < 0)
            b->stuck = -1;
        b->stuck--;
    } else {
        b->stuck = 35;
    }
}

/* BrObbOverlap: two oriented boxes overlap (the separating-axis test, all fifteen axes) */
static int BrObbOverlap(const fx *m, const fx *t, const fx *a, const fx *b)
{
    int ok, in;
    fx c, d, am[9];
    int k;

    for (k = 0; k < 9; k++)
        am[k] = ABS(m[k]);
    ok = 1;
    c = ABS(t[0]);
    in = c <= a[0] + FMUL(b[0], am[0]) + FMUL(b[1], am[1]) + FMUL(b[2], am[2]);
    ok &= in;
    d = FMUL(t[0], m[0]) + FMUL(t[1], m[3]) + FMUL(t[2], m[6]);
    c = ABS(d);
    in = c <= b[0] + FMUL(a[0], am[0]) + FMUL(a[1], am[3]) + FMUL(a[2], am[6]);
    ok &= in;
    c = ABS(t[1]);
    in = c <= a[1] + FMUL(b[0], am[3]) + FMUL(b[1], am[4]) + FMUL(b[2], am[5]);
    ok &= in;
    c = ABS(t[2]);
    in = c <= a[2] + FMUL(b[0], am[6]) + FMUL(b[1], am[7]) + FMUL(b[2], am[8]);
    ok &= in;
    d = FMUL(t[0], m[1]) + FMUL(t[1], m[4]) + FMUL(t[2], m[7]);
    c = ABS(d);
    in = c <= b[1] + FMUL(a[0], am[1]) + FMUL(a[1], am[4]) + FMUL(a[2], am[7]);
    ok &= in;
    d = FMUL(t[0], m[2]) + FMUL(t[1], m[5]) + FMUL(t[2], m[8]);
    c = ABS(d);
    in = c <= b[2] + FMUL(a[0], am[2]) + FMUL(a[1], am[5]) + FMUL(a[2], am[8]);
    ok &= in;
#define EDGE(p, q, r, s_, A, B, C, D) \
    d = FMUL(t[p], m[q]) - FMUL(t[r], m[s_]); \
    c = ABS(d); \
    ok &= c <= A + B + C + D;
    EDGE(2, 3, 1, 6, FMUL(a[1], am[6]), FMUL(a[2], am[3]), FMUL(b[1], am[2]), FMUL(b[2], am[1]))
    EDGE(2, 4, 1, 7, FMUL(a[1], am[7]), FMUL(a[2], am[4]), FMUL(b[0], am[2]), FMUL(b[2], am[0]))
    EDGE(2, 5, 1, 8, FMUL(a[1], am[8]), FMUL(a[2], am[5]), FMUL(b[0], am[1]), FMUL(b[1], am[0]))
    EDGE(0, 6, 2, 0, FMUL(a[0], am[6]), FMUL(a[2], am[0]), FMUL(b[1], am[5]), FMUL(b[2], am[4]))
    EDGE(0, 7, 2, 1, FMUL(a[0], am[7]), FMUL(a[2], am[1]), FMUL(b[0], am[5]), FMUL(b[2], am[3]))
    EDGE(0, 8, 2, 2, FMUL(a[0], am[8]), FMUL(a[2], am[2]), FMUL(b[0], am[4]), FMUL(b[1], am[3]))
    EDGE(1, 0, 0, 3, FMUL(a[0], am[3]), FMUL(a[1], am[0]), FMUL(b[1], am[8]), FMUL(b[2], am[7]))
    EDGE(1, 1, 0, 4, FMUL(a[0], am[4]), FMUL(a[1], am[1]), FMUL(b[0], am[8]), FMUL(b[2], am[6]))
    EDGE(1, 2, 0, 5, FMUL(a[0], am[5]), FMUL(a[1], am[2]), FMUL(b[0], am[7]), FMUL(b[1], am[6]))
#undef EDGE
    return ok;
}

/* BrCarCarCollide: every live pair within 5 units, as oriented 2.5 x 1 x 1 boxes; an
   overlapping pair pushed apart by an impulse along the line between them (the first
   pair that does not overlap ends the pass, as in the game) */
static void BrCarCarCollide(void)
{
    static const fx ext[3] = { FX(2.5f), FX(1.0f), FX(1.0f) };
    fx d[3], s, x, mA[3][3], mB[3][3], mR[3][3], dd[3], t[3], sd[3], imp[3];
    int i, j, k;
    RbState *pa, *pb;
    Car *ci, *cj;

    for (i = 0; i < g_world.ncars; i++) {
        ci = g_world.cars[i];
        if (ci == 0 || ci->fade == 2)
            continue;
        ci->hitAge++;
        pa = &ci->body.st;
        for (j = i + 1; j < g_world.ncars; j++) {
            cj = g_world.cars[j];
            if (cj == 0 || cj->fade == 2)
                continue;
            pb = &cj->body.st;
            d[0] = pa->pos[0] - pb->pos[0];
            d[1] = pa->pos[1] - pb->pos[1];
            d[2] = pa->pos[2] - pb->pos[2];
            if (FSQRT(FMUL(d[2], d[2]) + (FMUL(d[0], d[0]) + FMUL(d[1], d[1]))) < FX(5.0f)) {
                BrMat3FromMat4T(mB, cj->body.m);
                BrMat3FromMat4(mA, ci->body.m);
                BrMat3Mul(mR, mB, mA);
                dd[0] = pa->pos[0] - pb->pos[0];
                dd[1] = pa->pos[1] - pb->pos[1];
                dd[2] = pa->pos[2] - pb->pos[2];
                BrMat3MulVec(t, mA, dd);
                if (BrObbOverlap(&mR[0][0], t, ext, ext) == 0)
                    return;
                BrVec3NormaliseF(d);
                s = FX(0.0f);
                s += FMUL(d[2], pa->vel[2]) + (FMUL(pa->vel[0], d[0]) + FMUL(pa->vel[1], d[1]));
                x = s;
                s += FMUL(pb->vel[0], d[0]) + FMUL(pb->vel[1], d[1]) + FMUL(pb->vel[2], d[2]);
                s = FMUL(s, FX(0.5f));
                imp[0] = FMUL(d[0], s);
                imp[1] = FMUL(d[1], s);
                imp[2] = FMUL(d[2], s);
                if (s < x)
                    x = -(s - x);
                else
                    x = s - x;
                if (x > FX(27.0f))
                    x = FX(27.0f);
                if (ci->hitAge > 40)
                    cj->sndHitA = ci->sndHitA = (uint8_t)FTOI(FDIVK(FMUL(FX(127.0f), x), 27.0f) + FX(128.0f));
                ci->hitAge = 0;
                sd[0] = FMUL(d[0], FX(-1.0f));
                sd[1] = FMUL(d[1], FX(-1.0f));
                sd[2] = FMUL(d[2], FX(-1.0f));
                t[0] = FMUL(sd[0], FX(2.5f));
                t[1] = FMUL(sd[1], FX(2.5f));
                t[2] = FMUL(sd[2], FX(2.5f));
                BrMat4RotateVec(dd, ci->body.m, t);
                for (k = 0; k < 3; k++)
                    pa->vel[k] = pa->vel[k] - imp[k];
                ci->body.stB = ci->body.st;
                BrCrImpulseSolve(&ci->body, dd, d, 0, FX(0.45f));
                ci->body.st = ci->body.stB;
                for (k = 0; k < 3; k++)
                    ci->body.stB.vel[k] = ci->body.stB.vel[k] + imp[k];
                for (k = 0; k < 3; k++)
                    ci->body.stA.vel[k] = ci->body.stB.vel[k];
                for (k = 0; k < 3; k++)
                    pa->vel[k] = imp[k] + pa->vel[k];
                t[0] = FMUL(d[0], FX(2.5f));
                t[1] = FMUL(d[1], FX(2.5f));
                t[2] = FMUL(d[2], FX(2.5f));
                BrMat4RotateVec(dd, cj->body.m, t);
                for (k = 0; k < 3; k++)
                    pb->vel[k] = pb->vel[k] - imp[k];
                cj->body.stB = cj->body.st;
                BrCrImpulseSolve(&cj->body, dd, sd, 0, FX(0.45f));
                cj->body.st = cj->body.stB;
                for (k = 0; k < 3; k++)
                    cj->body.stB.vel[k] = cj->body.stB.vel[k] + imp[k];
                for (k = 0; k < 3; k++)
                    cj->body.stA.vel[k] = cj->body.stB.vel[k];
                for (k = 0; k < 3; k++)
                    pb->vel[k] = imp[k] + pb->vel[k];
            }
        }
    }
}
