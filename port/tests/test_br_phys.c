/* test_br_phys.c -- BEHAVIOUR tests for the ground probe and the wheel drop.
 *
 * Nothing here asserts that the code ran or counts how much work it did.
 * Every check is a geometric property that can be worked out on paper from
 * the plane equation, plus the four acceptance windows read out of the
 * binary, plus the two clamps.  Where the original does something surprising
 * -- accepting ground ABOVE the probe point, ignoring steep faces -- the
 * surprise is what is asserted, so a future pass that "fixes" it fails here
 * instead of silently changing the handling model.
 *
 * The collision grid is supplied by this file.  slice6_73.c owns the real
 * BrCollGridCellAcquire, but linking it drags the whole menu/phase tree in,
 * and its cell SELECTION is already covered by test_slice6_73.  The stand-in
 * here always returns cell 0 and RECORDS ITS ARGUMENTS, which is what lets
 * the grid key be pinned to the WORLD point rather than merely assumed.
 * This file used to assert the opposite -- that the key is the wheel's
 * body-local mount offset -- on the strength of a stack-offset misreading;
 * br_phys.h carries the adjudication and the frame walk that settles it.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "br_phys.h"

static int g_fail = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                               \
    do {                                                                    \
        double da_ = (double)(a), db_ = (double)(b);                        \
        if (!(fabs(da_ - db_) <= (eps))) {                                  \
            printf("FAIL %s:%d: %s (%.9g) !~ %s (%.9g)\n", __FILE__,        \
                   __LINE__, #a, da_, #b, db_);                             \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

/* ================================================================== */
/* The grid this file owns                                             */
/* ================================================================== */

static BrCollPlane s_grid[4 * BR_COLL_CELL_PLANES];
static uint16_t    s_count[4];

BrCollPlane        *g_pBrCollGrid      = s_grid;
const uint16_t     *g_pBrCollGridCount = s_count;

/* Stand-in for 0x1006F720.  Records what it was asked for. */
static float s_lastKeyX, s_lastKeyY;
static int   s_nAcquire;

short BrCollGridCellAcquire(float x, float y)
{
    s_lastKeyX = x;
    s_lastKeyY = y;
    ++s_nAcquire;
    return 0;
}

/* ================================================================== */
/* Building test geometry                                              */
/* ================================================================== */

/* Vertex pool; the planes hold pointers into it, as the real grid does. */
static BrVec3 s_verts[64];
static int    s_nVerts;

static BrVec3 *vtx(float x, float y, float z)
{
    BrVec3 *p = &s_verts[s_nVerts++];
    p->x = x; p->y = y; p->z = z;
    return p;
}

static void grid_reset(void)
{
    memset(s_grid, 0, sizeof(s_grid));
    memset(s_count, 0, sizeof(s_count));
    s_nVerts   = 0;
    s_nAcquire = 0;
}

/* Add one triangle to cell 0, with the plane built exactly the way
 * 0x1006F720 builds it: n = normalise((v1-v0) x (v2-v0)), d = -dot(n, v0). */
static void add_tri(BrVec3 *a, BrVec3 *b, BrVec3 *c, unsigned char flags)
{
    BrCollPlane *p = &s_grid[s_count[0]];
    float ax = b->x - a->x, ay = b->y - a->y, az = b->z - a->z;
    float bx = c->x - a->x, by = c->y - a->y, bz = c->z - a->z;
    float nx = ay * bz - az * by;
    float ny = az * bx - ax * bz;
    float nz = ax * by - ay * bx;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);

    nx /= len; ny /= len; nz /= len;

    p->nx = nx; p->ny = ny; p->nz = nz;
    p->d  = -(nx * a->x + ny * a->y + nz * a->z);
    p->pV0 = a; p->pV1 = b; p->pV2 = c;
    p->tri   = (uint16_t)(s_count[0] + 1);
    p->flags = flags;
    ++s_count[0];
}

/* A big horizontal quad at height z, wound so the normal points +Z. */
static void add_flat(float z, float half, unsigned char flags)
{
    BrVec3 *a = vtx(-half, -half, z);
    BrVec3 *b = vtx( half, -half, z);
    BrVec3 *c = vtx( half,  half, z);
    BrVec3 *d = vtx(-half,  half, z);
    add_tri(a, b, c, flags);
    add_tri(a, c, d, flags);
}

/* A quad through the origin tilted about the Y axis by `slope` = dz/dx, wound
 * so the normal has a positive Z.  The surface is z = slope * x. */
static void add_slope(float slope, float half)
{
    BrVec3 *a = vtx(-half, -half, -slope * half);
    BrVec3 *b = vtx( half, -half,  slope * half);
    BrVec3 *c = vtx( half,  half,  slope * half);
    BrVec3 *d = vtx(-half,  half, -slope * half);
    add_tri(a, b, c, 0);
    add_tri(a, c, d, 0);
}

/* ================================================================== */
/* 1. The straight-down probe over flat ground                         */
/* ================================================================== */

static float probe(float x, float y, float z)
{
    BrVec3 p; p.x = x; p.y = y; p.z = z;
    return BrGroundProbeZ(&p);
}

static void test_flat(void)
{
    grid_reset();
    add_flat(0.0f, 10.0f, 3);

    /* The result IS the drop to the surface, exactly. */
    CHECK_NEAR(probe(0.0f, 0.0f, 1.5f), 1.5, 1e-6);
    CHECK_NEAR(probe(3.0f, -4.0f, 0.5f), 0.5, 1e-6);
    CHECK_NEAR(probe(0.0f, 0.0f, 0.0f), 0.0, 1e-6);

    /* NOT a downward-only ray.  Ground ABOVE the point is found and reported
     * as a negative distance, down to the -2 window edge.  Anything that
     * treats the return as "height above ground" and assumes it is positive
     * is wrong about this function. */
    CHECK_NEAR(probe(0.0f, 0.0f, -1.0f), -1.0, 1e-6);
    CHECK_NEAR(probe(0.0f, 0.0f, -1.9f), -1.9, 1e-5);

    /* Outside the +-2 window in either direction there is no ground at all,
     * and the miss value is a DISTANCE, not a sentinel. */
    CHECK_NEAR(probe(0.0f, 0.0f, 2.5f), BR_PHYS_PROBE_MISS, 1e-6);
    CHECK_NEAR(probe(0.0f, 0.0f, -2.5f), BR_PHYS_PROBE_MISS, 1e-6);

    /* Off the edge of the geometry: the 2D containment test rejects it. */
    CHECK_NEAR(probe(50.0f, 0.0f, 1.0f), BR_PHYS_PROBE_MISS, 1e-6);

    /* The window is on the PLANE distance as well as on t, and for a
     * vertical ray onto a horizontal plane those are the same number, so the
     * boundary lands at exactly 2.  Just inside is a hit. */
    CHECK_NEAR(probe(0.0f, 0.0f, 1.999f), 1.999, 1e-4);
}

/* ================================================================== */
/* 2. Slopes -- the value, and which faces count as ground             */
/* ================================================================== */

static void test_slope(void)
{
    const float slope = 0.5f;          /* 26.57 degrees */
    float px, expect;

    grid_reset();
    add_slope(slope, 10.0f);

    /* The probe is vertical, so over a plane z = slope*x the answer is the
     * VERTICAL drop, pz - slope*px -- not the perpendicular distance.  The
     * two differ by 1/cos(angle) = 1.118 here, which is why this is worth
     * asserting rather than eyeballing. */
    for (px = -1.5f; px <= 1.5f; px += 0.75f) {
        expect = 1.0f - slope * px;
        CHECK_NEAR(probe(px, 0.5f, 1.0f), expect, 1e-4);
    }

    /* And the perpendicular distance is strictly smaller, which is the
     * identity that distinguishes the two readings. */
    CHECK(probe(1.0f, 0.0f, 1.0f) > 0.0f);
    CHECK_NEAR(probe(1.0f, 0.0f, 1.0f) * (1.0f / sqrtf(1.0f + slope * slope)),
               (1.0 - slope * 1.0) / sqrt(1.0 + (double)slope * slope), 1e-4);

    /* A face steeper than the n.z > 0.2 gate is not ground.  n.z for
     * z = slope*x is 1/sqrt(1+slope^2); slope = 5 gives 0.196, just under. */
    grid_reset();
    add_slope(5.0f, 10.0f);
    CHECK(1.0f / sqrtf(1.0f + 25.0f) < (float)BR_PHYS_PROBE_MINNZ);
    CHECK_NEAR(probe(0.0f, 0.5f, 0.5f), BR_PHYS_PROBE_MISS, 1e-6);

    /* slope = 4.8 gives n.z = 0.204, just over -- so the gate is real and
     * this is the boundary, not a coincidence of the geometry. */
    grid_reset();
    add_slope(4.8f, 10.0f);
    CHECK(1.0f / sqrtf(1.0f + 4.8f * 4.8f) > (float)BR_PHYS_PROBE_MINNZ);
    CHECK_NEAR(probe(0.0f, 0.5f, 0.5f), 0.5, 1e-4);
}

/* ================================================================== */
/* 3. Nearest wins, and a downward-facing face is never ground         */
/* ================================================================== */

static void test_nearest(void)
{
    grid_reset();
    add_flat(0.0f, 10.0f, 1);
    add_flat(0.5f, 10.0f, 2);

    /* Two candidates at 1.2 and 0.7; the closer one wins regardless of the
     * order they sit in the cell. */
    CHECK_NEAR(probe(0.0f, 0.0f, 1.2f), 0.7, 1e-5);

    /* Wound the other way, the normal points down and n.z = -1 fails the
     * gate, so a ceiling is invisible even though the ray hits it. */
    grid_reset();
    {
        BrVec3 *a = vtx(-10.0f, -10.0f, 0.0f);
        BrVec3 *b = vtx(-10.0f,  10.0f, 0.0f);
        BrVec3 *c = vtx( 10.0f,  10.0f, 0.0f);
        add_tri(a, b, c, 0);
        CHECK(s_grid[0].nz < 0.0f);
    }
    CHECK_NEAR(probe(-1.0f, 1.0f, 1.0f), BR_PHYS_PROBE_MISS, 1e-6);
}

/* ================================================================== */
/* 4. The wheel probe and the suspension clamp                         */
/* ================================================================== */

/* A chassis with four wheels.  The mount points are the only geometry that
 * matters to this pass. */
typedef struct Rig {
    BrRbBodyFull body;
    BrRbBodyFull wheel[4];
} Rig;

static const float kMountX[4] = { -0.8f,  0.8f, -0.8f,  0.8f };
static const float kMountY[4] = {  1.4f,  1.4f, -1.4f, -1.4f };

static void rig_init(Rig *pR, float z)
{
    int i;
    memset(pR, 0, sizeof(*pR));
    for (i = 0; i < 4; ++i) {
        pR->body.child[i]   = &pR->wheel[i];
        pR->wheel[i].f78.x  = kMountX[i];
        pR->wheel[i].f78.y  = kMountY[i];
        pR->wheel[i].f78.z  = 0.0f;
    }
    /* identity rotation, translation in the fourth ROW (row-vector) */
    pR->body.m.m[0][0] = 1.0f;
    pR->body.m.m[1][1] = 1.0f;
    pR->body.m.m[2][2] = 1.0f;
    pR->body.m.m[3][3] = 1.0f;
    pR->body.m.m[3][0] = 0.0f;
    pR->body.m.m[3][1] = 0.0f;
    pR->body.m.m[3][2] = z;
}

/* Roll the chassis about its own X axis by `a`, keeping the translation. */
static void rig_roll(Rig *pR, float a)
{
    float c = cosf(a), s = sinf(a);
    pR->body.m.m[1][1] =  c; pR->body.m.m[1][2] = s;
    pR->body.m.m[2][1] = -s; pR->body.m.m[2][2] = c;
}

static void test_wheel_probe(void)
{
    Rig  r;
    BrGroundHit hit;
    float t;

    grid_reset();
    add_flat(0.0f, 20.0f, 5);

    /* Level chassis at height h: every wheel is h above the ground and the
     * probe says so.  This is the "dropped on flat ground" case. */
    rig_init(&r, 0.75f);
    memset(&hit, 0, sizeof(hit));
    t = BrWheelGroundProbe(&r.body, &r.wheel[0], &hit);
    CHECK_NEAR(t, 0.75, 1e-5);
    CHECK(r.wheel[0].f19C != 0.0f);           /* contact flag set */
    CHECK(hit.pPlane != NULL);
    CHECK(hit.surface == 5);
    CHECK_NEAR(hit.nz, 1.0, 1e-6);            /* flat ground -> up normal */
    CHECK_NEAR(hit.nx, 0.0, 1e-6);
    CHECK_NEAR(hit.ny, 0.0, 1e-6);
    CHECK_NEAR(hit.d, 0.0, 1e-6);

    /* Too high to see the ground: the contact flag is CLEARED and the record
     * is left holding the previous frame's values, which is exactly what the
     * original does. */
    rig_init(&r, 5.0f);
    t = BrWheelGroundProbe(&r.body, &r.wheel[0], &hit);
    CHECK_NEAR(t, BR_PHYS_PROBE_MISS, 1e-6);
    CHECK(r.wheel[0].f19C == 0.0f);
    CHECK(hit.pPlane != NULL);                /* NOT cleared -- stale */

    /* Rolled chassis.  Two things change together and the test pins both:
     * the mount point RISES (row 1 of the matrix tilts, so the world Z of a
     * mount at local y is y*sin(a) + h), and the ray is the CAR's down axis
     * rather than world -Z, which divides the answer by cos(a).
     * A world-vertical probe would return y*sin(a) + h with no division; a
     * probe that forgot the transform would return h.  Both are excluded. */
    rig_init(&r, 0.75f);
    rig_roll(&r, 0.5f);                        /* 28.6 degrees */
    t = BrWheelGroundProbe(&r.body, &r.wheel[0], NULL);
    CHECK_NEAR(t, ((double)kMountY[0] * sin(0.5) + 0.75) / cos(0.5), 1e-4);
    CHECK(fabs((double)t - 0.75) > 0.5);       /* not the untransformed h  */
    CHECK(fabs((double)t
               - ((double)kMountY[0] * sin(0.5) + 0.75)) > 0.1); /* nor vertical */

    /* THE GRID KEY IS THE WORLD POINT, and this is the assertion that pins it.
     * Two passes read 0x10068070 as keying the grid on the wheel's BODY-LOCAL
     * mount offset -- see the adjudication in br_phys.h -- and this test used
     * to assert exactly that.  It was wrong: the reload the claim rested on
     * happens while esp is still 0xC below the frame the spill used, so the
     * two identical displacements name different slots.
     *
     * The car is translated 900 x, -400 y so that the local and world keys
     * cannot be confused for one another, and BOTH are checked: the key must
     * equal the transformed mount point and must NOT equal the raw mount. */
    rig_init(&r, 0.75f);
    r.body.m.m[3][0] = 900.0f;
    r.body.m.m[3][1] = -400.0f;
    s_nAcquire = 0;
    (void)BrWheelGroundProbe(&r.body, &r.wheel[1], NULL);
    CHECK(s_nAcquire == 1);
    /* 1e-4, not 1e-6: these are floats at magnitude 900, where one ulp is
     * already 6e-5.  A tighter bound would be testing float precision, not
     * the grid key. */
    CHECK_NEAR(s_lastKeyX, 900.0 + (double)kMountX[1], 1e-4);
    CHECK_NEAR(s_lastKeyY, -400.0 + (double)kMountY[1], 1e-4);
    CHECK(fabs((double)s_lastKeyX - (double)kMountX[1]) > 1.0);
    CHECK(fabs((double)s_lastKeyY - (double)kMountY[1]) > 1.0);

    /* And the key is the mount point ROTATED as well as translated, not the
     * body's own origin: under a roll the mount's world y and z move but the
     * key is still the transformed x/y, so a yaw is what separates "the body
     * origin" from "the transformed mount".  Yaw 90 degrees sends local
     * (x, y) to world (-y, x) under the row-vector convention. */
    rig_init(&r, 0.75f);
    r.body.m.m[0][0] =  0.0f; r.body.m.m[0][1] = 1.0f;
    r.body.m.m[1][0] = -1.0f; r.body.m.m[1][1] = 0.0f;
    r.body.m.m[3][0] = 50.0f;
    r.body.m.m[3][1] = 20.0f;
    s_nAcquire = 0;
    (void)BrWheelGroundProbe(&r.body, &r.wheel[1], NULL);
    CHECK(s_nAcquire == 1);
    CHECK_NEAR(s_lastKeyX, 50.0 - (double)kMountY[1], 1e-5);
    CHECK_NEAR(s_lastKeyY, 20.0 + (double)kMountX[1], 1e-5);
    /* not the body origin, and not the untransformed mount */
    CHECK(fabs((double)s_lastKeyX - 50.0) > 1.0);

    /* The straight-down probe keys on the world point too -- the two probes
     * agree, which is the whole reason the old reading looked anomalous. */
    s_nAcquire = 0;
    (void)probe(123.0f, -45.0f, 1.0f);
    CHECK_NEAR(s_lastKeyX, 123.0, 1e-6);
    CHECK_NEAR(s_lastKeyY, -45.0, 1e-6);

    /* g_brPhysWheelGridWorldKey is vestigial: setting it changes nothing.
     * If a future pass revives it as a switch, this fails. */
    {
        float keyX, keyY;
        rig_init(&r, 0.75f);
        r.body.m.m[3][0] = 900.0f;
        r.body.m.m[3][1] = -400.0f;
        g_brPhysWheelGridWorldKey = 0;
        (void)BrWheelGroundProbe(&r.body, &r.wheel[2], NULL);
        keyX = s_lastKeyX; keyY = s_lastKeyY;
        g_brPhysWheelGridWorldKey = 1;
        (void)BrWheelGroundProbe(&r.body, &r.wheel[2], NULL);
        CHECK(s_lastKeyX == keyX);
        CHECK(s_lastKeyY == keyY);
        g_brPhysWheelGridWorldKey = 0;
    }
}

static void test_suspension(void)
{
    Rig r;
    int i;

    grid_reset();
    add_flat(0.0f, 20.0f, 0);

    /* Inside the 0.4 of travel, the wheel's local Z tracks the ride height
     * one for one -- that IS the suspension.  f1D8 keeps the raw value. */
    rig_init(&r, 0.25f);
    BrWheelSuspensionSetZ(&r.body);
    for (i = 0; i < 4; ++i) {
        CHECK_NEAR(r.wheel[i].f1D8, -0.25, 1e-5);
        CHECK_NEAR(r.wheel[i].f78.z, -0.25, 1e-5);
    }

    /* Exactly at full droop. */
    rig_init(&r, 0.4f);
    BrWheelSuspensionSetZ(&r.body);
    CHECK_NEAR(r.wheel[0].f78.z, BR_PHYS_SUSP_MIN, 1e-6);

    /* Past it, the clamp holds the wheel at -0.4 while f1D8 keeps running --
     * so f1D8, not f78.z, is what the spring force is computed from. */
    rig_init(&r, 1.0f);
    BrWheelSuspensionSetZ(&r.body);
    for (i = 0; i < 4; ++i) {
        CHECK_NEAR(r.wheel[i].f1D8, -1.0, 1e-5);
        CHECK_NEAR(r.wheel[i].f78.z, BR_PHYS_SUSP_MIN, 1e-6);
    }

    /* No ground in range: the probe misses, f1D8 becomes -100 and the wheel
     * sits at full droop.  A miss is not distinguishable at f78.z from a very
     * long drop, which is why the contact flag exists. */
    rig_init(&r, 5.0f);
    BrWheelSuspensionSetZ(&r.body);
    for (i = 0; i < 4; ++i) {
        CHECK_NEAR(r.wheel[i].f1D8, -BR_PHYS_PROBE_MISS, 1e-6);
        CHECK_NEAR(r.wheel[i].f78.z, BR_PHYS_SUSP_MIN, 1e-6);
        CHECK(r.wheel[i].f19C == 0.0f);
    }

    /* Chassis BELOW the surface: the probe returns a negative distance, the
     * negation makes it positive, and the upper clamp pins the wheel at 0.
     * The suspension cannot push the wheel up through the body. */
    rig_init(&r, -0.3f);
    BrWheelSuspensionSetZ(&r.body);
    for (i = 0; i < 4; ++i) {
        CHECK_NEAR(r.wheel[i].f1D8, 0.3, 1e-5);
        CHECK_NEAR(r.wheel[i].f78.z, 0.0, 1e-6);
    }

    /* All four wheels are visited unconditionally, and each gets its own
     * answer: on a slope the two ends of the car differ by slope * wheelbase.
     * The mount X's are -0.8 and +0.8, so wheels 0/2 sit higher than 1/3. */
    grid_reset();
    add_slope(0.25f, 20.0f);
    rig_init(&r, 0.5f);
    BrWheelSuspensionSetZ(&r.body);
    CHECK_NEAR(r.wheel[0].f1D8, -(0.5 - 0.25 * -0.8), 1e-4);
    CHECK_NEAR(r.wheel[1].f1D8, -(0.5 - 0.25 *  0.8), 1e-4);
    CHECK_NEAR(r.wheel[2].f1D8, r.wheel[0].f1D8, 1e-6);
    CHECK_NEAR(r.wheel[3].f1D8, r.wheel[1].f1D8, 1e-6);
}

/* ================================================================== */
/* 5. The two constants that are facts about the build                 */
/* ================================================================== */

static void test_constants(void)
{
    /* 0x3D088889, the immediate pushed at every integrator call site in the
     * per-frame step.  1/30 s as a float, exactly. */
    CHECK(BR_PHYS_DT == (float)(1.0f / 30.0f));
    /* and it is NOT 1/30 as a double -- the step is a float constant. */
    CHECK((double)BR_PHYS_DT != 1.0 / 30.0);

    /* 0xBECCCCCD, and the suspension travel is 0.4, not 0.5. */
    CHECK_NEAR(BR_PHYS_SUSP_MIN, -0.4, 1e-7);
}

/* ================================================================== */

int main(void)
{
    test_flat();
    test_slope();
    test_nearest();
    test_wheel_probe();
    test_suspension();
    test_constants();

    if (g_fail != 0) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("br_phys: all checks passed\n");
    return 0;
}
