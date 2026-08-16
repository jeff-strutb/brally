/* test_slice6_77.c -- behavioural tests for packet 77.
 *
 * WHAT IS BEING TESTED, AND WHY IN THIS SHAPE
 * ===========================================
 * Neither function computes anything. Both are pure state transitions, so
 * volume assertions ("N writes happened") would say nothing; what can be
 * wrong is WHICH storage moves, to WHAT value, and IN WHAT ORDER relative to
 * the two calls. Those are what is asserted.
 *
 *   0x100586A0 -- the entire content of this function is that the free marker
 *       is -1 and NOT 0, and that the counter it clears is a SEPARATE object
 *       from the array. A plausible-but-wrong version fills with zero (which
 *       is a valid slot id, so nothing downstream would notice until a real
 *       id 0 was mistaken for a free slot) or folds the counter into the
 *       array. Both are checked directly, the second by putting a guard value
 *       immediately after the array and requiring it to survive.
 *
 *   0x100795D0 -- three properties, none of them a magic number:
 *       1. WHAT THE PROBE SEES. The forced configuration must be in place
 *          when BrFfbInit runs, not merely at some point during the call. The
 *          stand-in samples the three globals from inside itself, which is the
 *          only place the question can be answered.
 *       2. ORDER. Init strictly before shutdown -- the reverse also "works"
 *          in the sense of leaving the same end state, and is wrong.
 *       3. RESTORE AND SELECT. The saved mode and exclusive flag come back
 *          for every mode, and the record selection is a fall-through and not
 *          a clamp: mode 0 and mode 4 must BOTH give record 0, whereas a
 *          clamp would give 0 and 3. That is the one difference between the
 *          `dec/je` chain and the tidy version of it.
 *       Also checked: the function is idempotent on its two saved globals, so
 *       running it twice cannot ratchet them.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file, as the house
 * contract requires. The only real object linked besides the module is
 * br_slots.o, because BrSlotsResetArray IS 0x100586A0's loop -- faking it
 * would test the fake.
 */
#include "slice6_77.h"
#include "slice2_25.h"
#include "slice3_45.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { ++g_fails; \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ======================================================================
 * Stand-ins for the storage slice2_25.c owns and the calls slice3_45.c /
 * slice1_10.c own.
 * ====================================================================== */

/* The eight slots at 0x10AA2538, plus a guard that occupies the address space
 * a version folding the counter into the array would scribble on. The real
 * 0x10AA288C is 0x2F4 bytes further on, so this guard stands for "anything
 * that is not one of the eight records". */
BrSlot  g_aBrAA2538[BR_SLOT_COUNT];
static int32_t g_guardAfterSlots;
int32_t g_brAA288C;

int32_t g_brB4E1D0;
int32_t g_brB4E1E0;
void   *g_brB4E1D4;
unsigned char g_aBrB4DF30[BR_OPT_B4DF30_COUNT][BR_OPT_B4DF30_STRIDE];

BrFfb g_brFfb;

/* What the probe saw from inside itself, and the call order. */
static int   g_seq;              /* next sequence number to hand out       */
static int   g_seqInit, g_seqShutdown;
static int32_t g_modeSeenByInit, g_exclSeenByInit;
static void   *g_recSeenByInit;
static BrFfb  *g_pFfbGivenToShutdown;

int32_t BrFfbInit(void)
{
    g_seqInit        = ++g_seq;
    g_modeSeenByInit = g_brB4E1D0;
    g_exclSeenByInit = g_brB4E1E0;
    g_recSeenByInit  = g_brB4E1D4;
    return 0;
}

void BrFfbShutdown(BrFfb *pFfb)
{
    g_seqShutdown        = ++g_seq;
    g_pFfbGivenToShutdown = pFfb;
}

static void ResetProbeTrace(void)
{
    g_seq = g_seqInit = g_seqShutdown = 0;
    g_modeSeenByInit = g_exclSeenByInit = -12345;
    g_recSeenByInit = NULL;
    g_pFfbGivenToShutdown = NULL;
}

/* ======================================================================
 * 0x100586A0
 * ====================================================================== */

static void FillSlotsWithJunk(void)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        g_aBrAA2538[i].id = i;          /* including id 0, which is VALID */
        g_aBrAA2538[i].a  = 100 + i;
        g_aBrAA2538[i].b  = 200 + i;
    }
}

static void TestSlotResetMarker(void)
{
    int i;

    FillSlotsWithJunk();
    g_brAA288C = 7;

    BrSub100586A0();

    /* The free marker is -1. If it were 0 this loop would still "pass" a
     * looser test that only asked whether the table had been touched. */
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        CHECK(g_aBrAA2538[i].id == BR_SLOT_EMPTY);
        CHECK(g_aBrAA2538[i].id != 0);
        CHECK(g_aBrAA2538[i].a  == 0);
        CHECK(g_aBrAA2538[i].b  == 0);
    }
    CHECK(g_brAA288C == 0);
}

static void TestSlotResetTouchesEightAndNoMore(void)
{
    /* The bound is 8 records, derived from (0x259C-0x253C)/12. A version that
     * ran off the end would take the guard with it. */
    g_guardAfterSlots = 0x5A5A5A5A;
    FillSlotsWithJunk();

    BrSub100586A0();

    CHECK(g_guardAfterSlots == 0x5A5A5A5A);
}

static void TestSlotResetIsIdempotent(void)
{
    BrSlot before[BR_SLOT_COUNT];

    FillSlotsWithJunk();
    BrSub100586A0();
    memcpy(before, g_aBrAA2538, sizeof before);

    BrSub100586A0();

    CHECK(memcmp(before, g_aBrAA2538, sizeof before) == 0);
    CHECK(g_brAA288C == 0);
}

static void TestSlotResetOpensTheSendGate(void)
{
    /* br_slots.h's warning, asserted rather than left as prose: 0x10AA288C is
     * also the DirectPlay send gate, so this function unblocks tags 2..5 as a
     * side effect. If that ever stops being true, the network modules that
     * read the gate need re-reading, not this test relaxing. */
    g_brAA288C = 1;
    BrSub100586A0();
    CHECK(g_brAA288C == 0);
}

/* ======================================================================
 * 0x100795D0
 * ====================================================================== */

static void TestProbeSeesForcedConfiguration(void)
{
    /* Start from something that is NOT the probe configuration in all three
     * globals, so seeing the forced values cannot be an accident. */
    g_brB4E1D0 = 3;
    g_brB4E1E0 = 0;
    g_brB4E1D4 = NULL;
    ResetProbeTrace();

    BrFfbReprobe();

    /* Sampled from inside BrFfbInit: the probe runs under mode 2, record 2,
     * exclusive. Sampling after the call would not distinguish "forced then
     * restored" from "never forced at all". */
    CHECK(g_modeSeenByInit == 2);
    CHECK(g_exclSeenByInit == 1);
    CHECK(g_recSeenByInit  == (void *)g_aBrB4DF30[2]);
}

static void TestProbeCallOrderAndArgument(void)
{
    g_brB4E1D0 = 1;
    g_brB4E1E0 = 1;
    ResetProbeTrace();

    BrFfbReprobe();

    CHECK(g_seqInit == 1);
    CHECK(g_seqShutdown == 2);
    CHECK(g_seqInit < g_seqShutdown);       /* init BEFORE shutdown */
    CHECK(g_pFfbGivenToShutdown == &g_brFfb);
}

static void TestProbeRestoresSavedGlobals(void)
{
    static const int32_t aModes[] = { -1, 0, 1, 2, 3, 4, 99 };
    static const int32_t aExcl[]  = { 0, 1 };
    size_t i, j;

    for (i = 0; i < sizeof aModes / sizeof aModes[0]; ++i) {
        for (j = 0; j < sizeof aExcl / sizeof aExcl[0]; ++j) {
            g_brB4E1D0 = aModes[i];
            g_brB4E1E0 = aExcl[j];
            ResetProbeTrace();

            BrFfbReprobe();

            CHECK(g_brB4E1D0 == aModes[i]);
            CHECK(g_brB4E1E0 == aExcl[j]);
        }
    }
}

static void TestProbeSelectsRecordByFallThrough(void)
{
    /* THE point of this test. `dec/je` three times means 1, 2 and 3 select
     * records 1, 2 and 3 and EVERYTHING ELSE -- below and above -- selects
     * record 0. A clamp would send 4 and 99 to record 3, and would send a
     * negative mode to record 0 for the wrong reason. */
    static const struct { int32_t mode; int idx; } aCases[] = {
        { -7, 0 }, { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 },
        { 4, 0 }, { 5, 0 }, { 99, 0 }
    };
    size_t i;

    for (i = 0; i < sizeof aCases / sizeof aCases[0]; ++i) {
        g_brB4E1D0 = aCases[i].mode;
        g_brB4E1E0 = 0;
        g_brB4E1D4 = NULL;
        ResetProbeTrace();

        BrFfbReprobe();

        CHECK(g_brB4E1D4 == (void *)g_aBrB4DF30[aCases[i].idx]);
    }

    /* And the four records really are distinct objects 0xA8 apart, which is
     * what makes the selection meaningful at all. */
    CHECK((size_t)(g_aBrB4DF30[1] - g_aBrB4DF30[0]) == BR_OPT_B4DF30_STRIDE);
    CHECK((size_t)(g_aBrB4DF30[3] - g_aBrB4DF30[0]) == 3u * BR_OPT_B4DF30_STRIDE);
}

static void TestProbeIsIdempotent(void)
{
    int32_t mode, excl;
    void   *rec;

    g_brB4E1D0 = 2;
    g_brB4E1E0 = 1;
    ResetProbeTrace();
    BrFfbReprobe();

    mode = g_brB4E1D0; excl = g_brB4E1E0; rec = g_brB4E1D4;

    ResetProbeTrace();
    BrFfbReprobe();

    CHECK(g_brB4E1D0 == mode);
    CHECK(g_brB4E1E0 == excl);
    CHECK(g_brB4E1D4 == rec);
}

int main(void)
{
    TestSlotResetMarker();
    TestSlotResetTouchesEightAndNoMore();
    TestSlotResetIsIdempotent();
    TestSlotResetOpensTheSendGate();

    TestProbeSeesForcedConfiguration();
    TestProbeCallOrderAndArgument();
    TestProbeRestoresSavedGlobals();
    TestProbeSelectsRecordByFallThrough();
    TestProbeIsIdempotent();

    if (g_fails == 0) {
        printf("test_slice6_77: all checks passed\n");
        return 0;
    }
    printf("test_slice6_77: %d failures\n", g_fails);
    return 1;
}
