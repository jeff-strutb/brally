/* test_carphys.c -- BEHAVIOUR tests for 0x1005A7A0 and its force generators.
 *
 * The properties asserted here are the ones that a "tidied" transcription
 * would break silently:
 *
 *   - the three-way sign classifier's NaN arm (NaN classifies as ZERO, which
 *     is what makes the damper leave a NaN component alone rather than
 *     zeroing it);
 *   - the damper's polarity -- a component that CHANGES sign is zeroed, one
 *     that keeps it takes the new value;
 *   - the spring's dead band: a wheel with no ground produces exactly zero
 *     force, and the force is quadratic in the compression;
 *   - the shock absorber only ever resists an UPWARD wheel;
 *   - drag's second term needs speed AND surface AND not-mode-3;
 *   - and, on a flat plane of real collision triangles, the car FALLS,
 *     SETTLES and STAYS -- with the settling height checked against the
 *     spring/weight balance worked out on paper, not against whatever the
 *     code happened to produce.
 *
 * The collision grid is this file's, the same arrangement test_br_phys.c
 * uses and for the same reason: slice6_73.c owns the real
 * BrCollGridCellAcquire and linking it drags the menu tree in.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_carphys.h"

static int g_fail = 0;

static int g_checks;   /* a COUNT, not a bare OK -- see the note at main()'s tail */

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
        double da_ = (double)(a), db_ = (double)(b);                        \
        if (!(fabs(da_ - db_) <= (eps))) {                                  \
            printf("FAIL %s:%d: %s (%.9g) !~ %s (%.9g)\n", __FILE__,        \
                   __LINE__, #a, da_, #b, db_);                             \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

/* Compile-time assertions, the negative-array trick (C99, no _Static_assert
 * anywhere in this tree).  The whole module rests on body+0x78 being a
 * BrRbState, so that is the one worth proving at compile time. */
typedef int br_cp_assert_state_size[(sizeof(BrRbState) == 68) ? 1 : -1];
typedef int br_cp_assert_force_size[(sizeof(BrRbForce) >= 32) ? 1 : -1];

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

/* ================================================================== */
/* Stand-ins for the dependencies this test deliberately does NOT link  */
/*                                                                      */
/* slice3_42.o also carries the replay recorder and slice3_44.o calls    */
/* two engine stubs; pulling in slice1_02 / slice2_12 to satisfy them    */
/* drags the bit reader and the whole net path in.  None of the five is  */
/* on any path this file exercises -- the two that ARE called            */
/* (BrStub8B80_1p from BrRbBuildMatrix, BrGbiCall10075330 from           */
/* BrRbInitInertia) are stubs in the shipped build too, which            */
/* CONVENTIONS.md records for 0x10008B80.                                */
/* ================================================================== */

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

/* One big horizontal triangle pair at z == `h`, covering (-100..100)^2.
 * Winding is chosen so the normal points UP: the probe rejects anything with
 * n.z <= 0.2. */
static void BuildFlatGround(float h)
{
    static const float kQuad[2][3][2] = {
        { { -100.0f, -100.0f }, {  100.0f, -100.0f }, {  100.0f,  100.0f } },
        { { -100.0f, -100.0f }, {  100.0f,  100.0f }, { -100.0f,  100.0f } }
    };
    int t, k;

    memset(s_grid, 0, sizeof s_grid);
    for (t = 0; t < 2; ++t) {
        BrCollPlane *p = &s_grid[t];
        for (k = 0; k < 3; ++k) {
            s_verts[t * 3 + k].x = kQuad[t][k][0];
            s_verts[t * 3 + k].y = kQuad[t][k][1];
            s_verts[t * 3 + k].z = h;
        }
        p->nx = 0.0f; p->ny = 0.0f; p->nz = 1.0f;
        p->d  = -h;
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

static void TestSign(void)
{
    float nan = (float)strtod("nan", NULL);

    CHECK(BrCarPhysSign(0.0f)   == 0.0f);
    CHECK(BrCarPhysSign(-0.0f)  == 0.0f);
    CHECK(BrCarPhysSign(3.5f)   == 1.0f);
    CHECK(BrCarPhysSign(-3.5f)  == -1.0f);
    /* The x87 ZERO arm is `test ah,0x40`, i.e. EQUAL OR UNORDERED, so a NaN
     * classifies as zero and never reaches the second test.  Writing the
     * classifier as a plain if/else if would send NaN to -1 instead. */
    CHECK(BrCarPhysSign(nan)    == 0.0f);
}

static void TestSignDamp(void)
{
    BrRbState live, next;
    float     nan = (float)strtod("nan", NULL);

    memset(&live, 0, sizeof live);
    memset(&next, 0, sizeof next);

    live.vel.x = 2.0f;    next.vel.x = 1.0f;    /* same sign  -> take new  */
    live.vel.y = 2.0f;    next.vel.y = -1.0f;   /* sign flip  -> zero      */
    live.vel.z = 0.0f;    next.vel.z = 5.0f;    /* 0 vs +     -> zero      */
    live.angVel.x = -4.0f; next.angVel.x = -9.0f;
    live.angVel.y = -4.0f; next.angVel.y = 0.25f;
    live.angVel.z = nan;   next.angVel.z = 7.0f;

    BrCarPhysSignDamp(&live, &next);

    CHECK(live.vel.x == 1.0f);
    CHECK(live.vel.y == 0.0f);
    CHECK(live.vel.z == 0.0f);
    CHECK(live.angVel.x == -9.0f);
    CHECK(live.angVel.y == 0.0f);
    /* NaN classifies as ZERO, and next is +, so the signs differ and the
     * component is zeroed.  The NaN does NOT survive. */
    CHECK(live.angVel.z == 0.0f);
}

static void TestSpring(void)
{
    BrCarPhys car;
    uint8_t   touch = 0;
    float     k;

    BrCarPhysInit(&car, NULL);
    k = car.body.f1B8;
    CHECK(k > 0.0f);

    /* No ground: the probe's miss value is 100, so f1D8 lands at -100, which
     * is below the -0.3999 contact threshold. */
    car.wheel[0].f1D8 = -100.0f;
    car.wheel[1].f1D8 = -100.0f;
    car.wheel[2].f1D8 = -100.0f;
    car.wheel[3].f1D8 = -100.0f;
    car.wheel[0].f1B4 = 7.0f;
    BrCarPhysSpring(&car.body, &touch);
    CHECK(car.aListA[0].f.z == 0.0f);
    CHECK(car.aListA[3].f.z == 0.0f);
    /* the contact counter is reset by the no-contact arm */
    CHECK(car.wheel[0].f1B4 == 0.0f);

    /* Fully compressed: f1D8 == 0 gives v == 0.3 and force 0.3*0.3*k. */
    car.wheel[0].f1D8 = 0.0f;
    car.wheel[0].f1B4 = 0.0f;
    BrCarPhysSpring(&car.body, &touch);
    CHECK_NEAR(car.aListA[0].f.z, 0.3 * 0.3 * (double)k, 1.0);
    /* and the touchdown edge fired, because the counter went 0 -> 1 */
    CHECK(touch == BR_CP_TOUCHDOWN);

    /* Exactly at the travel limit: v == 0, no force, but still contact. */
    car.wheel[1].f1D8 = -0.3f;
    BrCarPhysSpring(&car.body, &touch);
    CHECK(car.aListA[1].f.z == 0.0f);
    CHECK(car.wheel[1].f1B4 != 0.0f);

    /* Half travel is a QUARTER of the force, not half -- sign(v)*v*v*k. */
    car.wheel[2].f1D8 = -0.15f;
    BrCarPhysSpring(&car.body, &touch);
    CHECK_NEAR(car.aListA[2].f.z, 0.15 * 0.15 * (double)k, 1.0);

    /* The spring never pulls: a wheel pushed ABOVE the ground (positive
     * f1D8) is clamped to zero compression before the rest offset, so it
     * saturates at full force rather than inverting. */
    car.wheel[3].f1D8 = 5.0f;
    BrCarPhysSpring(&car.body, &touch);
    CHECK_NEAR(car.aListA[3].f.z, 0.3 * 0.3 * (double)k, 1.0);

    /* x and y are cleared every call, for every node. */
    CHECK(car.aListA[0].f.x == 0.0f && car.aListA[0].f.y == 0.0f);
}

static void TestDamper(void)
{
    BrCarPhys car;
    BrRbState *pS;

    BrCarPhysInit(&car, NULL);
    car.body.pForces = &car.aListB[0];
    pS = BrCarPhysBodyState(&car.body);

    /* No contact -> no damping, whatever the velocity. */
    pS->vel.z = -10.0f;
    car.wheel[0].f1B4 = 0.0f;
    BrCarPhysDamper(&car.body);
    CHECK(car.aListB[0].f.z == 0.0f);

    /* Contact, wheel moving DOWN (v.z < 0) -> still nothing.  This is the
     * asymmetry: the shock only resists extension in one direction. */
    car.wheel[0].f1B4 = 3.0f;
    car.wheel[1].f1B4 = 3.0f;
    car.wheel[2].f1B4 = 3.0f;
    car.wheel[3].f1B4 = 3.0f;
    BrCarPhysDamper(&car.body);
    CHECK(car.aListB[0].f.z == 0.0f);

    /* Contact, wheel moving UP -> f1BC * v.z, and f1BC is negative. */
    pS->vel.z = 4.0f;
    BrCarPhysDamper(&car.body);
    CHECK(car.aListB[0].f.z < 0.0f);
    CHECK_NEAR(car.aListB[0].f.z, (double)car.body.f1BC * 4.0, 1e-3);
}

static void TestDrag(void)
{
    BrCarPhys   car;
    BrRbState  *pS;
    BrRbForce   node;
    BrGroundHit hit[4];
    int         i;

    BrCarPhysInit(&car, NULL);
    pS = BrCarPhysBodyState(&car.body);
    memset(&node, 0, sizeof node);
    memset(hit, 0, sizeof hit);

    /* Slow: only the linear term. */
    pS->vel.x = 1.0f; pS->vel.y = 0.0f; pS->vel.z = 0.0f;
    BrCarPhysDrag(&car.body, hit, &node, 0);
    CHECK_NEAR(node.f.x, (double)BR_CP_DRAG_K, 1e-4);

    /* Fast but on surface 1: still only the linear term. */
    pS->vel.x = 20.0f;
    for (i = 0; i < 4; ++i) hit[i].surface = 1;
    BrCarPhysDrag(&car.body, hit, &node, 0);
    CHECK_NEAR(node.f.x, 20.0 * (double)BR_CP_DRAG_K, 1e-3);

    /* Fast AND one wheel on surface 4: both terms. */
    hit[2].surface = BR_CP_DRAG_SURFACE;
    BrCarPhysDrag(&car.body, hit, &node, 0);
    CHECK_NEAR(node.f.x, 20.0 * ((double)BR_CP_DRAG_K + BR_CP_DRAG_K2), 1e-3);

    /* Mode 3 suppresses the second term even so. */
    BrCarPhysDrag(&car.body, hit, &node, 3);
    CHECK_NEAR(node.f.x, 20.0 * (double)BR_CP_DRAG_K, 1e-3);
}


/* ================================================================== */
/* 0x100651A0 -- the tyre pass                                         */
/* ================================================================== */

/* Put wheel `i` on flat ground with a contact record, so BrCarPhysTyre gets
 * past its two early exits. */
static void ArmWheel(BrCarPhys *pCar, int i, float nz)
{
    pCar->body.child[i]->f19C = 1.0f;
    pCar->body.child[i]->f1B4 = 1.0f;
    pCar->aHit[i].nx = 0.0f;
    pCar->aHit[i].ny = 0.0f;
    pCar->aHit[i].nz = nz;
    pCar->aHit[i].surface = 0;
}

static void TestTyre(void)
{
    BrCarPhys car;
    int       i;
    float     f0, f1;

    BrCarPhysInit(&car, NULL);
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);

    /* THE FINDING THIS TEST EXISTS FOR: the force is q == f1CC / f1C8 and
     * nothing else, so a wheel with no drive torque makes NO force.  If a
     * future pass "restores" a lateral term this fails, which is the point. */
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK(car.aWheelF[0].f.x == 0.0f);
    CHECK(car.aWheelF[0].f.y == 0.0f);
    CHECK(car.aWheelF[0].f.z == 0.0f);
    CHECK(car.fE7C == 0.0f);

    /* With drive torque there IS a force, and *pA takes half the RAW q. */
    car.body.child[0]->f1CC = 100.0f;      /* q = 100 / 0.5 = 200 */
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK_NEAR(car.fE7C, 100.0, 1e-3);     /* 0.5 * 200 */
    CHECK(car.aWheelF[0].f.x != 0.0f || car.aWheelF[0].f.y != 0.0f);

    /* WHEELSPIN.  The load is (mass + 4*wheelMass) * 2.943 * n.z * 3.5 ==
     * 10300.5 N here.  Demand one newton under it and one newton over it:
     * the force does not saturate, it COLLAPSES to a tenth of the load.
     * A saturating clamp would make these two nearly equal. */
    car.body.child[0]->f1CC = 10299.5f * 0.5f;   /* q = 10299.5 */
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    f0 = car.fE7C * 2.0f;                        /* recover q from *pA */
    CHECK_NEAR(f0, 10299.5, 1.0);
    {
        double mag = sqrt((double)(car.aWheelF[0].f.x * car.aWheelF[0].f.x
                                   + car.aWheelF[0].f.y * car.aWheelF[0].f.y
                                   + car.aWheelF[0].f.z * car.aWheelF[0].f.z));
        CHECK_NEAR(mag, 10299.5, 1.0);
    }
    car.body.child[0]->f1CC = 10301.5f * 0.5f;   /* q = 10301.5, over */
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    {
        double mag = sqrt((double)(car.aWheelF[0].f.x * car.aWheelF[0].f.x
                                   + car.aWheelF[0].f.y * car.aWheelF[0].f.y
                                   + car.aWheelF[0].f.z * car.aWheelF[0].f.z));
        CHECK_NEAR(mag, 0.1 * 10300.5, 2.0);
    }

    /* One wheel off the ground disables ALL FOUR tyres -- the free-spin arm
     * writes no force at all.  This is a whole-car property, not a per-wheel
     * one, and is easy to "tidy" into a per-wheel test. */
    car.body.child[3]->f1B4 = 0.0f;
    car.body.child[0]->f1CC = 100.0f;
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK(car.aWheelF[0].f.z == 0.0f);
    CHECK(car.fE7C == 0.0f);
    ArmWheel(&car, 3, 1.0f);

    /* Too steep: n.z below 0.7 bails out before anything, INCLUDING the
     * wheel's own spin update. */
    car.body.child[1]->f1C4 = 12.0f;
    car.aHit[1].nz = 0.69f;
    BrCarPhysTyre(&car, 1, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK(car.body.child[1]->f1C4 == 12.0f);
    /* THE BOUNDARY IS A DOUBLE, and that is observable: 0.7f is
     * 0.699999988079071 as a double, so the FLOAT 0.7 still bails.  Folding
     * the constant to a float literal would flip this. */
    car.aHit[1].nz = 0.7f;
    BrCarPhysTyre(&car, 1, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK(car.body.child[1]->f1C4 == 12.0f);
    car.aHit[1].nz = 0.70000005f;
    BrCarPhysTyre(&car, 1, &car.fE7C, &car.bE80, BR_PHYS_DT);
    CHECK(car.body.child[1]->f1C4 != 12.0f);

    /* The spin clamp is +-300 and the angle is folded into [0, 360]. */
    car.body.child[2]->f1CC = 1.0e9f;
    BrCarPhysTyre(&car, 2, &car.fE74, &car.bE78, BR_PHYS_DT);
    CHECK(fabs((double)car.body.child[2]->f1C4) <= 300.0);
    CHECK(car.body.child[2]->f1D4 >= 0.0f);
    CHECK(car.body.child[2]->f1D4 <= 360.0f);

    /* A non-finite angle is reset to zero rather than propagated. */
    car.body.child[2]->f1D4 = (float)strtod("inf", NULL);
    car.body.child[2]->f1CC = 0.0f;
    BrCarPhysTyre(&car, 2, &car.fE74, &car.bE78, BR_PHYS_DT);
    CHECK(car.body.child[2]->f1D4 == 0.0f);

    /* The grip table only applies when the gate BYTE is set, and the step
     * clears the front pair's gate immediately before calling them -- so the
     * gate is the whole reason the front pair never sees the table. */
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);
    car.body.child[0]->f1CC = 100.0f;
    car.bE80 = 0u;  car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    f0 = car.aWheelF[0].f.x;
    car.aWheelF[0].f.x = car.aWheelF[0].f.y = car.aWheelF[0].f.z = 0.0f;
    car.bE80 = 1u;  car.fE7C = 0.0f;
    BrCarPhysTyre(&car, 0, &car.fE7C, &car.bE80, BR_PHYS_DT);
    f1 = car.aWheelF[0].f.x;
    /* set A is a flat 0.9 for every compound, weather and surface */
    CHECK_NEAR(f1, 0.9 * (double)f0, 1e-3);
    /* ...and *pA takes the RAW q either way: the table is applied after. */
    CHECK_NEAR(car.fE7C, 100.0, 1e-3);
}

/* ================================================================== */
/* 0x100645A0 -- the drivetrain                                        */
/* ================================================================== */

static void TestDrive(void)
{
    BrCarPhys  car;
    BrRbState *pS;
    int        i;
    float      wy;

    BrCarPhysInit(&car, NULL);
    pS = BrCarPhysBodyState(&car.body);
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);

    /* THE FINDING THIS TEST EXISTS FOR: the drivetrain pins YAW and leaves
     * PITCH and ROLL alone.  Give the body all three and see which survive. */
    pS->angVel.x = 5.0f;
    pS->angVel.y = 7.0f;
    pS->angVel.z = 9.0f;
    pS->vel.x = 0.0f; pS->vel.y = 0.0f; pS->vel.z = 0.0f;
    BrRbBuildMatrix(&car.body.m, pS);
    BrCarPhysDrive(&car, BR_PHYS_DT);
    CHECK(pS->angVel.x == 5.0f);
    CHECK(pS->angVel.y == 7.0f);
    CHECK(pS->angVel.z != 9.0f);

    /* A wheel with no contact RECORD has its contact COUNT cleared, and with
     * a whole pair down the constraint does not run at all. */
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);
    car.body.child[2]->f19C = 0.0f;
    car.body.child[3]->f19C = 0.0f;
    pS->angVel.z = 9.0f;
    BrCarPhysDrive(&car, BR_PHYS_DT);
    CHECK(car.body.child[2]->f1B4 == 0.0f);
    CHECK(pS->angVel.z == 9.0f);

    /* Below the 8000 N hold-off the lateral velocity of pair A is ZEROED
     * outright, not left alone -- the `else` arm is a hard write, and the
     * yaw rate is then solved from the two axles.  8000 N over a 1000 kg
     * mass at 1/30 s is 0.2667 m/s, so 0.1 m/s is comfortably under. */
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);
    car.fE7C = 0.0f;  car.fE74 = 0.0f;
    car.bE80 = 0u;    car.bE78 = 0u;
    pS->angVel.x = 0.0f; pS->angVel.y = 0.0f; pS->angVel.z = 0.0f;
    pS->vel.x = 0.0f; pS->vel.y = 0.1f; pS->vel.z = 0.0f;
    BrRbBuildMatrix(&car.body.m, pS);
    BrCarPhysDrive(&car, BR_PHYS_DT);
    /* pair A's Y went to zero, pair B's did not, so the solved yaw is
     * (0 - 0.1) / (1.5 - -1.5) and the body's Y follows from it. */
    CHECK_NEAR(pS->angVel.z, -0.1 / 3.0, 1e-4);
    CHECK_NEAR(pS->vel.y, 0.0 - (-0.1 / 3.0) * 1.5, 1e-4);
    /* neither gate latched, because neither axle passed its hold-off */
    CHECK(car.bE80 == 0u);
    CHECK(car.bE78 == 0u);

    /* Above the hold-off pair A latches its gate and only PART of the
     * lateral velocity is removed -- the slip factor, not all of it. */
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);
    pS->angVel.x = 0.0f; pS->angVel.y = 0.0f; pS->angVel.z = 0.0f;
    pS->vel.x = 0.0f; pS->vel.y = 5.0f; pS->vel.z = 0.0f;
    BrRbBuildMatrix(&car.body.m, pS);
    car.bE80 = 0u; car.bE78 = 0u;
    BrCarPhysDrive(&car, BR_PHYS_DT);
    CHECK(car.bE80 == 1u);
    /* pair A is sliding: |vy| > 1 with |vx| <= 1 sets the 0x80 flag */
    CHECK(car.b209 == 0x80u);
    wy = pS->vel.y;
    CHECK(fabs((double)wy) < 5.0);          /* reduced ... */
    CHECK(fabs((double)wy) > 0.0);          /* ... but not to nothing */

    /* The chassis roll chases -8 * the retained side force at a fixed step,
     * so it can never move more than BR_CP_DRV_ROLL_STEP in one call. */
    {
        float was = car.body.f1D4;
        BrCarPhysDrive(&car, BR_PHYS_DT);
        CHECK(fabs((double)(car.body.f1D4 - was))
              <= (double)BR_CP_DRV_ROLL_STEP + 1e-4);
    }
}

/* The whole point: does a car fall, settle, and stay settled? */
static void TestSettle(void)
{
    BrCarPhys car;
    BrVec3    p;
    BrRbState *pS;
    int        i;
    float      zEq, zSettled, zEarlier;

    BuildFlatGround(0.0f);
    BrCarPhysInit(&car, NULL);
    BrCarPhysHoleReset();

    /* One metre, not three.  The OBB response (0x10067710) is now wired into
     * the substep loop, so a three-metre free-drop -- which sinks the body
     * ~0.9 m through the ground before the springs turn it -- is a hard
     * collision the box bounces, not a settle.  From one metre the fall is
     * gentle enough that the box (held clear of the ground at rest by its f1E8
     * offset) never fires, so the car eases onto its springs and the force
     * balance below is what the height measures.  See test_collresp.c
     * TestFlatSettleExact for the fuller note. */
    p.x = 10.0f; p.y = 10.0f; p.z = 1.0f;
    BrCarPhysPlace(&car, &p, 0.0f);

    pS = BrCarPhysBodyState(&car.body);

    /* One step must MOVE it, downwards, below the 1.0 m release height. */
    BrCarPhysStep(&car);
    CHECK(pS->pos.z < 1.0f);
    CHECK(pS->vel.z < 0.0f);

    for (i = 0; i < 400; ++i) {
        BrCarPhysStep(&car);
        if (i == 250) zEarlier = pS->pos.z;
    }
    zSettled = pS->pos.z;

    /* No NaN, no runaway. */
    CHECK(zSettled == zSettled);
    CHECK(fabs((double)zSettled) < 10.0);

    /* Settled means settled: the last 150 steps changed the height by less
     * than a millimetre. */
    CHECK_NEAR(zSettled, zEarlier, 1e-3);

    /* And the height is the one the force balance predicts, not whatever
     * came out.  Four springs each carry weight/4:
     *      4 * v^2 * k  ==  |gravity|      ->  v = sqrt(|g| / (4k))
     * the wheel sits at compression v below the 0.3 rest offset, i.e. the
     * probe distance is 0.3 - v, and the probe starts at the mount point,
     * which is BR_CP_WHEEL_Z below the body origin... except that the mount
     * Z is what the suspension WRITES, and the probe discards it.  So the
     * body origin ends up (0.3 - v) above the ground. */
    zEq = 0.3f - (float)sqrt((double)(-BR_CP_GRAVITY_BODY)
                             / (4.0 * (double)car.body.f1B8));
    CHECK_NEAR(zSettled, zEq, 0.02);

    /* Every hole was entered, so the report cannot claim more physics ran
     * than actually did.  BOTH are four substeps per frame now: BR_CP_HOLE_BOX
     * used to be counted ONCE per frame because it stood for the broad phase
     * (0x10066AD0, once at 0x10067CD1) as well as the response.  The broad
     * phase is ported, so the hole is 0x10067710 alone -- and that one is
     * inside the substep loop, at 0x10067D9B. */
    CHECK(g_aBrCarPhysHole[BR_CP_HOLE_CARCAR] == 4u * 401u);
    CHECK(g_aBrCarPhysHole[BR_CP_HOLE_BOX]    == 4u * 401u);
}

/* With no ground at all the car must fall freely, and at the acceleration the
 * constructor's own numbers imply -- gravity / mass, NOT 9.81. */
static void TestFreeFall(void)
{
    BrCarPhys car;
    BrVec3    p;
    BrRbState *pS;
    float      v0, v1, a;

    memset(s_count, 0, sizeof s_count);   /* no triangles anywhere */
    BrCarPhysInit(&car, NULL);
    p.x = 10.0f; p.y = 10.0f; p.z = 500.0f;
    BrCarPhysPlace(&car, &p, 0.0f);
    pS = BrCarPhysBodyState(&car.body);

    BrCarPhysStep(&car);
    v0 = pS->vel.z;
    BrCarPhysStep(&car);
    v1 = pS->vel.z;

    CHECK(v0 < 0.0f);
    CHECK(pS->pos.z < 500.0f);

    /* The step integrates velocity TWICE -- once with pass A (gravity) and
     * once with pass B (drag) -- so the observed acceleration is NOT g/m.
     * Both terms are predicted here rather than tolerated, which is what
     * makes this a test of the two-pass structure and not just of gravity:
     *
     *   dv = dt * (g/m)                          pass A
     *      + dt * (-110 * (v0 + dt*g/m)) / m     pass B, on the ALREADY
     *                                            once-integrated velocity
     *
     * Getting the order wrong -- drag on v0 instead of on the intermediate --
     * changes the third decimal, which this catches. */
    {
        double g   = (double)BR_CP_GRAVITY_BODY / (double)BR_CP_BODY_MASS;
        double dt  = (double)BR_PHYS_DT;
        double mid = (double)v0 + dt * g;
        double dv  = dt * g
                     + dt * ((double)BR_CP_DRAG_K * mid
                             / (double)BR_CP_BODY_MASS);
        a = (v1 - v0) / BR_PHYS_DT;
        CHECK_NEAR(a, dv / dt, 1e-3);
    }
    /* ... and it is strictly weaker than free gravity, because drag opposes. */
    CHECK(a > (float)((double)BR_CP_GRAVITY_BODY / (double)BR_CP_BODY_MASS));
}

/* ================================================================== */
/* THE ADAPTER'S FIELD ORDER                                           */
/*                                                                     */
/* BrCpIntegrateVelocity copies pBody->accel.x/y/z into tmp.accel[0/1/2] */
/* one at a time, and getting that order right is the entire reason the  */
/* adapter exists -- BrRbBodyFull and BrRbBody put `accel` at different  */
/* host offsets, so a cast would be the "two models of one object" bug.  */
/*                                                                      */
/* TestFreeFall above cannot see the order at all: it drops the car      */
/* straight down from rest, where accel is (0, 0, g) and swapping the    */
/* two zeroes changes nothing.  This drives it with a velocity that has  */
/* DIFFERENT x and y, so drag hands the integrator an acceleration whose */
/* three components are all different.                                   */
/*                                                                      */
/* The prediction is closed-form.  In free fall list A carries only the  */
/* gravity node, which is (0, 0, -15450.75), so pass A leaves x and y     */
/* alone; list B's drag node is -110 * v with no second term (surface 0   */
/* is not the surface-4 the second term needs).  So one frame multiplies  */
/* both x and y by exactly 1 + dt*(-110)/1000, and NOTHING mixes them.    */
/* Swap accel[0] and accel[1] and x is decayed by y's rate and y by x's,  */
/* which moves both by 0.08 m/s -- 300x the tolerances below.             */
/* ================================================================== */
static void TestFreeFallLateral(void)
{
    BrCarPhys  car;
    BrVec3     p;
    BrRbState *pS;
    double     f;

    memset(s_count, 0, sizeof s_count);   /* no triangles anywhere */
    BrCarPhysInit(&car, NULL);
    p.x = 10.0f; p.y = 10.0f; p.z = 500.0f;
    BrCarPhysPlace(&car, &p, 0.0f);
    pS = BrCarPhysBodyState(&car.body);

    /* x and y deliberately unequal, and non-zero: a zero component would
     * be masked by the sign-change damper, which zeroes any component
     * whose sign changed and so cannot distinguish "stayed 0" from
     * "moved off 0 and got damped back". */
    pS->vel.x = 30.0f;
    pS->vel.y =  7.0f;
    pS->vel.z =  0.0f;
    BrRbBuildMatrix(&car.body.m, pS);

    BrCarPhysStep(&car);

    f = 1.0 + (double)BR_PHYS_DT * (double)BR_CP_DRAG_K
              / (double)BR_CP_BODY_MASS;
    CHECK_NEAR(pS->vel.x, 30.0 * f, 1e-4);
    CHECK_NEAR(pS->vel.y,  7.0 * f, 1e-4);
    /* Same decay factor on both axes -- stated as its own check because it
     * is the property that survives if the drag constant is ever re-read. */
    CHECK_NEAR((double)pS->vel.x / 30.0, (double)pS->vel.y / 7.0, 1e-6);
    /* and the two axes did not trade places */
    CHECK(pS->vel.x > pS->vel.y);
}

/* ================================================================== */
/* 0x1006543F -- the weather row, and its SIXTEEN-BIT clamp            */
/*                                                                     */
/* BrCpWeatherRow is `(int16_t)(weather - 1)` clamped into [0, 2], and  */
/* the truncation is the interesting half: 0x10001 answers row 0, not   */
/* row 0x10000.  Nothing in this suite ever set g_brCarPhysWeather, so   */
/* the whole function could return a constant 0 unnoticed.               */
/*                                                                       */
/* It is observed through BrCpDrvSlip, which indexes three tables at      */
/* row*8 + surface.  For surface 0 and compound 0 rows 0 and 1 hold the   */
/* SAME numbers, so weather 1 and 2 are indistinguishable by design --    */
/* row 2 is the only one that reads differently, which is why every       */
/* comparison below is against weather 3.                                */
/* ================================================================== */

/* The lateral velocity left after one drivetrain call, with the axle
 * demand pushed above the table's cap so the slip fraction is
 *      (T3 / T2) * T1 * 20 * 1.5
 * and hence directly proportional to T1 * T3 / T2 for this weather row.
 * The speed is 30 m/s, above the 27 m/s floor, so the slow-speed
 * override does not flatten the two rows onto the same value. */
static float LateralAfterDrive(int32_t weather)
{
    BrCarPhys  car;
    BrRbState *pS;
    int        i;

    BrCarPhysInit(&car, NULL);
    g_brCarPhysWeather = weather;
    pS = BrCarPhysBodyState(&car.body);
    for (i = 0; i < 4; ++i) ArmWheel(&car, i, 1.0f);
    pS->angVel.x = 0.0f; pS->angVel.y = 0.0f; pS->angVel.z = 0.0f;
    pS->vel.x = 0.0f; pS->vel.y = 30.0f; pS->vel.z = 0.0f;
    BrRbBuildMatrix(&car.body.m, pS);
    car.bE80 = 0u; car.bE78 = 0u;
    car.fE7C = 0.0f; car.fE74 = 0.0f;
    BrCarPhysDrive(&car, BR_PHYS_DT);
    return pS->vel.y;
}

static void TestWeatherRow(void)
{
    float  y0, y1, y2, y3, y4, yHi, yLo, yNeg;
    double f0, f2;
    int32_t save = g_brCarPhysWeather;

    /* The three table entries the two rows differ in, as literals read out
     * of 0x100B4C30 / 0x100B4E70 / 0x100B4F90 -- row r, surface 0,
     * compound 0 is index 8*r for T2 and T3 and 8*r + 24*0 for T1. */
    CHECK(g_pBrCarPhysDrvT1[0]  == 0.0820000023f);
    CHECK(g_pBrCarPhysDrvT2[0]  == 90000.0f);
    CHECK(g_aBrCarPhysDrvT3[0]  == 3000.0f);
    CHECK(g_pBrCarPhysDrvT1[16] == 0.0649999976f);
    CHECK(g_pBrCarPhysDrvT2[16] == 120000.0f);
    CHECK(g_aBrCarPhysDrvT3[16] == 3000.0f);

    y0   = LateralAfterDrive(0);
    y1   = LateralAfterDrive(1);
    y2   = LateralAfterDrive(2);
    y3   = LateralAfterDrive(3);
    y4   = LateralAfterDrive(4);
    yHi  = LateralAfterDrive(0x00010003);
    yLo  = LateralAfterDrive(0x00010001);
    yNeg = LateralAfterDrive(-1);

    /* The row is READ.  Without this the function can return a constant. */
    CHECK(y3 != y1);
    /* Row 2's grip is lower, so it removes LESS lateral velocity. */
    CHECK(y3 > y1);

    /* And by exactly the ratio of the two rows' T1 * T3 / T2.  The rest of
     * the drivetrain is linear in the slip fraction, so the velocity
     * REMOVED scales with it. */
    f0 = 0.0820000023 * (3000.0 / 90000.0);
    f2 = 0.0649999976 * (3000.0 / 120000.0);
    CHECK_NEAR((30.0 - (double)y3) / (30.0 - (double)y1), f2 / f0, 1e-4);

    /* THE 16-BIT CLAMP.  0x10003 is 3 in sixteen bits, so it must answer
     * row 2 exactly as 3 does -- and 0x10001 must answer row 0, not some
     * row 0x10000 that does not exist. */
    CHECK(yHi == y3);
    CHECK(yLo == y1);
    /* Out of range in both directions folds to row 0: weather 4 gives
     * w == 3 (> 2) and weather 0 gives w == -1 (< 0). */
    CHECK(y4   == y1);
    CHECK(y0   == y1);
    CHECK(yNeg == y1);
    /* Rows 0 and 1 hold identical numbers here, so weather 2 must NOT
     * differ -- asserted so that "row 2 is special" cannot be mistaken for
     * "any non-1 weather is special". */
    CHECK(y2 == y1);

    g_brCarPhysWeather = save;
}

int main(void)
{
    TestSign();
    TestSignDamp();
    TestSpring();
    TestDamper();
    TestDrag();
    TestTyre();
    TestDrive();
    TestWeatherRow();
    TestSettle();
    TestFreeFall();
    TestFreeFallLateral();

    if (g_fail == 0) {
        /* A count, not a bare OK. tools/regress.sh calls an uncounted OK
         * UNPARSEABLE on purpose: a suite that prints OK without running
         * anything looks exactly like one that ran everything. */
        printf("test_carphys: %d checks, 0 failures\n", g_checks);
        return 0;
    }
    printf("test_carphys: %d failure(s)\n", g_fail);
    return 1;
}
