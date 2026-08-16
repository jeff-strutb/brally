/* test_slice8_86_perf.c -- packet 86, the OTHER half of the frame timer.
 *
 * WHY THIS IS A SECOND BINARY. 0x100751D0 probes for a high-resolution
 * counter ONCE and latches the answer in a global (0x118AB140 / 0x118AB144);
 * the original does the same. So within one process only one of the two
 * branches can ever run, and CONVENTIONS.md is explicit that a path which
 * never executes is not tested -- a green suite that only ever took the
 * no-counter arm would say nothing at all about the counter arm.
 *
 * test_slice8_86.c drives the no-counter arm. This file drives the counter
 * arm, and checks the three things that are specific to it: the period is the
 * frequency divided by the LITERAL 30 (a 30 Hz tick, not 60), the 64-bit
 * fields are used rather than the millisecond ones, and 0x1002C2C0 does NOT
 * call timeEndPeriod when a counter exists.
 *
 * Everything the module forwards to is a stand-in, as in the sibling file.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice8_86.h"
#include "slice7_82.h"

static int g_fail;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                      \
            printf(__VA_ARGS__);                                             \
            printf("\n");                                                    \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

/* --- stand-ins ------------------------------------------------------- */
void BrExt_1004A580(BrUiBuildCtx *c, BrUiPhase *p) { (void)c; (void)p; }
void BrExt_1004B430(BrUiBuildCtx *c, BrUiPhase *p) { (void)c; (void)p; }
void BrExt_1004BDC0(BrUiBuildCtx *c, BrUiPhase *p) { (void)c; (void)p; }
void BrExt_1004C4A0(BrUiBuildCtx *c, BrUiPhase *p) { (void)c; (void)p; }
void BrExt_1004CAC0(BrUiBuildCtx *c, BrUiPhase *p) { (void)c; (void)p; }

int BrAudioPlayTrack(BrAudio *a, int i) { (void)a; (void)i; return 0; }

void *BrGlobalHandle(void *p) { return p; }
int   BrGlobalUnlock(void *h) { (void)h; return 0; }
void *BrGlobalFree(void *h)   { free(h); return NULL; }

void *BrUiPageDelete_100484C0(BrUiPage *p, int32_t n) { (void)n; return p; }
int   BrUiPageFrame_10048530(BrScrGlobals *g, BrUiPage *p)
{ (void)g; (void)p; return 0; }
void *BrPhaseDelete_10048850(BrPhaseFull *p, int32_t n) { (void)n; return p; }
int   BrPhaseFn_100488B0(BrPhaseFull *p) { (void)p; return 1; }
int   BrPhaseTick_100488C0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)g; (void)p; return 0; }
int   BrPhaseRun_100489A0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)g; (void)p; return 0; }
void  BrPhaseReleasePages_10048AA0(BrScrGlobals *g, BrPhaseFull *p)
{ (void)g; (void)p; }
void  BrPhaseShutdown_10048B20(BrScrGlobals *g, void *a) { (void)g; (void)a; }

#define BR86T_FREQ    ((int64_t)3000000)
#define BR86T_COUNTER ((int64_t)0x1234567890LL)

static int g_nFreqCalls, g_nCounterCalls, g_nBegin, g_nEnd;

int32_t BrPlatQueryPerfFreq(int64_t *pFreq)
{ g_nFreqCalls++; *pFreq = BR86T_FREQ; return 1; }
int32_t BrPlatQueryPerfCounter(int64_t *pCount)
{ g_nCounterCalls++; *pCount = BR86T_COUNTER; return 1; }
uint32_t BrPlatTimeGetTime(void) { return 4242; }

static void os_set(void *h)       { (void)h; }
static void os_wait(void *h)      { (void)h; }
static void os_close(void *h)     { (void)h; }
static void os_lock(uint32_t h)   { (void)h; }
static void os_unlock(uint32_t h) { (void)h; }
static void os_begin(uint32_t ms) { (void)ms; g_nBegin++; }
static void os_end(uint32_t ms)   { (void)ms; g_nEnd++; }

static const BrPlatOs86 g_os = {
    os_set, os_wait, os_close, os_lock, os_unlock, os_begin, os_end
};

int main(void)
{
    unsigned char obj[0x24];
    int64_t  v64;
    int32_t  v32;

    g_pBrPlatOs86 = &g_os;

    memset(obj, 0, sizeof obj);
    BrX100751D0(obj);

    CHECK(g_nFreqCalls == 1, "the frequency was probed %d times", g_nFreqCalls);
    CHECK(g_nBegin == 0,
          "timeBeginPeriod must NOT be called when a counter exists");
    CHECK(g_nCounterCalls == 1, "the counter was read %d times",
          g_nCounterCalls);

    memcpy(&v64, obj + BR86_TMR_PERIOD, sizeof v64);
    CHECK(v64 == BR86T_FREQ / 30, "period is %lld, not freq/30 (%lld)",
          (long long)v64, (long long)(BR86T_FREQ / 30));

    memcpy(&v64, obj + BR86_TMR_NOW, sizeof v64);
    CHECK(v64 == BR86T_COUNTER, "now is %lld, not the counter",
          (long long)v64);

    memcpy(&v64, obj + BR86_TMR_DUE, sizeof v64);
    CHECK(v64 == BR86T_FREQ / 30, "due not seeded from period (%lld)",
          (long long)v64);

    /* The millisecond half of the object belongs to the other branch and is
     * untouched here. */
    memcpy(&v32, obj + BR86_TMR_PERIOD_MS, sizeof v32);
    CHECK(v32 == 0, "the counter path wrote period-ms (%d)", v32);
    memcpy(&v32, obj + BR86_TMR_NOW_MS, sizeof v32);
    CHECK(v32 == 0, "the counter path wrote now-ms (%d)", v32);

    /* A second call restarts the clock without re-probing. */
    BrX100751D0(obj);
    CHECK(g_nFreqCalls == 1, "the probe was repeated (%d)", g_nFreqCalls);
    CHECK(g_nCounterCalls == 2, "the restart did not re-read the counter");

    /* And the teardown does nothing when a counter exists. */
    BrX1002C2C0();
    CHECK(g_nEnd == 0, "timeEndPeriod called although a counter exists");

    if (g_fail == 0)
        printf("test_slice8_86_perf: all checks passed, 0 failures\n");
    else
        printf("test_slice8_86_perf: %d failures\n", g_fail);
    return g_fail != 0;
}
