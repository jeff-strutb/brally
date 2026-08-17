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

    /* THE TRANSLATION, and it is the CORRECTION br_collresp.h files.  This
     * used to assert (0, -32, 0) -- the "frame overlap" reading, in which
     * 0x1006DDD0 takes its translation from the SCALE argument and the box
     * matrix therefore has no translation at all.  0x1006DE2C's `[esp+0x24]`
     * is read with esp at R-0x20, i.e. arg1, so the translation comes from
     * the BODY MATRIX: -(1, 2, 3) run through pOut's transpose, which for
     * this diagonal pOut is (-1*2, -2*4, -3*8) == (-2, -8, -24).
     *
     * The two readings are not equally plausible and this test says which:
     * only the second one makes a world point at the car's own position map
     * to the box's centre, which is what the +-0.5 classify then means. */
    CHECK_NEAR(pOut->m[3][0],  -2.0f, 1e-6);
    CHECK_NEAR(pOut->m[3][1],  -8.0f, 1e-6);
    CHECK_NEAR(pOut->m[3][2], -24.0f, 1e-6);

    /* The car's own position lands at the box origin. */
    {
        BrVec3 at, out;
        at.x = 1.0f; at.y = 2.0f; at.z = 3.0f;
        BrMat4TransformPoint(&out, pOut, &at);
        CHECK_NEAR(out.x, 0.0f, 1e-5);
        CHECK_NEAR(out.y, 0.0f, 1e-5);
        CHECK_NEAR(out.z, 0.0f, 1e-5);
    }

    /* ...and the scale slots are untouched by the build. */
    CHECK(f.a[0] == 2.0f && f.a[1] == 4.0f && f.a[2] == 8.0f);

    (void)j;
}

/* ================================================================== */
/* The box, and where it comes from                                    */
/* ================================================================== */

/* This used to assert (0, 0, 2, 0) as "the constructor's own numbers, from
 * Glide 0x1005BD40 / 42 / 48 / 4E".  Those four stores are car+0x1DC -- the
 * initial BrRbState -- and not body+0x1DC == car+0x340; br_cardata.h has the
 * adjudication.  The box is written by 0x1006FD90 out of the .rca, and the
 * test is now what it should always have been: the loaded value, checked
 * against the file, with the no-file path measured rather than assumed. */
static void TestBoxFromCarData(void)
{
    BrCarPhys        car;
    const BrCarData *pD = BrCarDataDefault();

    BuildSlope(0.0f, 0.0, 0);
    BrCarPhysInit(&car, NULL);

    if (pD == NULL) {
        /* Asset policy: no CARS/ is a SKIP with a reason, and it must look
         * different from a pass.  The box is then unwritten, which is
         * exactly what BrCollRespBoxDegenerate exists to report. */
        printf("  SKIP: no CARS/ directory found -- box left unwritten\n");
        CHECK(BrCollRespBoxDegenerate(car.f1DC, car.f1E0, car.f1E4) != 0);
        return;
    }

    /* Car 0 is "ce" (0x100B7D00[0]), whose +0x94 name is "TYPE-CE". */
    CHECK(strcmp(pD->szName, "TYPE-CE") == 0);

    /* +0xC8..+0xD4, straight out of the file's little-endian half. */
    CHECK(car.f1DC == pD->boxX);
    CHECK(car.f1E0 == pD->boxY);
    CHECK(car.f1E4 == pD->boxZ);
    CHECK(car.f1E8 == pD->boxOffZ);

    CHECK_NEAR(car.f1DC, 3.5f, 1e-6);
    CHECK_NEAR(car.f1E0, 2.0f, 1e-6);
    CHECK_NEAR(car.f1E4, 0.8f, 1e-6);
    CHECK_NEAR(car.f1E8, 0.7f, 1e-6);

    /* ...so 0x10067C30's three reciprocals are finite and the OBB chain can
     * run at all.  That is the whole point of the loader. */
    CHECK(BrCollRespBoxDegenerate(car.f1DC, car.f1E0, car.f1E4) == 0);

    /* And the measurement still detects a box that is NOT real: two zero
     * extents give infinite reciprocals, which is the state every earlier
     * run of this suite was in. */
    CHECK(BrCollRespBoxDegenerate(0.0f, 0.0f, 2.0f) != 0);

    printf("  box from %s/%s.rca (\"%s\"): "
           "(%.4f, %.4f, %.4f) offset %.4f\n",
           BrCarDataDir(), BrCarDataName(0), pD->szName,
           (double)car.f1DC, (double)car.f1E0, (double)car.f1E4,
           (double)car.f1E8);
}

/* Every shipped car's box, read straight off the disc.  Not an assertion
 * about particular numbers -- that would encode this disc -- but about the
 * SHAPE: sixteen files, sixteen finite non-degenerate boxes. */
static void TestAllCarBoxes(void)
{
    BrCarData d;
    int       i, n = 0;

    if (BrCarDataDir() == NULL) {
        printf("  SKIP: no CARS/ directory found\n");
        return;
    }
    for (i = 0; i < BR_CARDATA_CARS; ++i) {
        if (BrCarDataLoadIndex(&d, NULL, i) != 0) {
            printf("  SKIP: cars/%s.rca missing\n", BrCarDataName(i));
            continue;
        }
        ++n;
        CHECK(BrCollRespBoxDegenerate(d.boxX, d.boxY, d.boxZ) == 0);
        printf("  %-3s %-12s box (%.2f, %.2f, %.2f) offset %.2f\n",
               BrCarDataName(i), d.szName, (double)d.boxX, (double)d.boxY,
               (double)d.boxZ, (double)d.boxOffZ);
    }
    CHECK(n == BR_CARDATA_CARS);
}

/* ================================================================== */
/* The broad phase's three primitives                                  */
/* ================================================================== */

static void TestBroadPrimitives(void)
{
    float  aV[9];
    BrVec3 a, b, n, p;

    /* --- 0x10066260, the 26-plane classify -------------------------- */

    /* A vertex at the origin is inside all six slabs: stage 1's early
     * accept, which is the only path that returns 1. */
    aV[0] = 0.0f; aV[1] = 0.0f; aV[2] = 0.0f;
    aV[3] = 9.0f; aV[4] = 9.0f; aV[5] = 9.0f;
    aV[6] = 9.0f; aV[7] = 0.0f; aV[8] = 0.0f;
    CHECK(BrCollRespBoxClassify(aV) == 1);

    /* All three vertices above +0.5 in x: one face plane separates. */
    aV[0] = 2.0f; aV[1] = 0.0f; aV[2] = 0.0f;
    aV[3] = 3.0f; aV[4] = 1.0f; aV[5] = 0.0f;
    aV[6] = 4.0f; aV[7] = 0.0f; aV[8] = 1.0f;
    CHECK(BrCollRespBoxClassify(aV) == 0);

    /* THE NaN ARM.  `fcom 0.5` on a NaN sets C0|C2|C3, so `test ah,0x41`
     * sends it to the -0.5 test, where `test ah,1` is set too -- it
     * classifies as BELOW, not inside.  All three the same, so the whole
     * triangle rejects.  A tidy `if (v > 0.5)` would classify NaN as
     * inside and the box would start reporting phantom collisions. */
    {
        float nan_ = (float)(0.0 / 0.0);
        int   i;
        for (i = 0; i < 9; ++i) aV[i] = nan_;
        CHECK(BrCollRespBoxClassify(aV) == 0);
    }

    /* A triangle that misses on a corner plane but on no face or edge
     * plane: the three vertices straddle in every axis but all of them
     * satisfy x+y+z > 1.5. */
    aV[0] = 0.9f; aV[1] = 0.9f; aV[2] = 0.0f;
    aV[3] = 0.0f; aV[4] = 0.9f; aV[5] = 0.9f;
    aV[6] = 0.9f; aV[7] = 0.0f; aV[8] = 0.9f;
    CHECK(BrCollRespBoxClassify(aV) == 0);

    /* ONE NAMED EDGE PLANE, x - y == +1, which is stage 2's bit 0x004.
     * Stage 2 has twelve arms and this suite reached none of them by name
     * -- the corner case above lands on x+y+z, so `c |= 0x004u` could be
     * deleted outright and nothing noticed.
     *
     * All three vertices satisfy x - y > 1, and no single FACE plane holds
     * for all three: v0 is right-of and below the box, v1 only right-of,
     * v2 only below, so stage 1's AND is empty and no vertex is inside.
     * That leaves stage 2, where 0x004 is the one bit every vertex sets.
     * Correct answer 0 (separated); with the arm gone the mask empties on
     * the first vertex and the routine answers -1, a phantom overlap. */
    aV[0] =  0.6f; aV[1] = -0.6f; aV[2] = 0.0f;   /* x-y = 1.2 */
    aV[3] =  2.0f; aV[4] =  0.0f; aV[5] = 0.0f;   /* x-y = 2.0 */
    aV[6] =  0.0f; aV[7] = -2.0f; aV[8] = 0.0f;   /* x-y = 2.0 */
    CHECK(BrCollRespBoxClassify(aV) == 0);
    /* Mirrored onto x - y < -1, i.e. bit 0x008 -- the else-arm of the same
     * block, which only runs because the 0x004 arm did NOT fire. */
    aV[0] = -0.6f; aV[1] =  0.6f;
    aV[3] = -2.0f; aV[4] =  0.0f;
    aV[6] =  0.0f; aV[7] =  2.0f;
    CHECK(BrCollRespBoxClassify(aV) == 0);

    /* --- 0x10066800, segment versus cube ---------------------------- */
    a.x = -3.0f; a.y = 0.0f;  a.z = 0.0f;
    b.x =  3.0f; b.y = 0.0f;  b.z = 0.0f;
    CHECK(BrCollRespSegBox(&a, &b) != 0);       /* straight through      */
    a.y = 2.0f;  b.y = 2.0f;
    CHECK(BrCollRespSegBox(&a, &b) == 0);       /* outside the y slab    */
    /* A segment that passes all three slabs but misses the cube on the
     * cross-product test: the line x + y == -1.4, which clears the corner
     * (-0.5, -0.5) where x + y == -1.  Its slab test passes in every axis,
     * so only 0x100668AF's |C| <= |S/2| can reject it. */
    a.x = -2.0f; a.y =  0.6f; a.z = 0.0f;
    b.x =  0.6f; b.y = -2.0f; b.z = 0.0f;
    CHECK(BrCollRespSegBox(&a, &b) == 0);
    /* ...and shifting it to x + y == -0.6 makes it clip the corner. */
    a.y = 1.4f; b.x = 1.4f;
    CHECK(BrCollRespSegBox(&a, &b) != 0);

    /* THE Z SLAB.  Nothing above ever rejects on axis 2: the x case is
     * covered by "outside the y slab" only for y, and every other segment
     * here lies in the z == 0 plane.  So `for (i = 0; i < 3; ++i)` over the
     * slabs could be `i < 2` and this suite stayed green -- in a codebase
     * that has already shipped one genuine "never sums Z" defect.
     *
     * Both z compares get their own case, because they are separate
     * instructions in the original.  Each segment is chosen so that the
     * three cross-product tests all PASS, which is what makes the slab the
     * only thing that can reject it: with d == (0, 0, 1) and a on the z
     * axis, every C term is identically zero and every h*h >= C*C holds. */
    a.x = 0.0f; a.y = 0.0f; a.z =  5.0f;    /* both ends above +0.5 */
    b.x = 0.0f; b.y = 0.0f; b.z =  6.0f;
    CHECK(BrCollRespSegBox(&a, &b) == 0);   /* a[2]*sgn > 0.5      */
    a.z = -6.0f; b.z = -5.0f;               /* both ends below -0.5 */
    CHECK(BrCollRespSegBox(&a, &b) == 0);   /* b[2]*sgn < -0.5     */
    /* ...and the same segment run through the cube still hits, so the two
     * above are the slab rejecting and not the cross tests. */
    a.z = -3.0f; b.z = 3.0f;
    CHECK(BrCollRespSegBox(&a, &b) != 0);

    /* --- 0x10066610, point in triangle ------------------------------ */
    aV[0] = -1.0f; aV[1] = -1.0f; aV[2] = 0.0f;
    aV[3] =  1.0f; aV[4] = -1.0f; aV[5] = 0.0f;
    aV[6] =  0.0f; aV[7] =  1.0f; aV[8] = 0.0f;
    n.x = 0.0f; n.y = 0.0f; n.z = 1.0f;
    p.x = 0.0f; p.y = 0.0f; p.z = 0.0f;
    CHECK(BrCollRespPointInTri(aV, &n, &p) != 0);
    p.x = 5.0f;
    CHECK(BrCollRespPointInTri(aV, &n, &p) == 0);
    /* The winding survives a flipped normal, which is what the sign test
     * at 0x10066688 is for. */
    n.z = -1.0f;
    p.x = 0.0f;
    CHECK(BrCollRespPointInTri(aV, &n, &p) != 0);
}

/* The gather itself, against this file's own grid.  BuildSlope lays two
 * large triangles at z == 0 in cell 0; a car sitting on them must find
 * them, and a car a hundred metres up must not. */
static void TestBroadPhase(void)
{
    BrCarPhys       car;
    BrVec3          p;
    BrCollRespFrame frame;
    int             nNear, nFar;

    BuildSlope(0.0f, 0.0, 0);
    BrCarPhysInit(&car, NULL);
    BrCollRespCountersReset();

    if (BrCollRespBoxDegenerate(car.f1DC, car.f1E0, car.f1E4)) {
        printf("  SKIP: no car data, so the box is unusable\n");
        return;
    }

    p.x = 10.0f; p.y = 10.0f; p.z = 0.2f;
    BrCarPhysPlace(&car, &p, 0.0f);
    memset(&frame, 0, sizeof frame);
    BrCollRespListReset();
    BrCollRespBuildBoxMatrix(&frame, &car.body.m, BR_CR_BROAD_SCALE,
                             BR_CR_BROAD_SCALE, BR_CR_BROAD_SCALE);
    nNear = BrCollRespBroadPhase(&car.body, BrCollRespFrameMat(&frame));
    CHECK(nNear > 0);
    CHECK(g_pBrCollRespList != NULL);

    p.z = 100.0f;
    BrCarPhysPlace(&car, &p, 0.0f);
    BrCollRespListReset();
    BrCollRespBuildBoxMatrix(&frame, &car.body.m, BR_CR_BROAD_SCALE,
                             BR_CR_BROAD_SCALE, BR_CR_BROAD_SCALE);
    nFar = BrCollRespBroadPhase(&car.body, BrCollRespFrameMat(&frame));
    CHECK(nFar == 0);
    CHECK(g_pBrCollRespList == NULL);

    printf("  broad phase: %d record(s) at z=0.2, %d at z=100\n",
           nNear, nFar);

    /* The in-loop box, 1/extent rather than 0.1, is TEN TIMES SMALLER --
     * that is the whole difference between 0x10067CC3 and 0x10067D7F -- so
     * it must gather no more than the broad one. */
    p.z = 0.2f;
    BrCarPhysPlace(&car, &p, 0.0f);
    BrCollRespListReset();
    BrCollRespBuildBoxMatrix(&frame, &car.body.m,
                             BR_CR_ONE / car.f1DC, BR_CR_ONE / car.f1E0,
                             BR_CR_ONE / car.f1E4);
    CHECK(BrCollRespBroadPhase(&car.body, BrCollRespFrameMat(&frame))
          <= nNear);
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
    /* The corner 0x10066D70 builds is
     *      (+-f1DC*0.5, +-f1E0*0.5, f1E8 - f1E4*0.5)
     * and with the loaded box (3.5, 2.0, 0.8, 0.7) its Z is +0.3, i.e. the
     * box's underside sits 0.3 ABOVE the body origin.  (With the box left
     * unwritten it is 0, and every distance below collapses to the body's own
     * height -- which is why these three heights are derived from the box
     * rather than written as literals.)  Put the body where the corner lands
     * 0.5 from a plane at z == 0: inside the 0.6 window (0x10077B78).  A car
     * at rest has |vn| == 0, inside the 0.1 window (0x10077AF4), and two
     * wheels in contact is inside 1..2. */
    p.x = 0.0f; p.y = 0.0f;
    p.z = 0.5f - (car.f1E8 - car.f1E4 * BR_CR_HALF);
    BrCarPhysPlace(&car, &p, 0.0f);

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

    /* Corner too far from the plane: raise the body so the corner sits 3 m
     * above it, well outside the 0.6 window. */
    p.z = 3.0f - (car.f1E8 - car.f1E4 * BR_CR_HALF);
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
    p.z = 0.5f - (car.f1E8 - car.f1E4 * BR_CR_HALF);
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

    /* Released from ONE metre, not three.  This changed when 0x10067710 landed
     * in the substep loop (br_carphys.c): the OBB response is no longer a
     * no-op hole.  A three-metre free-drop sinks the body ~0.9 m THROUGH the
     * ground before the suspension turns it (measured with the response off),
     * and the collision box -- correctly -- reads that as a hard impact and
     * bounces the car; it does not settle in 401 frames.  That is the box
     * doing its job, not a regression, and it is exercised elsewhere.
     *
     * THIS test measures the SUSPENSION FORCE BALANCE, which the box does not
     * touch at rest: the f1E8 box offset lifts the classified box clear of the
     * ground at the spring equilibrium, so at z=0.19 the response never fires.
     * One metre reaches that equilibrium the way the game does -- the box fires
     * 11 times on the way down, arrests the sink, and the car still settles to
     * 0.190132 to six decimals.  So the drop is now a STRONGER check than the
     * old three-metre one: it proves the wired response neither corrupts the
     * force balance nor fails to let the car rest. */
    p.x = 10.0f; p.y = 10.0f; p.z = 1.0f;
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

    /* THE BROAD PHASE, RUNNING.  0x10066AD0 gathered this many triangle
     * records over the 401 frames; the list it built is what 0x10067710
     * would consume.  Printed rather than asserted at a number, because the
     * number is a property of this test's grid; what IS asserted is that it
     * ran on every frame and found something, which is the difference
     * between "the collision code is linked" and "the collision code runs". */
    printf("  broad phase: %u frames, %u triangle records gathered"
           " (%u refused for pool space)\n",
           (unsigned)g_cBrCollRespBroad, (unsigned)g_cBrCollRespGathered,
           (unsigned)g_cBrCollRespOverflow);
    CHECK(g_cBrCollRespBroad == 401u);
    if (BrCarDataDefault() != NULL) {
        CHECK(g_cBrCollRespGathered > 0u);
    }
    CHECK(g_cBrCollRespOverflow == 0u);

    /* SIX DECIMALS.  If this moves, the force balance changed. */
    CHECK_NEAR(pS->pos.z, 0.190132, 5e-7);

    /* On flat ground all four wheels stay in contact, so 0x10066D70's
     * 1..2 gate never opens and it must never have fired. */
    CHECK(g_cBrCollRespTipKick == 0u);
    /* ...and the box was REAL on every one of the 401 frames.  This counter
     * used to be asserted at 401 (degenerate on all of them); that it is now
     * 0 is the single observable difference the car-data loader makes, so it
     * is asserted from the other side rather than deleted.  With no CARS/ it
     * goes back to 401, which is the honest reading of that run. */
    if (BrCarDataDefault() != NULL) {
        CHECK(g_cBrCollRespDegenerate == 0u);
    } else {
        CHECK(g_cBrCollRespDegenerate == 401u);
    }
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
    TestBoxFromCarData();
    TestAllCarBoxes();
    TestBroadPrimitives();
    TestBroadPhase();
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
