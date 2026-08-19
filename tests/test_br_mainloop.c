/* test_br_mainloop.c -- 0x10019730's loop, driven by a scripted queue.
 *
 * The checks that matter are the ones about ORDER and POLARITY, because those
 * are the only parts of this function that are the game's rather than Win32's:
 *
 *   - messages are drained BEFORE any frame runs
 *   - WM_QUIT ends the loop without running a frame
 *   - a frame runs only when both ready flags are set AND the suspend flag is
 *     clear -- the third test is inverted relative to the first two
 *   - when the gate is closed the loop BLOCKS rather than spinning
 *
 * Each of those is asserted so that inverting it fails. The suspend polarity in
 * particular: get it backwards and the game runs only while minimised, which
 * presents as a hang rather than as a wrong answer.
 */
#include "br_mainloop.h"
#include "br_boot.h"
#include "br_gamestep.h"

#include <stdio.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* ---- a scripted message queue -------------------------------------- */
typedef struct Q {
    int  cPending;      /* messages waiting */
    int  cQuitAfter;    /* GetMessage returns 0 on this many-th call */
    int  cGet, cPump, cWait, cPeek;
} Q;

/* A HARD ITERATION BOUND, and it is not defensive padding.
 *
 * Inverting the suspend polarity does not make this suite FAIL -- it makes it
 * HANG, because the loop then runs frames forever in a state where the
 * original would have blocked. That is the same shape as the real defect
 * ("the game only runs while minimised" looks like a freeze), so the mutation
 * reproduced the bug faithfully and the test became useless: a suite that
 * hangs reports nothing, blocks the whole run, and has to be killed by hand.
 *
 * The bound converts it into a named failure. Any test that drives a loop
 * whose termination is the thing under test needs one. */
#define Q_MAX_ITERS 10000
static int g_iters;

static int  q_peek(void *p)
{
    Q *q = (Q *)p;
    q->cPeek++;
    if (++g_iters > Q_MAX_ITERS) {
        if (g_iters == Q_MAX_ITERS + 1) {
            printf("FAIL %s: loop did not terminate within %d iterations\n",
                   __FILE__, Q_MAX_ITERS);
            g_fails++;
        }
        /* THE EXIT HAS TO BE WM_QUIT, and two weaker attempts failed first:
         *
         *   return 0            -> "queue empty" routes to the frame path,
         *                          which is where the non-terminating mutant
         *                          lives. No exit.
         *   + set the quit flag -> the frame's return value only matters if
         *                          the frame RUNS, and with the gate shut it
         *                          never does. The loop then spins between
         *                          peek and wait forever. No exit.
         *
         * Reporting a message and then failing GetMessage is the one path out
         * that does not depend on either the gate or the frame -- the two
         * things under test. */
        g_brAppContinue = 0;
        return 1;               /* claim a message is waiting ... */
    }
    return q->cPending > 0;
}
static int  q_get (void *p)
{
    Q *q = (Q *)p;
    q->cGet++;
    if (g_iters > Q_MAX_ITERS) return 0;      /* ... and make it WM_QUIT */
    if (q->cQuitAfter > 0 && q->cGet >= q->cQuitAfter) return 0;   /* WM_QUIT */
    if (q->cPending > 0) q->cPending--;
    return 1;
}
static void q_pump(void *p) { ((Q *)p)->cPump++; }
static void q_wait(void *p)
{
    Q *q = (Q *)p;
    q->cWait++;
    if (g_iters > Q_MAX_ITERS) return;
    /* A real WaitMessage blocks until something arrives. The script makes one
     * arrive, otherwise the test would hang exactly as the game would. */
    q->cPending = 1;
}

static BrMainLoopOps ops_for(Q *q)
{
    BrMainLoopOps o;
    o.pfnPeek = q_peek; o.pfnGet = q_get;
    o.pfnPump = q_pump; o.pfnWait = q_wait; o.pUser = q;
    return o;
}

static int32_t g_cFrames;
static int32_t g_cQuitAfterFrames;
static void frame_step(void)
{
    g_cFrames++;
    if (g_cQuitAfterFrames > 0 && g_cFrames >= g_cQuitAfterFrames)
        g_brAppContinue = 0;
}

static void arm(int32_t cQuitAfterFrames)
{
    BrAppResetForTest();
    BrGameStepSet(frame_step);
    g_brAppState = BR_APP_RUN;        /* skip the boot states */
    g_cFrames = 0;
    g_cQuitAfterFrames = cQuitAfterFrames;
    g_iters = 0;
    BrMainLoopSetReady(1, 1);
    BrMainLoopSetSuspended(0);
}

/* ---- the gate's polarity ------------------------------------------- */
static void test_gate_polarity(void)
{
    BrMainLoopSetReady(1, 1); BrMainLoopSetSuspended(0);
    CHECK(BrMainLoopFrameAllowed() == 1);

    /* both ready flags are required */
    BrMainLoopSetReady(0, 1); CHECK(BrMainLoopFrameAllowed() == 0);
    BrMainLoopSetReady(1, 0); CHECK(BrMainLoopFrameAllowed() == 0);

    /* and the third is INVERTED: set means DO NOT run */
    BrMainLoopSetReady(1, 1);
    BrMainLoopSetSuspended(1); CHECK(BrMainLoopFrameAllowed() == 0);
    BrMainLoopSetSuspended(0); CHECK(BrMainLoopFrameAllowed() == 1);
}

/* ---- messages are drained before any frame runs -------------------- */
static void test_messages_first(void)
{
    Q q; BrMainLoopOps o;
    arm(1);
    q.cPending = 3; q.cQuitAfter = 0; q.cGet = q.cPump = q.cWait = q.cPeek = 0;
    o = ops_for(&q);

    /* Returns 0, not 1: the single frame that ran is the one that asked to
     * quit, and 0x100197E7 breaks out BEFORE the continue path, so a quitting
     * frame is never counted as completed. test_frame_returning_zero_quits
     * asserts the same rule at 4 frames; this line disagreed with it and this
     * test was the one that was wrong. */
    CHECK(BrMainLoopRun(&o) == 0);
    /* three messages pumped, and only then the single frame that quit */
    CHECK(q.cPump == 3);
    CHECK(g_cFrames == 1);
    /* it never had to block: the queue was non-empty, then the gate was open */
    CHECK(q.cWait == 0);
}

/* ---- WM_QUIT ends the loop and does NOT run a frame ------------------ */
static void test_quit_message(void)
{
    Q q; BrMainLoopOps o;
    arm(0);                       /* the frame would never quit on its own */
    q.cPending = 1; q.cQuitAfter = 1; q.cGet = q.cPump = q.cWait = q.cPeek = 0;
    o = ops_for(&q);

    CHECK(BrMainLoopRun(&o) == 0);
    CHECK(g_cFrames == 0);        /* the frame must not have run */
    CHECK(q.cPump == 0);          /* nor dispatched the quit message */
}

/* ---- a closed gate BLOCKS, it does not spin -------------------------- */
static void test_blocks_when_gated(void)
{
    Q q; BrMainLoopOps o;
    arm(0);
    BrMainLoopSetSuspended(1);           /* gate shut */
    q.cPending = 0; q.cQuitAfter = 2; q.cGet = q.cPump = q.cWait = q.cPeek = 0;
    o = ops_for(&q);

    CHECK(BrMainLoopRun(&o) == 0);
    /* It reached WaitMessage rather than looping on the frame check. With the
     * suspend polarity inverted this would run frames instead and cWait would
     * be 0 -- which is the failure that presents as "the game only runs while
     * minimised". */
    CHECK(q.cWait >= 1);
    CHECK(g_cFrames == 0);
}

/* ---- the frame's return value stops the loop ------------------------- */
static void test_frame_returning_zero_quits(void)
{
    Q q; BrMainLoopOps o;
    arm(4);                       /* the 4th frame asks to quit */
    q.cPending = 0; q.cQuitAfter = 0; q.cGet = q.cPump = q.cWait = q.cPeek = 0;
    o = ops_for(&q);

    /* Four frames run; the fourth returns 0 and is NOT counted as completed,
     * matching 0x100197E7's `test eax,eax / je done` -- the count is
     * incremented only on the continue path. */
    CHECK(BrMainLoopRun(&o) == 3);
    CHECK(g_cFrames == 4);
}

int main(void)
{
    test_gate_polarity();
    test_messages_first();
    test_quit_message();
    test_blocks_when_gated();
    test_frame_returning_zero_quits();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_mainloop: all checks passed\n");
    return 0;
}
