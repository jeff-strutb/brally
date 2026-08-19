/* test_slice7_82.c -- behaviour tests for port/src/slice7_82.c.
 *
 * Everything asserted here is a property of the ORIGINAL or of the documented
 * platform contract the original was written against -- not of the port:
 *
 *   - _itoa's SIGN RULE (radix 10 signed, every other radix unsigned) and
 *     that it returns the caller's buffer.  Asserted as a ROUND TRIP through
 *     strtoul wherever a round trip is available, rather than against
 *     hand-typed digit strings.
 *   - GMEM_FIXED's three invariants: the handle is the pointer, GlobalUnlock
 *     on a fixed block is FALSE, GlobalFree yields NULL.
 *   - GetUserNameA's length convention: *pcb counts the NUL, and a buffer
 *     that is one byte too small FAILS without writing.
 *   - 0x1002BF40's ORDERING boundary: the pv == NULL test happens before the
 *     table is consulted, so a NULL argument is "registered" even with no
 *     table at all.  This is the exact case the stub answered backwards.
 *   - 0x10058700's toggle round trip (two calls restore the original value)
 *     and its -1 boundary: -1 is the "free" marker, so a UI object carrying
 *     -1 matches a FREE slot.  That is not a nice property; it is what the
 *     original does, and it is asserted so it cannot be "cleaned up".
 *   - 0x1003C520's out-parameter is written on EVERY path, failure included.
 *
 * Every cross-module callee and every cross-module global below is a STAND-IN
 * and lives only in this file, so the suite links against slice7_82.o alone.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "slice7_82.h"
#include "slice1_05.h"
#include "slice2_19.h"
#include "slice2_23.h"
#include "slice2_25.h"

static int g_fails;

static void check(int cond, const char *pszWhat)
{
    if (!cond) {
        printf("FAIL: %s\n", pszWhat);
        g_fails++;
    }
}

/* ====================================================================== */
/* Stand-ins for the cross-module callees and storage (test-only)         */
/* ====================================================================== */

/* --- slice5_60: the display-list registry pointer --------------------- */
BrPtrList *g_pBrDlPtrList;

/* --- slice2_25: the eight slots and the UI object --------------------- */
BrSlot   g_aBrAA2538[BR_SLOT_COUNT];
BrOptUi *g_brPA9D008;

static int s_nOpen2940;
int BrOptOpen2940(BrGameObj *pUnused)
{
    check(pUnused == NULL, "0x10043CD0 passes NULL through");
    s_nOpen2940++;
    return 1;
}

/* --- slice2_19: the 0x1003445A body ----------------------------------- */
static int         s_nOwnerFixup;
static BrDlOwner  *s_pLastOwner;
void BrDlOwnerFixup(BrDlOwner *pOwner)
{
    s_nOwnerFixup++;
    s_pLastOwner = pOwner;
}

/* --- slice2_23: the 0x1003E0E0 body ----------------------------------- */
static int                  s_n3E0E0;
static const BrActiveFlags *s_pLastFlags;
static int32_t              s_r3E0E0;
int32_t BrUiFn1003E0E0(const BrActiveFlags *pFlags)
{
    s_n3E0E0++;
    s_pLastFlags = pFlags;
    return s_r3E0E0;
}

/* ====================================================================== */
/* 1. 0x1008C000  _itoa                                                   */
/* ====================================================================== */

static void TestItoa(void)
{
    char sz[64];
    char *r;

    r = BrItoa(0, sz, 10);
    check(r == sz, "BrItoa returns the caller's buffer");
    check(strcmp(sz, "0") == 0, "0 base 10 is \"0\"");

    /* Round trips: whatever it produced must parse back to the same value.
     * This is a property, not a transcription of expected digits. */
    {
        static const int aV[] = { 1, 9, 10, 255, 65535, 1000000,
                                  2147483647 };
        static const int aR[] = { 2, 8, 10, 16, 36 };
        size_t i, j;

        for (i = 0; i < sizeof aV / sizeof aV[0]; ++i) {
            for (j = 0; j < sizeof aR / sizeof aR[0]; ++j) {
                unsigned long back;
                BrItoa(aV[i], sz, aR[j]);
                back = strtoul(sz, NULL, aR[j]);
                check(back == (unsigned long)aV[i],
                      "BrItoa round-trips through strtoul");
            }
        }
    }

    /* THE SIGN RULE.  Radix 10 is the only signed one. */
    BrItoa(-1, sz, 10);
    check(strcmp(sz, "-1") == 0, "-1 base 10 carries a sign");

    BrItoa(-1, sz, 16);
    check(sz[0] != '-', "-1 base 16 is UNSIGNED (no sign)");
    check(strtoul(sz, NULL, 16) == 0xFFFFFFFFuL,
          "-1 base 16 round-trips to 0xFFFFFFFF");

    BrItoa(-1, sz, 2);
    check(strlen(sz) == 32, "-1 base 2 is 32 unsigned digits");

    /* INT_MIN: negating it must not overflow. */
    BrItoa(INT_MIN, sz, 10);
    check(sz[0] == '-', "INT_MIN base 10 carries a sign");
    check(strtoul(sz + 1, NULL, 10) == 2147483648uL,
          "INT_MIN base 10 has the right magnitude");

    /* Digits above 9 are lower case, as the CRT emits them. */
    BrItoa(255, sz, 16);
    check(strcmp(sz, "ff") == 0, "hex digits are lower case");

    /* An out-of-range radix must not scribble. */
    memset(sz, 'X', sizeof sz);
    BrItoa(123, sz, 1);
    check(sz[0] == '\0', "an invalid radix yields an empty string");
}

/* ====================================================================== */
/* 2. The GlobalAlloc trio                                                */
/* ====================================================================== */

static int   s_nRelease;
static void *s_pReleased;
static void  TestRelease(void *p) { s_nRelease++; s_pReleased = p; }

static void TestGlobal(void)
{
    int   dummy;
    void *p = &dummy;
    void (*pfnSaved)(void *) = g_pfnBrGlobalRelease;

    check(BrGlobalHandle(p) == p, "a fixed block's handle IS its pointer");
    check(BrGlobalHandle(NULL) == NULL, "the handle of NULL is NULL");

    check(BrGlobalUnlock(p) == 0,
          "GlobalUnlock on a fixed block is FALSE -- the original's answer");

    g_pfnBrGlobalRelease = TestRelease;
    s_nRelease = 0; s_pReleased = NULL;

    check(BrGlobalFree(p) == NULL, "GlobalFree yields NULL on success");
    check(s_nRelease == 1, "GlobalFree releases exactly once");
    check(s_pReleased == p, "GlobalFree releases the block it was given");

    s_nRelease = 0;
    check(BrGlobalFree(NULL) == NULL, "GlobalFree(NULL) yields NULL");
    check(s_nRelease == 0, "GlobalFree(NULL) releases nothing");

    /* The idiom both call sites use, end to end. */
    s_nRelease = 0;
    (void)BrGlobalUnlock(BrGlobalHandle(p));
    (void)BrGlobalFree(BrGlobalHandle(p));
    check(s_nRelease == 1,
          "Unlock(Handle(p)) then Free(Handle(p)) releases p exactly once");

    g_pfnBrGlobalRelease = pfnSaved;
}

/* ====================================================================== */
/* 3. Timing                                                              */
/* ====================================================================== */

static void TestTiming(void)
{
    int64_t  freq = -1, a = -1, b = -1;
    uint32_t t0, t1;
    int      i;

    check(BrPlatQueryPerfFreq(&freq) != 0,
          "QueryPerfFreq reports a high-resolution counter");
    check(freq > 0, "the frequency is positive");
    check(BrPlatQueryPerfFreq(NULL) == 0, "a NULL frequency out-param fails");

    check(BrPlatQueryPerfCounter(&a) != 0, "QueryPerfCounter succeeds");
    for (i = 0; i < 100000; ++i) {
        /* burn a little time without calling anything that could sleep */
        static volatile int spin;
        spin = spin + 1;
    }
    check(BrPlatQueryPerfCounter(&b) != 0, "QueryPerfCounter succeeds twice");
    check(b >= a, "the performance counter is monotonic");

    /* slice4_50's 0x10075020 does `(c * 1000 + 500) / freq` in SIGNED 64-bit.
     * The counter must be small enough that the multiply cannot overflow. */
    check(b < (int64_t)9223372036854775807LL / 1000,
          "the counter cannot overflow 0x10075020's *1000");

    t0 = BrPlatTimeGetTime();
    t1 = BrPlatTimeGetTime();
    check(t1 >= t0, "timeGetTime does not run backwards");

    /* Sleep(0) must return; a hang here is the failure. */
    BrScrSleep(0);
    check(1, "BrScrSleep(0) returns");
}

/* ====================================================================== */
/* 4. GetUserNameA                                                        */
/* ====================================================================== */

static void TestUserName(void)
{
    char     sz[256];
    uint32_t cb;

    memset(sz, 'X', sizeof sz);
    cb = (uint32_t)sizeof sz;
    check(BrPlatGetUserName(sz, &cb) != 0, "GetUserName succeeds");
    check(cb >= 1u, "the reported size includes the NUL");
    check(strlen(sz) + 1u == (size_t)cb,
          "*pcb is strlen + 1 -- Win32 counts the NUL");

    /* One byte too small: fails, reports what it wanted, writes nothing. */
    {
        size_t want = strlen(sz) + 1u;
        char   szSmall[4];
        uint32_t cbSmall;

        memset(szSmall, 'X', sizeof szSmall);
        cbSmall = 1u;
        check(BrPlatGetUserName(szSmall, &cbSmall) == 0,
              "a one-byte buffer fails");
        check(szSmall[0] == 'X', "a failed call does not write the buffer");
        check((size_t)cbSmall == want,
              "a failed call reports the size it wanted");
    }

    cb = 16u;
    check(BrPlatGetUserName(NULL, &cb) == 0, "a NULL buffer fails");
    check(BrPlatGetUserName(sz, NULL) == 0, "a NULL size fails");
    cb = 0u;
    check(BrPlatGetUserName(sz, &cb) == 0, "a zero capacity fails");
}

/* ====================================================================== */
/* 5. 0x1002BF40  BrDlIsRegistered                                        */
/* ====================================================================== */

static void TestDlIsRegistered(void)
{
    static BrPtrList s_list;
    int a = 0, b = 0, c = 0;    /* addresses only; the values are never read */

    /* THE ORDERING BOUNDARY.  pv == NULL is answered before the table is
     * read, so it is 1 even when there is no table.  The stub said 0. */
    g_pBrDlPtrList = NULL;
    check(BrDlIsRegistered(NULL) == 1,
          "NULL is 'registered' even with no table -- tested first");
    check(BrDlIsRegistered(&a) == 0, "no table behaves as an empty table");

    s_list.n = 0;
    g_pBrDlPtrList = &s_list;
    check(BrDlIsRegistered(&a) == 0, "an empty table finds nothing");
    check(BrDlIsRegistered(NULL) == 1, "NULL is still 1 with an empty table");

    s_list.ap[0] = &a;
    s_list.ap[1] = &b;
    s_list.n = 2;
    check(BrDlIsRegistered(&a) == 1, "the first entry is found");
    check(BrDlIsRegistered(&b) == 1, "the last entry is found");
    check(BrDlIsRegistered(&c) == 0, "an absent pointer is not found");

    /* The count bounds the search: an entry past n is invisible. */
    s_list.ap[2] = &c;
    check(BrDlIsRegistered(&c) == 0,
          "the count bounds the search, not the array");
    s_list.n = 3;
    check(BrDlIsRegistered(&c) == 1, "raising the count reveals it");

    g_pBrDlPtrList = NULL;
}

/* ====================================================================== */
/* 6. 0x10058700                                                          */
/* ====================================================================== */

static void TestSub58700(void)
{
    BrOptUi ui;
    int     i;

    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        g_aBrAA2538[i].id = BR_SLOT_EMPTY;
        g_aBrAA2538[i].a  = 0;
        g_aBrAA2538[i].b  = 0;
    }

    g_brPA9D008 = NULL;
    check(BrSub10058700() == 0, "no UI object yields 0");

    ui.f00 = 0; ui.f04 = 0; ui.f08 = 77;
    g_brPA9D008 = &ui;
    check(BrSub10058700() == 0, "no matching slot yields 0");

    /* Toggle, and round-trip: two calls restore the original value. */
    g_aBrAA2538[3].id = 77;
    g_aBrAA2538[3].a  = 0;
    check(BrSub10058700() == 1, "0 toggles to 1 and 1 is returned");
    check(g_aBrAA2538[3].a == 1, "the slot now holds 1");
    check(BrSub10058700() == 0, "1 toggles back to 0 and 0 is returned");
    check(g_aBrAA2538[3].a == 0, "two toggles restore the original value");

    /* Only the matched slot moves. */
    g_aBrAA2538[5].id = 77;         /* a second slot with the same id */
    g_aBrAA2538[3].a  = 0;
    g_aBrAA2538[5].a  = 0;
    (void)BrSub10058700();
    check(g_aBrAA2538[3].a == 1 && g_aBrAA2538[5].a == 0,
          "the FIRST matching slot wins and later ones are untouched");

    /* THE -1 BOUNDARY.  br_slots.h's empty marker is -1, and the original
     * compares ids without excluding it, so a UI object carrying -1 matches
     * a FREE slot.  Asserted so it is not "fixed". */
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        g_aBrAA2538[i].id = BR_SLOT_EMPTY;
        g_aBrAA2538[i].a  = 0;
    }
    ui.f08 = BR_SLOT_EMPTY;
    check(BrSub10058700() == 1, "an id of -1 matches a FREE slot");
    check(g_aBrAA2538[0].a == 1, "and it is slot 0 that moves");

    /* Zero is a valid id, not an 'unset' marker. */
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        g_aBrAA2538[i].id = BR_SLOT_EMPTY;
        g_aBrAA2538[i].a  = 0;
    }
    g_aBrAA2538[6].id = 0;
    ui.f08 = 0;
    check(BrSub10058700() == 1, "0 is a valid id");
    check(g_aBrAA2538[6].a == 1, "and it selects slot 6");

    g_brPA9D008 = NULL;
}

/* ====================================================================== */
/* 7. 0x1003C520                                                          */
/* ====================================================================== */

static void *s_pFactoryOut;
static int32_t s_hrFactory;
static int     s_nFactory;

static int32_t TestFactory(void **ppOut)
{
    s_nFactory++;
    *ppOut = s_pFactoryOut;
    return s_hrFactory;
}

static void TestSub3C520(void)
{
    struct BrDPlay *pDp = (struct BrDPlay *)(void *)&s_nFactory;  /* poison */
    int32_t hr;

    g_pfnBrCoCreateDPlay = NULL;
    hr = BrSub1003C520(&pDp);
    check(hr == BR82_HR_CLASSNOTREG, "an unhooked factory reports a failure");
    check(hr < 0, "and that failure is a negative HRESULT");
    check(pDp == NULL, "the out-parameter is published even on failure");

    /* THE INVARIANT THAT MATTERS: the local is zeroed before the call and
     * stored back afterwards unconditionally, so a factory that FAILS but
     * still writes an out value has that value published.  slice6_70.c
     * depends on being able to test the published pointer after a >= 0
     * HRESULT. */
    s_pFactoryOut = (void *)&s_hrFactory;
    s_hrFactory   = -1;
    s_nFactory    = 0;
    g_pfnBrCoCreateDPlay = TestFactory;
    pDp = NULL;
    hr = BrSub1003C520(&pDp);
    check(s_nFactory == 1, "the factory is called exactly once");
    check(hr == -1, "the factory's HRESULT is returned unchanged");
    check((void *)pDp == s_pFactoryOut,
          "the out-parameter is published on the FAILURE path too");

    s_hrFactory = 0;
    pDp = NULL;
    hr = BrSub1003C520(&pDp);
    check(hr == 0, "a success HRESULT is returned unchanged");
    check((void *)pDp == s_pFactoryOut, "and the interface is published");

    g_pfnBrCoCreateDPlay = NULL;
}

/* ====================================================================== */
/* 8. The three adapters                                                  */
/* ====================================================================== */

static void TestAdapters(void)
{
    BrDlOwner owner;

    /* 0x1003445A -> BrDlOwnerFixup, one body, argument passed through. */
    s_nOwnerFixup = 0; s_pLastOwner = NULL;
    BrSub1003445A(&owner);
    check(s_nOwnerFixup == 1, "0x1003445A delegates exactly once");
    check(s_pLastOwner == &owner, "and passes its argument through");

    s_nOwnerFixup = 0; s_pLastOwner = &owner;
    BrSub1003445A(NULL);
    check(s_nOwnerFixup == 1 && s_pLastOwner == NULL,
          "a NULL owner is passed through unchanged, as the original does");

    /* 0x1003E0E0 -> BrUiFn1003E0E0. */
    g_pBrActiveFlags82 = NULL;
    s_n3E0E0 = 0; s_pLastFlags = NULL; s_r3E0E0 = 1;
    check(BrExt_1003E0E0() == 1, "0x1003E0E0 returns the body's answer");
    check(s_n3E0E0 == 1, "0x1003E0E0 delegates exactly once");
    check(s_pLastFlags != NULL,
          "an unwired flags pointer still hands the body a valid block");
    check(s_pLastFlags->a0 == 0 && s_pLastFlags->override == 0 &&
          s_pLastFlags->a8 == 0,
          "and that block is all zero, so nothing reads as active");

    {
        static BrActiveFlags s_host;
        s_host.a5 = 1;
        g_pBrActiveFlags82 = &s_host;
        s_n3E0E0 = 0; s_r3E0E0 = 0;
        check(BrExt_1003E0E0() == 0, "the body's answer is passed through");
        check(s_pLastFlags == &s_host,
              "a wired flags pointer reaches the body -- one object, not two");
        g_pBrActiveFlags82 = NULL;
    }

    /* 0x10043CD0 -> BrOptOpen2940, argument unread, result discarded. */
    s_nOpen2940 = 0;
    BrExt_10043CD0(0);
    check(s_nOpen2940 == 1, "0x10043CD0 delegates exactly once");
    BrExt_10043CD0(12345);
    check(s_nOpen2940 == 2,
          "and does so regardless of its argument, which is unread");

    /* atexit's contract. */
    check(BrXAtExit(NULL) == -1, "registering NULL fails");
}

int main(void)
{
    TestItoa();
    TestGlobal();
    TestTiming();
    TestUserName();
    TestDlIsRegistered();
    TestSub58700();
    TestSub3C520();
    TestAdapters();

    if (g_fails == 0) {
        printf("slice7_82: all checks passed\n");
        return 0;
    }
    printf("slice7_82: %d check(s) FAILED\n", g_fails);
    return 1;
}
