/* test_br_hudscene.c -- behaviour tests for drawing/br_hudscene.c, which
 * was the slice2_15.c pass-15 packet until it moved whole.
 *
 * These assert properties the ORIGINAL has (round-trips, clamps, sentinels,
 * bit-splice identities, state-machine transitions), not the shape of this
 * particular translation.
 *
 * Everything below the "stand-ins" banner is a TEST-ONLY stand-in for a
 * cross-slice callee. None of it is decompiled code.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "slice2_15.h"

static int g_fail = 0;
static int g_ran  = 0;

#define CHECK(cond) do {                                                   \
    ++g_ran;                                                               \
    if (!(cond)) {                                                         \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        ++g_fail;                                                          \
    }                                                                      \
} while (0)

#define CHECK_NEAR(a, b, eps) do {                                         \
    ++g_ran;                                                               \
    if (!(fabs((double)(a) - (double)(b)) <= (eps))) {                     \
        printf("FAIL %s:%d: %s ~= %s  (%g vs %g)\n", __FILE__, __LINE__,   \
               #a, #b, (double)(a), (double)(b));                          \
        ++g_fail;                                                          \
    }                                                                      \
} while (0)

/* =====================================================================
 * stand-ins for cross-slice callees (TEST ONLY -- not decompiled)
 * ===================================================================== */

/* scripted rand; falls back to an LCG once the script runs out */
static int  g_script[64];
static int  g_scriptN, g_scriptI;
static unsigned g_lcg = 1u;

static void ScriptReset(void) { g_scriptN = 0; g_scriptI = 0; g_lcg = 1u; }
static void ScriptPush(int v) { g_script[g_scriptN++] = v; }

int BrRandom(void)
{
    if (g_scriptI < g_scriptN)
        return g_script[g_scriptI++];
    g_lcg = g_lcg * 1103515245u + 12345u;
    return (int)((g_lcg >> 8) & 0x7FFFFFFFu);
}

/* text sink */
static char g_lastText[256];
static int  g_lastX, g_lastY, g_cText;
void BrTextDraw(const char *psz, int x, int y)
{
    if (psz) {
        strncpy(g_lastText, psz, sizeof g_lastText - 1);
        g_lastText[sizeof g_lastText - 1] = '\0';
    } else {
        g_lastText[0] = '\0';
    }
    g_lastX = x; g_lastY = y; ++g_cText;
}

static int g_suppress;
int  BrSub_1002B2A0(void)                 { return g_suppress; }
void BrSub_1003407D(float a, float b)     { (void)a; (void)b; }
void BrSub_100020D0(char *pszOut, float v){ sprintf(pszOut, "%.2f", (double)v); }
void BrSub_1003289F(int a, int b, int c, int d)
                                          { (void)a;(void)b;(void)c;(void)d; }
void BrSub_10017290(BrHudView *a)         { (void)a; }
void BrSub_100173F0(BrHudView *a, int b)  { (void)a; (void)b; }
void BrSub_10019260(void)                 { }
void BrSub_10019270(void)                 { }
void BrSub_10019280(void)                 { }
void BrSub_10019290(void)                 { }
static int g_lastSize;
void BrSub_100192F0(int size)             { g_lastSize = size; }

static int g_c2F900;
void BrSub_1002F900(BrGfxCmd *pCmd,
                    int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                    int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                    int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                    int32_t a13, int32_t a14, int32_t a15, int32_t a16)
{
    (void)a01;(void)a02;(void)a03;(void)a05;(void)a06;(void)a07;
    (void)a09;(void)a10;(void)a11;(void)a13;(void)a14;(void)a15;
    /* record the four tags so the caller's grouping can be checked */
    pCmd->w0 = (uint32_t)a04;
    pCmd->w1 = (uint32_t)(a08 + a12 + a16);
    ++g_c2F900;
}

void BrSub_10031140(BrMat4 *pM, int32_t a, int32_t b, float c)
{ (void)pM; (void)a; (void)b; (void)c; }

static int g_c31688;
static int32_t g_a31688[7];
void BrSub_10031688(int32_t x, int32_t y, int32_t w, int32_t h,
                    int32_t c0, int32_t c1, int32_t c2)
{
    g_a31688[0]=x; g_a31688[1]=y; g_a31688[2]=w; g_a31688[3]=h;
    g_a31688[4]=c0; g_a31688[5]=c1; g_a31688[6]=c2;
    ++g_c31688;
}

static BrMat4 g_dstMtx;
BrMat4 *BrSub_10069490(void) { return &g_dstMtx; }

static int32_t g_rect[4];
static int     g_cRect;
void BrSub_1001BE90(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{ g_rect[0]=x1; g_rect[1]=y1; g_rect[2]=x2; g_rect[3]=y2; ++g_cRect; }

static uint32_t g_lastC820[2];
void BrSub_1001C820(uint32_t w0, uint32_t w1)
{ g_lastC820[0]=w0; g_lastC820[1]=w1; }

static void *g_a290A0[3];
void BrSub_100290A0(void *p1, void *p2, void *p3)
{ g_a290A0[0]=p1; g_a290A0[1]=p2; g_a290A0[2]=p3; }

static uint32_t g_lastIndirect[2];
static void Indirect(uint32_t a, uint32_t b)
{ g_lastIndirect[0]=a; g_lastIndirect[1]=b; }

/* br_bits.h / br_vec.h / br_mat.h live in other translation units */
void *BrHandleLookup(void *const *apTable, uint32_t handle)
{
    if (handle < BR_HANDLE_MIN || handle > BR_HANDLE_MAX) return 0;
    return apTable ? apTable[handle] : 0;
}
void BrVec3MulAdd(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float s)
{ o->x = a->x + b->x*s; o->y = a->y + b->y*s; o->z = a->z + b->z*s; }
void BrVec3ScaleBy(BrVec3 *v, float s) { v->x*=s; v->y*=s; v->z*=s; }
void BrVec3AddTo(BrVec3 *a, const BrVec3 *b) { a->x+=b->x; a->y+=b->y; a->z+=b->z; }
void BrMat4Copy(const BrMat4 *pSrc, BrMat4 *pDst) { *pDst = *pSrc; }

/* =====================================================================
 * fixtures
 * ===================================================================== */

static BrGfxCmd g_dl[256];

static void DlReset(void)
{
    memset(g_dl, 0, sizeof g_dl);
    BrGfxGetOut()->pCur = g_dl;
}
static int DlCount(void)
{
    return (int)(BrGfxGetOut()->pCur - g_dl);
}

/* =====================================================================
 * 0x10016A60 -- rect emission, and the 0x1001C7A0 round trip
 * ===================================================================== */
static void TestDrawTexRect(void)
{
    BrScreenInfo *pScr = BrScreenGet();
    uint32_t lrs, lrt;

    pScr->cy = 480;

    DlReset();
    BrGfxDrawTexRect(0xAABBCCDDu, 40, 60, 32, 16);

    /* exactly three commands, and the opcodes the original picks */
    CHECK(DlCount() == 3);
    CHECK(g_dl[0].w0 == (0xDC000000u | 0x00BBCCDDu));
    CHECK(g_dl[0].w1 == 1u);
    CHECK(g_dl[1].w0 == 0xF2002002u);
    CHECK((g_dl[2].w0 >> 24) == 0xE3u);

    /* tile size carries (extent*4 - 2), i.e. extent - 0.5 in 10.2 */
    lrs = (g_dl[1].w1 >> 12) & 0xFFFu;
    lrt =  g_dl[1].w1        & 0xFFFu;
    CHECK(lrs == (uint32_t)(32 * 4 - 2));
    CHECK(lrt == (uint32_t)(16 * 4 - 2));

    /* the rect must decode through the INTEGER handler (0x1001C7A0) back to
     * the box that was asked for, with y flipped and the far edges adjusted */
    g_cRect = 0;
    BrCmdRectInt(&g_dl[2]);
    CHECK(g_cRect == 1);
    CHECK(g_rect[0] == 40);                    /* x1        */
    CHECK(g_rect[1] == 480 - (60 + 16) - 1);   /* cy-y2-1   */
    CHECK(g_rect[2] == 40 + 32 + 1);           /* x2+1      */
    CHECK(g_rect[3] == 480 - 60);              /* cy-y1     */

    /* zero-sized rect: x2 == x1, so the handler still yields a 1-wide box --
     * the +1 / -1 are unconditional in the original */
    DlReset();
    BrGfxDrawTexRect(0, 10, 10, 0, 0);
    g_cRect = 0;
    BrCmdRectInt(&g_dl[2]);
    CHECK(g_rect[2] - g_rect[0] == 1);
    CHECK(g_rect[3] - g_rect[1] == 1);
}

/* =====================================================================
 * 0x1001BE30 vs 0x1001C7A0 -- the two coordinate encodings
 * ===================================================================== */
static void TestRectEncodings(void)
{
    BrGfxCmd c;
    int32_t fixed[4], plain[4];

    BrScreenGet()->cy = 240;

    /* the same field values read two ways: fixed divides by four */
    c.w0 = (100u << 12) | 80u;
    c.w1 = ( 20u << 12) | 12u;

    BrCmdRectInt(&c);
    memcpy(plain, g_rect, sizeof plain);
    BrCmdRectFixed(&c);
    memcpy(fixed, g_rect, sizeof fixed);

    CHECK(plain[0] == 20);
    CHECK(fixed[0] == 20 / 4);
    CHECK(240 - plain[1] - 1 == 80);
    CHECK(240 - fixed[1] - 1 == 80 / 4);

    /* negative coordinates: both sign-extend the 12-bit field, so 0xFFF is -1 */
    c.w0 = (0xFFFu << 12) | 0xFFFu;
    c.w1 = 0u;
    BrCmdRectInt(&c);
    CHECK(g_rect[2] == -1 + 1);            /* x2 == -1 */
    CHECK(g_rect[1] == 240 - (-1) - 1);

    /* the fixed decoder masks to ten bits AFTER the arithmetic shift, so -1
     * becomes -1>>2 == -1, masked to 0x3FF -- a large positive number.
     * That wrap is in the original. */
    BrCmdRectFixed(&c);
    CHECK(g_rect[2] == 0x3FF + 1);

    /* both handlers advance by exactly one command */
    CHECK(BrCmdRectInt(&c)   == &c + 1);
    CHECK(BrCmdRectFixed(&c) == &c + 1);
}

/* =====================================================================
 * colour + mode-bit handlers
 * ===================================================================== */
static void TestColorHandlers(void)
{
    BrRdpRegs *pR = BrRdpGetRegs();
    BrGfxCmd c;

    c.w0 = 0;
    c.w1 = 0xFF800000u;      /* a=255 b=128 c=0 d=0 */
    CHECK(BrCmdSetColorA(&c) == &c + 1);
    CHECK_NEAR(pR->f4BBF04, 1.0, 1e-6);          /* high byte lands FIRST */
    CHECK_NEAR(pR->f4C0BAC, 128.0 / 255.0, 1e-6);
    CHECK_NEAR(pR->f4BBEB8, 0.0, 1e-9);
    CHECK_NEAR(pR->f4BBE2C, 0.0, 1e-9);

    c.w1 = 0x000000FFu;
    BrCmdSetColorB(&c);
    CHECK_NEAR(pR->f4C5154, 0.0, 1e-9);
    CHECK_NEAR(pR->f4C0BA8, 1.0, 1e-6);

    /* the two handlers write disjoint state */
    c.w1 = 0xFFFFFFFFu;
    BrCmdSetColorA(&c);
    CHECK_NEAR(pR->f4BBF04, 1.0, 1e-6);
    CHECK_NEAR(pR->f4C5154, 0.0, 1e-9);
}

static void TestModeBits(void)
{
    BrRdpRegs *pR = BrRdpGetRegs();
    BrGfxCmd c;
    uint32_t w;

    for (w = 0; w < 0x400u; ++w) {
        uint32_t v = (w * 2654435761u);
        c.w0 = 0;
        c.w1 = v;
        CHECK(BrCmdUnpackModeBits(&c) == &c + 1);

        /* splice identity: low three bits from one shift, the rest from another */
        if ((pR->c4BBF00 & 7u) != ((v >> 13) & 7u))     { CHECK(0); break; }
        if ((pR->c4BBF00 & 0xF8u) != ((v >> 8) & 0xF8u)){ CHECK(0); break; }
        if ((pR->c4BC194 & 7u) != ((v >> 8) & 7u))      { CHECK(0); break; }
        if ((pR->c4BC194 & 0xF8u) != ((v >> 3) & 0xF8u)){ CHECK(0); break; }
        /* c4C5150's high half is an 8-bit shift, so bits 6/7 of w1 are lost */
        if (pR->c4C5150 != (uint8_t)((uint8_t)((uint8_t)(v & 0xFEu) << 2)
                                     | (uint8_t)((v >> 3) & 7u)))
                                                        { CHECK(0); break; }
        /* the last one is a boolean fanned out to a whole byte */
        if (pR->c4C15CC != ((v & 1u) ? 0xFFu : 0x00u))  { CHECK(0); break; }
    }
    CHECK(w == 0x400u);
}

static void TestIndirectAndLatch(void)
{
    BrRdpRegs *pR = BrRdpGetRegs();
    BrGfxCmd c;

    pR->pfn18AA0B8 = Indirect;
    c.w0 = 0xAB123456u;
    c.w1 = 0x0000BEEFu;
    CHECK(BrCmdDispatchIndirect(&c) == &c + 1);
    CHECK(g_lastIndirect[0] == 0x00123456u);   /* opcode byte stripped */
    CHECK(g_lastIndirect[1] == 0x0000BEEFu);

    CHECK(BrCmdLatchPair(&c) == &c + 1);
    CHECK(pR->f4C5158 == c.w0);                /* latched UNMASKED */
    CHECK(pR->f4C515C == c.w1);
    CHECK(g_lastC820[0] == c.w0);
    CHECK(g_lastC820[1] == c.w1);
}

/* =====================================================================
 * the five recache thunks
 * ===================================================================== */
static void TestRecacheThunks(void)
{
    BrScreenInfo *pScr = BrScreenGet();
    BrRdpRegs *pR = BrRdpGetRegs();

    pScr->cx = 641;
    pScr->cy = 481;
    BrRdpCacheScreenWidth();
    BrRdpCacheScreenHeight();
    BrRdpCacheHalfWidthA();
    BrRdpCacheHalfWidthB();
    BrRdpCacheHalfHeight();

    CHECK(pR->f4C5164 == 641);
    CHECK(pR->f4C01A0 == 481);
    CHECK_NEAR(pR->f4BBF08, 320.0, 1e-9);
    CHECK_NEAR(pR->f4C0BB0, 320.0, 1e-9);
    CHECK_NEAR(pR->f4C0BB8, 240.0, 1e-9);
    /* the two half-width slots must always agree */
    CHECK(pR->f4BBF08 == pR->f4C0BB0);

    /* the original halves with cdq/sub/sar, i.e. truncation toward zero */
    pScr->cx = -7;
    BrRdpCacheHalfWidthA();
    CHECK_NEAR(pR->f4BBF08, -3.0, 1e-9);

    pScr->cx = 640; pScr->cy = 480;
}

/* =====================================================================
 * 0x10017CD0 / 0x10017C80 -- the gap computation
 * ===================================================================== */
static BrCar g_cars[4];

static void TestGap(void)
{
    BrHudEnv *pEnv = BrHudGetEnv();

    memset(g_cars, 0, sizeof g_cars);
    pEnv->cCars = 3;

    /* no car beats the 0xFF seed -> the running value stays 0 */
    g_cars[0].f0FF8 = 0xFF; g_cars[1].f0FF8 = 0x100; g_cars[2].f0FF8 = 0x200;
    CHECK_NEAR(BrHudGapSeconds(g_cars, 0), 0.0, 1e-9);

    /* the leader supplies the numerator; the denominator floors at 25 */
    g_cars[0].f0FF8 = 5;   g_cars[0].f0FF4 = 500.0f;
    g_cars[1].f0FF8 = 1;   g_cars[1].f0FF4 = 600.0f;
    g_cars[2].f0FF8 = 9;   g_cars[2].f0FF4 = 700.0f;
    g_cars[0].f1030 = 1.0f;             /* 1.0/2.24 == 0.446, well under 25 */
    CHECK_NEAR(BrHudGapSeconds(g_cars, 0), (600.0 - 500.0) / 25.0, 1e-4);

    /* above the floor the denominator is used as-is, never clamped down */
    g_cars[0].f1030 = 224.0f;           /* 224/2.24 == 100 */
    CHECK_NEAR(BrHudGapSeconds(g_cars, 0), 100.0 / 100.0, 1e-4);

    /* zero speed is the "no time" sentinel, not a division by zero */
    g_cars[0].f1030 = 0.0f;
    CHECK_NEAR(BrHudGapSeconds(g_cars, 0), 1000.0, 1e-6);

    /* a gap of exactly zero short-circuits before the speed is even read */
    g_cars[1].f0FF4 = g_cars[0].f0FF4;
    g_cars[0].f1030 = 0.0f;
    CHECK_NEAR(BrHudGapSeconds(g_cars, 0), 0.0, 1e-9);

    /* the string form: '+' only when strictly positive */
    g_cars[1].f0FF4 = 600.0f;
    g_cars[0].f1030 = 224.0f;
    CHECK(BrHudFormatGapString(g_cars, 0)[0] == '+');

    g_cars[1].f0FF4 = 400.0f;           /* leader "behind" -> negative gap */
    CHECK(BrHudFormatGapString(g_cars, 0)[0] == '\0');

    /* and it always hands back the same static buffer */
    CHECK(BrHudFormatGapString(g_cars, 0)
          == BrHudFormatGapString(g_cars, 0));
}

/* =====================================================================
 * 0x10017FE0 -- split-line formatting
 * ===================================================================== */
static void TestSplitLine(void)
{
    BrHudDrawSplitLine("", 1, 83.5f, 7, 9);
    CHECK(strcmp(g_lastText, "1. 1:23.50") == 0);
    CHECK(g_lastX == 7 && g_lastY == 9);

    /* the *100 is followed by a TRUNCATION, not a round: 83.45f is really
     * 83.44999... so the hundredths come out 44, not 45 */
    BrHudDrawSplitLine("", 1, 83.45f, 0, 0);
    CHECK(strcmp(g_lastText, "1. 1:23.44") == 0);

    BrHudDrawSplitLine(">", 12, 0.0f, 0, 0);
    CHECK(strcmp(g_lastText, ">12. 0:00.00") == 0);

    /* truncation toward zero, both ends: 59.999 s is 59.99, not 1:00 */
    BrHudDrawSplitLine("", 1, 59.999f, 0, 0);
    CHECK(strcmp(g_lastText, "1. 0:59.99") == 0);

    /* every division is signed and truncating, so negatives come out negative
     * in every field -- this is the original's behaviour, bug or not */
    BrHudDrawSplitLine("", 1, -1.5f, 0, 0);
    CHECK(strchr(g_lastText, '-') != NULL);
}

/* =====================================================================
 * 0x10018070 -- plain-clear predicate truth table
 * ===================================================================== */
static void TestPlainClear(void)
{
    BrSceneEnv *pE = BrSceneGetEnv();
    int i;

    /* the ONLY zero case: three flags clear, f6C7C98 set, f0B4050 != 2 */
    memset(pE, 0, sizeof *pE);
    pE->f6C7C98 = 1;
    pE->f0B4050 = 0;
    CHECK(BrSceneUsePlainClear() == 0);

    for (i = 0; i < 3; ++i) {
        memset(pE, 0, sizeof *pE);
        pE->f6C7C98 = 1;
        if (i == 0) pE->f6C661C = 1;
        if (i == 1) pE->f6C6620 = 1;
        if (i == 2) pE->f6C6624 = 1;
        CHECK(BrSceneUsePlainClear() == 1);
    }

    /* inverted sense: f6C7C98 == 0 forces the plain path */
    memset(pE, 0, sizeof *pE);
    CHECK(BrSceneUsePlainClear() == 1);

    memset(pE, 0, sizeof *pE);
    pE->f6C7C98 = 1;
    pE->f0B4050 = 2;
    CHECK(BrSceneUsePlainClear() == 1);

    /* but 1 and 3 are not special */
    pE->f0B4050 = 3;
    CHECK(BrSceneUsePlainClear() == 0);
}

/* =====================================================================
 * 0x100180B0 -- frame setup
 * ===================================================================== */
static BrHudView g_views[4];

static void TestSceneAccumReset(void)
{
    /* The reset zeroes both accumulators no matter what they held. */
    g_4B16A0 = 123.5f;
    g_4B16AC = -7.0f;
    BrSceneAccumReset();
    CHECK(g_4B16A0 == 0.0f);
    CHECK(g_4B16AC == 0.0f);

    /* Idempotent -- zeroing an already-zero pair leaves it zero. */
    BrSceneAccumReset();
    CHECK(g_4B16A0 == 0.0f);
    CHECK(g_4B16AC == 0.0f);
}

static void TestSceneSetup(void)
{
    BrSceneEnv *pE = BrSceneGetEnv();
    BrScreenInfo *pScr = BrScreenGet();
    int nAll, i;

    memset(g_views, 0, sizeof g_views);
    g_views[0].x = 4; g_views[0].y = 8; g_views[0].w = 100; g_views[0].h = 50;
    pScr->iView = 0;
    pScr->cViews = 1;

    /* the four opening 0xBC commands are unconditional; f6C6608 stops the rest */
    memset(pE, 0, sizeof *pE);
    pE->f6C6608 = 1;
    DlReset();
    BrSceneSetupFrame(g_views);
    CHECK(DlCount() == 4);
    CHECK((g_dl[0].w0 >> 24) == 0xBCu);
    CHECK((g_dl[3].w0 >> 24) == 0xBCu);

    /* plain-clear path: no further commands, one call to the filler, and the
     * colour components arrive in the order (c6C0260, c6C1614, c6C0200) */
    memset(pE, 0, sizeof *pE);
    pE->c6C0200 = 10; pE->c6C1614 = 20; pE->c6C0260 = 30;
    DlReset();
    g_c31688 = 0;
    BrSceneSetupFrame(g_views);
    CHECK(DlCount() == 4);
    CHECK(g_c31688 == 1);
    CHECK(g_a31688[0] == 4 && g_a31688[1] == 8);
    CHECK(g_a31688[2] == 100 && g_a31688[3] == 50);
    CHECK(g_a31688[4] == 30 && g_a31688[5] == 20 && g_a31688[6] == 10);

    /* an ODD, POSITIVE lightning counter brightens the fill; an even one and a
     * negative one both leave it alone */
    pE->f0A79CC = 3;
    BrSceneSetupFrame(g_views);
    CHECK(g_a31688[6] == ((10 + 0x55) * 3) / 4);
    pE->f0A79CC = 2;
    BrSceneSetupFrame(g_views);
    CHECK(g_a31688[6] == 10);
    pE->f0A79CC = -1;
    BrSceneSetupFrame(g_views);
    CHECK(g_a31688[6] == 10);

    /* full path */
    memset(pE, 0, sizeof *pE);
    pE->f6C7C98 = 1;
    pE->pCam = (BrCamObj *)calloc(1, sizeof(BrCamObj));
    DlReset();
    g_c31688 = 0;
    g_c2F900 = 0;
    BrSceneSetupFrame(g_views);
    CHECK(g_c31688 == 0);
    CHECK(g_c2F900 == 1);
    nAll = DlCount();
    CHECK(nAll > 20);

    /* the 0xFB env-colour command appears only with f6C6618 set, and it packs
     * the four byte globals high-to-low */
    for (i = 0; i < nAll; ++i)
        CHECK((g_dl[i].w0 >> 24) != 0xFBu);

    pE->f6C6618 = 1;
    pE->c6C0260 = 0x11; pE->c6C1614 = 0x22;
    pE->c6C0200 = 0x33; pE->c690BE8 = 0x44;
    DlReset();
    BrSceneSetupFrame(g_views);
    {
        int found = -1;
        for (i = 0; i < DlCount(); ++i)
            if ((g_dl[i].w0 >> 24) == 0xFBu) found = i;
        CHECK(found >= 0);
        if (found >= 0)
            CHECK(g_dl[found].w1 == 0x11223344u);
    }
    /* f6C6618 also adds one 0xB7 command, so the list is two longer */
    CHECK(DlCount() == nAll + 2);

    /* f6C3364 == f6C1174 swaps the two mask constants, and they always differ */
    {
        uint32_t b7 = 0, b6 = 0;
        int seenB7 = 0;
        pE->f6C3364 = pE->f6C1174 = 7;
        DlReset();
        BrSceneSetupFrame(g_views);
        for (i = 0; i < DlCount(); ++i) {
            if ((g_dl[i].w0 >> 24) == 0xB7u && g_dl[i].w1 >= 0x1000u
                && g_dl[i].w1 <= 0x2000u && !seenB7) { b7 = g_dl[i].w1; seenB7 = 1; }
            if ((g_dl[i].w0 >> 24) == 0xB6u && (g_dl[i].w1 == 0x1000u
                || g_dl[i].w1 == 0x2000u)) b6 = g_dl[i].w1;
        }
        CHECK(b7 == 0x2000u);
        CHECK(b6 == 0x1000u);
        pE->f6C1174 = 8;
        DlReset();
        BrSceneSetupFrame(g_views);
        seenB7 = 0;
        for (i = 0; i < DlCount(); ++i) {
            if ((g_dl[i].w0 >> 24) == 0xB7u && g_dl[i].w1 >= 0x1000u
                && g_dl[i].w1 <= 0x2000u && !seenB7) { b7 = g_dl[i].w1; seenB7 = 1; }
            if ((g_dl[i].w0 >> 24) == 0xB6u && (g_dl[i].w1 == 0x1000u
                || g_dl[i].w1 == 0x2000u)) b6 = g_dl[i].w1;
        }
        CHECK(b7 == 0x1000u);
        CHECK(b6 == 0x2000u);
    }

    free(pE->pCam);
    pE->pCam = NULL;
}

/* =====================================================================
 * weather
 * ===================================================================== */
static void TestWind(void)
{
    BrWeather *pW = BrWeatherGet();
    double g, dt;

    ScriptReset();
    memset(pW, 0, sizeof *pW);
    pW->dt = 1.0f;
    pW->windAngle = 6.0f;
    pW->windGain  = 0.9f;
    ScriptPush(0xFFFF);   /* +0.99997 */
    ScriptPush(0x8000);   /* ~0        */
    BrWeatherStepWind();
    /* 6.0 + ~1.0 exceeds 2*pi, so it wraps down */
    CHECK(pW->windAngle >= 0.0f && pW->windAngle < 6.2831855f);

    ScriptReset();
    pW->windAngle = 0.5f;
    pW->windGain  = 0.9f;
    ScriptPush(0);        /* -1.0 */
    ScriptPush(0x8000);
    BrWeatherStepWind();
    /* went negative, so it wrapped up by subtracting -2*pi */
    CHECK(pW->windAngle >= 0.0f && pW->windAngle < 6.2831855f);

    /* the gain clamp is a closed [0.5, 1.0] */
    ScriptReset();
    pW->windGain = 1.0f;
    ScriptPush(0x8000); ScriptPush(0xFFFF);
    BrWeatherStepWind();
    CHECK_NEAR(pW->windGain, 1.0, 1e-6);

    ScriptReset();
    pW->windGain = 0.5f;
    ScriptPush(0x8000); ScriptPush(0);
    BrWeatherStepWind();
    CHECK_NEAR(pW->windGain, 0.5, 1e-6);

    /* the wind vector is a polar decomposition of gain*dt, and z is always 0 */
    g  = pW->windGain;
    dt = pW->dt;
    CHECK_NEAR(sqrt((double)pW->windX * pW->windX
                    + (double)pW->windY * pW->windY), g * dt, 1e-5);
    CHECK(pW->windZ == 0.0f);
}

static void TestLightning(void)
{
    BrWeather *pW = BrWeatherGet();
    int i;

    ScriptReset();
    memset(pW, 0, sizeof *pW);
    pW->dt = 1.0f;
    pW->lightning = -1;
    pW->f6C7C80 = 77;

    /* idle: a roll of 0x80 or more does nothing at all */
    ScriptPush(0x80);
    BrWeatherStepLightning();
    CHECK(pW->lightning == -1);
    CHECK(pW->thunderDist == 0.0f);

    /* under 0x80 it fires: three frames queued and a flash position picked */
    ScriptReset();
    ScriptPush(0x7F); ScriptPush(0x123); ScriptPush(0x456);
    BrWeatherStepLightning();
    CHECK(pW->lightning == 3);
    CHECK(pW->thunderDist == 0.0f);
    CHECK_NEAR(pW->flashX, (double)(0x123 & 0x7FF), 1e-9);
    CHECK_NEAR(pW->flashY, (double)(0x456 & 0x7FF), 1e-9);
    CHECK(pW->flashZ == 77);

    /* the counter runs 3 -> 0 while the sound front travels 343 per second */
    for (i = 3; i > 0; --i) {
        BrWeatherStepLightning();
        CHECK(pW->lightning == i - 1);
    }
    CHECK_NEAR(pW->thunderDist, 3.0 * 343.0, 1e-3);

    /* at zero it keeps integrating until the front passes 2048 */
    while (pW->lightning == 0 && pW->thunderDist < 4096.0f)
        BrWeatherStepLightning();
    CHECK(pW->lightning == -1);
    CHECK(pW->thunderDist > 2048.0f);
}

static void TestParticleSeed(void)
{
    BrWeather *pW = BrWeatherGet();
    int layer, i;

    ScriptReset();
    memset(pW, 0, sizeof *pW);

    /* mark the spare tail so it can be shown untouched */
    for (layer = 0; layer < BR_PARTICLE_LAYERS; ++layer)
        for (i = BR_PARTICLES_PER_LAYER; i < BR_PARTICLE_STRIDE; ++i)
            pW->aParticles[layer][i][0] = 0x5A5A;

    BrWeatherRandomiseParticles();

    CHECK(pW->cParticles == 0x200);

    /* the ten spare records per layer are NOT filled: the original's outer
     * step (0xC3C) is wider than the 512 records it writes */
    for (layer = 0; layer < BR_PARTICLE_LAYERS; ++layer)
        for (i = BR_PARTICLES_PER_LAYER; i < BR_PARTICLE_STRIDE; ++i)
            CHECK(pW->aParticles[layer][i][0] == 0x5A5A);

    /* and the filled part really did change */
    {
        int nonzero = 0;
        for (i = 0; i < BR_PARTICLES_PER_LAYER; ++i)
            if (pW->aParticles[0][i][0] != 0 || pW->aParticles[1][i][0] != 0)
                ++nonzero;
        CHECK(nonzero > BR_PARTICLES_PER_LAYER / 2);
    }
}

static BrCamBlock g_blk;
static const BrCamBlock *GetBlock(int iView) { (void)iView; return &g_blk; }

static void TestParticleStep(void)
{
    BrWeather *pW = BrWeatherGet();
    BrScreenInfo *pScr = BrScreenGet();
    int16_t before;

    ScriptReset();
    memset(pW, 0, sizeof *pW);
    memset(&g_blk, 0, sizeof g_blk);
    pW->dt = 1.0f / 30.0f;
    pW->pfnGetBlock = GetBlock;
    pW->rain = 1;
    pW->windGain = 0.75f;
    pScr->cViews = 2;

    /* first call seeds and latches the camera position.
     * GOTCHA: the seeding call resets cParticles to the FULL 0x200, because
     * 0x10019490 overwrites the 0x200/cViews the caller just stored. Only
     * from the second frame on is the count actually split between views. */
    BrWeatherStepParticles();
    CHECK(pW->fInit == 1);
    CHECK(pW->cParticles == 0x200);
    CHECK_NEAR(pW->aPrev[0].x, 0.0, 1e-9);

    /* a still camera: the speed is zero, so k is pinned at 1 */
    CHECK_NEAR(pW->speed, 0.0, 1e-9);
    CHECK_NEAR(pW->k, 1.0, 1e-9);

    /* move it; the particle field must respond */
    before = pW->aParticles[0][0][2];
    g_blk.v30.x = 30.0f;
    BrWeatherStepParticles();
    CHECK(pW->cParticles == 0x200 / 2);
    CHECK_NEAR(pW->aPrev[0].x, 30.0, 1e-6);
    CHECK(pW->aParticles[0][0][2] != before);

    /* both weather flags clear -> the whole pass is abandoned */
    pW->rain = 0;
    pW->storm = 0;
    pW->aPrev[0].x = -1.0f;
    g_blk.v30.x = 99.0f;
    BrWeatherStepParticles();
    CHECK_NEAR(pW->aPrev[0].x, -1.0, 1e-9);

    /* cViews == 0 must not divide by zero */
    pScr->cViews = 0;
    BrWeatherStepParticles();
    CHECK(pW->cParticles == 0);

    pScr->cViews = 1;
}

/* =====================================================================
 * HUD text placement
 * ===================================================================== */
static void TestHudText(void)
{
    BrHudEnv *pEnv = BrHudGetEnv();
    BrScreenInfo *pScr = BrScreenGet();
    static BrRace race;

    memset(&race, 0, sizeof race);
    pEnv->pRace = &race;
    pEnv->f6909B4 = 0;
    pEnv->pszCentre = "hello";
    pScr->iView = 0;
    pScr->cViews = 1;
    g_suppress = 0;

    memset(g_views, 0, sizeof g_views);
    g_views[0].x = 10; g_views[0].y = 20;
    g_views[0].w = 101; g_views[0].h = 100;

    g_cText = 0;
    BrHudDrawViewCentreText(g_views);
    CHECK(g_cText == 1);
    CHECK(strcmp(g_lastText, "hello") == 0);
    CHECK(g_lastX == 10 + 101 / 2);                /* centred, w/2 truncates */
    CHECK(g_lastY == 20 + 100 / 3 + 0x18 + (0x1E * 3) / 16);

    /* the suppression gate short-circuits before anything is drawn */
    g_suppress = 1;
    g_cText = 0;
    BrHudDrawViewCentreText(g_views);
    CHECK(g_cText == 0);
    g_suppress = 0;

    /* a NULL string draws nothing */
    pEnv->pszCentre = NULL;
    g_cText = 0;
    BrHudDrawViewCentreText(g_views);
    CHECK(g_cText == 0);
    pEnv->pszCentre = "hello";

    /* the message variant: psz0FFC wins, and it uses the LARGER text size */
    race.psz0FFC = "first";
    race.psz1004 = "second";
    g_cText = 0; g_lastSize = -1;
    BrHudDrawViewMessage(g_views);
    CHECK(g_cText == 1);
    CHECK(strcmp(g_lastText, "first") == 0);
    CHECK(g_lastSize == 0x1E);
    CHECK(g_lastY == 20 + 100 / 3 + 0x1E / 4);

    race.psz0FFC = NULL;
    g_cText = 0; g_lastSize = -1;
    BrHudDrawViewMessage(g_views);
    CHECK(strcmp(g_lastText, "second") == 0);
    CHECK(g_lastSize == 0x14);                     /* the SMALLER size */
    CHECK(g_lastY == 20 + 100 / 3 + (0x1E * 3) / 16);

    /* f6909B4 gates the whole message function */
    pEnv->f6909B4 = 1;
    g_cText = 0;
    BrHudDrawViewMessage(g_views);
    CHECK(g_cText == 0);
    pEnv->f6909B4 = 0;

    /* split list: one line per split, 15 pixels apart, right-aligned */
    {
        static const float splits[3] = { 1.0f, 2.0f, 3.0f };
        int firstY;
        race.cSplits = 3;
        race.aSplits = splits;
        pEnv->f0BD3F0 = 1;
        pEnv->pszSplitPrefix = "";
        pScr->cx = 640;
        g_cText = 0;
        BrHudDrawSplitList(g_views);
        CHECK(g_cText == 3);
        CHECK(g_lastX == 640 - 0x10);
        firstY = 0x1E + (20 + 0x14) + 0x25;
        CHECK(g_lastY == firstY + 2 * 0x0F);

        pEnv->f0BD3F0 = 0;
        g_cText = 0;
        BrHudDrawSplitList(g_views);
        CHECK(g_cText == 0);
        pEnv->f0BD3F0 = 1;
    }
}

/* =====================================================================
 * 0x10016B40 gating
 * ===================================================================== */
static uint8_t g_sprites[BR_HUDSPRITE_STRIDE * 2];

static void TestDialGates(void)
{
    BrHudEnv *pEnv = BrHudGetEnv();
    BrScreenInfo *pScr = BrScreenGet();
    static BrRace race;
    BrHudSprite *pSpr;
    int n, n1;

    memset(&race, 0, sizeof race);
    memset(g_sprites, 0, sizeof g_sprites);
    pSpr = (BrHudSprite *)g_sprites;
    pSpr->e4 = 32; pSpr->e5 = 16;
    pSpr->fEC = 0.0f; pSpr->fF0 = 1.0f;

    pEnv->pRace = &race;
    pEnv->pSprites = g_sprites;
    pEnv->f6909B4 = 1;                 /* pin the jitter so this is repeatable */
    pEnv->f6C65EC = 0;
    pScr->iView = 0;
    pScr->cViews = 1;
    pScr->cx = 640; pScr->cy = 480;

    memset(g_views, 0, sizeof g_views);
    g_views[0].w = 200; g_views[0].h = 100;

    /* both gates must be satisfied or nothing is emitted at all */
    pEnv->f0BD3F4 = 0; pEnv->f22AF1C = 0;
    DlReset();
    BrHudDrawDial(g_views);
    CHECK(DlCount() == 0);

    pEnv->f0BD3F4 = 1; pEnv->f22AF1C = 1;
    DlReset();
    BrHudDrawDial(g_views);
    CHECK(DlCount() == 0);

    /* mode 0 runs the whole function: dial block AND needle */
    pEnv->f0BD3F4 = 1; pEnv->f22AF1C = 0;
    pSpr->mode = 0;
    pEnv->aLastSeq[0] = -1;
    DlReset();
    BrHudDrawDial(g_views);
    n = DlCount();
    CHECK(n > 0);

    /* the needle is a quad: the two tip vertices sit further from the centre
     * than the two base vertices, and all four are coplanar at z = 0 */
    {
        BrHudQuad *pQ = &pEnv->aQuads[0];
        double cx = 640 - 32 - 0x10;
        double cy = 480 - (0 + 100 - 16 - 4);
        double rTip  = hypot(pQ->v[0].x - cx, pQ->v[0].y - cy);
        double rBase = hypot(pQ->v[2].x - cx, pQ->v[2].y - cy);
        CHECK(rTip > rBase);
        CHECK_NEAR(rTip,  20.0, 1.5);   /* cViews != 2 -> 20 / 7 */
        CHECK_NEAR(rBase,  7.0, 1.5);
        CHECK(pQ->v[0].z == 0.0f && pQ->v[3].z == 0.0f);
        CHECK(pQ->v[0].f18 == 255.0f && pQ->v[3].f18 == 255.0f);
        CHECK(pQ->v[0].f14 == 0.0f && pQ->v[0].f1C == 0.0f);
    }

    /* mode 1 stops right after the dial block -- no needle -- so it emits
     * strictly fewer commands than mode 0 */
    pSpr->mode = 1;
    pEnv->aLastSeq[0] = -1;
    DlReset();
    BrHudDrawDial(g_views);
    n1 = DlCount();
    CHECK(n1 > 0);
    CHECK(n1 < n);

    /* mode 2 takes exactly the same branch as mode 1 */
    pSpr->mode = 2;
    pEnv->aLastSeq[0] = -1;
    DlReset();
    BrHudDrawDial(g_views);
    CHECK(DlCount() == n1);

    /* the sequence cache: a repeat call with the same sequence id emits one
     * command fewer, and the cache is per-view */
    {
        race.f0E68 = 1.0f;
        race.f0E70 = 5;
        pEnv->aLastSeq[0] = -1;
        DlReset();
        BrHudDrawDial(g_views);
        CHECK(DlCount() == n1);
        CHECK(pEnv->aLastSeq[0] == 6);
        DlReset();
        BrHudDrawDial(g_views);
        CHECK(DlCount() == n1 - 1);
    }

    /* a NEGATIVE f0E68 forces the sequence id to zero regardless of f0E70 */
    race.f0E68 = -1.0f;
    DlReset();
    BrHudDrawDial(g_views);
    CHECK(pEnv->aLastSeq[0] == 0);

    /* a non-zero mode stops before the needle, so the quad is left alone */
    {
        BrHudQuad *pQ = &pEnv->aQuads[0];
        pQ->v[0].x = 12345.0f;
        DlReset();
        BrHudDrawDial(g_views);
        CHECK(pQ->v[0].x == 12345.0f);
    }
}

/* =====================================================================
 * 0x1001A4B0
 * ===================================================================== */
static void TestForward(void)
{
    BrWeather *pW = BrWeatherGet();
    static void *tbl[4];
    static int marker;

    tbl[2] = &marker;
    pW->apTable = tbl;
    BrForward1001A4B0(2);
    CHECK(g_a290A0[0] == &pW->f2554);
    CHECK(g_a290A0[1] == &pW->f2558);
    CHECK(g_a290A0[2] == &marker);
}

int main(void)
{
    TestDrawTexRect();
    TestRectEncodings();
    TestColorHandlers();
    TestModeBits();
    TestIndirectAndLatch();
    TestRecacheThunks();
    TestGap();
    TestSplitLine();
    TestPlainClear();
    TestSceneAccumReset();
    TestSceneSetup();
    TestWind();
    TestLightning();
    TestParticleSeed();
    TestParticleStep();
    TestHudText();
    TestDialGates();
    TestForward();

    printf("slice2_15: %d checks, %d failures\n", g_ran, g_fail);
    return g_fail != 0;
}
