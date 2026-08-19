/* test_br_drawenv.c -- structural tests for BrEnvEmit (0x10017110).
 *
 * NOT a command-stream golden-bytes test: the FP-dense inner loops are
 * TODO and the command sequence has not been independently validated.
 * These tests verify GUARD behaviour (early returns) and that the
 * function produces DL output when guards pass.
 */
#include "br_drawenv.h"
#include "br_drawcar.h"
#include "slice1_05.h"
#include "slice2_17.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_fails;
#define CHECK(c, ...) do { if (!(c)) { \
        printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); \
        printf("\n"); ++g_fails; } } while (0)

/* ---- DL cursor (same pattern as test_br_drawcar.c) ---- */
uint32_t *BrG_6C0680;
static uint32_t s_dl[4096];
static void dl_reset(void) { memset(s_dl, 0, sizeof(s_dl)); BrG_6C0680 = s_dl; }
static int  dl_count(void) { return (int)(BrG_6C0680 - s_dl) / 2; }

/* ---- stubs for callees ---- */
static int s_invertCalls;
int BrMtxInvert(BrMat4 *pOut, const BrMat4 *pSrc)
{
    (void)pOut; (void)pSrc;
    ++s_invertCalls;
    return 1;
}
void BrMtxMul(BrMat4 *pOut, const BrMat4 *pA, const BrMat4 *pB)
{
    (void)pOut; (void)pA; (void)pB;
}
void BrMemFill(void *pDst, uint32_t count, int32_t value)
{
    memset(pDst, value, count);
}
void BrVec3NormaliseGuard(BrVec3 *pV) { (void)pV; }
void BrRdpSetCombineLERP(BrGfxWords *p,
    int a0, int b0, int c0, int d0, int Aa0, int Ab0, int Ac0, int Ad0,
    int a1, int b1, int c1, int d1, int Aa1, int Ab1, int Ac1, int Ad1)
{
    (void)p;
    (void)a0;(void)b0;(void)c0;(void)d0;
    (void)Aa0;(void)Ab0;(void)Ac0;(void)Ad0;
    (void)a1;(void)b1;(void)c1;(void)d1;
    (void)Aa1;(void)Ab1;(void)Ac1;(void)Ad1;
}

/* Globals the test supplies. */
int32_t  BrG_6C6624;
int32_t  BrG_6C1174;
uint32_t BrG_6C0258;
int32_t  g_brRaceBeginDifficulty;
int32_t  g_BrFpsScreenW, g_BrFpsScreenH;

/* br_drawcar.h globals this test needs. */
int32_t  g_BrDrawWheelAlt;
void    *g_BrDrawTrackFlags;
BrMat4   g_BrDrawCombined;
BrMat4   g_BrDrawScale;
uint32_t g_BrDrawRenderMode;
int32_t  g_BrDrawFogAlpha;

/* ---- fixture ---- */
static unsigned char s_trackFlags[256];
static uint16_t s_flagIndices[4];
static uint32_t s_texLookup[4];
static uint8_t  s_bitmap[0x2000];

static void env_reset(void)
{
    memset(s_trackFlags, 0, sizeof s_trackFlags);
    memset(s_flagIndices, 0, sizeof s_flagIndices);
    memset(&g_BrDrawCombined, 0, sizeof g_BrDrawCombined);
    memset(&g_BrDrawScale, 0, sizeof g_BrDrawScale);
    memset(s_bitmap, 0, sizeof s_bitmap);

    g_BrDrawWheelAlt      = 1;
    BrG_6C6624            = 0;
    g_BrDrawTrackFlags    = s_trackFlags;
    g_BrEnvFlagCount      = 0;
    g_BrEnvFlagIndices    = s_flagIndices;
    g_BrEnvSection        = 0;
    g_BrEnvCamPtr         = &g_BrDrawCombined;
    g_BrEnvOthermode      = 0x12345678u;
    g_BrEnvTexLookup      = s_texLookup;
    g_BrEnvTexDefault     = 0x00AABBCC;
    g_BrEnvSegCount       = 0;
    g_BrEnvSegBase        = NULL;
    g_BrEnvBitmap         = s_bitmap;
    g_BrEnvLightDirs      = NULL;
    g_brRaceBeginDifficulty = 0;
    BrG_6C1174            = 0;
    g_BrFpsScreenW        = 640;
    g_BrFpsScreenH        = 480;
    s_invertCalls         = 0;
    dl_reset();
}

/* ---- tests ---- */

static void test_env_guard_both_zero(void)
{
    env_reset();
    g_BrDrawWheelAlt = 0;
    BrG_6C6624       = 0;
    BrEnvEmit();
    CHECK(dl_count() == 0, "guard both zero: %d commands", dl_count());
}

static void test_env_guard_wheelalt_only(void)
{
    env_reset();
    g_BrDrawWheelAlt = 1;
    BrG_6C6624       = 0;
    BrEnvEmit();
    CHECK(dl_count() > 0, "wheelalt only: %d commands", dl_count());
}

static void test_env_guard_6624_only(void)
{
    env_reset();
    g_BrDrawWheelAlt = 0;
    BrG_6C6624       = 1;
    BrEnvEmit();
    CHECK(dl_count() > 0, "6624 only: %d commands", dl_count());
}

static void test_env_guard_trackflag(void)
{
    env_reset();
    g_BrEnvFlagCount = 1;
    s_flagIndices[0] = 0;
    s_trackFlags[0 * 84 + 0x4C] = 0x10;
    BrEnvEmit();
    CHECK(dl_count() == 0, "trackflag guard: %d commands", dl_count());
}

static void test_env_guard_trackflag_clear(void)
{
    env_reset();
    g_BrEnvFlagCount = 1;
    s_flagIndices[0] = 0;
    s_trackFlags[0 * 84 + 0x4C] = 0x00;
    BrEnvEmit();
    CHECK(dl_count() > 0, "trackflag clear: %d commands", dl_count());
}

static void test_env_produces_output(void)
{
    env_reset();
    BrEnvEmit();
    CHECK(dl_count() >= 10, "produces output: %d commands", dl_count());
    CHECK(s_invertCalls == 1, "one matrix invert, got %d", s_invertCalls);
}

static void test_env_tail_commands(void)
{
    int n;
    env_reset();
    BrEnvEmit();
    n = dl_count();
    CHECK(n >= 2, "need at least 2 commands for tail");
    if (n >= 2) {
        CHECK(s_dl[(n-2)*2] == 0xE7000000u, "tail sync: %08X", s_dl[(n-2)*2]);
        CHECK(s_dl[(n-1)*2] == 0xBA001301u, "tail othermode: %08X", s_dl[(n-1)*2]);
        CHECK(s_dl[(n-1)*2+1] == 0x00080000u, "tail value: %08X", s_dl[(n-1)*2+1]);
    }
}

int main(void)
{
    test_env_guard_both_zero();
    test_env_guard_wheelalt_only();
    test_env_guard_6624_only();
    test_env_guard_trackflag();
    test_env_guard_trackflag_clear();
    test_env_produces_output();
    test_env_tail_commands();
    printf("test_br_drawenv: %d failures\n", g_fails);
    return g_fails ? 1 : 0;
}
