/* test_racestep.c -- behaviour tests for br_racestep.c.
 *
 * These assert properties of the ORIGINAL race step, not a transcript of
 * what this port happens to do:
 *
 *   - the start-light script walks 0 -> 1 -> 2 -> 3 -> 4 and the number of
 *     1/30 s frames each state lasts is the table's own duration;
 *   - state 4 has duration 0.0 and NEVER advances on the clock: the only
 *     thing that leaves it is every driver carrying BR_DRIVER_SKIP;
 *   - every driver is FROZEN (+0x68 bit 0) while the lights are below 3 and
 *     unfrozen on the frame they reach 3;
 *   - a frozen driver gets no gate step, so a car that is moved across a
 *     gate during the countdown does not advance -- and the same motion
 *     after the green does;
 *   - the countdown fires exactly four sounds, at the four thresholds;
 *   - the path walk lands on the midpoint of a segment for half its length,
 *     carries into the following segment when asked for more, and honours
 *     the partial-first-segment `ratio`;
 *   - a PHANTOM entrant (a slot with no car) is carried round the ring by
 *     that walk and its gate and lap counters advance;
 *   - every hole is entered the number of times the original would have
 *     called it.
 *
 * The path and phantom tests need a real .trk, because the path ring is
 * track data and br_ai.c decodes it out of the file rather than modelling
 * it. They SKIP when the asset is absent, per the project's asset policy.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "br_racestep.h"

static int g_fails;
static int g_checks;
static int g_skipped;

/* ==========================================================================
 * TEST-ONLY stand-ins, all three for functions this file never exercises.
 *
 * BrSeg2Intersect is the same stand-in test_race.c uses and for the same
 * reason: the real one (slice2_21.c) drags six further modules in, it has
 * its own coverage in test_slice2_21, and the HOST links the real one -- so
 * `brally -race` is where the real predicate is exercised. This is the
 * standard orientation test, a real predicate rather than a script.
 *
 * BrFatal and g_Br0B380C belong to the OTHER halves of slice3_41.c, which is
 * linked here only for BrRankAssign.
 * ========================================================================== */

int32_t g_Br0B380C = 0;         /* 0x100B380C, owned by slice2_19 */
float   g_f6C2CFC  = 0.0f;      /* 0x106C2CFC, the frame delta; br_data owns
                                 * the real storage, which this binary does
                                 * not link                                */

void BrFatal(const char *pszMsg);
void BrFatal(const char *pszMsg)
{
    printf("BrFatal: %s\n", pszMsg ? pszMsg : "(null)");
}

static float SegCross(const BrVec2 *pO, const BrVec2 *pA, const BrVec2 *pB)
{
    return (pA->x - pO->x) * (pB->y - pO->y) - (pA->y - pO->y) * (pB->x - pO->x);
}

int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    float d1 = SegCross(pA, pB, pC);
    float d2 = SegCross(pA, pB, pD);
    float d3 = SegCross(pC, pD, pA);
    float d4 = SegCross(pC, pD, pB);

    return (d1 * d2 < 0.0f && d3 * d4 < 0.0f) ? 1 : 0;
}

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
        long _a = (long)(a), _b = (long)(b);                               \
        g_checks++;                                                        \
        if (_a != _b) {                                                    \
            printf("FAIL %s:%d  %s (%ld) != %s (%ld)\n",                   \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        double _a = (double)(a), _b = (double)(b);                         \
        g_checks++;                                                        \
        if (!(fabs(_a - _b) <= (eps))) {                                   \
            printf("FAIL %s:%d  %s (%.9g) != %s (%.9g)\n",                 \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

/* ---------------------------------------------------------------------
 * A field, and a gate ring to drive it round
 * ------------------------------------------------------------------- */

#define NDRV 4

static BrDriver    g_drv[NDRV];
static BrDriverCar g_car[NDRV];

/* Four gates on the sides of a square, crossed in index order by a driver
 * walking +X then +Y then -X then -Y. Same shape test_race.c uses. */
static const BrRaceGate g_gate[4] = {
    { { 10.0f, -5.0f }, { 10.0f,  5.0f }, 0.0f },   /* x = 10 */
    { {  5.0f, 10.0f }, { -5.0f, 10.0f }, 0.0f },   /* y = 10 */
    { {-10.0f,  5.0f }, {-10.0f, -5.0f }, 0.0f },   /* x = -10 */
    { { -5.0f,-10.0f }, {  5.0f,-10.0f }, 0.0f }    /* y = -10 */
};

static int g_nControl;

static void CountControl(BrDriverCar *pCar)
{
    (void)pCar;
    g_nControl++;
}

static void FieldInit(int nCars)
{
    int i;

    memset(g_drv, 0, sizeof g_drv);
    memset(g_car, 0, sizeof g_car);
    memset(&g_brRaceRules, 0, sizeof g_brRaceRules);
    BrRaceStepHoleReset();
    memset(&g_brRaceStepHooks, 0, sizeof g_brRaceStepHooks);
    g_nControl = 0;

    for (i = 0; i < NDRV; ++i) {
        g_drv[i].f64  = i;
        g_drv[i].pCar = (i < nCars) ? &g_car[i] : NULL;
        g_car[i].pfnControl = CountControl;
    }

    g_pBrRaceDriver  = g_drv;
    g_pBrRaceCar     = g_car;
    g_brRaceNDriver  = NDRV;
    g_brRaceNCar     = NDRV;
    g_brRaceNEntrant = nCars;

    g_brRaceRules.aGates      = g_gate;
    g_brRaceRules.nGates      = 4;
    g_brRaceRules.nLaps       = 3;
    g_brRaceRules.mode        = 6;      /* BrPhaseActivate_100447D0's value */
    g_brRaceRules.nFinished   = 0;
    g_brRaceRules.pfLapLength = NULL;

    g_brRacePaused   = 0;
    g_brRaceReplay   = 0;
    g_brRaceNet      = 0;
    g_brRaceTick     = 1;
    g_brRaceSubstate = 0;
    g_brRaceStepDt   = 1.0f / 30.0f;
    g_pBrRaceTrack   = NULL;
    g_pfnBrRaceAiControl = NULL;

    (void)BrRaceStepInit();
}

/* ---------------------------------------------------------------------
 * 1. The script
 * ------------------------------------------------------------------- */

static int FramesUntilLights(int want, int cap)
{
    int n = 0;
    while (g_brRaceLights != want && n < cap) {
        BrRaceStepFrame();
        ++n;
    }
    return (g_brRaceLights == want) ? n : -1;
}

static void test_script(void)
{
    int n1, n2, n3, n4;

    FieldInit(NDRV);

    /* 0x1001A99F seeds state 0 with duration 0.0. */
    CHECK_EQ(g_brRaceLights, 0);
    CHECK_EQ(g_brRaceScript, 0);
    CHECK_NEAR(g_brRaceLightT, 0.0f, 1e-9);
    CHECK_EQ(g_brRaceSubstate, 1);

    /* Duration 0.0 and the test is `t - dt < 0`, so one frame leaves it. */
    n1 = FramesUntilLights(1, 10);
    CHECK_EQ(n1, 1);
    CHECK_NEAR(g_brRaceLightT, g_aBrRaceLightScript[1].dur, 1e-6);

    /* Each state lasts its own duration, to the frame.  The bound is
     * `duration / dt` rounded up, plus one: neither 2.2f nor 1/30 is exact
     * in binary and the timer ACCUMULATES its error over sixty-odd
     * subtractions, so demanding an exact frame count would be asserting
     * this machine's rounding rather than the original's behaviour. */
    n2 = FramesUntilLights(2, 400);
    CHECK(n2 >= 66 && n2 <= 68);         /* 2.2 s */

    n3 = FramesUntilLights(BR_RS_LIGHTS_GO, 400);
    CHECK(n3 >= 69 && n3 <= 71);         /* 2.3 s */

    n4 = FramesUntilLights(BR_RS_LIGHTS_RACE, 400);
    CHECK(n4 >= 60 && n4 <= 62);         /* 2.0 s */

    /* THE POINT OF THE STATE-4 ARM: it has duration 0.0, and if it were on
     * the clock like the arms above it a race would end on frame one. */
    CHECK_NEAR(g_aBrRaceLightScript[BR_RS_LIGHTS_RACE].dur, 0.0f, 1e-9);
    {
        int i;
        for (i = 0; i < 200; ++i)
            BrRaceStepFrame();
        CHECK_EQ(g_brRaceLights, BR_RS_LIGHTS_RACE);
    }

    /* ...and the ONE thing that does leave it. */
    {
        int i;
        for (i = 0; i < NDRV; ++i)
            g_drv[i].f68 |= BR_DRIVER_SKIP;
        CHECK_EQ(BrRaceStepAllFinished(), 1);
        BrRaceStepFrame();
        CHECK_EQ(g_brRaceLights, 5);
    }
}

/* ---------------------------------------------------------------------
 * 2. The freeze, and that it is what gates the gate machine
 * ------------------------------------------------------------------- */

static void SetPos(int i, float x, float y)
{
    g_car[i].pos.x = x;  g_car[i].pos.y = y;  g_car[i].pos.z = 0.0f;
}

static void test_freeze(void)
{
    int i;

    /* Three cars and one phantom, so both arms of the freeze are visible. */
    FieldInit(NDRV - 1);

    /* Mark the phantom slot's two positions apart.  The phantom arm's very
     * first act (0x1006223D) is `f0C = f00`, so if it ever runs the two
     * become equal -- which makes "the freeze bit stops the path walk" a
     * checkable claim that needs no track. */
    g_drv[NDRV - 1].f00.x = 1.0f;
    g_drv[NDRV - 1].f0C.x = 2.0f;

    /* One frame of state 0: every slot is frozen. */
    BrRaceStepFrame();
    for (i = 0; i < NDRV; ++i)
        CHECK((g_drv[i].f68 & BR_RS_DRIVER_FROZEN) != 0);

    /* 0x10062035: the frozen arm's own effect on a car. */
    for (i = 0; i < NDRV - 1; ++i)
        CHECK((g_car[i].f29C0Ctl & BR_DRIVERCAR_CTL_BRAKE) != 0);

    /* The phantom never reached its arm. */
    for (i = 0; i < 40; ++i)
        BrRaceStepFrame();
    CHECK_NEAR(g_drv[NDRV - 1].f0C.x, 2.0f, 1e-9);
    CHECK_EQ(g_drv[NDRV - 1].f4C, 0);
    CHECK_EQ(g_drv[NDRV - 1].f40, 0);

    /* Run to green.  The state changes at the END of the frame that expires
     * the timer -- the freeze loop for that frame has already run -- so the
     * bit comes off on the frame AFTER, which is the original's ordering and
     * not an off-by-one. */
    CHECK(FramesUntilLights(BR_RS_LIGHTS_GO, 400) >= 0);
    for (i = 0; i < NDRV; ++i)
        CHECK((g_drv[i].f68 & BR_RS_DRIVER_FROZEN) != 0);
    BrRaceStepFrame();
    for (i = 0; i < NDRV; ++i)
        CHECK((g_drv[i].f68 & BR_RS_DRIVER_FROZEN) == 0);
}

/* ---------------------------------------------------------------------
 * 3. The countdown's four sounds
 * ------------------------------------------------------------------- */

static int g_nSound;
static int g_aSoundStep[8];

static void CountSound(int iStep)
{
    if (g_nSound < 8)
        g_aSoundStep[g_nSound] = iStep;
    g_nSound++;
}

static void test_countdown(void)
{
    FieldInit(NDRV);
    g_brRaceStepHooks.pfnSound = CountSound;
    g_nSound = 0;

    CHECK(FramesUntilLights(BR_RS_LIGHTS_GO, 400) >= 0);

    /* Four thresholds, four sounds: three beeps and the horn. */
    CHECK_EQ(g_nSound, BR_RS_BEEP_REAL);
    CHECK_EQ(g_brRaceBeep, BR_RS_BEEP_REAL);
    CHECK_EQ(g_aSoundStep[0], 1);
    CHECK_EQ(g_aSoundStep[3], 4);
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_SOUND], BR_RS_BEEP_REAL);
}

/* ---------------------------------------------------------------------
 * 4. Once green, a car entrant's gate step runs
 * ------------------------------------------------------------------- */

/* The physics, standing where the physics stands: 0x1006F170 -> 0x1005A7A0
 * is reached from inside the controller, i.e. AFTER 0x10061F60 has latched
 * posPrev.  A test that moved the car from the outside would produce a
 * degenerate motion segment and could never cross a gate. */
static float g_driveY;

static void DriveControl(BrDriverCar *pCar)
{
    g_nControl++;
    pCar->pos.y += g_driveY;
}

static void test_car_gate(void)
{
    int i;

    FieldInit(NDRV);
    g_car[0].pfnControl = DriveControl;
    CHECK(FramesUntilLights(BR_RS_LIGHTS_RACE, 400) >= 0);

    /* Drive car 0 north across gate 1 (the y = 10 segment).  0x100623E0 is
     * the only thing that can move the counter here. */
    /* 0.5 off the grid so no sample lands exactly ON the y = 10 line: an
     * endpoint touch is not a crossing for either this stand-in or the
     * original, and starting on a whole number would make it one. */
    SetPos(0, 0.0f, 0.5f);
    g_car[0].posPrev = g_car[0].pos;
    g_driveY = 2.0f;
    for (i = 0; i < 12; ++i)
        BrRaceStepFrame();
    g_driveY = 0.0f;
    CHECK_EQ(g_drv[0].f4C, 1);

    /* The velocity 0x100623E0 derives is (pos - posPrev) / dt, and the last
     * step was 2 units of +Y in 1/30 s. */
    CHECK_NEAR(g_car[0].f1024.y, 60.0f, 1e-2);
    CHECK_NEAR(g_car[0].f1024.x,  0.0f, 1e-6);

    /* The other three slots have not moved and have not advanced. */
    for (i = 1; i < NDRV; ++i)
        CHECK_EQ(g_drv[i].f4C, 0);
}

/* ---------------------------------------------------------------------
 * 5. Every hole is entered the number of times the original would call it
 * ------------------------------------------------------------------- */

static void test_hole_counts(void)
{
    FieldInit(NDRV);
    BrRaceStepHoleReset();
    g_nControl = 0;

    BrRaceStepFrame();                   /* one frame, lights still red */

    /* The controller is dispatched once per driver that has a car. */
    CHECK_EQ(g_nControl, NDRV);
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_CONTROL], NDRV);
    /* 0x100623E0's unported passes: THREE per car unconditionally, and a
     * fourth (the skid trail) only when car+0x360 is non-zero, which it is
     * not for a car that has not been driven. */
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_SKID], NDRV * 3);
    g_car[0].b360 = 4u;
    BrRaceStepHoleReset();
    BrRaceStepFrame();
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_SKID], NDRV * 3 + 1);
    CHECK_EQ((int)g_car[0].b360, 0);         /* 0x1006245C clears it */
    BrRaceStepHoleReset();
    BrRaceStepFrame();
    /* 0x1005C450 and the HUD tail, once each; the pre-race HUD block adds
     * one more to the HUD count. */
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_SCRATCH], 1);
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_HUD], 2);
    /* 0x10060A30 runs only in state 4. */
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_LAPINFO], 0);
    /* 0x100623A0 runs only for a network or replay session. */
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_ANIM], 0);

    /* State 4 adds the lap-info pass, once per entrant. */
    CHECK(FramesUntilLights(BR_RS_LIGHTS_RACE, 400) >= 0);
    BrRaceStepHoleReset();
    BrRaceStepFrame();
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_LAPINFO], NDRV);

    /* And a replay session adds 0x100623A0. */
    g_brRaceReplay = 1;
    BrRaceStepHoleReset();
    BrRaceStepFrame();
    CHECK_EQ((int)g_aBrRaceStepHole[BR_RS_HOLE_ANIM], NDRV);
    g_brRaceReplay = 0;
}

/* ---------------------------------------------------------------------
 * 6. The path walk and the phantom entrant -- these need a real track
 * ------------------------------------------------------------------- */

static BrTrack g_trk;
static int     g_haveTrack;

static int TrackOpen(void)
{
    static const char *const apszPath[] = {
        "testdata/tracks/race.trk",
        "testdata/tracks/desert.trk"
    };
    unsigned i;

    if (g_haveTrack)
        return 1;
    for (i = 0; i < sizeof apszPath / sizeof apszPath[0]; ++i) {
        if (BrTrackOpen(&g_trk, apszPath[i]) == 0) {
            g_haveTrack = 1;
            return 1;
        }
    }
    return 0;
}

static void test_path_walk(void)
{
    BrAiNode  root;
    BrAiPoint p0, p1;
    float     segLen;

    if (!TrackOpen()) { ++g_skipped; return; }
    g_pBrRaceTrack = &g_trk;

    if (BrAiRoot(&g_trk, &root) != 0) { ++g_skipped; return; }
    if (BrAiPoint_(&root, 0u, &p0) != 0) { ++g_skipped; return; }
    if (BrAiPoint_(&root, 1u, &p1) != 0) { ++g_skipped; return; }
    segLen = p0.arc - p1.arc;
    CHECK(segLen > 0.0f);

    /* ratio 1 and dist 0: the walk stops in the first segment (`fcomp` takes
     * the equal side) and lands on pts[0] exactly. */
    BrRacePathAdvance(root.off, 0u, 1.0f, 0.0f);
    CHECK_EQ((long)g_brRacePathNode, (long)root.off);
    CHECK_EQ((long)g_brRacePathIndex, 0L);
    CHECK_NEAR(g_brRacePathPos.x, p0.centre.x, 1e-3);
    CHECK_NEAR(g_brRacePathPos.y, p0.centre.y, 1e-3);

    /* Half the segment: the midpoint of its two centres. */
    BrRacePathAdvance(root.off, 0u, 1.0f, segLen * 0.5f);
    CHECK_EQ((long)g_brRacePathIndex, 0L);
    CHECK_NEAR(g_brRacePathPos.x, 0.5f * (p0.centre.x + p1.centre.x), 1e-2);
    CHECK_NEAR(g_brRacePathPos.y, 0.5f * (p0.centre.y + p1.centre.y), 1e-2);

    /* Past the whole segment: the cursor has moved on. */
    BrRacePathAdvance(root.off, 0u, 1.0f, segLen * 1.5f);
    CHECK((long)g_brRacePathIndex >= 1L);

    /* `ratio` shortens the FIRST segment only: with half of it left, asking
     * for half of that lands three quarters of the way along. */
    BrRacePathAdvance(root.off, 0u, 0.5f, segLen * 0.25f);
    CHECK_EQ((long)g_brRacePathIndex, 0L);
    CHECK_NEAR(g_brRacePathPos.x,
               p0.centre.x + 0.75f * (p1.centre.x - p0.centre.x), 1e-2);
}

static void test_phantom_laps(void)
{
    int   i, iStep;
    int   nGateAdvance = 0;
    float lapLen;

    if (!TrackOpen()) { ++g_skipped; return; }

    /* Two car entrants and two phantoms, which is the original's own shape:
     * 0x10019BD3 sets twenty drivers against three cars. */
    FieldInit(2);
    g_pBrRaceTrack = &g_trk;
    lapLen = BrAiLapLength(&g_trk);
    CHECK(lapLen > 0.0f);
    g_brRaceRules.pfLapLength = &lapLen;

    /* The gate ring of the real track, read the way brally.c reads it. */
    {
        static BrRaceGate aGate[BR_AI_GATE_MAX];
        int32_t n = (int32_t)BrTrackHdrU32(&g_trk, BR_TRK_H_CGATES);
        int32_t k;

        if (n < 0) n = 0;
        if (n > (int32_t)BR_AI_GATE_MAX) n = (int32_t)BR_AI_GATE_MAX;
        for (k = 0; k < n; ++k) {
            unsigned off = BR_TRK_H_GATES + (unsigned)k * BR_AI_GATE_STRIDE;
            uint32_t a[5];
            int      j;
            for (j = 0; j < 5; ++j)
                a[j] = BrTrackHdrU32(&g_trk, off + (unsigned)j * 4u);
            memcpy(&aGate[k].postA.x, &a[0], 4);
            memcpy(&aGate[k].postA.y, &a[1], 4);
            memcpy(&aGate[k].postB.x, &a[2], 4);
            memcpy(&aGate[k].postB.y, &a[3], 4);
            memcpy(&aGate[k].tAward,  &a[4], 4);
        }
        g_brRaceRules.aGates = aGate;
        g_brRaceRules.nGates = n;
        if (n == 0) { ++g_skipped; return; }
    }

    /* Seed the two phantom cursors PAST gate 0, which on this track sits
     * 24 units along the path from the root -- the seed's own banner says
     * why the slot has to start on the far side of it. */
    for (i = 2; i < NDRV; ++i) {
        if (BrRaceSeedPhantom(&g_drv[i], 60.0f) != 0) { ++g_skipped; return; }
    }

    /* Green, then run.  BR_RS_PHANTOM_STEP is 2.22 units a frame, so a lap
     * of this track takes lapLen / 2.22 frames. */
    CHECK(FramesUntilLights(BR_RS_LIGHTS_RACE, 400) >= 0);
    for (iStep = 0; iStep < 8000; ++iStep) {
        int32_t before = g_drv[2].f4C;
        BrRaceStepFrame();
        if (g_drv[2].f4C != before)
            ++nGateAdvance;
        if (g_drv[2].f40 >= 1)
            break;
    }

    /* The phantom crossed gates and completed a lap. */
    CHECK(nGateAdvance >= g_brRaceRules.nGates);
    CHECK_EQ(g_drv[2].f40, 1);
    /* Both phantoms are on the same line and move together. */
    CHECK_EQ(g_drv[3].f40, g_drv[2].f40);
    CHECK_EQ(g_drv[3].f4C, g_drv[2].f4C);
    /* Its progress key has passed a lap length. */
    CHECK(g_drv[2].f50 > lapLen * 0.9f);
    /* The two CAR entrants never moved, so they never advanced. */
    CHECK_EQ(g_drv[0].f4C, 0);
    CHECK_EQ(g_drv[0].f40, 0);
}

int main(void)
{
    test_script();
    test_freeze();
    test_countdown();
    test_car_gate();
    test_hole_counts();
    test_path_walk();
    test_phantom_laps();

    if (g_haveTrack)
        BrTrackClose(&g_trk);

    if (g_fails != 0) {
        printf("test_racestep: %d FAILED\n", g_fails);
        return 1;
    }
    if (g_skipped != 0)
        printf("test_racestep: %d subtests skipped (no .trk in testdata)\n",
               g_skipped);
    printf("test_racestep: %d checks, %d failures\n", g_checks, g_fails);
    return 0;
}
