/* test_slice1_04.c -- behaviour/invariant tests for slice1_04.c */

#include "slice1_04.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static int g_checks = 0;

#define CHECK(cond) do {                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_fails;                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                  \
    } while (0)

/* ------------------------------------------------------------------ */
/* Texture size codecs                                                  */
/* ------------------------------------------------------------------ */

static void TestTexShift(void)
{
    int s, s2, n;

    /* 256 >> shift is the next power of two at or above max(a,b). */
    for (n = 1; n <= 256; ++n) {
        int pow2 = 1;
        while (pow2 < n) { pow2 <<= 1; }
        CHECK(BrTexShiftFromSize(&s, n, 1) == 1);
        CHECK((256 >> s) == pow2);
        /* symmetric in its two size arguments */
        CHECK(BrTexShiftFromSize(&s2, 1, n) == 1);
        CHECK(s2 == s);
        CHECK(BrTexShiftFromSize(&s2, n, n) == 1);
        CHECK(s2 == s);
    }

    /* Boundary: 256 is the last accepted size; 257 fails but still writes. */
    CHECK(BrTexShiftFromSize(&s, 256, 256) == 1);
    CHECK(s == 0);
    s = 99;
    CHECK(BrTexShiftFromSize(&s, 257, 1) == 0);
    CHECK(s == 0);

    /* Non-positive sizes fall into the "<= 1" rung. */
    CHECK(BrTexShiftFromSize(&s, 0, 0) == 1);
    CHECK(s == 8);
    CHECK(BrTexShiftFromSize(&s, -4, -4) == 1);
    CHECK(s == 8);
}

static void TestTexAspectRoundTrip(void)
{
    int w, h, ratio, dir;

    /* Exact round-trip for every power-of-two pair within 8:1 either way. */
    for (w = 1; w <= 256; w <<= 1) {
        for (dir = 0; dir < 2; ++dir) {
            for (ratio = 1; ratio <= 8; ratio <<= 1) {
                int shift, code, oa, ob, okS, okA;
                h = dir ? w * ratio : w / ratio;
                if (h < 1 || h > 256) { continue; }

                okS = BrTexShiftFromSize(&shift, w, h);
                okA = BrTexAspectFromSize(&code, w, h);
                CHECK(okS == 1);
                CHECK(okA == 1);
                CHECK(code >= 0 && code <= 6);

                oa = ob = -1;
                BrTexSizeFromShiftAspect(&oa, &ob, shift, code);
                CHECK(oa == w);
                CHECK(ob == h);
            }
        }
    }

    /* Documented code assignments at the two extremes and the square case. */
    {
        int code;
        CHECK(BrTexAspectFromSize(&code, 64, 8) == 1); CHECK(code == 0);
        CHECK(BrTexAspectFromSize(&code, 32, 8) == 1); CHECK(code == 1);
        CHECK(BrTexAspectFromSize(&code, 16, 8) == 1); CHECK(code == 2);
        CHECK(BrTexAspectFromSize(&code,  8, 8) == 1); CHECK(code == 3);
        CHECK(BrTexAspectFromSize(&code,  8,16) == 1); CHECK(code == 4);
        CHECK(BrTexAspectFromSize(&code,  8,32) == 1); CHECK(code == 5);
        CHECK(BrTexAspectFromSize(&code,  8,64) == 1); CHECK(code == 6);

        /* Beyond 8:1 the code clamps and the return value reports inexact. */
        CHECK(BrTexAspectFromSize(&code, 128, 8) == 0); CHECK(code == 0);
        CHECK(BrTexAspectFromSize(&code, 8, 128) == 0); CHECK(code == 6);
        /* Non-power-of-two ratio: 3:1 -> 24/8 rungs down to code 2. */
        CHECK(BrTexAspectFromSize(&code, 24, 8) == 0); CHECK(code == 2);
    }
}

static void TestTexSizeDecodeEdges(void)
{
    int a, b;

    /* shift beyond the jump table shares the index-8 arm. */
    a = b = -1;
    BrTexSizeFromShiftAspect(&a, &b, 8, 3);
    CHECK(a == 1 && b == 1);
    a = b = -1;
    BrTexSizeFromShiftAspect(&a, &b, 1000, 3);
    CHECK(a == 1 && b == 1);
    a = b = -1;
    BrTexSizeFromShiftAspect(&a, &b, -1, 3);
    CHECK(a == 1 && b == 1);

    /* GOTCHA: aspect > 6 leaves *pB untouched while *pA is already set. */
    a = -1; b = -12345;
    BrTexSizeFromShiftAspect(&a, &b, 0, 7);
    CHECK(a == 0x100);
    CHECK(b == -12345);
}

static void TestTexFormatCode(void)
{
    int a, b, c;

    CHECK(BrTexFormatCode(0, 4, 1) == 11);
    CHECK(BrTexFormatCode(0, 4, 0) == 2);
    CHECK(BrTexFormatCode(0, 4, 2) == 2);
    CHECK(BrTexFormatCode(0, 4, -1) == 2);
    CHECK(BrTexFormatCode(0, 2, 1) == 11);
    CHECK(BrTexFormatCode(0, 3, 1) == 11);

    CHECK(BrTexFormatCode(1, 3, 1) == 12);
    CHECK(BrTexFormatCode(1, 3, 0) == 4);
    CHECK(BrTexFormatCode(1, 3, 9) == 4);
    CHECK(BrTexFormatCode(1, 4, 1) == 2);
    CHECK(BrTexFormatCode(1, 4, 0) == 2);
    CHECK(BrTexFormatCode(1, 2, 1) == 11);

    /* Everything with a >= 2 is the catch-all, including the dead-load arm. */
    for (a = 2; a < 6; ++a) {
        for (b = 0; b < 6; ++b) {
            for (c = 0; c < 3; ++c) {
                CHECK(BrTexFormatCode(a, b, c) == 11);
            }
        }
    }
    /* Only the listed (a,b) pairs escape 11. */
    for (b = 0; b < 8; ++b) {
        if (b != 4) { CHECK(BrTexFormatCode(0, b, 1) == 11); }
        if (b != 3 && b != 4) { CHECK(BrTexFormatCode(1, b, 1) == 11); }
    }
}

/* ------------------------------------------------------------------ */
/* Record table search                                                  */
/* ------------------------------------------------------------------ */

static void TestTblFind(void)
{
    BrTblRec aRecs[4];
    BrTblRec probe;
    int i;

    CHECK(sizeof(BrTblRec) == BR_TBL_REC_SIZE);
    CHECK((size_t)((char *)&aRecs[0].f4C  - (char *)&aRecs[0]) == 0x4C);
    CHECK((size_t)((char *)&aRecs[0].f50  - (char *)&aRecs[0]) == 0x50);
    CHECK((size_t)((char *)&aRecs[0].f268 - (char *)&aRecs[0]) == 0x268);
    CHECK((size_t)((char *)&aRecs[0].f294 - (char *)&aRecs[0]) == 0x294);

    memset(aRecs, 0, sizeof aRecs);
    memset(&probe, 0, sizeof probe);
    for (i = 0; i < 4; ++i) {
        aRecs[i].f4C  = 100 + i;
        aRecs[i].f50  = 200 + i;
        aRecs[i].f268 = 1;
        memset(aRecs[i].f294, (unsigned char)(0x10 + i), 8);
    }

    /* count == 0 short-circuits before anything is read. */
    CHECK(BrTblFind(aRecs, 0, &probe) == -1);

    /* exact match on all three keys */
    probe.f4C = 102; probe.f50 = 202; probe.f268 = 1;
    memset(probe.f294, 0x12, 8);
    CHECK(BrTblFind(aRecs, 4, &probe) == 2);

    /* f4C / f50 mismatch rejects even with an identical key */
    probe.f50 = 999;
    CHECK(BrTblFind(aRecs, 4, &probe) == -1);
    probe.f50 = 202;

    /* key mismatch rejects while both sides claim f268 == 1 */
    memset(probe.f294, 0xAB, 8);
    CHECK(BrTblFind(aRecs, 4, &probe) == -1);

    /* GOTCHA: f268 != 1 on EITHER side accepts regardless of the key. */
    probe.f268 = 0;
    CHECK(BrTblFind(aRecs, 4, &probe) == 2);
    probe.f268 = 1;
    aRecs[2].f268 = 7;
    CHECK(BrTblFind(aRecs, 4, &probe) == 2);
    aRecs[2].f268 = 1;

    /* first match wins */
    memset(probe.f294, 0x12, 8);
    aRecs[0].f4C = 102; aRecs[0].f50 = 202;
    memset(aRecs[0].f294, 0x12, 8);
    CHECK(BrTblFind(aRecs, 4, &probe) == 0);
}

/* ------------------------------------------------------------------ */
/* EAR loader                                                           */
/* ------------------------------------------------------------------ */

static int g_haveIas;
static int g_havePds;
static int g_missingProc;      /* index to fail, or -1 */
static int g_getModuleCalls;
static int g_loadLibraryCalls;
static int g_wndMsgCalls;
static char g_lastLoaded[32];

static void StubProc(void) { }

static void *StubGetModuleHandle(const char *pszName)
{
    ++g_getModuleCalls;
    (void)pszName;
    return NULL;     /* nothing is ever already-resident in these tests */
}

static void *StubLoadLibrary(const char *pszName)
{
    ++g_loadLibraryCalls;
    if (strcmp(pszName, "earias.dll") == 0 && g_haveIas) {
        strcpy(g_lastLoaded, pszName);
        return (void *)(uintptr_t)0x1000;
    }
    if (strcmp(pszName, "earpds.dll") == 0 && g_havePds) {
        strcpy(g_lastLoaded, pszName);
        return (void *)(uintptr_t)0x2000;
    }
    return NULL;
}

static BrEarProc StubGetProcAddress(void *hModule, const char *pszName)
{
    int i;
    if (hModule == NULL) {
        return NULL;
    }
    for (i = 0; i < BR_EAR_PROC_COUNT; ++i) {
        if (strcmp(pszName, g_apszBrEarProc[i]) == 0) {
            return (i == g_missingProc) ? NULL : StubProc;
        }
    }
    return NULL;
}

static unsigned int StubRegisterWindowMessage(const char *pszName)
{
    ++g_wndMsgCalls;
    return (strcmp(pszName, "EAR Interactive Around-Sound") == 0) ? 0xC001u : 0u;
}

static const BrEarPlatform g_stub = {
    StubGetModuleHandle,
    StubLoadLibrary,
    StubGetProcAddress,
    StubRegisterWindowMessage
};

static void EarReset(int haveIas, int havePds, int missingProc)
{
    g_haveIas = haveIas;
    g_havePds = havePds;
    g_missingProc = missingProc;
    g_getModuleCalls = g_loadLibraryCalls = g_wndMsgCalls = 0;
    g_lastLoaded[0] = '\0';
}

static void TestEarLoad(void)
{
    BrEarState st;
    int i;

    /* The export table must be complete and unique. */
    for (i = 0; i < BR_EAR_PROC_COUNT; ++i) {
        int j;
        CHECK(g_apszBrEarProc[i] != NULL);
        for (j = 0; j < i; ++j) {
            CHECK(strcmp(g_apszBrEarProc[i], g_apszBrEarProc[j]) != 0);
        }
    }
    CHECK(BR_EAR_PROC_COUNT == 31);

    /* usePds == 0, earias present: no fallback, all 31 bound. */
    memset(&st, 0, sizeof st);
    EarReset(1, 1, -1);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 1);
    CHECK(strcmp(g_lastLoaded, "earias.dll") == 0);
    CHECK(st.fallbackTried == 0);
    CHECK(st.preferPds == 0);
    CHECK(st.usedLoadLibrary == 1);
    CHECK(st.windowMessage == 0xC001u);
    CHECK(g_wndMsgCalls == 1);
    for (i = 0; i < BR_EAR_PROC_COUNT; ++i) {
        CHECK(st.apfn[i] != NULL);
    }

    /* Second call takes the early-out: no further platform traffic. */
    EarReset(1, 1, -1);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 1);
    CHECK(g_getModuleCalls == 0);
    CHECK(g_loadLibraryCalls == 0);
    CHECK(g_wndMsgCalls == 0);

    /* usePds == 0, earias absent: falls back to earpds. */
    memset(&st, 0, sizeof st);
    EarReset(0, 1, -1);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 1);
    CHECK(strcmp(g_lastLoaded, "earpds.dll") == 0);
    CHECK(st.fallbackTried == 1);
    CHECK(st.usedLoadLibrary == 1);

    /* usePds != 0 goes straight to earpds and sets preferPds. */
    memset(&st, 0, sizeof st);
    EarReset(1, 1, -1);
    CHECK(BrEarLoad(&st, &g_stub, 1) == 1);
    CHECK(strcmp(g_lastLoaded, "earpds.dll") == 0);
    CHECK(st.preferPds == 1);
    CHECK(st.fallbackTried == 0);

    /* Nothing loadable, usePds == 0: two names x two lookups, then fail. */
    memset(&st, 0, sizeof st);
    EarReset(0, 0, -1);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 0);
    CHECK(g_getModuleCalls == 2);
    CHECK(g_loadLibraryCalls == 2);
    CHECK(g_wndMsgCalls == 0);

    /* GOTCHA: nothing loadable with usePds != 0 does NOT bail early -- it
     * resolves against a NULL handle and only then reports failure. */
    memset(&st, 0, sizeof st);
    EarReset(0, 0, -1);
    CHECK(BrEarLoad(&st, &g_stub, 1) == 0);
    CHECK(g_getModuleCalls == 1);
    CHECK(g_loadLibraryCalls == 1);
    CHECK(st.hModule == NULL);
    CHECK(g_wndMsgCalls == 0);

    /* One missing export fails the whole bind and skips the window message,
     * but the other 30 slots are still populated. */
    memset(&st, 0, sizeof st);
    EarReset(1, 1, BR_EAR_REGISTER_MATRIX);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 0);
    CHECK(g_wndMsgCalls == 0);
    CHECK(st.apfn[BR_EAR_REGISTER_MATRIX] == NULL);
    CHECK(st.apfn[BR_EAR_UPDATE_EAR] != NULL);

    /* GOTCHA (original bug): because UpdateEar did resolve, the very next
     * call reports success over that half-bound table. */
    EarReset(1, 1, BR_EAR_REGISTER_MATRIX);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 1);
    CHECK(st.apfn[BR_EAR_REGISTER_MATRIX] == NULL);
    CHECK(g_getModuleCalls == 0);

    /* If UpdateEar itself is the missing one there is no such stickiness. */
    memset(&st, 0, sizeof st);
    EarReset(1, 1, BR_EAR_UPDATE_EAR);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 0);
    EarReset(1, 1, BR_EAR_UPDATE_EAR);
    CHECK(BrEarLoad(&st, &g_stub, 0) == 0);
    CHECK(g_getModuleCalls == 1);
}

int main(void)
{
    TestTexShift();
    TestTexAspectRoundTrip();
    TestTexSizeDecodeEdges();
    TestTexFormatCode();
    TestTblFind();
    TestEarLoad();

    printf("%d checks, %d failures\n", g_checks, g_fails);
    return g_fails != 0;
}
