/* test_collresp.c -- BEHAVIOUR tests for 0x10066D70 and the 0x10067C30 frame,
 * plus the PER-COMPONENT angular-velocity measurement the divergence hunt
 * needs.
 *
 * The two things asserted here that a "tidied" transcription would break:
 *
 *   - 0x10066D70's three gates are all NEGATED x87 tests, and two of them
 *     REJECT on the strictly-greater side (`test ah,0x41` + `je <ret 0>`).
 *     Written the tidy way, the |vn| gate inverts and the kick fires exactly
 *     when the original suppresses it.
 *   - the 0x4C-byte frame 0x10067C30 hands to 0x1006DDD0 OVERLAPS its own
 *     output.  Two disjoint objects give a different translation row.
 *
 * And one measurement rather than an assertion: TestSlopeDivergence prints
 * angVel per COMPONENT.  br_carphys.h's whole pitch finding rests on the fact
 * that |angVel| hid a pure-Y divergence for two passes, so the number this
 * suite exists to publish is the triple, not the magnitude.
 *
 * The collision grid is this file's own, the same arrangement test_carphys.c
 * and test_br_phys.c use and for the same reason: slice6_73.c owns the real
 * BrCollGridCellAcquire and linking it drags the menu tree in.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_carphys.h"
#include "br_collresp.h"

static int g_fail = 0;
static int g_checks;

#define CHECK(cond)                                                         \
    do {                                                                    \
        g_checks++;                                                         \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                               \
    do {                                                                    \
        g_checks++;                                                         \
        {                                                                   \
            double da_ = (double)(a), db_ = (double)(b);                    \
            if (!(fabs(da_ - db_) <= (eps))) {                              \
                printf("FAIL %s:%d: %s (%.9g) !~ %s (%.9g)\n", __FILE__,    \
                       __LINE__, #a, da_, #b, db_);                         \
                ++g_fail;                                                   \
            }                                                               \
        }                                                                   \
    } while (0)

/* The frame is 0x4C bytes and the matrix starts twelve into it. */
typedef int br_cr_assert_frame[(sizeof(BrCollRespFrame) == 0x4C) ? 1 : -1];

/* ================================================================== */
/* The grid this file owns                                             */
/* ================================================================== */

#define TEST_TRIS 8
static BrCollPlane s_grid[4 * BR_COLL_CELL_PLANES];
static uint16_t    s_count[4];
static BrVec3      s_verts[3 * TEST_TRIS];

BrCollPlane        *g_pBrCollGrid      = s_grid;
const uint16_t     *g_pBrCollGridCount = s_count;

short BrCollGridCellAcquire(float x, float y)
{
    (void)x; (void)y;
    return 0;
}

/* The same stand-ins test_carphys.c uses, and for the same reason. */
void BrStub8B80_1p(const void *p0)          { (void)p0; }
void BrGbiCall10075330(void *pv)            { (void)pv; }
void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc)
{ memset(pDst, 0, sizeof *pDst); (void)pSrc; }
void BrCarStateUnpack(BrCarState *pDst, const BrCarPacked *pSrc)
{ memset(pDst, 0, sizeof *pDst); (void)pSrc; }
void BrCarRecordToState(BrCarState *pDst, void *pCar)
{ memset(pDst, 0, sizeof *pDst); (void)pCar; }
void BrCarRecordFromState(void *pCar, const BrCarState *pSrc)
{ (void)pCar; (void)pSrc; }

/* One big triangle pair through (0,0,h) that RISES with `grade` along the
 * axis `iAx` (0 == x, the car's own longitudinal axis at yaw 0, so a slope
 * there is a PITCH input; 1 == y, which is a ROLL input). */
static void BuildSlope(float h, double grade, int iAx)
{
    static const float kQuad[2][3][2] = {
        { { -200.0f, -200.0f }, {  200.0f, -200.0f }, {  200.0f,  200.0f } },
        { { -200.0f, -200.0f }, {  200.0f,  200.0f }, { -200.0f,  200.0f } }
    };
    double inv = 1.0 / sqrt(1.0 + grade * grade);
    float  nx  = (iAx == 0) ? (float)(-grade * inv) : 0.0f;
    float  ny  = (iAx == 0) ? 0.0f : (float)(-grade * inv);
    float  nz  = (float)inv;
    int    t, k;

    memset(s_grid, 0, sizeof s_grid);
    for (t = 0; t < 2; ++t) {
        BrCollPlane *p = &s_grid[t];
        for (k = 0; k < 3; ++k) {
            float x = kQuad[t][k][0], y = kQuad[t][k][1];
            s_verts[t * 3 + k].x = x;
            s_verts[t * 3 + k].y = y;
            s_verts[t * 3 + k].z =
                (float)(h + grade * (double)((iAx == 0) ? x : y));
        }
        p->nx = nx; p->ny = ny; p->nz = nz;
        /* the plane through (0,0,h): n . p + d == 0 */
        p->d  = -nz * h;
        p->pV0 = &s_verts[t * 3 + 0];
        p->pV1 = &s_verts[t * 3 + 1];
        p->pV2 = &s_verts[t * 3 + 2];
        p->tri = (uint16_t)(t + 1);
        p->flags = 1;
    }
    s_count[0] = 2;
    s_count[1] = s_count[2] = s_count[3] = 0;
}

/* ================================================================== */
/* The overlapped frame                                                */
/* ================================================================== */

static void TestFrame(void)
{
    BrCollRespFrame f;
    BrMat4          m, *pOut;
    int             i, j;

    /* An easy body matrix: identity rotation, translation (1, 2, 3). */
    memset(&m, 0, sizeof m);
    for (i = 0; i < 4; ++i) m.m[i][i] = 1.0f;
    m.m[3][0] = 1.0f; m.m[3][1] = 2.0f; m.m[3][2] = 3.0f;

    memset(&f, 0, sizeof f);
    BrCollRespBuildBoxMatrix(&f, &m, 2.0f, 4.0f, 8.0f);
    pOut = BrCollRespFrameMat(&f);

    /* The matrix really does start three floats in -- 0x10067C9A and
     * 0x10067C9E are 0xC apart. */
    CHECK((void *)pOut == (void *)&f.a[3]);

    /* out[i][j] = m[j][i] * scale[j], so with the identity the diagonal is
     * the scale itself. */
    CHECK_NEAR(pOut->m[0][0], 2.0f, 1e-6);
    CHECK_NEAR(pOut->m[1][1], 4.0f, 1e-6);
    CHECK_NEAR(pOut->m[2][2], 8.0f, 1e-6);
    for (i = 0; i < 3; ++i) {
        CHECK(pOut->m[i][3] == 0.0f);
    }
    CHECK(pOut->m[3][3] == 1.0f);

    /* THE OVERLAP, asserted rather than described.  0x1006DDD0 builds its
     * translation from pS->m[3][0..2] -- pS+0x30..+0x38 -- and with this
     * frame those three words ARE pOut->m[2][1], m[2][2] and m[2][3], which
     * it has already written to 0, 8 and 0.  So the translation row is
     * -(0, 8, 0) run through pOut's own transpose, i.e. (0, -32, 0), and NOT
     * -(1, 2, 3) as a reader who assumed two disjoint objects would predict. */
    CHECK_NEAR(pOut->m[3][0],   0.0f, 1e-6);
    CHECK_NEAR(pOut->m[3][1], -32.0f, 1e-6);
    CHECK_NEAR(pOut->m[3][2],   0.0f, 1e-6);

    /* ...and the scale slots are untouched by the build. */
    CHECK(f.a[0] == 2.0f && f.a[1] == 4.0f && f.a[2] == 8.0f);

    (void)j;
}

/* ================================================================== */
/* The degenerate-box measurement                                      */
/* ================================================================== */

static void TestDegenerate(void)
{
    BrCarPhys car;

    BuildSlope(0.0f, 0.0, 0);
    BrCarPhysInit(&car, NULL);

    /* The constructor's own numbers, from Glide 0x1005BD40 / 42 / 48 / 4E. */
    CHECK(car.f1DC == 0.0f);
    CHECK(car.f1E0 == 0.0f);
    CHECK(car.f1E4 == 2.0f);
    CHECK(car.f1E8 == 0.0f);

    /* ...and they make 0x10067C30's reciprocals infinite, which is exactly
     * why the OBB half of the collision system cannot run in this port. */
    CHECK(BrCollRespBoxDegenerate(car.f1DC, car.f1E0, car.f1E4) != 0);
    /* A car whose data HAS been loaded is not degenerate. */
    CHECK(BrCollRespBoxDegenerate(3.5f, 2.0f, 1.5f) == 0);
}

/* ================================================================== */
/* 0x10066D70's three gates                                            */
/* ================================================================== */

/* Put the car flat on ground at z == 0 with all four wheels counted as in
 * contact and a plane record on each. */
static void ArmAllWheels(BrCarPhys *pCar, int cContact)
{
    int i;
    for (i = 0; i < 4; ++i) {
        pCar->wheel[i].f1B4 = (i < cContact) ? 1.0f : 0.0f;
        pCar->wheel[i].f19C = (i < cContact) ? 1.0f : 0.0f;
        pCar->aHit[i].nx = 0.0f;
        pCar->aHit[i].ny = 0.0f;
        pCar->aHit[i].nz = 1.0f;
        pCar->aHit[i].d  = 0.0f;
        pCar->aHit[i].surface = 0;
    }
}

static void TestTipKickGates(void)
{
    BrCarPhys car;
    BrVec3    p;
    int       r;

    BuildSlope(0.0f, 0.0, 0);
    BrCarPhysInit(&car, NULL);
    p.x = 0.0f; p.y = 0.0f; p.z = 0.5f;
    BrCarPhysPlace(&car, &p, 0.0f);

    /* The corner 0x10066D70 builds with the constructor's box is
     * (0, 0, f1E8 - f1E4*0.5) == (0, 0, -1).  With the body at z = 0.5 that
     * lands 0.5 BELOW a plane at z = 0, so |distance| is 0.5 -- inside the
     * 0.6 window (0x10077B78) -- and a car at rest has |vn| = 0, inside the
     * 0.1 window (0x10077AF4).  Two wheels in contact is inside 1..2. */
    ArmAllWheels(&car, 2);
    car.save = *BrCarPhysBodyState(&car.body);
    r = BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                          car.f1DC, car.f1E0, car.f1E4, car.f1E8);
    CHECK(r != 0);
    /* The chassis plane normal is zero, so `s == 0` takes the NEGATIVE arm
     * (`test ah,0x41` is set for EQUAL too) and the kick is -0.2 about the
     * car's own +Y -- which at yaw 0 is world -Y, i.e. PURE PITCH. */
    CHECK_NEAR(car.save.angVel.y, -2.0f * BR_CR_TIP_KICK, 1e-6);
    CHECK_NEAR(car.save.angVel.x, 0.0f, 1e-6);
    CHECK_NEAR(car.save.angVel.z, 0.0f, 1e-6);

    /* THREE wheels in contact: outside 1..2, nothing happens. */
    ArmAllWheels(&car, 3);
    car.save = *BrCarPhysBodyState(&car.body);
    CHECK(BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                            car.f1DC, car.f1E0, car.f1E4, car.f1E8) == 0);
    CHECK(car.save.angVel.y == 0.0f);

    /* NO wheels in contact: likewise. */
    ArmAllWheels(&car, 0);
    car.save = *BrCarPhysBodyState(&car.body);
    CHECK(BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                            car.f1DC, car.f1E0, car.f1E4, car.f1E8) == 0);

    /* Corner too far from the plane: raise the body so the corner sits 2 m
     * below it, well outside the 0.6 window. */
    p.z = 3.0f;
    BrCarPhysPlace(&car, &p, 0.0f);
    ArmAllWheels(&car, 1);
    car.save = *BrCarPhysBodyState(&car.body);
    CHECK(BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                            car.f1DC, car.f1E0, car.f1E4, car.f1E8) == 0);

    /* THE GATE THAT MATTERS FOR THE DIVERGENCE, and the one whose polarity is
     * easiest to invert: a corner MOVING is rejected.  `fcomp 0.1f` +
     * `test ah,0x41` + `je <ret 0>` leaves on the strictly-greater side, so
     * the kick is for a car that has come to REST on one or two wheels.  A
     * tidy `if (fabs(vn) > 0.1) apply` fires here instead of there. */
    p.z = 0.5f;
    BrCarPhysPlace(&car, &p, 0.0f);
    ArmAllWheels(&car, 2);
    car.save = *BrCarPhysBodyState(&car.body);
    car.save.vel.z = -5.0f;          /* driving straight into the plane */
    car.body.vel.z = -5.0f;
    CHECK(BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                            car.f1DC, car.f1E0, car.f1E4, car.f1E8) == 0);

    /* ...and the sign flips with the chassis plane normal, which is the one
     * field nothing in the original writes.  A positive `s` takes +0.1. */
    car.save = *BrCarPhysBodyState(&car.body);
    car.body.vel.z = 0.0f;
    car.bodyPlaneN.x = 1.0f;         /* dot with m row 0 == +1 at yaw 0 */
    CHECK(BrCollRespTipKick(&car.body, car.aHit, &car.bodyPlaneN, &car.save,
                            car.f1DC, car.f1E0, car.f1E4, car.f1E8) != 0);
    CHECK_NEAR(car.save.angVel.y, 2.0f * BR_CR_TIP_KICK, 1e-6);
}

/* ================================================================== */
/* The two measurements                                                */
/* ================================================================== */

/* The analytic settling height, worked out on paper in test_carphys.c:
 * four springs each carrying weight/4, so the body origin ends up
 * 0.3 - sqrt(|g| / 4k) above the ground.  Quoted to six decimals because
 * that is the bar this module is measured against. */
static void TestFlatSettleExact(void)
{
    BrCarPhys  car;
    BrVec3     p;
    BrRbState *pS;
    double     zEq;
    int        i;

    BuildSlope(0.0f, 0.0, 0);
    BrCarPhysInit(&car, NULL);
    BrCarPhysHoleReset();
    BrCollRespCountersReset();

    p.x = 10.0f; p.y = 10.0f; p.z = 3.0f;
    BrCarPhysPlace(&car, &p, 0.0f);
    pS = BrCarPhysBodyState(&car.body);

    for (i = 0; i < 401; ++i) {
        BrCarPhysStep(&car);
    }

    zEq = 0.3 - sqrt((double)(-BR_CP_GRAVITY_BODY)
                     / (4.0 * (double)car.body.f1B8));

    printf("  flat settle: z = %.6f  (analytic %.6f)\n",
           (double)pS->pos.z, zEq);
    printf("  flat settle: angVel = (%.6f, %.6f, %.6f)\n",
           (double)pS->angVel.x, (double)pS->angVel.y, (double)pS->angVel.z);

    /* SIX DECIMALS.  If this moves, the force balance changed. */
    CHECK_NEAR(pS->pos.z, 0.190132, 5e-7);

    /* On flat ground all four wheels stay in contact, so 0x10066D70's
     * 1..2 gate never opens and it must never have fired. */
    CHECK(g_cBrCollRespTipKick == 0u);
    /* ...and the box was degenerate on every one of the 401 frames, which is
     * what makes "no collisions were reported" mean something. */
    CHECK(g_cBrCollRespDegenerate == 401u);
}

/* The measurement br_carphys.h's pitch finding came out of, reproduced here
 * so it can be re-run without a track file: a 1-in-100 slope, 3 cm across the
 * 3 m wheelbase, and angVel printed PER COMPONENT.
 *
 * This is deliberately NOT an assertion.  The car diverges on this slope and
 * asserting a particular divergence would encode today's behaviour rather
 * than a property of the code -- exactly the failure CONVENTIONS.md records
 * three times.  It prints. */
static void TestSlopeDivergence(void)
{
    BrCarPhys  car;
    BrVec3     p;
    BrRbState *pS;
    int        i;

    BuildSlope(0.0f, 0.01, 0);       /* 1 in 100 ALONG THE WHEELBASE */
    BrCarPhysInit(&car, NULL);
    BrCarPhysHoleReset();
    BrCollRespCountersReset();

    p.x = 0.0f; p.y = 0.0f; p.z = 0.6f;
    BrCarPhysPlace(&car, &p, 0.0f);
    pS = BrCarPhysBodyState(&car.body);

    printf("  1-in-100 slope, angVel PER COMPONENT (the magnitude hid this):\n");
    printf("   step        x           y           z        |w|      z pos\n");
    for (i = 1; i <= 24; ++i) {
        BrCarPhysStep(&car);
        if (i <= 4 || (i % 2) == 0) {
            double wx = (double)pS->angVel.x;
            double wy = (double)pS->angVel.y;
            double wz = (double)pS->angVel.z;
            printf("   %4d  %10.4f  %10.4f  %10.4f  %8.4f  %8.4f\n",
                   i, wx, wy, wz, sqrt(wx * wx + wy * wy + wz * wz),
                   (double)pS->pos.z);
        }
    }
    printf("  0x10066D70 fired %u time(s); box degenerate on %u frame(s)\n",
           (unsigned)g_cBrCollRespTipKick,
           (unsigned)g_cBrCollRespDegenerate);

    /* The one thing that IS a property rather than a number: the divergence
     * is rotational and it is about the car's LATERAL axis.  Roll and yaw
     * stay small.  If a future pass makes |angVel| smaller by feeding roll or
     * yaw instead, this catches it. */
    CHECK(fabs((double)pS->angVel.x) < 1.0);
    CHECK(fabs((double)pS->angVel.z) < 1.0);
}

int main(void)
{
    TestFrame();
    TestDegenerate();
    TestTipKickGates();
    TestFlatSettleExact();
    TestSlopeDivergence();

    if (g_fail == 0) {
        printf("test_collresp: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    printf("test_collresp: %d failure(s)\n", g_fail);
    return 1;
}
