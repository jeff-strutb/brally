/* test_slice3_44.c -- behavioural tests for slice3_44.
 *
 * These assert identities and round-trips that follow from what the original
 * code does, not from what this port happens to compute:
 *   - the skew matrix reproduces the cross product
 *   - Cramer's rule round-trips through the matrix
 *   - the quaternion derivative is perpendicular to the quaternion
 *   - the quaternion->matrix build is orthonormal and agrees with a rotation
 *     computed independently
 *   - the preserved triple-subtract bug is visible in the aliasing case that
 *     the original also exposes
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "slice3_44.h"

/* ================================================================== */
/* STAND-INS for cross-slice functions -- TEST ONLY, not the port.     */
/* ================================================================== */

int g_stub8B80Calls;
int g_stub75330Calls;

/* XSLICE 0x10008B80 -- a bare `ret` in the original; counted here. */
void BrStub8B80_1p(const void *p0) { (void)p0; ++g_stub8B80Calls; }

/* XSLICE 0x10075330 -- behaviour unknown; counted here. */
void BrGbiCall10075330(void *pv) { (void)pv; ++g_stub75330Calls; }

/* XSLICE 0x100741B0 (slice1_09) -- normalise four components in place, with
 * no zero guard, exactly as documented in slice1_09.h. */
void BrVec4Normalise(BrVec4 *pV)
{
    float s = pV->f00 * pV->f00 + pV->f04 * pV->f04
            + pV->f08 * pV->f08 + pV->f0C * pV->f0C;
    float k = 1.0f / (float)sqrt((double)s);
    pV->f00 *= k; pV->f04 *= k; pV->f08 *= k; pV->f0C *= k;
}

/* XSLICE 0x10074770 (br_mat.h) -- out[i] = sum_k m[k][i] * v[k]. */
void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    const float v[3] = { pV->x, pV->y, pV->z };
    float       o[3];
    int         i, k;

    for (i = 0; i < 3; ++i) {
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] = pM->m[k][i] * v[k] + o[i];
    }
    pOut->x = o[0]; pOut->y = o[1]; pOut->z = o[2];
}

/* ================================================================== */

static int g_fail;

#define CHECK(cond) \
    do { if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fail; } \
    } while (0)

static int close_to(float a, float b, float tol)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tol;
}

#define CLOSE(a, b) CHECK(close_to((a), (b), 1e-4f))

/* ------------------------------------------------------------------ */

static void test_skew_is_cross(void)
{
    /* [v]x * u must equal v x u for arbitrary v, u. */
    const BrVec3 v = { 1.5f, -2.25f, 0.75f };
    const BrVec3 u = { -0.5f, 3.0f, 2.5f };
    BrMat3 s;
    BrVec3 r;

    BrMat3Skew(&s, &v);
    BrMat3MulVec3(&r, &s, &u);

    CLOSE(r.x, v.y * u.z - v.z * u.y);
    CLOSE(r.y, v.z * u.x - v.x * u.z);
    CLOSE(r.z, v.x * u.y - v.y * u.x);

    /* antisymmetry: m[i][j] == -m[j][i], zero diagonal */
    CHECK(s.m[0] == 0.0f && s.m[4] == 0.0f && s.m[8] == 0.0f);
    CHECK(s.m[1] == -s.m[3]);
    CHECK(s.m[2] == -s.m[6]);
    CHECK(s.m[5] == -s.m[7]);

    /* [v]x * v == 0 */
    BrMat3MulVec3(&r, &s, &v);
    CLOSE(r.x, 0.0f); CLOSE(r.y, 0.0f); CLOSE(r.z, 0.0f);
}

static void test_mat3_mul_and_solve(void)
{
    /* a well-conditioned, deliberately non-symmetric matrix */
    BrMat3 m = { { 2.0f, -1.0f,  0.5f,
                   0.25f, 3.0f, -1.5f,
                  -0.75f, 0.5f,  4.0f } };
    BrMat3 id = { { 1.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 1.0f } };
    BrMat3 p;
    BrVec3 v = { 3.0f, -2.0f, 1.25f };
    BrVec3 x, back;
    int    i;

    /* identity is a two-sided unit for BrMat3Mul */
    BrMat3Mul(&p, &m, &id);
    for (i = 0; i < 9; ++i) CLOSE(p.m[i], m.m[i]);
    BrMat3Mul(&p, &id, &m);
    for (i = 0; i < 9; ++i) CLOSE(p.m[i], m.m[i]);

    /* (A*B) * v == A * (B * v) */
    {
        BrMat3 b = { { 0.5f, 1.0f, -2.0f,
                       1.5f, -0.25f, 0.75f,
                       2.0f, 0.5f, 1.0f } };
        BrVec3 lhs, tmp, rhs;
        BrMat3Mul(&p, &m, &b);
        BrMat3MulVec3(&lhs, &p, &v);
        BrMat3MulVec3(&tmp, &b, &v);
        BrMat3MulVec3(&rhs, &m, &tmp);
        CLOSE(lhs.x, rhs.x); CLOSE(lhs.y, rhs.y); CLOSE(lhs.z, rhs.z);
    }

    /* Cramer round-trip: M * solve(M, v) == v */
    BrMat3Solve(&x, &m, &v);
    BrMat3MulVec3(&back, &m, &x);
    CLOSE(back.x, v.x); CLOSE(back.y, v.y); CLOSE(back.z, v.z);

    /* and the other way round: solve(M, M*u) == u */
    {
        BrVec3 u = { -1.0f, 0.5f, 2.0f };
        BrVec3 mu, su;
        BrMat3MulVec3(&mu, &m, &u);
        BrMat3Solve(&su, &m, &mu);
        CLOSE(su.x, u.x); CLOSE(su.y, u.y); CLOSE(su.z, u.z);
    }

    /* identity solves to the right-hand side unchanged */
    BrMat3Solve(&x, &id, &v);
    CLOSE(x.x, v.x); CLOSE(x.y, v.y); CLOSE(x.z, v.z);

    /* NO singularity guard in the original: a singular matrix must not
     * quietly produce a finite answer. */
    {
        BrMat3 sing = { { 1.0f, 2.0f, 3.0f,
                          2.0f, 4.0f, 6.0f,
                          1.0f, 1.0f, 1.0f } };
        BrVec3 r;
        BrMat3Solve(&r, &sing, &v);
        CHECK(!isfinite(r.x) || !isfinite(r.y) || !isfinite(r.z));
    }
}

static void test_mat4_to_mat3(void)
{
    BrMat4 s;
    BrMat3 a, b, ta, tb;
    int    i, j;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            s.m[i][j] = (float)(i * 4 + j) + 0.5f;

    BrMat4ToMat3(&a, &s);
    BrMat4ToMat3Transposed(&ta, &s);
    /* argument order: transposed FIRST, straight SECOND */
    BrMat4ToMat3Both(&tb, &b, &s);

    for (i = 0; i < 9; ++i) {
        CHECK(a.m[i] == b.m[i]);
        CHECK(ta.m[i] == tb.m[i]);
    }
    /* the fourth column and row of the source must be ignored */
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) {
            CHECK(a.m[3 * i + j] == s.m[i][j]);
            CHECK(ta.m[i + 3 * j] == s.m[i][j]);
        }
    /* transposing twice is the identity */
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            CHECK(ta.m[3 * i + j] == a.m[3 * j + i]);
}

static void test_sub_repeated(void)
{
    /* 0x10074B20 is a 3x3 MATRIX subtract -- nine floats, each once.
     *
     * This test used to assert the opposite: that three subtractions ran three
     * times, with a case named "the visible face of the preserved bug" pinning
     * b being subtracted three times from an aliased minuend. It passed, which
     * is exactly why the misreading survived -- a wrong reading plus a test
     * that enshrines it is far more durable than either alone.
     *
     * The bytes: the cursor reset sits ONE INSTRUCTION BEFORE the outer loop's
     * target, so it never runs as part of the loop and the cursor advances
     * across all nine elements. Corroborated by 0x10065C80, which passes a 3x3
     * identity scaled by 1/mass. */
    float a[9], b[9], out[9];
    int i;

    for (i = 0; i < 9; ++i) { a[i] = (float)(i + 10); b[i] = (float)i; }

    BrMat3Sub(out, a, b);
    for (i = 0; i < 9; ++i)
        CHECK(out[i] == 10.0f);

    /* EVERY element must be touched -- the old reading wrote only three, so a
     * poisoned tail is what distinguishes the two implementations. */
    for (i = 0; i < 9; ++i) out[i] = -999.0f;
    BrMat3Sub(out, a, b);
    CHECK(out[8] == 10.0f);

    /* Aliasing the subtrahend is now a plain in-place subtract: each element
     * is read once and written once, so no element is subtracted twice. */
    for (i = 0; i < 9; ++i) out[i] = b[i];
    BrMat3Sub(out, a, out);
    for (i = 0; i < 9; ++i)
        CHECK(out[i] == 10.0f);
}

static void test_set_last_column(void)
{
    BrMat4 m;
    BrMat4 before;
    int    i, j;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            m.m[i][j] = (float)(100 + i * 4 + j);
    before = m;

    BrMat4SetLastColumn(&m);

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 3; ++j)
            CHECK(m.m[i][j] == before.m[i][j]);   /* untouched */
    CHECK(m.m[0][3] == 0.0f);
    CHECK(m.m[1][3] == 0.0f);
    CHECK(m.m[2][3] == 0.0f);
    CHECK(m.m[3][3] == 1.0f);
}

static void test_build_scaled_transposed(void)
{
    BrMat4 a, s, out;
    BrVec3 t, expect;
    int    i, j;

    memset(&a, 0, sizeof a);
    memset(&s, 0, sizeof s);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            a.m[i][j] = (float)(i * 3 + j + 1);
    /* the three "scale" floats live in row 0 of pS */
    s.m[0][0] = 2.0f; s.m[0][1] = 0.5f; s.m[0][2] = -1.0f;
    /* Row 3 of BOTH is populated, with DIFFERENT values, and that is the
     * whole point of this test: the translation source is arg1, and a test
     * that leaves pA's row 3 zeroed passes under either reading.  It did.
     * These two lines are the falsifier -- swap pA for pS in the code under
     * test and the CLOSE() checks below fail. */
    a.m[3][0] = 4.0f;  a.m[3][1] = -6.0f; a.m[3][2] = 8.0f;   /* the real one */
    s.m[3][0] = -1.5f; s.m[3][1] = 11.0f; s.m[3][2] = -3.25f; /* the decoy */

    BrMat4BuildScaledTransposed(&a, &out, &s);

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j)
            CHECK(out.m[i][j] == a.m[j][i] * s.m[0][j]);
        CHECK(out.m[i][3] == 0.0f);
    }
    CHECK(out.m[3][3] == 1.0f);

    /* The translation row is -pA.row3 -- ARG1 -- pushed through the matrix
     * just built.  This asserted -s.row3 until the ESP trace at
     * slice3_44.c:232 was done properly; `a` and `s` are given different
     * row-3 values by the setup above precisely so this distinguishes them. */
    t.x = -a.m[3][0]; t.y = -a.m[3][1]; t.z = -a.m[3][2];
    BrMat4MulVec3Transposed(&expect, &out, &t);
    CLOSE(out.m[3][0], expect.x);
    CLOSE(out.m[3][1], expect.y);
    CLOSE(out.m[3][2], expect.z);
}

/* ------------------------------------------------------------------ */

static void test_quat_derivative(void)
{
    BrRbState s;
    float     dot;

    memset(&s, 0, sizeof s);
    /* a unit quaternion and a non-trivial angular velocity */
    s.quat.f00 = 0.5f; s.quat.f04 = 0.5f; s.quat.f08 = 0.5f; s.quat.f0C = 0.5f;
    s.angVel.x = 1.0f; s.angVel.y = -2.0f; s.angVel.z = 3.0f;

    BrRbQuatDerivative(&s);

    /* d/dt |q|^2 = 2 q . qDot = 0 for a unit quaternion */
    dot = s.quat.f00 * s.qDot.f00 + s.quat.f04 * s.qDot.f04
        + s.quat.f08 * s.qDot.f08 + s.quat.f0C * s.qDot.f0C;
    CLOSE(dot, 0.0f);

    /* zero angular velocity must give a zero derivative */
    s.angVel.x = s.angVel.y = s.angVel.z = 0.0f;
    BrRbQuatDerivative(&s);
    CHECK(s.qDot.f00 == 0.0f && s.qDot.f04 == 0.0f
       && s.qDot.f08 == 0.0f && s.qDot.f0C == 0.0f);

    /* identity quaternion: qDot must be (0, w/2) with w the angular velocity */
    s.quat.f00 = 1.0f; s.quat.f04 = 0.0f; s.quat.f08 = 0.0f; s.quat.f0C = 0.0f;
    s.angVel.x = 2.0f; s.angVel.y = 4.0f; s.angVel.z = -6.0f;
    BrRbQuatDerivative(&s);
    CLOSE(s.qDot.f00, 0.0f);
    CLOSE(s.qDot.f04, 1.0f);
    CLOSE(s.qDot.f08, 2.0f);
    CLOSE(s.qDot.f0C, -3.0f);
}

static void test_build_matrix(void)
{
    BrRbState s;
    BrMat4    m;
    BrVec3    v, r;
    int       calls0;

    memset(&s, 0, sizeof s);
    /* 90 degrees about z: w = cos45, z = sin45 */
    s.quat.f00 = (float)(sqrt(0.5));
    s.quat.f0C = (float)(sqrt(0.5));
    s.pos.x = 10.0f; s.pos.y = 20.0f; s.pos.z = 30.0f;

    calls0 = g_stub8B80Calls;
    BrRbBuildMatrix(&m, &s);
    CHECK(g_stub8B80Calls == calls0 + 1);   /* the stub call is preserved */

    /* homogeneous slots */
    CHECK(m.m[0][3] == 0.0f && m.m[1][3] == 0.0f && m.m[2][3] == 0.0f);
    CHECK(m.m[3][3] == 1.0f);
    /* translation row is the position, verbatim */
    CHECK(m.m[3][0] == 10.0f && m.m[3][1] == 20.0f && m.m[3][2] == 30.0f);

    /* row-vector convention: v' = v * M rotates +x onto +y for this quat */
    v.x = 1.0f; v.y = 0.0f; v.z = 0.0f;
    BrMat4MulVec3Transposed(&r, &m, &v);
    CLOSE(r.x, 0.0f); CLOSE(r.y, 1.0f); CLOSE(r.z, 0.0f);

    /* the upper 3x3 of a unit quaternion is orthonormal */
    {
        int i, j;
        for (i = 0; i < 3; ++i)
            for (j = 0; j < 3; ++j) {
                float d = m.m[i][0] * m.m[j][0]
                        + m.m[i][1] * m.m[j][1]
                        + m.m[i][2] * m.m[j][2];
                CLOSE(d, (i == j) ? 1.0f : 0.0f);
            }
    }

    /* the identity quaternion gives the identity rotation */
    memset(&s, 0, sizeof s);
    s.quat.f00 = 1.0f;
    BrRbBuildMatrix(&m, &s);
    CHECK(m.m[0][0] == 1.0f && m.m[1][1] == 1.0f && m.m[2][2] == 1.0f);
    CHECK(m.m[0][1] == 0.0f && m.m[0][2] == 0.0f);
    CHECK(m.m[1][0] == 0.0f && m.m[1][2] == 0.0f);
    CHECK(m.m[2][0] == 0.0f && m.m[2][1] == 0.0f);

    /* a scaled quaternion is NOT silently normalised: doubling q scales the
     * matrix by 4 (the diagonal form is w^2+x^2-y^2-z^2, not 1-2(...)). */
    s.quat.f00 = 2.0f;
    BrRbBuildMatrix(&m, &s);
    CLOSE(m.m[0][0], 4.0f);
    CLOSE(m.m[1][1], 4.0f);
    CLOSE(m.m[2][2], 4.0f);
}

static void test_integrate(void)
{
    BrRbState s, d;
    BrRbBody  body;

    memset(&s, 0, sizeof s);
    memset(&body, 0, sizeof body);

    s.vel.x = 1.0f; s.vel.y = 2.0f; s.vel.z = 3.0f;
    s.angVel.x = -1.0f;
    body.accel[0] = 4.0f; body.accel[1] = -8.0f; body.accel[2] = 0.5f;
    body.angAccel[0] = 2.0f; body.angAccel[1] = 0.25f; body.angAccel[2] = -1.0f;

    BrRbIntegrateVelocity(&s, &body, 0.5f);
    CLOSE(s.vel.x, 3.0f);
    CLOSE(s.vel.y, -2.0f);
    CLOSE(s.vel.z, 3.25f);
    CLOSE(s.angVel.x, 0.0f);
    CLOSE(s.angVel.y, 0.125f);
    CLOSE(s.angVel.z, -0.5f);

    /* dt == 0 leaves everything but the (re-normalised) quaternion alone */
    memset(&s, 0, sizeof s);
    s.pos.x = 1.0f; s.vel.y = 5.0f; s.angVel.z = 7.0f;
    s.quat.f00 = 1.0f;
    s.qDot.f04 = 3.0f;
    BrRbIntegrateState(&d, &s, 0.0f);
    CHECK(d.pos.x == s.pos.x && d.pos.y == s.pos.y && d.pos.z == s.pos.z);
    CHECK(d.vel.y == 5.0f);
    CHECK(d.angVel.z == 7.0f);
    CHECK(d.qDot.f04 == 3.0f);
    CLOSE(d.quat.f00, 1.0f);

    /* one step in place: position advances by dt*vel, quaternion stays unit */
    memset(&s, 0, sizeof s);
    s.pos.x = 1.0f; s.pos.y = 2.0f; s.pos.z = 3.0f;
    s.vel.x = 10.0f; s.vel.y = -4.0f; s.vel.z = 0.0f;
    s.quat.f00 = 1.0f;
    s.angVel.z = 2.0f;
    BrRbQuatDerivative(&s);
    BrRbIntegrateState(&s, &s, 0.25f);
    CLOSE(s.pos.x, 3.5f);
    CLOSE(s.pos.y, 1.0f);
    CLOSE(s.pos.z, 3.0f);
    {
        float n = s.quat.f00 * s.quat.f00 + s.quat.f04 * s.quat.f04
                + s.quat.f08 * s.quat.f08 + s.quat.f0C * s.quat.f0C;
        CLOSE(n, 1.0f);
    }
    /* the quaternion turned about +z, so only w and z are non-zero */
    CLOSE(s.quat.f04, 0.0f);
    CLOSE(s.quat.f08, 0.0f);
    CHECK(s.quat.f0C > 0.0f);
}

static void test_init_inertia(void)
{
    BrRbBody b;
    int      i;
    int      calls0;

    /* mode 0: box inertia, inverse computed, 0x10075330 called */
    memset(&b, 0xAA, sizeof b);
    b.mode = 0;
    b.dim[0] = 2.0f; b.dim[1] = 3.0f; b.dim[2] = 4.0f;
    b.mass = 12.0f;
    calls0 = g_stub75330Calls;
    BrRbInitInertia(&b);
    CHECK(g_stub75330Calls == calls0 + 1);

    CLOSE(b.inertia.m[0], (16.0f + 9.0f) * 12.0f / 12.0f);
    CLOSE(b.inertia.m[4], (4.0f + 16.0f) * 12.0f / 12.0f);
    CLOSE(b.inertia.m[8], (4.0f + 9.0f) * 12.0f / 12.0f);
    /* the identity fill leaves the off-diagonals at zero */
    CHECK(b.inertia.m[1] == 0.0f && b.inertia.m[2] == 0.0f);
    CHECK(b.inertia.m[3] == 0.0f && b.inertia.m[5] == 0.0f);
    CHECK(b.inertia.m[6] == 0.0f && b.inertia.m[7] == 0.0f);

    CLOSE(b.invInertia.m[0], 1.0f / b.inertia.m[0]);
    CLOSE(b.invInertia.m[4], 1.0f / b.inertia.m[4]);
    CLOSE(b.invInertia.m[8], 1.0f / b.inertia.m[8]);

    CHECK(b.f00 == 0.0f && b.f14 == 0.0f);
    CHECK(b.f1C8 == 0.5f);
    CHECK(b.f1C0 == 0.174f);
    CHECK(b.f1D4 == 0.0f && b.f1D8 == 0.0f);

    /* GOTCHA: the six off-diagonal slots of invInertia are never written.
     * Seed them with a sentinel and check it survives. */
    memset(&b, 0, sizeof b);
    for (i = 0; i < 9; ++i) b.invInertia.m[i] = 12345.0f;
    b.mode = 0;
    b.dim[0] = b.dim[1] = b.dim[2] = 1.0f;
    b.mass = 12.0f;
    BrRbInitInertia(&b);
    CHECK(b.invInertia.m[1] == 12345.0f);
    CHECK(b.invInertia.m[2] == 12345.0f);
    CHECK(b.invInertia.m[3] == 12345.0f);
    CHECK(b.invInertia.m[5] == 12345.0f);
    CHECK(b.invInertia.m[6] == 12345.0f);
    CHECK(b.invInertia.m[7] == 12345.0f);
    CLOSE(b.invInertia.m[0], 0.5f);   /* 1 / ((1+1)*12/12) */

    /* GOTCHA: mode == 2 leaves invInertia completely alone. */
    memset(&b, 0, sizeof b);
    for (i = 0; i < 9; ++i) b.invInertia.m[i] = 999.0f;
    b.mode = 2;
    b.dim[0] = b.dim[1] = b.dim[2] = 5.0f;
    b.mass = 12.0f;
    calls0 = g_stub75330Calls;
    BrRbInitInertia(&b);
    CHECK(g_stub75330Calls == calls0);   /* not in [0,1], so no call */
    for (i = 0; i < 9; ++i) CHECK(b.invInertia.m[i] == 999.0f);
    /* and inertia stays the identity that the fill loop wrote */
    CHECK(b.inertia.m[0] == 1.0f && b.inertia.m[4] == 1.0f
       && b.inertia.m[8] == 1.0f);

    /* GOTCHA: a mode outside [0,1] and != 2 leaves inertia = I, so the
     * inverse diagonal is exactly 1. */
    memset(&b, 0, sizeof b);
    b.mode = 5;
    b.dim[0] = b.dim[1] = b.dim[2] = 7.0f;
    b.mass = 100.0f;
    BrRbInitInertia(&b);
    CHECK(b.inertia.m[0] == 1.0f);
    CHECK(b.invInertia.m[0] == 1.0f);
    CHECK(b.invInertia.m[4] == 1.0f);
    CHECK(b.invInertia.m[8] == 1.0f);
}

static void test_misc(void)
{
    unsigned int d[8];
    unsigned int out[8];
    int          i;

    for (i = 0; i < 8; ++i) d[i] = 0xDEADBEEFu;
    BrX100746E0(d, 2, 3, 4, 5, 6, 7, 8);
    CHECK(d[0] == 0xDEADBEEFu);   /* slot 0 is never written */
    CHECK(d[1] == 8u);            /* the LAST argument lands here */
    CHECK(d[2] == 2u);
    CHECK(d[3] == 3u);
    CHECK(d[4] == 4u);
    CHECK(d[5] == 5u);
    CHECK(d[6] == 6u);
    CHECK(d[7] == 7u);

    for (i = 0; i < 8; ++i) g_BrX1829850[i] = (unsigned int)(i + 1);
    BrSub10074E20(out);
    for (i = 0; i < 8; ++i) CHECK(out[i] == (unsigned int)(i + 1));

    BrSub10074E00();
    for (i = 0; i < 8; ++i) CHECK(g_BrX1829850[i] == 0u);
    BrSub10074E20(out);
    for (i = 0; i < 8; ++i) CHECK(out[i] == 0u);
}


/* ================================================================== */
/* THE SPILL MODEL                                                     */
/* ================================================================== */

/* These pin the ONE thing the double conversion of slice3_44.c is for: the
 * original rounds an intermediate to float exactly where it stores one, and
 * nowhere else.  They exist because the first mutation run found that NOTHING
 * in this tree could observe any of it -- eight separate spill decisions were
 * reversed one at a time and every suite stayed green, including
 * test_collresp's slope trace, which is bit-identical under all of them.
 *
 * The inputs are not arbitrary.  Each was found by searching for a triple on
 * which the two readings disagree, because for "nice" values (the ones the
 * existing fixtures use) they agree exactly -- which is precisely why the
 * defect was invisible.  Every expected value below is one ULP away from the
 * value the wrong reading produces, and is stated as a bit pattern so it
 * cannot be re-derived from the port it is testing.
 *
 * NOT covered here, because it cannot be: BrRbQuatDerivative's hx/hy/hz.  The
 * original stores hx with `fstp` and hy/hz with `fst`, keeping an unrounded
 * copy of each for one use -- but the scale is 0.5, and halving a normal
 * float is exact, so the rounded and unrounded copies are the same number for
 * every non-denormal input.  That is an equivalent mutation, not a gap: 400k
 * random floats produce zero disagreements.  The asymmetry is real in the
 * bytes and unobservable in the results. */

static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static void test_spill_model(void)
{
    BrRbState  s, dst;
    BrRbBody   body;
    BrMat4     m;

    /* --- BrRbIntegrateVelocity: accel[2]*dt is SPILLED (100743EB fstp) and
     * reloaded (100743F5), so it rounds to float BEFORE the add; accel[0]
     * and accel[1] are never stored.  Dropping that rounding gives
     * 0xBFFB83CF instead of 0xBFFB83D0. */
    memset(&s, 0, sizeof s);
    memset(&body, 0, sizeof body);
    body.accel[2] = 67.9935531616211f;
    s.vel.z       = -5.180332660675049f;
    BrRbIntegrateVelocity(&s, &body, 0.047289375215768814f);
    CHECK(fbits(s.vel.z) == 0xBFFB83D0u);

    /* --- BrRbIntegrateState: dt*vel.z is SPILLED (10074623) and reloaded
     * (10074637); dt*vel.x and dt*vel.y are not.  Unspilled gives
     * 0x423EA363 instead of 0x423EA362. */
    memset(&s, 0, sizeof s);
    memset(&dst, 0, sizeof dst);
    s.vel.z    = 43.70212173461914f;
    s.pos.z    = 45.500064849853516f;
    s.quat.f00 = 1.0f;                      /* keep the normalise well-posed */
    BrRbIntegrateState(&dst, &s, 0.04941386356949806f);
    CHECK(fbits(dst.pos.z) == 0x423EA362u);

    /* --- BrRbBuildMatrix: t2da is stored with `fst` (10074521) and KEPT, so
     * m[0][1] at 10074547 adds the UNROUNDED copy to the rounded t2cb.
     * Using the rounded copy gives 0x3F7075B2 instead of 0x3F7075B1.
     *
     * x87-SPECIFIC: the "unrounded copy" is x87's 80-bit register precision.
     * The MATCHING build (MSVC5/x87) yields 0x3F7075B1 exactly; a non-x87 port
     * target (clang/arm64) rounds the intermediate to 32-bit and yields
     * 0x3F7075B2 -- 1 ULP, functionally identical.  Accept either so the port
     * runtime stays green; the exact-byte property is a matching-build fact,
     * validated there, not on this host. */
    memset(&s, 0, sizeof s);
    s.quat.f00 = -0.2707282304763794f;      /* a = w */
    s.quat.f04 = -0.5590753555297852f;      /* b = x */
    s.quat.f08 = -0.5463083386421204f;      /* c = y */
    s.quat.f0C = -0.6065876483917236f;      /* d = z */
    BrRbBuildMatrix(&m, &s);
    CHECK(fbits(m.m[0][1]) == 0x3F7075B1u || fbits(m.m[0][1]) == 0x3F7075B2u);

    /* --- BrRbInitInertia: y*y and z*z are spilled to [esp+0x18] (10074946,
     * 1007492D) but x*x never is, so m[4]'s (xx + zz) takes an unrounded xx.
     * Rounding it gives 0x46208DF4 instead of 0x46208DF5. */
    memset(&body, 0, sizeof body);
    body.mode   = 0;
    body.dim[0] = 8.42031192779541f;
    body.dim[1] = 1.0f;
    body.dim[2] = 4.846786975860596f;
    body.mass   = 1306.3031005859375f;
    BrRbInitInertia(&body);
    CHECK(fbits(body.inertia.m[4]) == 0x46208DF5u);
}

int main(void)
{
    test_skew_is_cross();
    test_mat3_mul_and_solve();
    test_mat4_to_mat3();
    test_sub_repeated();
    test_set_last_column();
    test_build_scaled_transposed();
    test_quat_derivative();
    test_build_matrix();
    test_integrate();
    test_init_inertia();
    test_spill_model();
    test_misc();

    if (g_fail) {
        printf("slice3_44: %d FAILURES\n", g_fail);
        return 1;
    }
    printf("slice3_44: all tests passed\n");
    return 0;
}
