/* test_br_boot.c -- the game's top-level state machine.
 *
 * WHAT THIS ASSERTS AND WHY IT CAN FAIL
 *
 * The transitions are read off four small functions in BRGlide.dll, and the
 * whole value of this suite is that it pins the ORDER. Every check below
 * changes its answer if a single `mov [0x105CCBBC], n` is transcribed with
 * the wrong n -- which is the only real way to get this module wrong, since
 * the bodies are otherwise a handful of calls.
 *
 * Two habits this project learned the hard way are applied here:
 *
 *  - assert absolute values, never ratios or "changed". A suite that checked
 *    a ratio once hid a bug that dropped 96% of track geometry.
 *  - make the fixture able to distinguish the right answer from the wrong
 *    one. A fixture was found here whose setup left the deciding field zero,
 *    so it passed under both readings. The frontier hit-counts below are the
 *    guard against that: they fail if a state runs the wrong body even when
 *    it happens to reach the right next state.
 */
#include "br_boot.h"
#include "br_bootfrontier.h"
#include "br_gamestep.h"

#include <stdio.h>
#include <string.h>

static int g_fails;

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

static int32_t g_cFrameCalls;
static void quit_from_inside_the_frame(void) { g_cFrameCalls++; g_brAppContinue = 0; }
static void count_the_frame(void)            { g_cFrameCalls++; }

static int32_t hits(const char *pszNeedle)
{
    int i, n = BrBootFrontierCount();
    for (i = 0; i < n; i++)
        if (strstr(BrBootFrontierName(i), pszNeedle) != NULL)
            return BrBootFrontierHits(i);
    return -1;   /* name not found -- a rename would show up as a failure */
}

/* ---- the transition table, straight off the disassembly ------------ */
static void test_transitions(void)
{
    BrAppResetForTest();
    g_cFrameCalls = 0;
    BrGameStepSet(count_the_frame);

    /* 0x1001CD70: cold init -> 4, returns 1 */
    CHECK(g_brAppState == BR_APP_COLD_INIT);
    CHECK(BrAppFrame() == 1);
    CHECK(g_brAppState == BR_APP_SET_MODE);
    /* and it ran the three calls in its body, one of them the bank select */
    CHECK(hits("0x10032530") == 1);
    CHECK(hits("0x1006C290") == 1);
    CHECK(hits("0x10058AF0") == 1);

    /* 0x1001CE20 first arm: both gates zero -> install 640x480, -> state 3 */
    CHECK(BrAppFrame() == 1);
    CHECK(g_brAppState == BR_APP_LOADING);
    CHECK(g_brAppModeW == 640);      /* 0x280 */
    CHECK(g_brAppModeH == 480);      /* 0x1E0 */
    CHECK(hits("0x1006C460") == 1);
    /* the first arm returns before the tail -- 0x1001CE70/75/76 */
    CHECK(hits("config tail") == 0);
    /* and it must NOT have taken the second arm */
    CHECK(hits("0x10056260") == 0);
    CHECK(hits("0x1006C290") == 1);  /* still 1, not 2 */

    /* 0x1001CDD0: loading -> 1, having asked for loading.img */
    CHECK(BrAppFrame() == 1);
    CHECK(g_brAppState == BR_APP_ENTER_RUN);
    CHECK(hits("0x10063970") == 1);
    CHECK(hits("0x1006C990") == 1);
    CHECK(hits("0x100628B0") == 1);

    /* 0x1001CDA0: -> 2 and nothing else. No frontier entry may be reached. */
    {
        int i, n = BrBootFrontierCount(), before = 0, after = 0;
        for (i = 0; i < n; i++) before += BrBootFrontierHits(i);
        CHECK(BrAppFrame() == 1);
        CHECK(g_brAppState == BR_APP_RUN);
        for (i = 0; i < n; i++) after += BrBootFrontierHits(i);
        CHECK(before == after);
    }

    /* 0x1001CDB0: RUN ticks the frame counter, calls the frame, and STAYS. */
    CHECK(g_brAppFrame == 0);
    CHECK(BrAppFrame() == 1);
    CHECK(g_brAppFrame == 1);
    CHECK(g_brAppState == BR_APP_RUN);      /* terminal -- no transition */
    CHECK(g_cFrameCalls == 1);

    CHECK(BrAppFrame() == 1);
    CHECK(g_brAppFrame == 2);
    CHECK(g_cFrameCalls == 2);
}

/* ---- 0x100A98F8 is the quit signal, and state 2 returns it --------- */
static void test_quit(void)
{
    BrAppResetForTest();
    g_cFrameCalls = 0;
    BrGameStepSet(count_the_frame);
    g_brAppState = BR_APP_RUN;

    CHECK(BrAppFrame() == 1);          /* continue flag starts set */
    g_brAppContinue = 0;
    CHECK(BrAppFrame() == 0);          /* 0 is what stops the main loop */
    /* the frame still RAN -- the flag is read after the call, at 0x1001CDBB */
    CHECK(g_cFrameCalls == 2);
    CHECK(g_brAppFrame == 2);
}

/* ---- the full boot order, as one sequence -------------------------- *
 * This is the check worth having: it states the order 0 -> 4 -> 3 -> 1 -> 2
 * as a single assertion, so any single mis-transcribed target breaks it.    */
static void test_boot_order(void)
{
    static const int32_t aExpect[] = {
        BR_APP_SET_MODE,   /* after state 0 */
        BR_APP_LOADING,    /* after state 4 */
        BR_APP_ENTER_RUN,  /* after state 3 */
        BR_APP_RUN,        /* after state 1 */
        BR_APP_RUN         /* state 2 is terminal */
    };
    size_t i;
    BrAppResetForTest();
    for (i = 0; i < sizeof aExpect / sizeof aExpect[0]; i++) {
        CHECK(BrAppFrame() == 1);
        CHECK(g_brAppState == aExpect[i]);
    }
}

/* ---- the range check is OURS, and is documented as a deviation ----- */
static void test_out_of_range_state(void)
{
    BrAppResetForTest();
    g_brAppState = 99;
    /* The original would jump through the table into the "BossRally" string
     * and execute ASCII. This port returns 0. Asserted so the deviation is
     * pinned rather than merely commented. */
    CHECK(BrAppFrame() == 0);
}

/* ---- the flag is read AFTER the frame runs, not before --------------
 * 0x1001CDB0 is `call 0x1002E324` then `mov eax,[0x100A98F8]`, so a frame
 * that asks to quit is obeyed on the same tick. Without this, a mutant that
 * hoisted the read above the call passed every other check in this file. */

static void test_flag_read_after_frame(void)
{
    BrAppResetForTest();
    g_brAppState = BR_APP_RUN;
    CHECK(g_brAppContinue == 1);
    g_cFrameCalls = 0;
    BrGameStepSet(quit_from_inside_the_frame);

    /* The frame clears the flag; because the read follows the call, this tick
     * must already report "quit". Hoist the read and this returns 1. */
    CHECK(BrAppFrame() == 0);
    CHECK(g_cFrameCalls == 1);
    CHECK(g_brAppFrame == 1);
}

int main(void)
{
    test_flag_read_after_frame();
    test_transitions();
    test_quit();
    test_boot_order();
    test_out_of_range_state();

    if (g_fails != 0) {
        printf("%d FAILURE(S)\n", g_fails);
        return 1;
    }
    printf("br_boot: all checks passed\n");
    return 0;
}
