/* test_slice1_10.c -- behaviour tests for BrFfbShutdown (0x10079550).
 *
 * The only externally visible behaviour of this routine is (a) how it moves
 * the nesting counter and (b) which vtable slots it invokes on which objects,
 * in which order. Both are recorded with fake COM objects whose methods
 * append to a trace, so the assertions are about the control flow the
 * original actually has, not about numbers picked to match the port.
 */
#include "slice1_10.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ---- trace ---------------------------------------------------------- */

static char g_trace[128];

static void trace(const char *s)
{
    size_t n = strlen(g_trace);
    if (n + strlen(s) + 1 < sizeof(g_trace)) {
        if (n) g_trace[n++] = ',';
        strcpy(g_trace + n, s);
    }
}

/* ---- fake DirectInput objects ---------------------------------------- */

typedef struct FakeObj {
    BrDiObj     base;       /* must be first: the port reads pVtbl at +0 */
    const char *name;
} FakeObj;

static long fakeRelease(BrDiObj *pThis)
{
    char buf[32];
    sprintf(buf, "rel:%s", ((FakeObj *)pThis)->name);
    trace(buf);
    return 0;
}

static long fakeUnacquire(BrDiObj *pThis)
{
    char buf[32];
    sprintf(buf, "unacq:%s", ((FakeObj *)pThis)->name);
    trace(buf);
    return 0;
}

/* A vtable that traps every slot the routine must NOT call: they stay NULL,
 * so any stray call crashes the test rather than passing quietly. */
static const BrDiVtbl g_vtbl = {
    NULL, NULL, fakeRelease, NULL, NULL, NULL, NULL, NULL, fakeUnacquire
};

static FakeObj g_dev, g_spring, g_square;

/* Build a fully populated BrFfb with the given nesting count. */
static void setup(BrFfb *pFfb, int count)
{
    g_trace[0] = '\0';

    g_dev.base.pVtbl = &g_vtbl;    g_dev.name    = "dev";
    g_spring.base.pVtbl = &g_vtbl; g_spring.name = "spring";
    g_square.base.pVtbl = &g_vtbl; g_square.name = "square";

    pFfb->pDevice       = &g_dev.base;
    pFfb->pEffectSpring = &g_spring.base;
    pFfb->pEffectSquare = &g_square.base;
    pFfb->initCount     = count;
}

/* ---- a self-nulling Unacquire, for gotcha 4 --------------------------- */

static BrFfb *g_pTarget;
static int    g_reReadSeen;

static long nullingUnacquire(BrDiObj *pThis)
{
    (void)pThis;
    /* If the port cached the device pointer instead of re-reading the field,
     * this write would be invisible to it. */
    g_pTarget->pDevice = &g_spring.base;   /* a DIFFERENT live object */
    return 0;
}

static long witnessRelease(BrDiObj *pThis)
{
    if (pThis == &g_spring.base) g_reReadSeen = 1;
    return 0;
}

static const BrDiVtbl g_vtblReRead = {
    NULL, NULL, witnessRelease, NULL, NULL, NULL, NULL, NULL, nullingUnacquire
};

int main(void)
{
    BrFfb ffb;

    /* --- nesting: a shutdown that is not the last one releases nothing --- */
    setup(&ffb, 3);
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 2, "count 3 -> 2");
    check(g_trace[0] == '\0', "no release while the count is still raised");
    check(ffb.pDevice != NULL && ffb.pEffectSpring != NULL
          && ffb.pEffectSquare != NULL, "pointers untouched while nested");

    /* --- the last shutdown tears down, in the original's fixed order ----- */
    setup(&ffb, 1);
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 0, "count 1 -> 0");
    check(strcmp(g_trace, "rel:square,rel:spring,unacq:dev,rel:dev") == 0,
          "order: square, spring, then unacquire+release device");
    check(ffb.pDevice == NULL && ffb.pEffectSpring == NULL
          && ffb.pEffectSquare == NULL, "all three pointers cleared");

    /* --- teardown is idempotent: a second call finds nothing to do ------- */
    g_trace[0] = '\0';
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 0, "second shutdown leaves the count at 0");
    check(g_trace[0] == '\0', "second shutdown releases nothing");

    /* --- underflow clamp: 0 -> 0, and NO teardown on that path ----------- */
    setup(&ffb, 0);
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 0, "count 0 -> 0, never -1");
    check(g_trace[0] == '\0',
          "the clamp path returns early -- it does not fall into teardown");
    check(ffb.pDevice != NULL, "objects survive an unmatched shutdown");

    /* The clamp exists because the initialiser can skip its increment when
     * force feedback is disabled; several unmatched shutdowns must not drive
     * the counter negative and make a later real init un-tearable-down. */
    setup(&ffb, 0);
    BrFfbShutdown(&ffb);
    BrFfbShutdown(&ffb);
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 0, "repeated unmatched shutdowns stay pinned at 0");
    g_trace[0] = '\0';
    ffb.initCount = 1;
    BrFfbShutdown(&ffb);
    check(strcmp(g_trace, "rel:square,rel:spring,unacq:dev,rel:dev") == 0,
          "a real init after unmatched shutdowns still tears down");

    /* --- an already-negative count is also clamped, still without release  */
    setup(&ffb, -4);
    BrFfbShutdown(&ffb);
    check(ffb.initCount == 0, "negative count clamps to 0");
    check(g_trace[0] == '\0', "negative count releases nothing");

    /* --- partial state: each pointer is tested independently ------------- */
    setup(&ffb, 1);
    ffb.pEffectSpring = NULL;
    BrFfbShutdown(&ffb);
    check(strcmp(g_trace, "rel:square,unacq:dev,rel:dev") == 0,
          "a NULL effect is skipped, the rest still released");

    setup(&ffb, 1);
    ffb.pDevice = NULL;             /* the initialiser's failure paths do this
                                     * while leaving the counter raised */
    BrFfbShutdown(&ffb);
    check(strcmp(g_trace, "rel:square,rel:spring") == 0,
          "a NULL device is skipped: no unacquire, no release");
    check(ffb.initCount == 0, "count still reaches 0 with a NULL device");

    setup(&ffb, 1);
    ffb.pDevice = NULL; ffb.pEffectSpring = NULL; ffb.pEffectSquare = NULL;
    BrFfbShutdown(&ffb);
    check(g_trace[0] == '\0', "fully empty teardown is a clean no-op");

    /* --- gotcha 4: the device pointer is re-read after Unacquire --------- */
    setup(&ffb, 1);
    ffb.pEffectSpring = NULL;
    ffb.pEffectSquare = NULL;
    g_dev.base.pVtbl    = &g_vtblReRead;
    g_spring.base.pVtbl = &g_vtblReRead;
    g_pTarget    = &ffb;
    g_reReadSeen = 0;
    BrFfbShutdown(&ffb);
    check(g_reReadSeen,
          "Release runs on the RE-READ device field, not a cached pointer");

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
