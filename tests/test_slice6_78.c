/* test_slice6_78.c -- behavioural tests for packet 78.
 *
 * WHAT IS BEING TESTED, AND WHY IN THIS SHAPE
 * ===========================================
 * Seven of the sixteen entry points are ADAPTERS, and an adapter has exactly
 * one thing that can be wrong: it can forward to the wrong place, with the
 * wrong arguments, or in the wrong order.  Asserting the arithmetic of the
 * callee would test the callee's module, which already has its own suite --
 * so every adapter here is checked against a recording stand-in for IDENTITY
 * of the object and ORDER of the arguments, and nothing else.  The sixteen
 * arguments of 0x1002F900 are given sixteen DISTINCT values for that reason:
 * a transposition in the forwarding is the realistic failure and equal values
 * would hide it.
 *
 * The transcribed functions are checked as properties, not as volume:
 *
 *   0x10008B90  is a backward scan with three boundaries the original picked
 *       and a tidier routine would not: the LAST character is never examined,
 *       so "dir\" comes back whole; a one-character string short-circuits
 *       before the loop, so even "\" comes back whole; and the scan stops at
 *       the start of the string rather than running off it.  Those three are
 *       asserted directly, because each is the difference between this and
 *       the obvious `strrchr` version -- which would return "" , "" and the
 *       same answer respectively, i.e. wrong twice.
 *
 *   0x10002F90  is asserted as a ROUND TRIP, which is the whole content of
 *       the function: the size is correct AND the stream position is exactly
 *       where it was.  A version that omitted the final fseek would pass a
 *       size-only test and break every caller that sizes a file mid-read.
 *
 *   0x10002910  is a dispatch whose test is `== 1`, not `!= 0`.  The shipped
 *       g_brCdEnabled is 2, so a zero-test version would take the WRONG arm
 *       in the configuration that actually ships.  Both 1 and 2 are exercised.
 *       The backend's third gate is a MASK, not a branch, so "gate set, track
 *       zero" and "gate clear" are required to be indistinguishable -- that is
 *       what `neg/sbb/and` means and it is the reason the function cannot
 *       report "nothing selected" separately from track 0.
 *
 *   0x10008CC0  does not return.  That is its entire observable behaviour, so
 *       it is checked in a forked child: exit status 1, no signal.
 *
 * Every stand-in for a cross-slice symbol lives in THIS file, as the house
 * contract requires.  The real objects linked besides the module are
 * slice1_01.o (BrChkAlloc / BrChkVerbose ARE the allocator the file helpers
 * call -- faking them would test the fake) and br_crt.o (BrOperatorNew).
 */
#include "slice6_78.h"
#include "br_tmpfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { ++g_fails; \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ======================================================================
 * Stand-ins for the bodies the adapters forward to, and for the CD
 * globals br_data.c owns.
 * ====================================================================== */

uint8_t g_br4B0358;

/* 0x1002F900 -- slice1_05.c.  Records the object and all sixteen arguments so
 * a transposition is visible. */
struct BrGfxWords;
static const void *g_lerpObj;
static int         g_lerpArg[16];
static int         g_lerpCalls;

void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1);

void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    int i = 0;
    g_lerpObj = (const void *)pOut;
    g_lerpArg[i++] = a0;  g_lerpArg[i++] = b0;  g_lerpArg[i++] = c0;
    g_lerpArg[i++] = d0;  g_lerpArg[i++] = Aa0; g_lerpArg[i++] = Ab0;
    g_lerpArg[i++] = Ac0; g_lerpArg[i++] = Ad0; g_lerpArg[i++] = a1;
    g_lerpArg[i++] = b1;  g_lerpArg[i++] = c1;  g_lerpArg[i++] = d1;
    g_lerpArg[i++] = Aa1; g_lerpArg[i++] = Ab1; g_lerpArg[i++] = Ac1;
    g_lerpArg[i++] = Ad1;
    ++g_lerpCalls;
}

/* 0x10019260 / 0x10019270 / 0x100192F0 -- slice5_63.c. */
static int g_seq;                 /* next sequence number to hand out */
static int g_seq19260, g_seq19270;
static int g_sizeSeen, g_sizeCalls;

void BrSub_10019260(void);
void BrSub_10019270(void);
void BrSub_100192F0(int size);

void BrSub_10019260(void) { g_seq19260 = ++g_seq; }
void BrSub_10019270(void) { g_seq19270 = ++g_seq; }
void BrSub_100192F0(int size) { g_sizeSeen = size; ++g_sizeCalls; }

/* 0x100192A0 -- slice1_03.c. */
static int g_colors[6];
static int g_colorCalls;

void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6);
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    g_colors[0] = a1; g_colors[1] = a2; g_colors[2] = a3;
    g_colors[3] = a4; g_colors[4] = a5; g_colors[5] = a6;
    ++g_colorCalls;
}

/* 0x1003BD50 -- slice4_52.c.  A counter, so "the adapter called it once" and
 * "the adapter returned what it got" are separable. */
static int g_randNext = 1000;
static int g_randCalls;

int BrRandom(void);
int BrRandom(void) { ++g_randCalls; return g_randNext++; }

/* 0x1002BA00 -- slice2_16.c. */
static void *g_swapPv;
static int   g_swapN;
static int   g_swapCalls;

void BrSwapU16x4Array(void *pv, int count);
void BrSwapU16x4Array(void *pv, int count)
{
    g_swapPv = pv; g_swapN = count; ++g_swapCalls;
}

/* br_data.c -- the CD globals.  The shipped value of g_brCdEnabled is 2. */
int g_brCdEnabled  = 2;
int g_brCdPlaying;
int g_brCdTrackCur;

/* ======================================================================
 * 0x10008B90 -- path to basename
 * ====================================================================== */

static void Basename(const char *pszSrc, char *pszDst)
{
    /* The first argument is the original's dead __thiscall `this`; a
     * recognisable non-NULL value goes in so that a body which started
     * reading it would fault rather than quietly work. */
    BrPodWriterMakeName((void *)(uintptr_t)0x1u, pszSrc, pszDst);
}

static void TestBasenameSplitsOnLastSeparator(void)
{
    char buf[64];

    Basename("a\\b", buf);          CHECK(strcmp(buf, "b") == 0);
    Basename("a\\b\\c", buf);       CHECK(strcmp(buf, "c") == 0);
    Basename("\\a", buf);           CHECK(strcmp(buf, "a") == 0);
    Basename("dir\\sub\\f.pod", buf);
    CHECK(strcmp(buf, "f.pod") == 0);
}

static void TestBasenamePassesThroughWhenThereIsNoSeparator(void)
{
    char buf[64];

    Basename("file.pod", buf);      CHECK(strcmp(buf, "file.pod") == 0);
    Basename("a", buf);             CHECK(strcmp(buf, "a") == 0);
}

static void TestBasenameNeverExaminesTheLastCharacter(void)
{
    char buf[64];

    /* The scan starts at len-1 and only ever tests p[-1], so a TRAILING
     * separator is not a separator.  strrchr() would give "" for both. */
    Basename("dir\\", buf);         CHECK(strcmp(buf, "dir\\") == 0);
    Basename("a\\", buf);           CHECK(strcmp(buf, "a\\") == 0);

    /* A one-character string short-circuits before the loop even runs, so a
     * lone separator survives too. */
    Basename("\\", buf);            CHECK(strcmp(buf, "\\") == 0);
}

static void TestBasenameStopsAtTheStartOfTheString(void)
{
    /* Guard bytes on both sides: a scan that ran off the front would have to
     * pass through the leading guard, and one that over-copied would trip the
     * trailing guard on the destination. */
    static const char kSrc[] = "\x5C\x5C" "no_separator_here_at_all";
    char        srcBuf[64];
    char        dstBuf[64];
    const char *pSrc = srcBuf + 8;

    memset(srcBuf, 0x5C, sizeof(srcBuf));      /* backslashes everywhere */
    memcpy(srcBuf + 8, kSrc + 2, sizeof(kSrc) - 2);

    memset(dstBuf, 0x7F, sizeof(dstBuf));
    Basename(pSrc, dstBuf);

    /* pSrc[-1] is a backslash, but it lies BEFORE the string: the scan must
     * terminate on `p == pszSrc` first and hand back the whole string. */
    CHECK(strcmp(dstBuf, "no_separator_here_at_all") == 0);
    CHECK(dstBuf[strlen("no_separator_here_at_all") + 1] == 0x7F);
}

static void TestBasenameEmptySourceIsTheDocumentedDeviation(void)
{
    char buf[8];

    memset(buf, 0x7F, sizeof(buf));
    Basename("", buf);

    /* DEVIATION, deliberately pinned so it cannot drift back into the
     * original's unbounded backward scan: the empty string is copied. */
    CHECK(buf[0] == '\0');
    CHECK(buf[1] == 0x7F);
}

/* ======================================================================
 * The CHK_* file helpers
 * ====================================================================== */

static const char *WriteTempFile(const char *pszBody)
{
    const char *szPath = BrTmpPath(0, "test_slice6_78_tmp");
    FILE *pf = fopen(szPath, "wb");

    if (pf == NULL) {
        return NULL;
    }
    fwrite(pszBody, 1, strlen(pszBody), pf);
    fclose(pf);
    return szPath;
}

static void TestChkOpenSizeCloseRoundTrip(void)
{
    const char *pszPath = WriteTempFile("0123456789ABCDEF");
    FILE      **pp;

    if (pszPath == NULL) { printf("SKIP no writable cwd\n"); return; }

    pp = BrChkFReadOpen(pszPath);
    CHECK(pp != NULL);
    if (pp == NULL) { return; }

    /* The handle's FIRST field is the FILE *, which is the whole content of
     * the `FILE **` pun slice2_20.c relies on. */
    CHECK(*pp != NULL);

    CHECK(BrChkFileSize(pp) == 16);

    BrChkFClose(pp);
    remove(pszPath);
}

static void TestChkFileSizePreservesThePosition(void)
{
    const char *pszPath = WriteTempFile("0123456789ABCDEF");
    FILE      **pp;
    char        c;

    if (pszPath == NULL) { return; }

    pp = BrChkFReadOpen(pszPath);
    if (pp == NULL) { return; }

    CHECK(fread(&c, 1, 1, *pp) == 1);
    CHECK(c == '0');
    CHECK(ftell(*pp) == 1);

    /* The point of the function's four calls: measuring must not move the
     * stream.  A size-only implementation leaves it at EOF. */
    CHECK(BrChkFileSize(pp) == 16);
    CHECK(ftell(*pp) == 1);

    CHECK(fread(&c, 1, 1, *pp) == 1);
    CHECK(c == '1');

    BrChkFClose(pp);
    remove(pszPath);
}

static void TestChkFileSizeOnAnEmptyFile(void)
{
    const char *pszPath = WriteTempFile("");
    FILE      **pp;

    if (pszPath == NULL) { return; }

    pp = BrChkFReadOpen(pszPath);
    if (pp == NULL) { return; }

    /* Zero is a real answer here and is not an error signal. */
    CHECK(BrChkFileSize(pp) == 0);
    CHECK(ftell(*pp) == 0);

    BrChkFClose(pp);
    remove(pszPath);
}

static void TestChkOpenCopiesThePathRatherThanBorrowingIt(void)
{
    char        szPath[64];
    const char *pszWritten = WriteTempFile("xyz");
    FILE      **pp;

    if (pszWritten == NULL) { return; }

    strcpy(szPath, pszWritten);
    pp = BrChkFReadOpen(szPath);
    if (pp == NULL) { return; }

    /* The original allocates a private copy and every later diagnostic reads
     * THAT.  Scribbling the caller's buffer must not affect the handle -- if
     * the name were borrowed, BrChkFClose would read freed or mutated text. */
    memset(szPath, 'Z', sizeof(szPath) - 1);
    szPath[sizeof(szPath) - 1] = '\0';

    CHECK(BrChkFileSize(pp) == 3);
    BrChkFClose(pp);                 /* must not read szPath */
    remove(pszWritten);
}

/* ======================================================================
 * 0x10002910 / 0x10002490 -- the CD track query
 * ====================================================================== */

static int  g_hookCalls;
static int  HookTrack(void) { ++g_hookCalls; return 77; }

static void TestCdDispatchTestsEqualOneNotNonZero(void)
{
    g_pfnBrCdTrackGet0024C0 = HookTrack;
    g_hookCalls = 0;

    g_brCdPlaying  = 1;
    g_brCdMediaOk  = 1;
    g_brCdTrackCur = 5;

    g_brCdEnabled = 1;
    CHECK(BrCdTrackGet() == 77);
    CHECK(g_hookCalls == 1);

    /* 2 is the value that SHIPS.  A `!= 0` dispatch would send it to the MCI
     * arm; the original's `cmp ...,1 / jne` sends it to the portable one. */
    g_brCdEnabled = 2;
    CHECK(BrCdTrackGet() == 5);
    CHECK(g_hookCalls == 1);

    g_brCdEnabled = 3;
    CHECK(BrCdTrackGet() == 5);
    CHECK(g_hookCalls == 1);

    g_pfnBrCdTrackGet0024C0 = NULL;
}

static void TestCdBackendGates(void)
{
    g_brCdEnabled  = 2;
    g_brCdPlaying  = 1;
    g_brCdMediaOk  = 1;
    g_brCdTrackCur = 9;
    CHECK(BrCdTrackGetEar() == 9);

    g_brCdPlaying = 0;
    CHECK(BrCdTrackGetEar() == 0);
    g_brCdPlaying = 1;

    g_brCdEnabled = 0;
    CHECK(BrCdTrackGetEar() == 0);
    g_brCdEnabled = 2;

    g_brCdMediaOk = 0;
    CHECK(BrCdTrackGetEar() == 0);
    g_brCdMediaOk = 1;

    CHECK(BrCdTrackGetEar() == 9);
}

static void TestCdBackendCannotDistinguishTrackZero(void)
{
    /* `neg / sbb / and` is a mask: "playing track 0" and "not playing" both
     * come back as 0.  That is why br_audio.h's "-1 when nothing is selected"
     * is its own contract and not this function's -- see the header. */
    g_brCdEnabled  = 2;
    g_brCdPlaying  = 1;
    g_brCdMediaOk  = 1;
    g_brCdTrackCur = 0;
    CHECK(BrCdTrackGetEar() == 0);

    g_brCdMediaOk = 0;
    CHECK(BrCdTrackGetEar() == 0);

    g_brCdMediaOk = 1;
}

/* ======================================================================
 * The single-store functions
 * ====================================================================== */

static void TestSingleStores(void)
{
    g_br675540 = 0x5A5A5A5A;
    BrSegSetFlag(0u);
    CHECK(g_br675540 == 0);
    BrSegSetFlag(0xFFFFFFFFu);
    CHECK(g_br675540 == -1);        /* stored as-is, not clamped */
    BrSegSetFlag(1u);
    CHECK(g_br675540 == 1);

    /* One byte, two writers, and they are NOT the same address as
     * slice5_63's 0x104B0358 -- see the header. */
    g_br4B0360 = 0x7F;
    BrSub_10019240();
    CHECK(g_br4B0360 == 1u);
    BrSub_10019250();
    CHECK(g_br4B0360 == 0u);
    BrSub_10019240();
    CHECK(g_br4B0360 == 1u);
}

/* ======================================================================
 * The adapters: identity and order of what is forwarded
 * ====================================================================== */

static void TestGfx2F900ForwardsObjectAndAllSixteenInOrder(void)
{
    uint32_t aCmd[2];
    int      i;

    g_lerpCalls = 0;
    g_lerpObj   = NULL;
    memset(g_lerpArg, 0, sizeof(g_lerpArg));

    BrGfx2F900(aCmd, 101, 102, 103, 104, 105, 106, 107, 108,
                     109, 110, 111, 112, 113, 114, 115, 116);

    CHECK(g_lerpCalls == 1);
    CHECK(g_lerpObj == (const void *)aCmd);
    for (i = 0; i < 16; ++i) {
        CHECK(g_lerpArg[i] == 101 + i);
    }
}

static void TestTextAdaptersForward(void)
{
    int i;

    g_sizeCalls = 0;
    BrTextSetSize(37);
    CHECK(g_sizeCalls == 1);
    CHECK(g_sizeSeen == 37);

    /* Negative sizes reach the state unchanged; nothing clamps. */
    BrTextSetSize(-4);
    CHECK(g_sizeSeen == -4);

    g_br4B0358 = 99;
    BrTextFlag358Clear();
    CHECK(g_br4B0358 == 0);

    g_seq = 0; g_seq19270 = 0;
    BrTextAlignCentre();
    CHECK(g_seq19270 == 1);

    g_colorCalls = 0;
    BrTextSetColor6(1, 2, 3, 4, 5, 6);
    CHECK(g_colorCalls == 1);
    for (i = 0; i < 6; ++i) {
        CHECK(g_colors[i] == i + 1);
    }
}

static void TestRandForwardsAndReturns(void)
{
    int a, b;

    g_randCalls = 0;
    g_randNext  = 4242;

    a = BrRand();
    b = BrRand();

    CHECK(g_randCalls == 2);
    CHECK(a == 4242);
    CHECK(b == 4243);
    CHECK(a != b);               /* the stub returned a constant 0 */
}

static void TestSwapRec8ArrayForwards(void)
{
    unsigned char aBuf[16];

    g_swapCalls = 0;
    BrSwapRec8Array(aBuf, 2);

    CHECK(g_swapCalls == 1);
    CHECK(g_swapPv == (void *)aBuf);
    CHECK(g_swapN == 2);

    /* n <= 0 is the callee's no-op, not the adapter's: the adapter must still
     * forward it rather than filtering. */
    BrSwapRec8Array(aBuf, 0);
    CHECK(g_swapCalls == 2);
    CHECK(g_swapN == 0);
}

/* ======================================================================
 * 0x10008CC0 -- does not return
 * ====================================================================== */

static void TestErrorfTerminatesWithStatusOne(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        BrErrorf("unreachable %s %d", "tail", 1);
        _exit(99);               /* reached only if BrErrorf returned */
    }

    if (pid < 0) {
        printf("SKIP fork unavailable\n");
        return;
    }

    {
        int status = 0;
        CHECK(waitpid(pid, &status, 0) == pid);
        CHECK(WIFEXITED(status));
        if (WIFEXITED(status)) {
            /* 1 is the original's exit code; 99 would mean it returned. */
            CHECK(WEXITSTATUS(status) == 1);
        }
    }
}

int main(void)
{
    TestBasenameSplitsOnLastSeparator();
    TestBasenamePassesThroughWhenThereIsNoSeparator();
    TestBasenameNeverExaminesTheLastCharacter();
    TestBasenameStopsAtTheStartOfTheString();
    TestBasenameEmptySourceIsTheDocumentedDeviation();

    TestChkOpenSizeCloseRoundTrip();
    TestChkFileSizePreservesThePosition();
    TestChkFileSizeOnAnEmptyFile();
    TestChkOpenCopiesThePathRatherThanBorrowingIt();

    TestCdDispatchTestsEqualOneNotNonZero();
    TestCdBackendGates();
    TestCdBackendCannotDistinguishTrackZero();

    TestSingleStores();

    TestGfx2F900ForwardsObjectAndAllSixteenInOrder();
    TestTextAdaptersForward();
    TestRandForwardsAndReturns();
    TestSwapRec8ArrayForwards();

    TestErrorfTerminatesWithStatusOne();

    if (g_fails == 0) {
        printf("test_slice6_78: all checks passed\n");
        return 0;
    }
    printf("test_slice6_78: %d failures\n", g_fails);
    return 1;
}
