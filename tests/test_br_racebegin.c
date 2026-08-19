/* test_br_racebegin.c -- Glide 0x10019A70's opening and one-time arm, and the
 * seven small functions below it.
 *
 * WHAT IS WORTH ASSERTING HERE.  The arm has almost no arithmetic.  What it
 * has is an ORDER, a seven-way SWITCH, a set of LITERALS, and one genuine
 * round trip -- the eight-byte replay header, whose writer and reader are both
 * in this function.  So the suite is built round those, and every expected
 * value below was derived from the DISASSEMBLY, never from a banner
 * (CONVENTIONS.md records three assertions in this tree that encode the wrong
 * answer because they were written from a comment).
 *
 * THE ROUND TRIP IS THE STRONGEST ASSERTION AVAILABLE, and it is why it is
 * here: the mode-2 arm reads a car's equipment into eight bytes and the mode-4
 * arm puts eight bytes back onto a car.  Any single-byte error in either
 * direction -- a wrong offset, a crossed pair, a signedness slip -- breaks it,
 * and NEITHER end can be adjusted to hide a mistake in the other, because the
 * two are separate blocks 0xB00 bytes apart in the original.
 *
 * NOT asserted, because it is not observable:
 *   - the order of the three colour submissions in BrRaceHudFrame.  All three
 *     go to 0x10008D60, a bare `ret`.  The COUNT is asserted, because the
 *     count is the evidence the arm reached those points.
 *   - the three 0xFF bytes at car+0x29AC..AE (0x10019E79) and the render block
 *     at 0x118EEF50 (0x10019D23).  Neither has a host model, and inventing one
 *     to assert against would be asserting against this file.
 */
#include "br_racebegin.h"
#include "br_gamestep.h"

/* Owned by br_appstart.c and br_carphys.c; br_racebegin.c declares them and
 * never defines them, so this suite supplies the STORAGE -- exactly as
 * test_br_racestart.c does, and for the same reason. */
int32_t g_brCfgChosenTrack;    /* 0x100B3014 */
int32_t g_brCfgChosenCar;      /* 0x10226E7C */
int32_t g_brCfgChosenWeather;  /* 0x10226E80 */
int32_t g_brCarPhysWeather;    /* 0x104B15E8 */

#include <stdio.h>
#include <string.h>
#include <math.h>

/* TEST-ONLY stand-ins for the OTHER halves of slice3_41.c, which is linked
 * here only for the BrDriverCar layout.  Same three test_racestep.c supplies,
 * for the same reason: the real bodies drag six further modules behind a
 * struct definition. */
int32_t g_Br0B380C = 0;         /* 0x100B380C, owned by slice2_19 */
float   g_f6C2CFC  = 0.0f;      /* 0x106C2CFC, owned by br_data   */

void BrFatal(const char *pszMsg);
void BrFatal(const char *pszMsg) { (void)pszMsg; }

int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD);
int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{ (void)pA; (void)pB; (void)pC; (void)pD; return 0; }

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } \
    else printf("  [PASS] %s\n", #c); } while (0)

/* ==================================================================== *
 * The fixture
 * ==================================================================== */

#define NCAR    4
#define NDRIVER 4

static BrDriverCar s_aCar[NCAR];
static BrDriver    s_aDrv[NDRIVER];
static BrRaceCtl   s_aCtl[NCAR];
static uint8_t     s_aEquip[NCAR][0x200];

static int32_t s_cModelSet;
static int32_t s_aModelSet[NCAR];
static int32_t s_cRampA, s_cRampB, s_cRampC;
static int32_t s_cTrace;
static int32_t s_c10019840, s_c10032E40;
static int32_t s_c1005F530;
static int32_t s_c1006E3F0, s_c1006E360;
static int32_t s_c1006F170;
static int32_t s_cTrackLoad, s_iTrackLoaded;
static uint32_t s_tick;
static float   s_sqrtArg;
static int32_t s_c1005C490;

static uint32_t op_tick(void)              { return s_tick; }
static void     op_trace(void)             { ++s_cTrace; }
static void     op_carmodel(BrDriverCar *pCar, int32_t m)
{
    int i;
    ++s_cModelSet;
    for (i = 0; i < NCAR; ++i)
        if (pCar == &s_aCar[i]) s_aModelSet[i] = m;
    /* DELIBERATELY does not write car+0x29A8.  br_racebegin.h's reading is
     * that 0x1006FD50 is what applies the model there, and a stand-in that
     * implemented that reading would make the round-trip test assert this
     * pass's own hypothesis instead of the original's bytes. */
}
static void     op_rampA(float a, float b) { (void)a; (void)b; ++s_cRampA; }
static void     op_rampB(float a, float b) { (void)a; (void)b; ++s_cRampB; }
static void     op_rampC(float a, float b) { (void)a; (void)b; ++s_cRampC; }
static void     op_19840(void)             { ++s_c10019840; }
static void     op_32E40(void)             { ++s_c10032E40; }
static void     op_5F530(BrDriver *p)      { (void)p; ++s_c1005F530; }
static void     op_6E3F0(void)             { ++s_c1006E3F0; }
static void     op_6E360(void)             { ++s_c1006E360; }
static void     op_6F170(BrDriverCar *p)   { (void)p; ++s_c1006F170; }
static void     op_trackload(int32_t i)    { ++s_cTrackLoad; s_iTrackLoaded = i; }
static float    op_sqrt(float x)           { s_sqrtArg = x; return (float)sqrt((double)x); }
static void     op_5C490(BrDriverCar *p)   { (void)p; ++s_c1005C490; }
static int32_t s_cNetAdd;
static uint32_t op_netcount(void) { return 2u; }
static int32_t  op_netnext(void)  { return 1; }
static void     op_netadd(int32_t i)
{
    (void)i;
    ++s_cNetAdd;
    /* 0x1001C6A0 is what makes the count move; without that the original
     * spins here too. */
    ++g_brRaceNCar;
}
static BrRaceSpecial s_aSpecial[6];
static int32_t       s_cSpecial;
static int32_t op_special(int32_t i, BrRaceSpecial *pOut)
{
    if (i < 0 || i >= s_cSpecial) return 0;
    *pOut = s_aSpecial[i];
    return 1;
}
static void     op_ctl_human(BrDriverCar *p){ (void)p; }
static void     op_ctl_ai(BrDriverCar *p)  { (void)p; }
static uint8_t *op_equip(BrDriverCar *pCar)
{
    int i;
    for (i = 0; i < NCAR; ++i)
        if (pCar == &s_aCar[i]) return s_aEquip[i];
    return NULL;
}
static BrRaceCtl *op_ctl(BrDriverCar *pCar)
{
    int i;
    for (i = 0; i < NCAR; ++i)
        if (pCar == &s_aCar[i]) return &s_aCtl[i];
    return NULL;
}

static void fixture(int32_t mode, int32_t nEntrant)
{
    memset(s_aCar, 0, sizeof(s_aCar));
    memset(s_aDrv, 0, sizeof(s_aDrv));
    memset(s_aCtl, 0, sizeof(s_aCtl));
    memset(s_aEquip, 0, sizeof(s_aEquip));
    memset(g_aBrRaceBeginRec, 0, sizeof(g_aBrRaceBeginRec));
    memset(g_aBrRaceBeginRecIn, 0, sizeof(g_aBrRaceBeginRecIn));
    memset(g_aBrRaceBeginRecHdr, 0, sizeof(g_aBrRaceBeginRecHdr));

    g_pBrRaceCar     = s_aCar;
    g_pBrRaceDriver  = s_aDrv;
    g_brRaceNCar     = NCAR;
    g_brRaceNDriver  = NDRIVER;
    g_brRaceNEntrant = nEntrant;
    g_brRaceRules.mode = mode;
    g_brRaceReplay   = 0;
    g_brRacePaused   = 0;
    g_brRaceNet      = 0;
    g_brRaceSubstate = 0;
    g_brRaceBeginStage = 0;
    g_brRaceBeginRecLen = BR_RACEBEGIN_HDR_LEN;
    g_brRaceClockCursor = -1;
    g_brRaceClockLen    = BR_RACEBEGIN_CLOCK_LEN;
    g_brRaceClockLast   = 0;
    g_brRaceClockAccum  = 0;
    g_brRaceClockCount  = 0;

    s_cModelSet = s_cRampA = s_cRampB = s_cRampC = s_cTrace = 0;
    s_c10019840 = s_c10032E40 = s_c1005F530 = 0;
    s_c1006E3F0 = s_c1006E360 = s_c1006F170 = 0;
    s_cTrackLoad = 0; s_iTrackLoaded = -1;
    s_c1005C490 = 0; s_cNetAdd = 0;
    memset(s_aModelSet, -1, sizeof(s_aModelSet));

    memset(&g_brRaceBeginOps, 0, sizeof(g_brRaceBeginOps));
    g_brRaceBeginOps.pfnTickMs       = op_tick;
    g_brRaceBeginOps.pfnTrace        = op_trace;
    g_brRaceBeginOps.pfnCarModelSet  = op_carmodel;
    g_brRaceBeginOps.pfnRampA        = op_rampA;
    g_brRaceBeginOps.pfnRampB        = op_rampB;
    g_brRaceBeginOps.pfnRampC        = op_rampC;
    g_brRaceBeginOps.pfn10019840     = op_19840;
    g_brRaceBeginOps.pfn10032E40     = op_32E40;
    g_brRaceBeginOps.pfn1005F530     = op_5F530;
    g_brRaceBeginOps.pfn1006E3F0     = op_6E3F0;
    g_brRaceBeginOps.pfn1006E360     = op_6E360;
    g_brRaceBeginOps.pfn1006F170     = op_6F170;
    g_brRaceBeginOps.pfnTrackLoad    = op_trackload;
    g_brRaceBeginOps.pfnSqrt         = op_sqrt;
    g_brRaceBeginOps.pfn1005C490     = op_5C490;
    g_brRaceBeginOps.pfnCtlHuman     = op_ctl_human;
    g_brRaceBeginOps.pfnCtlAi        = op_ctl_ai;
    g_brRaceBeginOps.pfnEquipRecord  = op_equip;
    g_brRaceBeginOps.pfnCtl          = op_ctl;
    g_brRaceBeginOps.pfnSpecial      = op_special;
    s_cSpecial = 0;
    memset(s_aSpecial, 0, sizeof(s_aSpecial));
    g_brRaceBeginSpecialsN = 0;
    BrRaceBeginReset();
}

/* ==================================================================== *
 * The frame clock, 0x10019A70..0x10019AF8
 * ==================================================================== */
static void test_clock(void)
{
    int i;

    printf("the frame clock (0x10019A70)\n");

    fixture(3, 1);
    g_brRaceClockCursor = -1;         /* 0x100A935C ships as -1 */
    g_brRaceClockLen    = 5;          /* 0x100A9358 ships as 5  */
    g_brRaceClockLast   = 1000u;
    s_tick              = 1017u;
    memset(g_aBrRaceClockRing, 0xAB, sizeof(g_aBrRaceClockRing));

    (void)BrRaceStepClock();

    /* 0x10019AAB `rep stosd` with ecx == len.  DWORDS, not bytes -- so
     * exactly five slots move and the sixth does not. */
    for (i = 0; i < 5; ++i)
        if (g_aBrRaceClockRing[i] != 17u) break;
    CHECK(i == 5);
    CHECK(g_aBrRaceClockRing[5] == 0xABABABABu);

    /* 0x10019ACC `inc eax` from eax == len, then `cmp eax,esi / jl`: len+1 is
     * not less than len, so the cursor lands at 0. */
    CHECK(g_brRaceClockCursor == 0);

    /* 0x10019A84 replaces the stamp with the value just read. */
    CHECK(g_brRaceClockLast == 1017u);

    /* A second frame does NOT re-prime: the cursor is no longer negative. */
    s_tick = 1050u;
    memset(g_aBrRaceClockRing, 0, sizeof(g_aBrRaceClockRing));
    (void)BrRaceStepClock();
    CHECK(g_brRaceClockCursor == 1);
    CHECK(g_aBrRaceClockRing[1] == 33u);
    CHECK(g_aBrRaceClockRing[0] == 0u);   /* untouched */

    /* The cursor wraps at len, not at the array's size. */
    for (i = 0; i < 4; ++i) { s_tick += 33u; (void)BrRaceStepClock(); }
    CHECK(g_brRaceClockCursor == 0);
}

static void test_clock_accum(void)
{
    printf("the accumulator's `inc` then `je` (0x10019AB5)\n");

    fixture(3, 1);
    g_brRaceClockAccum = 999u;
    BrRaceClockReset();

    /* 0x10019A4A stores -1, NOT 0.  That is what makes the NEXT frame the one
     * that clears the total: `inc ecx / je`. */
    CHECK(g_brRaceClockCount == -1);
    CHECK(g_brRaceClockAccum == 999u);   /* the reset itself clears nothing */

    g_brRaceClockLast = 0u; s_tick = 40u;
    (void)BrRaceStepClock();
    CHECK(g_brRaceClockCount == 0);
    CHECK(g_brRaceClockAccum == 0u);     /* cleared, not += 40 */

    s_tick = 70u;
    (void)BrRaceStepClock();
    CHECK(g_brRaceClockCount == 1);
    CHECK(g_brRaceClockAccum == 30u);    /* now it adds */

    /* 0x10019A54 raises the limiter's own flag. */
    CHECK(g_brRaceBeginLimitOn == 1);
    /* 0x10019A5E is a TAIL call, so it always runs. */
    CHECK(s_c1006E3F0 == 1 && s_c1006E360 == 1);
}

static void test_clock_substate(void)
{
    printf("the substate branch (0x10019AE4 / 0x10019AF8)\n");

    fixture(3, 1);
    g_brRaceSubstate = 0;
    CHECK(BrRaceStepClock() != 0);       /* -> the one-time arm  */
    g_brRaceSubstate = 1;
    CHECK(BrRaceStepClock() == 0);       /* -> the per-frame arm */
    /* Every frame stamps the clock, whichever arm it takes. */
    CHECK(g_brRaceClockLast == s_tick);
}

/* ==================================================================== *
 * The mode switch, 0x10019BC1's table at 0x1001C648
 * ==================================================================== */
static void test_mode_table(void)
{
    printf("the mode jump table (0x1001C648)\n");

    /* mode 0 -- 0x10019BD3 stores 0x14 and 0x10019BDD stores 3. */
    fixture(0, 1);  BrRaceStepBegin();
    CHECK(g_brRaceNDriver == 20 && g_brRaceNCar == 3);

    /* mode 1 -- 0x10019BF7 / 0x10019C03, both 2; and TWO model calls, the
     * second on car[1] (0x10019C14's 0x10AF3D70). */
    fixture(1, 1);  BrRaceStepBegin();
    CHECK(g_brRaceNDriver == 2 && g_brRaceNCar == 2);
    CHECK(s_aModelSet[0] >= 0 && s_aModelSet[1] >= 0);

    /* mode 5 -- 0x10019D87..0x10019D93, all three set to 1. */
    fixture(5, 3);  BrRaceStepBegin();
    CHECK(g_brRaceNEntrant == 1 && g_brRaceNDriver == 1 && g_brRaceNCar == 1);

    /* mode 3 IS the default arm: 0x1001C654 holds 0x1001A1EE, the same target
     * 0x10019BBB's `ja` uses.  Both take the entrant count. */
    fixture(3, 2);  BrRaceStepBegin();
    CHECK(g_brRaceNDriver == 2 && g_brRaceNCar == 2);
    fixture(9, 2);  BrRaceStepBegin();        /* out of range -> `ja` */
    CHECK(g_brRaceNDriver == 2 && g_brRaceNCar == 2);

    /* mode 6 -- 0x10019C41's `je` puts the ONES on the CLEAR side. */
    fixture(6, 1);  g_brRaceBegin226A4C = 0;  BrRaceStepBegin();
    CHECK(g_brRaceNDriver == 1 && g_brRaceNCar == 1);
    fixture(6, 1);  g_brRaceBegin226A4C = 1;  BrRaceStepBegin();
    CHECK(g_brRaceNDriver == 0 && g_brRaceNCar == 0);
    g_brRaceBegin226A4C = 0;
}

static void test_mode5_track(void)
{
    printf("mode 5's fixed track (0x10019B3B / 0x1001A28C)\n");

    /* 0x1001A28A `cmp eax,esi` with esi == 5, then `mov eax,0xC` BEFORE the
     * `je`, so mode 5 loads 0xC and every other mode the chosen track. */
    fixture(5, 1);  g_brCfgChosenTrack = 7;  BrRaceStepBegin();
    CHECK(s_iTrackLoaded == BR_RACEBEGIN_MODE5_TRACK);

    fixture(3, 1);  g_brCfgChosenTrack = 7;  BrRaceStepBegin();
    CHECK(s_iTrackLoaded == 7);
}

/* ==================================================================== *
 * THE ROUND TRIP -- the eight-byte replay header
 * ==================================================================== */
static void test_header_roundtrip(void)
{
    int i;

    printf("the eight-byte replay header, both ends (0x1001A0FC / 0x10019E94)\n");

    /* --- WRITE, the mode-2 arm ------------------------------------------ */
    fixture(2, 1);
    g_brCfgChosenTrack   = 5;
    g_brCfgChosenWeather = 3;
    s_aCar[0].f29A8      = 11;          /* the APPLIED model -- 0x1001A127 */
    s_aCar[0].f29A4      = 99;          /* the requested one is NOT read   */
    s_aEquip[0][BR_RACEBEGIN_EQ_HANDLING] = 1;
    s_aEquip[0][BR_RACEBEGIN_EQ_TRANS]    = 2;
    s_aEquip[0][BR_RACEBEGIN_EQ_TIRE]     = 3;
    s_aEquip[0][BR_RACEBEGIN_EQ_SUSP]     = 4;
    s_aEquip[0][BR_RACEBEGIN_EQ_FIFTH]    = 5;
    /* Make the incoming record acceptable so the arm takes its OK path. */
    g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_TRACK]   = 5;
    g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_WEATHER] = 3;

    BrRaceStepBegin();

    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_TRACK]    == 5);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_CARMODEL] == 11);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_HANDLING] == 1);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_TRANS]    == 2);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_TIRE]     == 3);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_SUSP]     == 4);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_EQUIP5]   == 5);
    CHECK(g_aBrRaceBeginRecHdr[0][BR_RACEBEGIN_HDR_WEATHER]  == 3);

    /* 0x1001A058 / 0x1001A097 -- the acceptance path's two stores. */
    CHECK(s_aCtl[1].f48 == BR_RACEBEGIN_REC_LEN0);
    CHECK(s_aCtl[1].f4C == g_brRaceBeginRecLen);
    /* 0x1001A1CA / 0x1001A1D8 */
    CHECK(s_aCtl[0].aLen[0] == BR_RACEBEGIN_REC_LEN0);
    CHECK(s_aCtl[0].aCap[0] == BR_RACEBEGIN_REC_CAP);

    /* 0x10019F5C's header and 0x105BC8E0's source are DIFFERENT objects, and
     * the only way to see that is to stop the copy: with a zero length the
     * acceptance tests at 0x1001A018 read whatever the slot header already
     * held, not the source.  Aliasing the two makes a mismatching slot look
     * acceptable. */
    {
        uint8_t keep[BR_RACEBEGIN_HDR_LEN];
        memcpy(keep, g_aBrRaceBeginRecHdr[0], sizeof(keep));

        fixture(2, 1);
        g_brCfgChosenTrack   = 5;
        g_brCfgChosenWeather = 3;
        g_brRaceBeginRecLen  = 0;                 /* the copy moves nothing */
        g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_TRACK]     = 5;   /* would match  */
        g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_WEATHER]   = 3;
        g_aBrRaceBeginRecIn[BR_RACEBEGIN_HDR_TRACK]   = 4;   /* does not     */
        g_aBrRaceBeginRecIn[BR_RACEBEGIN_HDR_WEATHER] = 3;
        BrRaceStepBegin();
        /* 0x1001A0CD / 0x1001A0D3 -- the WRONG TRACK arm. */
        CHECK(g_brRaceNCar == 1 && g_brRaceNDriver == 1);
        CHECK(s_aCtl[1].pHdr == NULL);            /* 0x1001A0CA */

        memcpy(g_aBrRaceBeginRecHdr[0], keep, sizeof(keep));
    }

    /* --- keep the eight bytes ------------------------------------------- */
    {
        uint8_t saved[BR_RACEBEGIN_HDR_LEN];
        memcpy(saved, g_aBrRaceBeginRecHdr[0], sizeof(saved));

        /* --- READ, the mode-4 arm -------------------------------------- */
        fixture(4, 1);
        g_brRaceBeginStage = 9;         /* past 0..2, so no movie is played */
        g_brCfgChosenTrack   = 0;
        g_brCfgChosenWeather = 0;
        memcpy(g_aBrRaceBeginRec, saved, sizeof(saved));

        BrRaceStepBegin();

        CHECK(g_brCfgChosenTrack   == 5);
        CHECK(s_aCar[0].f29A4      == 11);   /* the REQUEST field          */
        CHECK(s_aCar[0].fE98       == 1);    /* handling                   */
        CHECK(s_aCar[0].fE9C       == 2);    /* transmission               */
        CHECK(s_aCar[0].fE90       == 3);    /* tire                       */
        CHECK(s_aCar[0].fE94       == 4);    /* suspension                 */
        CHECK(s_aCtl[0].b25        == 5);    /* equip +0x108               */
        CHECK(g_brCfgChosenWeather == 3);
        /* 0x10019F04 mirrors the weather into the physics global too. */
        CHECK(g_brCarPhysWeather   == 3);
        /* 0x10019EAC hands the SAME byte to the model setter. */
        CHECK(s_aModelSet[0] == 11);

        (void)i;
    }
}

static void test_header_signed(void)
{
    printf("the header bytes are read SIGNED (`movsx`, 0x10019EBD)\n");

    /* Every read of the header uses `movsx`, so 0xFF is -1, not 255.  A
     * `movzx` transcription passes every test above and fails this one. */
    fixture(4, 1);
    g_brRaceBeginStage = 9;
    memset(g_aBrRaceBeginRec, 0xFF, sizeof(g_aBrRaceBeginRec));
    BrRaceStepBegin();
    CHECK(g_brCfgChosenTrack == -1);
    CHECK(s_aCar[0].fE98     == -1);
    CHECK(s_aCar[0].f29A4    == -1);
    /* ctl->b25 is a BYTE copy (`mov dl,[ecx+6] / mov [eax+0x25],dl`), so it
     * keeps 0xFF rather than sign-extending. */
    CHECK(s_aCtl[0].b25 == 0xFFu);
}

/* ==================================================================== *
 * The common tail
 * ==================================================================== */
static void test_equipment_copy(void)
{
    printf("the equipment copy, and the CROSSED pair (0x1001A490)\n");

    fixture(3, 2);
    s_aEquip[0][BR_RACEBEGIN_EQ_HANDLING] = 10;
    s_aEquip[0][BR_RACEBEGIN_EQ_TRANS]    = 20;
    s_aEquip[0][BR_RACEBEGIN_EQ_TIRE]     = 30;
    s_aEquip[0][BR_RACEBEGIN_EQ_SUSP]     = 40;
    BrRaceStepBegin();

    /* 0x1001A498 +0xFC -> car+0xE9C, 0x1001A4A3 +0x104 -> car+0xE94,
     * 0x1001A4AE +0x100 -> car+0xE90, 0x1001A4B9 +0xF8 -> car+0xE98. */
    CHECK(s_aCar[0].fE9C == 20);
    CHECK(s_aCar[0].fE94 == 40);
    CHECK(s_aCar[0].fE90 == 30);
    CHECK(s_aCar[0].fE98 == 10);
}

static void test_equipment_defaults(void)
{
    printf("the defaults past the entrant count (0x1001A568)\n");

    /* Mode 3's own arm collapses the car count to the entrant count
     * (0x1001A20E), so it cannot reach this loop at all.  Mode 0 is the arm
     * that leaves three cars behind one entrant (0x10019BDD), which is
     * exactly the shape the defaults exist for. */
    fixture(0, 1);
    BrRaceStepBegin();
    CHECK(g_brRaceNCar == 3);

    CHECK(s_aCar[1].fE90 == 2 && s_aCar[1].fE94 == 1 &&
          s_aCar[1].fE98 == 0 && s_aCar[1].fE9C == 1);
    CHECK(s_aCar[2].fE90 == 2);
    /* The entrant itself is NOT touched by that loop -- the cursor starts
     * where the entrant loop left it, at index 1. */
    CHECK(s_aCar[0].fE90 == 0);
    /* ...and the car past the count is not touched either. */
    CHECK(s_aCar[3].fE90 == 0);
}

static void test_controllers(void)
{
    printf("one controller per car (0x1001A5BA)\n");

    /* Mode 0 leaves three cars behind one entrant: the entrant gets the
     * human body, everything past it gets the AI one, and 0x1001A607 runs
     * the setup call first. */
    fixture(0, 1);
    BrRaceStepBegin();
    CHECK(s_aCar[0].pfnControl == op_ctl_human);
    CHECK(s_aCar[1].pfnControl == op_ctl_ai);
    CHECK(s_c1005C490 == 2);            /* cars 1 and 2, not car 0 */
    CHECK(s_aCar[0].b29AF == 0u && s_aCar[1].b29AF == 0u);
    CHECK(s_aCar[0].f29B0 == BR_RACEBEGIN_FULL);

    /* Mode 6 installs 0x100199A0 -- the outro controller, which IS ported.
     *
     * Its own arm sets the car count to ONE (0x10019C53), so that arm is only
     * reachable once 0x10019CC7's loop has raised the count -- which is what
     * 0x1001C6A0 does per player.  Driving it that way is the only honest way
     * to reach 0x1001A5F9, and it exercises the unsigned `jae` loop too. */
    fixture(6, 1);
    g_brRaceBegin226A4C = 0;
    g_brRaceBeginOps.pfnNetCount = op_netcount;
    g_brRaceBeginOps.pfnNetNext  = op_netnext;
    g_brRaceBeginOps.pfnNetAdd   = op_netadd;
    g_brRaceNet = 0;
    s_cNetAdd = 0;
    BrRaceStepBegin();
    CHECK(g_brRaceNCar == 2);           /* the loop added exactly one */
    CHECK(s_cNetAdd == 1);
    CHECK(s_aCar[1].pfnControl == BrRaceCarCtlOutro);
    CHECK(s_c1005C490 == 0);            /* mode 6 skips 0x1005C490 */

    /* Mode 2's opponent: 0x1001A5E2 stores the low byte of the MODE, so the
     * recovery byte starts at 2 and the level at 0.375 -- and that arm jumps
     * PAST the `+0x29AF = 0` at 0x1001A612, so the 2 survives. */
    fixture(2, 1);
    g_brRaceNCar = 3;
    g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_TRACK]   = 0;
    g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_WEATHER] = 0;
    g_brCfgChosenTrack = 0; g_brCfgChosenWeather = 0;
    BrRaceStepBegin();
    CHECK(s_aCar[1].b29AF == 2u);
    CHECK(s_aCar[1].f29B0 == BR_RACEBEGIN_LINK_RECOVER);
    CHECK(s_aCar[0].b29AF == 0u);                       /* the entrant */
    CHECK(s_aCar[0].f29B0 == BR_RACEBEGIN_FULL);
}

static void test_specials(void)
{
    printf("the specials list and the airplane's three-way gate (0x1001A31A)\n");

    /* --- the five-way switch at 0x1001C664 ------------------------------ */
    /* Track 5, because 0x1001AA08 takes the airplane out again on tracks 0
     * and 6 -- which is asserted on its own below. */
    fixture(3, 1);
    g_brCfgChosenTrack = 5;
    s_cSpecial = 6;
    s_aSpecial[0].kind = BR_RACEBEGIN_SPECIAL_TRIGGER;    s_aSpecial[0].f00 = 11;
    s_aSpecial[1].kind = BR_RACEBEGIN_SPECIAL_PATHLEFT;   s_aSpecial[1].f00 = 22;
                                                          s_aSpecial[1].f04 = 7;
    s_aSpecial[2].kind = BR_RACEBEGIN_SPECIAL_PATHRIGHT;  s_aSpecial[2].f00 = 33;
    s_aSpecial[3].kind = BR_RACEBEGIN_SPECIAL_AIRPLANE;   s_aSpecial[3].f00 = 44;
    s_aSpecial[4].kind = BR_RACEBEGIN_SPECIAL_WATERFALL;  s_aSpecial[4].f00 = 55;
    s_aSpecial[5].kind = 2;                               s_aSpecial[5].f00 = 66;
    g_brRaceBeginSpecialsN = 6;
    BrRaceStepBegin();

    CHECK(g_brRaceBeginAirTrigger == 11);
    CHECK(g_brRaceBeginPathLeft   == 22);
    CHECK(g_brRaceBeginPathLen    == 7);    /* only kind 4 reads +0x04 */
    CHECK(g_brRaceBeginPathRight  == 33);
    CHECK(g_brRaceBeginAirplane   == 44);
    /* 0x1001A324's compare is UNSIGNED after `add eax,-3`, so kind 2 -- which
     * would be -1 signed -- is skipped, not treated as index 0. */
    CHECK(g_brRaceBeginFxCount    == 1);
    CHECK(g_aBrRaceBeginFx[0]     == 55);

    /* --- 0x1001A3A6: the airplane needs ALL THREE ----------------------- */
    /* with all three present it survives, and the scale is then computed */
    CHECK(g_brRaceBeginAirplane == 44);

    /* drop the trigger only */
    fixture(3, 1);
    g_brCfgChosenTrack = 5;
    s_cSpecial = 3;
    s_aSpecial[0].kind = BR_RACEBEGIN_SPECIAL_PATHLEFT;   s_aSpecial[0].f00 = 22;
    s_aSpecial[1].kind = BR_RACEBEGIN_SPECIAL_PATHRIGHT;  s_aSpecial[1].f00 = 33;
    s_aSpecial[2].kind = BR_RACEBEGIN_SPECIAL_AIRPLANE;   s_aSpecial[2].f00 = 44;
    g_brRaceBeginSpecialsN = 3;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginAirplane == 0);

    /* drop the left path only */
    fixture(3, 1);
    g_brCfgChosenTrack = 5;
    s_cSpecial = 3;
    s_aSpecial[0].kind = BR_RACEBEGIN_SPECIAL_TRIGGER;    s_aSpecial[0].f00 = 11;
    s_aSpecial[1].kind = BR_RACEBEGIN_SPECIAL_PATHRIGHT;  s_aSpecial[1].f00 = 33;
    s_aSpecial[2].kind = BR_RACEBEGIN_SPECIAL_AIRPLANE;   s_aSpecial[2].f00 = 44;
    g_brRaceBeginSpecialsN = 3;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginAirplane == 0);

    /* drop the right path only */
    fixture(3, 1);
    g_brCfgChosenTrack = 5;
    s_cSpecial = 3;
    s_aSpecial[0].kind = BR_RACEBEGIN_SPECIAL_TRIGGER;    s_aSpecial[0].f00 = 11;
    s_aSpecial[1].kind = BR_RACEBEGIN_SPECIAL_PATHLEFT;   s_aSpecial[1].f00 = 22;
    s_aSpecial[2].kind = BR_RACEBEGIN_SPECIAL_AIRPLANE;   s_aSpecial[2].f00 = 44;
    g_brRaceBeginSpecialsN = 3;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginAirplane == 0);

    /* 0x1001A2B4..0x1001A2F2: the block clears its own globals first, which
     * is why the three tests above are properties of the code and not of the
     * fixture. */
    fixture(3, 1);
    g_brRaceBeginAirTrigger = 7;
    g_brRaceBeginPathLeft   = 7;
    g_brRaceBeginPathRight  = 7;
    g_brRaceBeginAirplane   = 7;
    g_brRaceBeginSpecialsN  = 0;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginAirTrigger == 0);
    CHECK(g_brRaceBeginAirplane   == 0);
    CHECK(g_brRaceBeginPathT == 0.0f && g_brRaceBeginPathSeg == 0.0f);

    /* 0x1001AA08: tracks 0 and 6 take the airplane out AFTER the specials
     * block has put it in -- a second, later kill switch. */
    {
        int32_t t;
        for (t = 0; t < 8; ++t) {
            fixture(3, 1);
            g_brCfgChosenTrack = t;
            s_cSpecial = 4;
            s_aSpecial[0].kind = BR_RACEBEGIN_SPECIAL_TRIGGER;   s_aSpecial[0].f00 = 11;
            s_aSpecial[1].kind = BR_RACEBEGIN_SPECIAL_PATHLEFT;  s_aSpecial[1].f00 = 22;
            s_aSpecial[2].kind = BR_RACEBEGIN_SPECIAL_PATHRIGHT; s_aSpecial[2].f00 = 33;
            s_aSpecial[3].kind = BR_RACEBEGIN_SPECIAL_AIRPLANE;  s_aSpecial[3].f00 = 44;
            g_brRaceBeginSpecialsN = 4;
            BrRaceStepBegin();
            if (t == 0 || t == 6)
                CHECK(g_brRaceBeginAirplane == 0);
            else
                CHECK(g_brRaceBeginAirplane == 44);
        }
    }
}

static void test_substate_and_ramps(void)
{
    printf("the arm runs once, and the three ramps (0x1001AA5E)\n");

    fixture(3, 1);
    CHECK(g_brRaceSubstate == 0);
    BrRaceStepBegin();
    /* 0x1001AA5E raises the substate, so the clock's branch flips. */
    CHECK(g_brRaceSubstate == 1);

    /* 0x1001AA26 / 0x1001AA38 / 0x1001AA4A -- three, exactly once each. */
    CHECK(s_cRampA == 1 && s_cRampB == 1 && s_cRampC == 1);

    /* 0x1001AB60's clock reset is the LAST thing the arm does -- so this has
     * to be read BEFORE anything ticks the clock, which is what would move
     * the counter off -1. */
    CHECK(g_brRaceClockCount == -1);
    CHECK(g_brRaceBeginLimitOn == 1);

    CHECK(BrRaceStepClock() == 0);
}

static void test_texset_count(void)
{
    printf("the texture-set count (0x1001A430)\n");

    /* 0x1001A43C's `jne` is still reading the `cmp eax,ebp` at 0x1001A428 --
     * the REPLAY flag.  Replaying gives 1; otherwise the entrant count. */
    fixture(3, 3);
    g_brRaceReplay = 0;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginNTexSet == 3);

    /* 0x1001A436 pins the BOUND at 1 when replaying, and 0x1001A973 writes
     * the loop's COUNTER, a different stack slot.  Reading the two as one
     * object gives 3 here -- which is what this pass wrote first. */
    fixture(3, 3);
    g_brRaceReplay = 1;
    BrRaceStepBegin();
    CHECK(g_brRaceBeginNTexSet == 1);
    /* ...and with the counter already at 3 the paint loop cannot run. */
    CHECK(BrRaceBeginReplayPaintSkipped() == 1);
    /* and the "best car" selection at 0x1001A44A only happens when replaying */
    fixture(3, 1);
    g_brRaceReplay = 1;  g_brRaceBeginBestCar = 2;
    BrRaceStepBegin();
    CHECK(g_brRaceBegin6E86C8 == 2);
    CHECK(g_brRaceBegin6E8720 == 0);

    fixture(3, 1);
    g_brRaceReplay = 0;  g_brRaceBeginBestCar = 2;
    BrRaceStepBegin();
    CHECK(g_brRaceBegin6E86C8 == 0);
    CHECK(g_brRaceBegin6E8720 == 1);
    g_brRaceReplay = 0;
}

/* ==================================================================== *
 * The seven neighbours
 * ==================================================================== */
static void test_hudframe(void)
{
    printf("0x10019890 -- three submissions round two calls\n");

    fixture(3, 1);
    g_brRacePaused = 0;
    BrRaceHudFrame();
    CHECK(s_cTrace == 3);
    CHECK(s_c10019840 == 1 && s_c10032E40 == 1);

    /* 0x10019897: paused takes the whole body out, including the calls. */
    fixture(3, 1);
    g_brRacePaused = 1;
    BrRaceHudFrame();
    CHECK(s_cTrace == 0 && s_c10019840 == 0 && s_c10032E40 == 0);
    g_brRacePaused = 0;
}

static void s_step_body(void) { }

static void test_enter_outro(void)
{
    printf("0x10019900 -- mode 4, stage 2, and the step install\n");

    fixture(1, 1);
    BrGameStepSet(NULL);
    BrRaceEnterOutro(s_step_body);
    CHECK(g_brRaceRules.mode == 4);
    CHECK(g_brRaceBeginStage == 2);
    CHECK(BrGameStepGet() == s_step_body);
}

static void test_cues(void)
{
    int32_t base;

    printf("0x10019930 / 0x10019980 -- the cue schedule\n");

    memset(g_aBrRaceCue, 0, sizeof(g_aBrRaceCue));
    g_brRaceCueArmed = 1;
    g_brRaceCueBase  = 100;

    g_aBrRaceCue[0].len = 8;  g_aBrRaceCue[0].gap = 3;
    g_aBrRaceCue[1].len = 12; g_aBrRaceCue[1].gap = 1;  g_aBrRaceCue[1].next = 1;
    g_aBrRaceCue[2].len = 4;  g_aBrRaceCue[2].gap = 0;  g_aBrRaceCue[2].next = 1;
    /* cue[3].next stays 0, which is the terminator the loop reads. */

    BrRaceCueLayout();

    /* start[0] = base + (3*8)/4 = 100 + 6.
     * The cursor then advances by (8 - 6) + 3 == 11, i.e. len + gap. */
    CHECK(g_aBrRaceCue[0].start == 106);
    base = 100 + 8 + 3;                        /* 111 */
    CHECK(g_aBrRaceCue[1].start == base + (12 * 3) / 4);   /* 111 + 9 == 120 */
    base += 12 + 1;                            /* 124 */
    CHECK(g_aBrRaceCue[2].start == base + (4 * 3) / 4);    /* 124 + 3 == 127 */

    /* The terminator is the NEXT record's +0x0C (0x10019972 reads [esi+8]
     * AFTER the cursor advanced), so cue[3] is never written. */
    CHECK(g_aBrRaceCue[3].start == 0);

    /* 0x1001993E: an unarmed list is not walked at all. */
    g_aBrRaceCue[0].start = 0;
    g_brRaceCueArmed = 0;
    BrRaceCueLayout();
    CHECK(g_aBrRaceCue[0].start == 0);

    /* 0x10019980 steps every laid-out cue back by one, over the same span. */
    g_brRaceCueArmed = 1;
    BrRaceCueLayout();
    {
        int32_t a = g_aBrRaceCue[0].start;
        int32_t b = g_aBrRaceCue[2].start;
        int32_t d = g_aBrRaceCue[3].start;
        BrRaceCueRewind();
        CHECK(g_aBrRaceCue[0].start == a - 1);
        CHECK(g_aBrRaceCue[2].start == b - 1);
        CHECK(g_aBrRaceCue[3].start == d);     /* past the terminator */
    }
}

static void test_outro_controller(void)
{
    printf("0x100199A0 -- speed from velocity\n");

    fixture(6, 1);
    s_aCar[0].f730   = 0;
    s_aCar[0].f1030  = 12345.0f;
    s_aCar[0].f1E8.x = 3.0f; s_aCar[0].f1E8.y = 4.0f; s_aCar[0].f1E8.z = 0.0f;
    BrRaceCarCtlOutro(&s_aCar[0]);
    /* 0x100199AC: a zero +0x730 skips the speed entirely... */
    CHECK(s_aCar[0].f1030 == 12345.0f);
    /* ...but the control chain still runs -- 0x10019A00 is outside the `if`. */
    CHECK(s_c1006F170 == 1);

    s_aCar[0].f730 = 1;
    BrRaceCarCtlOutro(&s_aCar[0]);
    /* |(3,4,0)| == 5, and 0x100199F1 scales by 0x100773A8 == 2.24f. */
    CHECK(s_sqrtArg == 25.0f);
    CHECK(fabs((double)s_aCar[0].f1030 - 5.0 * 2.240000009536743) < 1e-5);
    CHECK(s_c1006F170 == 2);
}

static void test_driver_reset(void)
{
    printf("0x10019A10 -- once per driver\n");

    fixture(3, 1);
    g_brRaceNDriver = 3;
    BrRaceDriverReset();
    CHECK(s_c1005F530 == 3);

    /* 0x10019A18's `jle` -- an empty field runs the callee zero times. */
    fixture(3, 1);
    g_brRaceNDriver = 0;
    BrRaceDriverReset();
    CHECK(s_c1005F530 == 0);
}

/* ==================================================================== *
 * The frontier is COUNTED, which is what makes "the race began" falsifiable
 * ==================================================================== */
static void test_frontier_counted(void)
{
    printf("an unwired op is skipped AND counted\n");

    fixture(3, 1);
    memset(&g_brRaceBeginOps, 0, sizeof(g_brRaceBeginOps));
    BrRaceBeginReset();
    BrRaceStepBegin();

    CHECK(BrRaceBeginSkipped(BR_RB_TRACKLOAD) == 1);
    CHECK(BrRaceBeginSkipped(BR_RB_RAMP_A) == 1);
    CHECK(BrRaceBeginSkipped(BR_RB_CARMODELSET) >= 1);
    CHECK(BrRaceBeginSkipped(BR_RB_TRACE) > 0);
    CHECK(strcmp(BrRaceBeginOpName(BR_RB_TICKMS), "0x1006E280 tick ms") == 0);

    /* An unwired clock reports zero elapsed time -- it does not invent one. */
    fixture(3, 1);
    memset(&g_brRaceBeginOps, 0, sizeof(g_brRaceBeginOps));
    g_brRaceClockLast = 500u;
    (void)BrRaceStepClock();
    CHECK(g_brRaceClockLast == 0u);
}

int main(void)
{
    test_clock();
    test_clock_accum();
    test_clock_substate();
    test_mode_table();
    test_mode5_track();
    test_header_roundtrip();
    test_header_signed();
    test_equipment_copy();
    test_equipment_defaults();
    test_controllers();
    test_specials();
    test_substate_and_ramps();
    test_texset_count();
    test_hudframe();
    test_enter_outro();
    test_cues();
    test_outro_controller();
    test_driver_reset();
    test_frontier_counted();

    if (g_fails != 0) {
        printf("\n%d failures\n", g_fails);
        return 1;
    }
    printf("\nALL PASSED\n");
    return 0;
}
