/* test_br_racestart.c -- Glide 0x100628B0 / D3D 0x10069840.
 *
 * The function has almost no arithmetic.  What it HAS is an order, a set of
 * literals, two mode-dependent arms and one block of stores whose offsets are
 * crossed.  So the suite is built around those four things and around the
 * globals that belong to other modules, which is where a decompilation of
 * this shape actually goes wrong.
 *
 * THE STAND-IN GLOBALS.  Ten of the globals this module writes are owned by
 * br_racestep.c, br_carphys.c and br_appstart.c.  br_racestart.c declares
 * them `extern` and never defines them -- that is the point, because the
 * original had one object per address.  Linking those three modules here
 * would drag their closures (the collision grid, the replay packer, the
 * DirectPlay tags) behind a 525-byte function, so this suite supplies the
 * STORAGE instead, exactly as build.sh describes.  The real link uses the
 * owners'; nothing about the module changes.
 *
 * NOT asserted, because it is not observable:
 *   - the ORDER of the three 0xFFFF word stores (+0xF2 before +0xF0 before
 *     +0xF4).  All three write the same value, so no reader can tell.  Which
 *     offsets are written IS asserted, byte-exactly.
 *   - the two redundant null-step installs at 0x1006294F and 0x10062958.
 *     Nothing calls out between them and the race-step install, so no hook
 *     can see the slot in that state.  BrRaceEntrantCountSet is tested
 *     directly instead, which is where the pairing lives.
 */
#include "br_racestart.h"
#include "br_gamestep.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ==================================================================== *
 * STAND-IN STORAGE for globals three other modules own.  See the banner.
 * ==================================================================== */
int32_t g_brRaceTick;           /* 0x10226A44  br_racestep.h */
int32_t g_brRaceNCar;           /* 0x100B2F04  br_racestep.h */
int32_t g_brRaceNEntrant;       /* 0x100B3858  br_racestep.h */
int32_t g_brCarPhysWeather;     /* 0x104B15E8  br_carphys.h  */
int32_t g_brCfgGameMode;        /* 0x100A9360  br_appstart.h */
int32_t g_brCfgChosenWeather;   /* 0x10226E80  br_appstart.h */
int32_t g_brCfgHandlingType;    /* 0x1007B320  br_appstart.h */
int32_t g_brCfgTransmission;    /* 0x1007B324  br_appstart.h */
int32_t g_brCfgSuspensionType;  /* 0x1007B328  br_appstart.h */
int32_t g_brCfgTireType;        /* 0x1007B32C  br_appstart.h */

/* ==================================================================== *
 * Recording hooks
 * ==================================================================== */
static char           g_szLog[2048];
static unsigned char  g_abRec[0x200];
static int            g_fRecNull;          /* make pfnEquipRecord say NULL */
static int32_t        g_aPhase[8];
static int            g_cLoad;
static int            g_aLoadPerPhase[2];
static int32_t        g_loadReturn;        /* what pfnLoadStep answers with */
static int            g_loadAfter;         /* ...after this many calls      */
static uint32_t       g_aTrace[8][2];
static int            g_cTrace;
static int32_t        g_weatherDuringLoad;
static uint32_t       g_6EECC8DuringLoad;
static int            g_stepIsRaceDuringLoad;
static int32_t        g_nEntrantBefore10059E00;

static void logs(const char *psz)
{
    size_t n = strlen(g_szLog);
    snprintf(g_szLog + n, sizeof g_szLog - n, "%s%s", (n != 0) ? "," : "", psz);
}

/* The two bodies standing for 0x10019A70 and 0x10008D60. */
static int g_cRaceStepRan, g_cNullStepRan;
static void f_raceStep(void) { ++g_cRaceStepRan; }
static void f_nullStep(void) { ++g_cNullStepRan; }

static void h_703A0(void *p)  { (void)p; logs("703A0"); }
static void h_2DEC3(void *p)  { (void)p; logs("2DEC3"); }
static void h_2E334(void *p)  { (void)p; logs("2E334"); }
static void h_2E2E3(void *p)  { (void)p; logs("2E2E3"); }
static void h_62870(void *p)  { (void)p; logs("62870"); }
static void h_2E136(void *p)  { (void)p; logs("2E136"); }
static void h_2E32F(void *p)  { (void)p; logs("2E32F"); }

static int32_t g_cursorArg = -1;
static void h_cursor(void *p, int32_t v) { (void)p; g_cursorArg = v;
                                           logs("cursor"); }

static unsigned char *h_rec(void *p)
{
    (void)p;
    logs("rec");
    return g_fRecNull ? NULL : g_abRec;
}

static int32_t h_load(void *p, int32_t phase, int32_t a2)
{
    (void)p;
    if (g_cLoad < 8) {
        g_aPhase[g_cLoad] = phase;
    }
    if (phase == BR_RACESTART_LOAD_PHASE_A) { ++g_aLoadPerPhase[0]; }
    if (phase == BR_RACESTART_LOAD_PHASE_B) { ++g_aLoadPerPhase[1]; }
    ++g_cLoad;
    CHECK(a2 == 1);            /* the literal, NOT the macro -- see below */

    /* Sampled here because this is the first hook that runs AFTER the race
     * step is installed and after the interim weather value is written. */
    g_weatherDuringLoad     = g_brCarPhysWeather;
    g_6EECC8DuringLoad      = g_brRace6EECC8;
    g_stepIsRaceDuringLoad  = (BrGameStepGet() == f_raceStep);

    logs("load");
    return (g_cLoad > g_loadAfter) ? g_loadReturn : 0;
}

static void h_59E00(void *p)
{
    (void)p;
    g_nEntrantBefore10059E00 = g_brRaceNEntrant;
    /* Poisoned so the LATER `if (mode == 0 || mode == 2)` store is visible
     * as a store rather than as a value that happened to already be 1. */
    g_brRaceNEntrant = 99;
    logs("59E00");
}

static void h_sel(void *p) { (void)p; logs("sel"); }

static void h_trace(void *p, uint32_t a1, uint32_t a2)
{
    (void)p;
    if (g_cTrace < 8) { g_aTrace[g_cTrace][0] = a1; g_aTrace[g_cTrace][1] = a2; }
    ++g_cTrace;
    logs("trace");
}

static BrRaceStartOps g_ops;

static void arm(int32_t mode)
{
    memset(&g_ops, 0, sizeof g_ops);
    g_ops.pfn100703A0     = h_703A0;
    g_ops.pfn1002DEC3     = h_2DEC3;
    g_ops.pfn1002E334     = h_2E334;
    g_ops.pfn1002E2E3     = h_2E2E3;
    g_ops.pfn10062870     = h_62870;
    g_ops.pfn1002E136     = h_2E136;
    g_ops.pfnCursorPairSet = h_cursor;
    g_ops.pfnEquipRecord  = h_rec;
    g_ops.pfnLoadStep     = h_load;
    g_ops.pfn10059E00     = h_59E00;
    g_ops.pfnSelLookup    = h_sel;
    g_ops.pfnTrace        = h_trace;
    g_ops.pfn1002E32F     = h_2E32F;

    g_szLog[0] = '\0';
    memset(g_abRec, 0xAA, sizeof g_abRec);
    g_fRecNull = 0;
    g_cLoad = 0;
    g_aLoadPerPhase[0] = g_aLoadPerPhase[1] = 0;
    g_loadReturn = 1;
    g_loadAfter = 0;          /* done on the first call of each phase */
    g_cTrace = 0;
    g_cursorArg = -1;
    g_cRaceStepRan = g_cNullStepRan = 0;
    g_weatherDuringLoad = -1;
    g_6EECC8DuringLoad = 0;
    g_stepIsRaceDuringLoad = -1;
    g_nEntrantBefore10059E00 = -1;

    g_brRaceTick = 0x5A;
    g_brRaceNCar = 0x5A;
    g_brRaceNEntrant = 0x5A;
    g_brCarPhysWeather = 0x5A;
    g_brCfgGameMode = mode;
    g_brCfgChosenWeather = 3;
    g_brCfgHandlingType   = 0x11111111;
    g_brCfgTransmission   = 0x22222222;
    g_brCfgSuspensionType = 0x33333333;
    g_brCfgTireType       = 0x44444444;

    g_brRaceB71A68 = 0x1234;
    g_brRaceB71A6C = 0x5678;
    g_brRace6EC760 = 0;
    g_brRace6E9A34 = 0;
    g_brRace18EEED8 = 7; g_brRace5CCB80 = 7; g_brRace6ED6DC = 7;
    g_brRaceB71288 = 7; g_brRace4ABB20 = 7; g_brRace4ABB24 = 7;
    g_brRace5BC8D8 = 0; g_brRace6EECC8 = 0;

    BrGameStepSet(NULL);
    BrRaceStartResetForTest();
}

static uint32_t rd32(const unsigned char *pb, int off)
{
    return (uint32_t)pb[off] | ((uint32_t)pb[off + 1] << 8)
         | ((uint32_t)pb[off + 2] << 16) | ((uint32_t)pb[off + 3] << 24);
}

/* ================================================================== *
 * 1.  The order, on the mode that takes neither special arm.
 * ================================================================== */
static void test_order(void)
{
    static const char szExpect[] =
        "703A0,2DEC3,2E334,2E2E3,62870,2E136,cursor,"      /* the head      */
        "trace,load,"                                       /* phase 0      */
        "trace,load,"                                       /* phase 2      */
        "59E00,"
        "rec,"                                              /* equipment    */
        "trace,trace,trace,trace,"                          /* the four     */
        "2E32F";

    arm(3);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);

    CHECK(strcmp(g_szLog, szExpect) == 0);
    if (strcmp(g_szLog, szExpect) != 0) {
        printf("     log was: %s\n", g_szLog);
    }

    /* The two plain copies at the very top. */
    CHECK(g_brRace6EC760 == 0x1234);
    CHECK(g_brRace6E9A34 == 0x5678);

    /* The six zeroing stores and the one literal 8. */
    CHECK(g_brRace18EEED8 == 0 && g_brRace5CCB80 == 0);
    CHECK(g_brRace6ED6DC == 0 && g_brRaceB71288 == 0);
    CHECK(g_brRace4ABB20 == 0 && g_brRace4ABB24 == 0);
    CHECK(g_brRace5BC8D8 == 8);

    CHECK(g_cursorArg == 0);
    CHECK(g_brRaceNCar == 2);

    /* 0x1002F6C0 ran, and it ran before the first load call. */
    CHECK(g_brRace6EECC8 == 0x80096400u);
    CHECK(g_6EECC8DuringLoad == 0x80096400u);

    /* The race step is installed before the loading loops run, not after --
     * which is what makes the loading screen a step of the race. */
    CHECK(g_stepIsRaceDuringLoad == 1);
    CHECK(BrGameStepGet() == f_raceStep);

    /* Neither body was CALLED; they were only installed. */
    CHECK(g_cRaceStepRan == 0 && g_cNullStepRan == 0);

    /* Mode 3 takes neither the mode-0 lookup nor the 0/2 entrant store, so
     * the poisoned value survives.  This is what separates "the store
     * happened" from "the value was already 1". */
    CHECK(g_nEntrantBefore10059E00 == 1);
    CHECK(g_brRaceNEntrant == 99);
}

/* ================================================================== *
 * 2.  g_brRaceTick is `mode == 4`, not the mode.
 * ================================================================== */
static void test_tick(void)
{
    arm(4);  BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_brRaceTick == 1);

    arm(3);  BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_brRaceTick == 0);

    arm(0);  BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_brRaceTick == 0);

    /* 5 is neither 4 nor a flag-ish value; a `!= 0` reading would give 1. */
    arm(5);  BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_brRaceTick == 0);
}

/* ================================================================== *
 * 3.  The two mode arms.
 * ================================================================== */
static void test_modes(void)
{
    /* Mode 0: the lookup runs AND the entrant count is restored. */
    arm(0);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(strstr(g_szLog, "59E00,sel,") != NULL);
    CHECK(g_brRaceNEntrant == 1);

    /* Mode 2: no lookup, but the entrant count IS restored -- mode 0 falls
     * through into mode 2's store, and only these two reach it. */
    arm(2);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(strstr(g_szLog, "sel") == NULL);
    CHECK(g_brRaceNEntrant == 1);

    /* Mode 1: no lookup, no restore -- and it DOES take the 0xFFFF arm. */
    arm(1);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(strstr(g_szLog, "sel") == NULL);
    CHECK(g_brRaceNEntrant == 99);
}

/* ================================================================== *
 * 4.  The 0xFFFF words: which modes, which offsets, and nothing else.
 * ================================================================== */
static void test_ffff(void)
{
    int i;

    arm(1);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    /* Two `rec` fetches on this arm: the words, then the equipment. */
    CHECK(g_abRec[0xF0] == 0xFF && g_abRec[0xF1] == 0xFF);
    CHECK(g_abRec[0xF2] == 0xFF && g_abRec[0xF3] == 0xFF);
    CHECK(g_abRec[0xF4] == 0xFF && g_abRec[0xF5] == 0xFF);
    /* Exactly six bytes, not eight: +0xF6 must be untouched. */
    CHECK(g_abRec[0xF6] == 0xAA && g_abRec[0xF7] == 0xAA);
    /* ...and nothing below +0xF0 either. */
    for (i = 0; i < 0xF0; i++) {
        if (g_abRec[i] != 0xAA) { CHECK(g_abRec[i] == 0xAA); break; }
    }

    arm(6);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_abRec[0xF0] == 0xFF && g_abRec[0xF4] == 0xFF);

    /* Every other mode leaves them alone.  5 is between the two so an
     * `mode >= 1 && mode <= 6` reading would fail here. */
    arm(5);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_abRec[0xF0] == 0xAA && g_abRec[0xF2] == 0xAA &&
          g_abRec[0xF4] == 0xAA);
    arm(0);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_abRec[0xF0] == 0xAA);
}

/* ================================================================== *
 * 5.  The equipment block, and the crossed pair.
 * ================================================================== */
static void test_equipment(void)
{
    arm(3);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);

    CHECK(rd32(g_abRec, BR_RACESTART_OFF_HANDLING)     == 0x11111111u);
    CHECK(rd32(g_abRec, BR_RACESTART_OFF_TRANSMISSION) == 0x22222222u);
    /* The crossed pair: SuspensionType (0x1007B328) lands at +0x104 and
     * TireType (0x1007B32C) at +0x100.  Reading the four config globals in
     * address order would put them the other way round, which is the single
     * most likely transcription error in this function. */
    CHECK(rd32(g_abRec, BR_RACESTART_OFF_TIRE)         == 0x44444444u);
    CHECK(rd32(g_abRec, BR_RACESTART_OFF_SUSPENSION)   == 0x33333333u);

    /* Four dwords, not five: slice5_60's +0x108 is NOT written here. */
    CHECK(g_abRec[0x108] == 0xAA && g_abRec[0x10B] == 0xAA);

    /* Little-endian, byte-wise, because this record is written to disc
     * verbatim as br_save.h's payload. */
    arm(3);
    g_brCfgHandlingType = 0x01020304;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_abRec[0xF8] == 0x04 && g_abRec[0xF9] == 0x03 &&
          g_abRec[0xFA] == 0x02 && g_abRec[0xFB] == 0x01);

    /* A record that is not loaded yet is a state the original has.  Nothing
     * is written and nothing crashes; the step is counted as reached. */
    arm(1);
    g_fRecNull = 1;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_abRec[0xF0] == 0xAA && g_abRec[0xF8] == 0xAA);
}

/* ================================================================== *
 * 6.  Weather is written twice, and the second write wins.
 * ================================================================== */
static void test_weather(void)
{
    arm(3);
    g_brCfgChosenWeather = 3;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);

    /* The interim literal 1 at 0x10062986, sampled from inside the loading
     * loop -- otherwise the first of the two stores is invisible. */
    CHECK(g_weatherDuringLoad == 1);
    /* ...and the config value at 0x10062A2D. */
    CHECK(g_brCarPhysWeather == 3);
}

/* ================================================================== *
 * 7.  The loading loops: two phases, the right numbers, and `test al,al`.
 * ================================================================== */
static void test_load(void)
{
    arm(3);
    g_loadAfter = 0;                 /* done immediately */
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_cLoad == 2);
    /* The LITERAL phase numbers, not the macros.  Comparing a recorded
     * value against the same macro the module used is a tautology that
     * passes whatever the macro says -- mutation testing caught exactly
     * that here, and every constant this suite checks is now a literal. */
    CHECK(g_aPhase[0] == 0);
    CHECK(g_aPhase[1] == 2);
    CHECK(BrRaceStartSpun() == 0);

    /* Not done for two calls: three calls in the first phase, one in the
     * second -- the loops are independent. */
    arm(3);
    g_loadAfter = 2;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(g_aLoadPerPhase[0] == 3);
    CHECK(g_aLoadPerPhase[1] == 1);

    /* `test al,al` looks at the LOW BYTE only.  0x100 is non-zero as an int
     * and zero as a byte, so the original would keep spinning. */
    arm(3);
    g_loadReturn = 0x100;
    g_loadAfter = 0;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(BrRaceStartSpun() == 2);
    CHECK(g_aLoadPerPhase[0] == BR_RACESTART_LOAD_SPINS);

    /* 0x101 has the low bit set, so it IS done. */
    arm(3);
    g_loadReturn = 0x101;
    g_loadAfter = 0;
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);
    CHECK(BrRaceStartSpun() == 0);
    CHECK(g_cLoad == 2);
}

/* ================================================================== *
 * 8.  The six folded-empty calls and their arguments.
 * ================================================================== */
static void test_traces(void)
{
    arm(3);
    BrRaceStart(&g_ops, f_raceStep, f_nullStep);

    CHECK(g_cTrace == 6);
    /* Two zero-argument calls, one inside each loading block. */
    CHECK(g_aTrace[0][0] == 0u && g_aTrace[0][1] == 0u);
    CHECK(g_aTrace[1][0] == 0u && g_aTrace[1][1] == 0u);
    /* Then the four at the tail, in order. */
    CHECK(g_aTrace[2][0] == 0x80025C00u && g_aTrace[2][1] == 0x10B25798u);
    CHECK(g_aTrace[3][1] == 0x15F88u);   /* sizeof(UltraCarHeader) == 89992 */
    CHECK(g_aTrace[4][1] == 0x2B68u);    /* sizeof(Vehicle)                 */
    CHECK(g_aTrace[5][1] == 0x80u);      /* sizeof(Enemy)                   */
    CHECK(g_aTrace[3][0] == 0x100B3884u);
    CHECK(g_aTrace[4][0] == 0x100B3870u);
    CHECK(g_aTrace[5][0] == 0x100B385Cu);
}

/* ================================================================== *
 * 9.  0x10062850 on its own: one store and one install, together.
 * ================================================================== */
static void test_entrantset(void)
{
    BrGameStepSet(f_raceStep);
    g_brRaceNEntrant = 0x5A;
    BrRaceEntrantCountSet(7, f_nullStep);
    CHECK(g_brRaceNEntrant == 7);
    CHECK(BrGameStepGet() == f_nullStep);

    g_brRace6EECC8 = 0;
    BrRaceSub1002F6C0();
    CHECK(g_brRace6EECC8 == 0x80096400u);
}

/* ================================================================== *
 * 10. An unwired run counts every site and invents nothing.
 * ================================================================== */
static void test_unwired(void)
{
    BrRaceStartOps ops;

    memset(&ops, 0, sizeof ops);
    g_brCfgGameMode = 1;               /* so the 0xFFFF arm is reached too */
    BrRaceStartResetForTest();
    BrGameStepSet(NULL);
    BrRaceStart(&ops, f_raceStep, f_nullStep);

    CHECK(BrRaceStartSkipped(BR_RACESTART_100703A0) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_1002DEC3) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_1002E334) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_1002E2E3) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_10062870) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_1002E136) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_CURSORPAIR) == 1);
    /* Mode 1 fetches the record twice: the words and the equipment. */
    CHECK(BrRaceStartSkipped(BR_RACESTART_EQUIPRECORD) == 2);
    CHECK(BrRaceStartSkipped(BR_RACESTART_10059E00) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_SELLOOKUP) == 0);  /* mode 1 */
    CHECK(BrRaceStartSkipped(BR_RACESTART_1002E32F) == 1);
    CHECK(BrRaceStartSkipped(BR_RACESTART_TRACE) == 6);

    /* With no load hook the spin is never satisfied, so both phases hit the
     * bound.  That is the port's bound, not the original's behaviour. */
    CHECK(BrRaceStartSpun() == 2);
    CHECK(BrRaceStartSkipped(BR_RACESTART_LOADSTEP)
          == 2 * BR_RACESTART_LOAD_SPINS);

    /* Everything that does not depend on a hook still happened. */
    CHECK(g_brRaceNCar == 2);
    CHECK(g_brRace5BC8D8 == 8);
    CHECK(BrGameStepGet() == f_raceStep);
}

int main(void)
{
    test_order();
    test_tick();
    test_modes();
    test_ffff();
    test_equipment();
    test_weather();
    test_load();
    test_traces();
    test_entrantset();
    test_unwired();

    printf("test_br_racestart: %d failures\n", g_fails);
    return (g_fails == 0) ? 0 : 1;
}
