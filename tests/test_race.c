/* test_race.c -- behaviour tests for br_race.c.
 *
 * These assert properties of the ORIGINAL gate machine, not a transcript of
 * what this port happens to do:
 *
 *   - a driver dragged forward round the ring gains exactly one lap per
 *     circuit, and the lap counter STOPS at the configured total;
 *   - the finished flag is set exactly at that total, and the finishing
 *     order is handed out in the order drivers reach it -- including to
 *     drivers with no car record;
 *   - the per-lap time array records one entry per lap, at the index of the
 *     lap that just ended;
 *   - going backwards is the exact inverse of going forwards for the gate
 *     counter, so a lap-and-back round trip restores the state;
 *   - the gate index is a FLOOR modulus, so it is continuous across zero;
 *   - the two unordered-compare traps in the best-lap test take the true
 *     side, and __ftol's out-of-range zero reaches the lap time.
 *
 * The gate ring used here is a square of four gates, laid out so that a
 * driver walking round it in +X/+Y crosses them in index order.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "br_race.h"

static int g_fails;

static int g_checks;   /* see the note at the bottom of main() */

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        g_checks++;                                                        \
        long _a = (long)(a), _b = (long)(b);                               \
        if (_a != _b) {                                                    \
            printf("FAIL %s:%d  %s (%ld) != %s (%ld)\n",                   \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        double _a = (double)(a), _b = (double)(b);                         \
        if (!(fabs(_a - _b) <= (eps))) {                                   \
            printf("FAIL %s:%d  %s (%.9g) != %s (%.9g)\n",                 \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * Stand-in for slice2_21.c's BrSeg2Intersect (0x1003BA70 / Glide 0x100350F0).
 *
 * TEST-ONLY. The real one is linked in the host and has its own coverage in
 * test_slice2_21; substituting it here keeps this binary to two objects and,
 * more usefully, lets a test drive an exact crossing sequence rather than
 * hoping a synthetic track produces one.
 *
 * The stand-in is a real predicate, not a script: it is the standard
 * orientation test, which agrees with the original on every non-degenerate
 * case the tests below construct.
 * ========================================================================== */

static long g_nSegCalls;

static float BrTestCross(const BrVec2 *pO, const BrVec2 *pA, const BrVec2 *pB)
{
    return (pA->x - pO->x) * (pB->y - pO->y) - (pA->y - pO->y) * (pB->x - pO->x);
}

int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    float d1 = BrTestCross(pA, pB, pC);
    float d2 = BrTestCross(pA, pB, pD);
    float d3 = BrTestCross(pC, pD, pA);
    float d4 = BrTestCross(pC, pD, pB);

    g_nSegCalls++;
    if (d1 * d2 < 0.0f && d3 * d4 < 0.0f)
        return 1;
    return 0;
}

/* ==========================================================================
 * A four-gate ring: a unit square walked anticlockwise.
 *
 *   gate 0 spans x = 0,  y in [-1, 1]      crossed moving through x = 0
 *   gate 1 spans y = 0,  x in [-1, 1]  ... no: the ring is four radial gates
 *
 * Simpler and closer to the real thing: four gates on the four compass
 * points of a circle of radius 10, each one a short radial segment. A driver
 * on that circle crosses them in order.
 * ========================================================================== */

#define NGATE 4

static BrRaceGate g_aGate[NGATE];

static void gates_init(void)
{
    /* gate i sits on the ray at angle i * 90 degrees, spanning radius 5..15 */
    static const float dx[NGATE] = {  1.0f,  0.0f, -1.0f,  0.0f };
    static const float dy[NGATE] = {  0.0f,  1.0f,  0.0f, -1.0f };
    int i;

    for (i = 0; i < NGATE; ++i) {
        g_aGate[i].postA.x = dx[i] * 5.0f;
        g_aGate[i].postA.y = dy[i] * 5.0f;
        g_aGate[i].postB.x = dx[i] * 15.0f;
        g_aGate[i].postB.y = dy[i] * 15.0f;
        g_aGate[i].tAward  = 1.0f + (float)i;
    }
}

static float g_fLapLength = 1000.0f;

static void rules_init(BrRaceRules *pR, int32_t nLaps, int32_t mode)
{
    memset(pR, 0, sizeof *pR);
    pR->aGates      = g_aGate;
    pR->nGates      = NGATE;
    pR->nLaps       = nLaps;
    pR->mode        = mode;
    pR->nFinished   = 0;
    pR->pfLapLength = &g_fLapLength;
}

/* Move the driver to angle `deg` on the radius-10 circle, keeping the
 * previous position where it was.
 *
 * The position has to go into the CAR when there is one: BrRaceGateStep
 * mirrors the car's +0x30 / +0xF80 over the driver's +0x00 / +0x0C before it
 * does anything else, so a test that wrote only the driver record would have
 * its positions overwritten with the car's zeros and nothing would ever
 * cross. That is the original's data flow, not an artefact. */
static void place(BrDriver *pD, double deg)
{
    double r = deg * 3.14159265358979 / 180.0;
    BrVec3 p;

    p.x = (float)(10.0 * cos(r));
    p.y = (float)(10.0 * sin(r));
    p.z = 0.0f;

    if (pD->pCar != NULL) {
        pD->pCar->posPrev = pD->pCar->pos;
        pD->pCar->pos     = p;
    } else {
        pD->f0C = pD->f00;
        pD->f00 = p;
    }
}

static void driver_init(BrDriver *pD, BrDriverCar *pC)
{
    BrVec3 p;

    memset(pD, 0, sizeof *pD);
    if (pC != NULL)
        memset(pC, 0, sizeof *pC);
    pD->pCar = pC;

    /* start just past gate 0, on the ring */
    p.x = 10.0f; p.y = 0.5f; p.z = 0.0f;
    if (pC != NULL) {
        pC->pos = p; pC->posPrev = p;
    } else {
        pD->f00 = p; pD->f0C = p;
    }
}

/* Walk forward through one whole ring, 8 half-quadrant steps. Returns the
 * number of laps BrRaceGateStep reported. */
static int walk_lap(BrRaceRules *pR, BrDriver *pD, double *pDeg)
{
    int laps = 0, k;

    for (k = 0; k < 8; ++k) {
        *pDeg += 45.0;
        place(pD, *pDeg);
        laps += BrRaceGateStep(pR, pD);
    }
    return laps;
}

/* ==========================================================================
 * 1. The gate index is a FLOOR modulus and is continuous across zero
 * ========================================================================== */

static void test_gate_index(void)
{
    int32_t i;

    for (i = -13; i <= 13; ++i) {
        int32_t v = BrRaceGateIndex(i, 5);
        CHECK(v >= 0 && v < 5);
    }
    /* The property that matters: stepping the unwrapped counter by one steps
     * the ring index by one, in BOTH directions, with no discontinuity at
     * zero. A truncating `%` would give ... 1, 0, -1 ... and break this. */
    for (i = -12; i <= 12; ++i) {
        int32_t a = BrRaceGateIndex(i, 5);
        int32_t b = BrRaceGateIndex(i + 1, 5);
        CHECK_EQ(b, (a + 1) % 5);
    }
    CHECK_EQ(BrRaceGateIndex(-1, 5), 4);
    CHECK_EQ(BrRaceGateIndex(-5, 5), 0);
    CHECK_EQ(BrRaceGateIndex(-6, 5), 4);
}

/* ==========================================================================
 * 2. Hundredths truncation, and __ftol's out-of-range zero
 * ========================================================================== */

static void test_trunc(void)
{
    CHECK_NEAR(BrRaceTruncHundredths(1.239f), 1.23f, 1e-6);
    CHECK_NEAR(BrRaceTruncHundredths(0.0f),   0.0f,  1e-9);
    /* Truncation toward zero, not floor: a negative time keeps its magnitude
     * rounded DOWN in absolute value. */
    CHECK_NEAR(BrRaceTruncHundredths(-1.239f), -1.23f, 1e-6);

    /* 0x1007C8A0 returns the LOW DWORD of the 64-bit indefinite when the
     * value will not fit, i.e. 0. So an absurd or NaN lap timer records a lap
     * time of exactly zero rather than a huge one. */
    CHECK_NEAR(BrRaceTruncHundredths(1.0e30f), 0.0f, 1e-9);
    {
        float nan = (float)(0.0 / 0.0);
        CHECK_NEAR(BrRaceTruncHundredths(nan), 0.0f, 1e-9);
    }
}

/* ==========================================================================
 * 3. N laps advance the counter, and it STOPS at the configured total
 * ========================================================================== */

static void test_laps_and_finish(void)
{
    BrRaceRules  r;
    BrDriver     d;
    BrDriverCar  c;
    double       deg = 5.0;   /* just past gate 0 */
    int          i, laps;

    gates_init();
    rules_init(&r, 3, 6);
    driver_init(&d, &c);
    place(&d, deg);

    /* Three laps: the counter reaches exactly nLaps and the flag comes up. */
    for (i = 0; i < 3; ++i) {
        laps = walk_lap(&r, &d, &deg);
        CHECK_EQ(laps, 1);
        CHECK_EQ(d.f40, i + 1);
        CHECK_EQ(c.lap, i + 1);
        /* the car mirror is written on every call */
        CHECK_EQ(c.gate, d.f4C);
        CHECK_EQ(c.gateHi, d.f48);
    }
    CHECK_EQ(d.f40, 3);
    CHECK((d.f68 & BR_DRIVER_SKIP) != 0);
    CHECK_EQ(r.nFinished, 1);
    CHECK_EQ(c.fFF8, 0);              /* first to finish */

    /* Two more circuits. The lap counter must NOT move past the total: the
     * increment is behind `f40 < nLaps`. The finish arm does re-fire, which
     * is the original's behaviour and is why nFinished keeps climbing. */
    for (i = 0; i < 2; ++i) {
        laps = walk_lap(&r, &d, &deg);
        CHECK_EQ(laps, 0);
        CHECK_EQ(d.f40, 3);
    }
    CHECK_EQ(d.f40, 3);
    CHECK_EQ(r.nFinished, 3);

    /* The gate counter keeps climbing regardless -- it is the lap that is
     * clamped, not the progress. Five circuits of four gates. */
    CHECK_EQ(d.f48, 5 * NGATE);
}

/* ==========================================================================
 * 4. One recorded lap time per lap, at the index of the lap that ended
 * ========================================================================== */

static void test_lap_times(void)
{
    BrRaceRules  r;
    BrDriver     d;
    BrDriverCar  c;
    double       deg = 5.0;
    int          i;

    gates_init();
    rules_init(&r, 3, 6);
    driver_init(&d, &c);
    place(&d, deg);

    for (i = 0; i < 3; ++i) {
        /* pretend the lap took (i + 1) * 10 seconds */
        d.f30 = 10.0f * (float)(i + 1);
        c.tRun = d.f30;
        (void)walk_lap(&r, &d, &deg);

        /* lapTimeFinal is indexed by the lap that JUST ENDED, i.e. the value
         * of f40 before the increment. */
        CHECK_NEAR(c.aLapTime[i], 10.0f * (float)(i + 1), 1e-4);
        /* and the running timer has that much taken off it */
        CHECK_NEAR(d.f30, 0.0f, 1e-4);
    }
    /* Untouched entries stay zero -- no off-by-one into lap 3. */
    CHECK_NEAR(c.aLapTime[3], 0.0f, 1e-9);

    /* Best lap is the smallest of the three, and it was set on the first
     * lap because the seed value 0.0 means "none yet" rather than "zero
     * seconds" -- the fcomp/test ah,0x40 arm. */
    CHECK_NEAR(d.f34, 10.0f, 1e-4);
    CHECK_NEAR(c.tBest, 10.0f, 1e-4);
}

/* ==========================================================================
 * 5. Backwards is the inverse of forwards: a round trip restores the state
 * ========================================================================== */

static void test_round_trip(void)
{
    BrRaceRules r;
    BrDriver    d;
    double      deg = 5.0;
    int32_t     gate0, lap0;
    int         k;

    gates_init();
    rules_init(&r, 9, 6);
    driver_init(&d, NULL);            /* no car: the mirror is skipped */
    place(&d, deg);

    gate0 = d.f4C;
    lap0  = d.f40;

    /* forward one whole ring, then back the way we came */
    (void)walk_lap(&r, &d, &deg);
    CHECK_EQ(d.f4C, gate0 + NGATE);
    CHECK_EQ(d.f40, lap0 + 1);

    for (k = 0; k < 8; ++k) {
        deg -= 45.0;
        place(&d, deg);
        (void)BrRaceGateStep(&r, &d);
    }
    /* The unwrapped gate counter comes back exactly. The high-water mark
     * does NOT -- that is the point of keeping two of them. */
    CHECK_EQ(d.f4C, gate0);
    CHECK_EQ(d.f48, gate0 + NGATE);
}

/* ==========================================================================
 * 6. A driver with no car still finishes, and still takes a place
 * ========================================================================== */

static void test_carless_finishes(void)
{
    BrRaceRules r;
    BrDriver    dCar, dGhost;
    BrDriverCar c;
    double      degA = 5.0, degB = 5.0;

    gates_init();
    rules_init(&r, 1, 6);

    driver_init(&dGhost, NULL);
    driver_init(&dCar, &c);
    place(&dGhost, degB);
    place(&dCar, degA);

    /* the ghost gets round first */
    (void)walk_lap(&r, &dGhost, &degB);
    CHECK((dGhost.f68 & BR_DRIVER_SKIP) != 0);
    CHECK_EQ(r.nFinished, 1);         /* the counter moved with no car */

    (void)walk_lap(&r, &dCar, &degA);
    CHECK((dCar.f68 & BR_DRIVER_SKIP) != 0);
    CHECK_EQ(c.fFF8, 1);              /* second place, behind the ghost */
    CHECK_EQ(r.nFinished, 2);
}

/* ==========================================================================
 * 7. Mode 3 pins the lap counter at zero and rewinds the progress key
 * ========================================================================== */

static void test_mode_wrap(void)
{
    BrRaceRules r;
    BrDriver    d;
    double      deg = 5.0;
    float       prog0;
    int         laps;

    gates_init();
    rules_init(&r, 3, BR_RACE_MODE_WRAP);
    driver_init(&d, NULL);
    place(&d, deg);
    d.f50 = 12345.0f;
    prog0 = d.f50;

    laps = walk_lap(&r, &d, &deg);
    /* the lap was taken and immediately given back */
    CHECK_EQ(laps, 0);
    CHECK_EQ(d.f40, 0);
    CHECK_EQ(d.f44, 0);
    /* ...and so were the gate counters and one lap of track length */
    CHECK_EQ(d.f4C, 0);
    CHECK_EQ(d.f48, 0);
    CHECK_NEAR(d.f50, prog0 - g_fLapLength, 1e-3);
    /* never finishes */
    CHECK_EQ(d.f68 & BR_DRIVER_SKIP, 0);
}

/* ==========================================================================
 * 8. Zero gates makes the whole step a no-op, mirror included
 * ========================================================================== */

static void test_no_gates(void)
{
    BrRaceRules r;
    BrDriver    d;
    BrDriverCar c;
    long        nBefore;

    gates_init();
    rules_init(&r, 3, 6);
    r.nGates = 0;
    driver_init(&d, &c);
    c.lap = 7;                        /* would be mirrored in if we got there */

    nBefore = g_nSegCalls;
    CHECK_EQ(BrRaceGateStep(&r, &d), 0);
    CHECK_EQ(g_nSegCalls, nBefore);   /* no crossing test at all */
    CHECK_EQ(d.f40, 0);               /* the mirror did not run */
    CHECK_EQ(c.lap, 7);
}

/* ==========================================================================
 * 9. The mirror is a faithful round trip
 * ========================================================================== */

static void test_mirror(void)
{
    BrDriver    d;
    BrDriverCar c;

    memset(&d, 0, sizeof d);
    memset(&c, 0, sizeof c);
    d.pCar = &c;

    c.pos.x = 1.0f; c.pos.y = 2.0f; c.pos.z = 3.0f;
    c.posPrev.x = 4.0f; c.posPrev.y = 5.0f; c.posPrev.z = 6.0f;
    c.gateHi = 11; c.gate = 9; c.lap = 2; c.lapB = 3;
    c.tRun = 1.5f; c.tBest = 2.5f; c.fFF4 = 3.5f;

    BrRaceLoadFromCar(&d);
    CHECK_NEAR(d.f00.x, 1.0f, 0); CHECK_NEAR(d.f0C.z, 6.0f, 0);
    CHECK_EQ(d.f48, 11); CHECK_EQ(d.f4C, 9);
    CHECK_EQ(d.f40, 2);  CHECK_EQ(d.f44, 3);
    CHECK_NEAR(d.f30, 1.5f, 0);
    CHECK_NEAR(d.f34, 2.5f, 0);
    CHECK_NEAR(d.f50, 3.5f, 0);

    d.f48 = 21; d.f4C = 19; d.f40 = 5; d.f44 = 6;
    d.f30 = 7.5f; d.f34 = 8.5f; d.f50 = 9.5f;
    /* the positions must NOT be written back */
    d.f00.x = 99.0f; d.f0C.z = 99.0f;
    BrRaceStoreToCar(&d);
    CHECK_EQ(c.gateHi, 21); CHECK_EQ(c.gate, 19);
    CHECK_EQ(c.lap, 5);     CHECK_EQ(c.lapB, 6);
    CHECK_NEAR(c.tRun, 7.5f, 0);
    CHECK_NEAR(c.tBest, 8.5f, 0);
    CHECK_NEAR(c.fFF4, 9.5f, 0);
    CHECK_NEAR(c.pos.x, 1.0f, 0);        /* untouched */
    CHECK_NEAR(c.posPrev.z, 6.0f, 0);

    /* both ends are no-ops on a carless driver */
    d.pCar = NULL;
    BrRaceLoadFromCar(&d);
    BrRaceStoreToCar(&d);
    CHECK_EQ(d.f48, 21);
}

/* ==========================================================================
 * 10. The best-lap test's two unordered traps
 * ========================================================================== */

static void test_best_lap_unordered(void)
{
    BrRaceRules r;
    BrDriver    d;
    double      deg = 5.0;

    gates_init();
    rules_init(&r, 9, 6);

    /* (a) a NaN incumbent best is REPLACED: `test ah,0x40` reads C3, which
     *     is set for unordered as well as equal, so the "no best yet" arm is
     *     taken. */
    driver_init(&d, NULL);
    place(&d, deg);
    d.f34 = (float)(0.0 / 0.0);
    d.f30 = 42.0f;
    (void)walk_lap(&r, &d, &deg);
    CHECK_NEAR(d.f34, 42.0f, 1e-3);

    /* (b) a NaN challenger is ACCEPTED against a finite best: `test ah,1`
     *     reads C0, set for less-than and for unordered, and the skip is on
     *     the jump-if-clear. It arrives as 0.0 rather than NaN, because
     *     __ftol has already flattened it -- which is exactly why the two
     *     traps have to be read together. */
    deg = 5.0;
    driver_init(&d, NULL);
    place(&d, deg);
    d.f34 = 12.0f;
    d.f30 = (float)(0.0 / 0.0);
    (void)walk_lap(&r, &d, &deg);
    CHECK_NEAR(d.f34, 0.0f, 1e-9);
}

int main(void)
{
    test_gate_index();
    test_trunc();
    test_laps_and_finish();
    test_lap_times();
    test_round_trip();
    test_carless_finishes();
    test_mode_wrap();
    test_no_gates();
    test_mirror();
    test_best_lap_unordered();

    if (g_fails != 0) {
        printf("test_race: %d FAILED\n", g_fails);
        return 1;
    }
    /* A count, not a bare "ok" -- tools/regress.sh cannot tell an "ok" that
     * ran nothing from one that ran everything, and says so. */
    printf("test_race: %d checks, %d failures\n", g_checks, g_fails);
    return 0;
}
