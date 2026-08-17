/* test_br_dlcmd.c -- the nine transcribed display-list opcode handlers.
 *
 * WHAT THIS SUITE IS FOR
 * ----------------------------------------------------------------------
 * The dispatch loop is `while (p) p = table[p[3]](p);`.  A handler that does
 * the right work and returns the wrong next-pointer desynchronises every
 * command after it and produces garbage that looks like a data bug, so the
 * RETURN VALUE is asserted for every handler on every path it has -- not just
 * once per handler.  That is section 1, and it is the section that matters.
 *
 * After that, one property per handler that would be silently wrong if the
 * transcription were wrong:
 *   - 0xF6 is 10.2 FIXED POINT, w0 is the lower-right corner, and the Y axis
 *     is flipped against the screen height.  Three separate ways to be wrong.
 *   - 0xF7 expands RGBA5551 with the 5->8 replicate, alpha to 0 or 255.
 *   - 0xFA is 0..255 and 0xFB is 0..1.  Same four bytes, different scale.
 *   - 0xFC latches BEFORE it applies.
 *   - 0x04 reads n and v0 out of F3DEX's fields, transforms row-vector, and
 *     computes the seven outcodes with the NaN-takes-the-true-side sense.
 *   - 0xBF / 0xB1 pick the right index bytes, in the right order, and take
 *     the right one of three exits; and 0xB1's second triangle survives the
 *     first being rejected.
 *
 * Assertions are properties, not volume: identities (a rect's spans are the
 * corner differences plus one, whatever the corners), boundaries actually
 * present in the original (n == 0, an all-shared outcode, an exact half in
 * the quarter-pixel snap), and round-trips (5->8 expansion of 0x1F is 0xFF).
 *
 * Every assertion below was mutation-tested; the table is in the report.
 */
#include "br_dlcmd.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------------ */
/* fixtures                                                            */
/* ------------------------------------------------------------------ */

/* What the sink saw, so a test can assert the ARGUMENTS the original
 * computes and not merely the state it leaves behind. */
typedef struct Spy {
    int      cClip, cDraw, cRect, cCombine, cConst, cFog;
    BrDlVtx *a, *b, *c;                      /* last tri, in call order   */
    int32_t  minx, miny, maxx, maxy;         /* last 0x1001E380 call      */
    uint32_t cbW0, cbW1;                     /* last 0x1001E7A0 call      */
    uint32_t constColour, fogColour;
    /* Latched at the moment pfnCombine fires -- proves the two stores
     * precede the call rather than following it. */
    uint32_t seenW0, seenW1;
    const BrDlCmd *pS;
    const uint8_t *pVtxSrc;
    size_t   cbVtxSrc;
} Spy;

static void spy_clip(void *u, BrDlVtx *a, BrDlVtx *b, BrDlVtx *c)
{ Spy *s = u; s->cClip++; s->a = a; s->b = b; s->c = c; }
static void spy_draw(void *u, BrDlVtx *a, BrDlVtx *b, BrDlVtx *c)
{ Spy *s = u; s->cDraw++; s->a = a; s->b = b; s->c = c; }
static void spy_rect(void *u, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{ Spy *s = u; s->cRect++; s->minx = x0; s->miny = y0; s->maxx = x1; s->maxy = y1; }
static void spy_combine(void *u, uint32_t w0, uint32_t w1)
{
    Spy *s = u;
    s->cCombine++; s->cbW0 = w0; s->cbW1 = w1;
    s->seenW0 = s->pS->combineW0;    /* what the globals held at call time */
    s->seenW1 = s->pS->combineW1;
}
static void spy_const(void *u, uint32_t c) { Spy *s = u; s->cConst++; s->constColour = c; }
static void spy_fog(void *u, uint32_t c)   { Spy *s = u; s->cFog++;  s->fogColour  = c; }
static const uint8_t *spy_resolve(void *u, uint32_t addr, size_t cbNeed)
{
    Spy *s = u;
    (void)addr;
    if (s->pVtxSrc == NULL || cbNeed > s->cbVtxSrc)
        return NULL;
    return s->pVtxSrc;
}

static void setup(BrDlCmd *pS, Spy *pSpy, int32_t cy)
{
    memset(pSpy, 0, sizeof(*pSpy));
    BrDlCmdInit(pS, cy);
    pSpy->pS = pS;
    pS->sink.pUser            = pSpy;
    pS->sink.pfnClipTri       = spy_clip;
    pS->sink.pfnDrawTri       = spy_draw;
    pS->sink.pfnFillRect      = spy_rect;
    pS->sink.pfnCombine       = spy_combine;
    pS->sink.pfnConstantColor = spy_const;
    pS->sink.pfnFogColor      = spy_fog;
    pS->sink.pfnResolve       = spy_resolve;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void putflt(uint8_t *p, float f)
{ uint32_t v; memcpy(&v, &f, sizeof(v)); put32(p, v); }

/* ------------------------------------------------------------------ */
/* 1. the next-command pointer -- every handler, every path            */
/* ------------------------------------------------------------------ */
static void test_advance(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[16];
    uint8_t src[8 * 0x20];
    int fAll = 1, i;
    /* Two vertices set up so that a triangle can be made to take each of the
     * three exits; filled in by the outcode field directly. */

    printf("next-command pointer\n");

    memset(src, 0, sizeof(src));
    for (i = 0; i < 8; ++i) {
        putflt(src + i * 0x20 + 0x00, 0.0f);
        putflt(src + i * 0x20 + 0x04, 0.0f);
        putflt(src + i * 0x20 + 0x08, 0.0f);
    }

    /* Each row: opcode, w0, w1, and a short description of the PATH it
     * drives.  Every one must answer cmd + 8. */
    {
        struct { unsigned op; uint32_t w0, w1; const char *what; } aCase[] = {
            { 0x04, 0x04000000u, 0x1000u, "0x04 n == 0 (loop never entered)" },
            { 0x04, 0x04000C00u, 0x1000u, "0x04 n == 3, all resolved"        },
            { 0x04, 0x0400FC00u, 0x1000u, "0x04 n == 63, clamped at 32"      },
            { 0xBF, 0xBF000000u, 0x00000000u, "0xBF drawn"                   },
            { 0xB1, 0xB1000000u, 0x00000000u, "0xB1 both drawn"              },
            { 0xF6, 0xF6000000u, 0x00000000u, "0xF6 fill rect"               },
            { 0xF7, 0xF7000000u, 0x0000FFFFu, "0xF7 fill colour"             },
            { 0xF8, 0xF8000000u, 0x11223344u, "0xF8 fog colour"              },
            { 0xFA, 0xFA000000u, 0x11223344u, "0xFA prim colour"             },
            { 0xFB, 0xFB000000u, 0x11223344u, "0xFB env colour"              },
            { 0xFC, 0xFCFFFFFFu, 0xFFFCF87Cu, "0xFC set combine"            }
        };
        size_t k;
        for (k = 0; k < sizeof(aCase) / sizeof(aCase[0]); ++k) {
            const uint8_t *pNext;
            setup(&s, &spy, 480);
            spy.pVtxSrc  = src;
            spy.cbVtxSrc = sizeof(src);
            put32(cmd, aCase[k].w0);
            put32(cmd + 4, aCase[k].w1);
            pNext = BrDlCmdLookup(aCase[k].op)(&s, cmd);
            if (pNext != cmd + 8) {
                fAll = 0;
                printf("    %s returned +%ld\n", aCase[k].what,
                       (long)(pNext - cmd));
            }
        }
    }
    check(fAll, "every ordinary path returns p + 8");

    /* 0x04 with a source the sink refuses -- the port's DEVIATION path.  The
     * original would fault; what must not change is the return value. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = NULL;
    put32(cmd, 0x04000C00u); put32(cmd + 4, 0x1000u);
    check(BrDlCmdVtx(&s, cmd) == cmd + 8 && s.cVtxUnresolved == 1,
          "0x04 unresolved source still returns p + 8");

    /* 0xBF's three exits.  The outcodes are written straight into the vertex
     * array, which is exactly what a preceding 0x04 would have done. */
    setup(&s, &spy, 480);
    s.aVtx[4].outcode = s.aVtx[5].outcode = s.aVtx[6].outcode = BR_DL_CLIP_LEFT;
    memset(cmd, 0, 8); cmd[3] = 0xBF; cmd[4] = 4; cmd[5] = 5; cmd[6] = 6;
    check(BrDlCmdTri1(&s, cmd) == cmd + 8 && s.cTriRejected == 1 &&
          spy.cDraw == 0 && spy.cClip == 0,
          "0xBF trivial reject returns p + 8 ([esp+0x18] path)");

    setup(&s, &spy, 480);
    s.aVtx[4].outcode = BR_DL_CLIP_LEFT;
    s.aVtx[5].outcode = BR_DL_CLIP_RIGHT;
    s.aVtx[6].outcode = 0;
    check(BrDlCmdTri1(&s, cmd) == cmd + 8 && s.cTriClipped == 1 &&
          spy.cClip == 1,
          "0xBF clipped returns p + 8 ([esp+0x24] path)");

    /* 0xB1: reject the first triangle, and the SECOND must still run.  This
     * is the one control-flow fact about 0x1001FA30 that is not obvious --
     * both of the first triangle's failure exits land on 0x1001FBAA, which
     * is the second triangle, not the epilogue. */
    setup(&s, &spy, 480);
    s.aVtx[0].outcode = s.aVtx[1].outcode = s.aVtx[2].outcode = BR_DL_CLIP_TOP;
    memset(cmd, 0, 8);
    cmd[0] = 0; cmd[1] = 1; cmd[2] = 2;
    cmd[3] = 0xB1;
    cmd[4] = 4; cmd[5] = 5; cmd[6] = 6;
    check(BrDlCmdTri2(&s, cmd) == cmd + 8 && s.cTriRejected == 1 &&
          s.cTriDrawn == 1 && spy.cDraw == 1,
          "0xB1 rejecting triangle 1 still draws triangle 2");

    setup(&s, &spy, 480);
    s.aVtx[0].outcode = BR_DL_CLIP_TOP;
    s.aVtx[1].outcode = BR_DL_CLIP_BOTTOM;
    check(BrDlCmdTri2(&s, cmd) == cmd + 8 && s.cTriClipped == 1 &&
          s.cTriDrawn == 1,
          "0xB1 clipping triangle 1 still draws triangle 2");

    /* And the negative control for the whole section: the module must NOT
     * claim opcodes it does not own, because a table that answers for 0xDC
     * would return p + 8 where the original returns p + 8*w1. */
    check(BrDlCmdLookup(0xDC) == NULL && BrDlCmdLookup(0xE4) == NULL &&
          BrDlCmdLookup(0x06) == NULL && BrDlCmdLookup(0xB8) == NULL &&
          BrDlCmdLookup(0x00) == NULL,
          "the four variable-length opcodes are not claimed here");
    check(BrDlCmdLookup(0x04) == BrDlCmdVtx &&
          BrDlCmdLookup(0xB1) == BrDlCmdTri2 &&
          BrDlCmdLookup(0xBF) == BrDlCmdTri1 &&
          BrDlCmdLookup(0xF6) == BrDlCmdFillRect &&
          BrDlCmdLookup(0xF7) == BrDlCmdFillColour &&
          BrDlCmdLookup(0xF8) == BrDlCmdFogColour &&
          BrDlCmdLookup(0xFA) == BrDlCmdPrimColour &&
          BrDlCmdLookup(0xFB) == BrDlCmdEnvColour &&
          BrDlCmdLookup(0xFC) == BrDlCmdSetCombine,
          "each of the nine opcodes reaches its own handler");
}

/* ------------------------------------------------------------------ */
/* 2. 0xF6 -- fixed point, corner assignment, Y flip                   */
/* ------------------------------------------------------------------ */
static void test_fillrect(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[8];
    const int32_t H = 480;

    printf("0xF6 G_FILLRECT (0x1001E320)\n");

    /* ulx=10 uly=20 lrx=100 lry=200, all in 10.2 -- i.e. multiplied by 4.
     *   w0 = lrx<<14 | lry<<2   (the X field is bits 23:12, so <<12 of a 10.2
     *                            value whose integer part is <<2 inside it)
     *   w1 = ulx<<14 | uly<<2 */
    setup(&s, &spy, H);
    put32(cmd, (100u << 14) | (200u << 2));
    put32(cmd + 4, (10u << 14) | (20u << 2));
    BrDlCmdFillRect(&s, cmd);
    check(spy.cRect == 1, "0xF6 calls 0x1001E380 once");
    check(spy.minx == 10, "minx is w1's X -- w1 is the UPPER-LEFT corner");
    check(spy.maxx == 101, "maxx is w0's X + 1 -- w0 is the LOWER-RIGHT");
    check(spy.miny == H - 200 - 1, "miny is cyScreen - w0.Y - 1 (flipped)");
    check(spy.maxy == H - 20, "maxy is cyScreen - w1.Y (flipped)");
    /* The identity that survives any corner choice: both spans are the
     * inclusive pixel counts. */
    check(spy.maxx - spy.minx == 100 - 10 + 1 &&
          spy.maxy - spy.miny == 200 - 20 + 1,
          "both spans are (corner difference + 1) pixels");

    /* FIXED POINT, and this is the assertion that separates 0xF6 from 0xE1.
     * The low two bits of each field are the fraction and must be DISCARDED.
     * If the handler read plain integers, adding 3 to every field would move
     * every corner by 3; reading 10.2 it moves nothing. */
    setup(&s, &spy, H);
    put32(cmd, (100u << 14) | (200u << 2) | (3u << 12) | 3u);
    put32(cmd + 4, (10u << 14) | (20u << 2) | (3u << 12) | 3u);
    BrDlCmdFillRect(&s, cmd);
    check(spy.minx == 10 && spy.maxx == 101 &&
          spy.miny == H - 200 - 1 && spy.maxy == H - 20,
          "0xF6 discards the 10.2 fraction: +3 in every field changes nothing");

    /* The 10-bit mask.  A 12-bit field of 0xFFF is 1023.75 in 10.2; the
     * integer part 1023 fits in ten bits, so the mask is invisible there.
     * It becomes visible one step up: the ORIGINAL sign-extends before
     * masking, so what must be asserted is that the result is always in
     * 0..1023 whatever the field. */
    setup(&s, &spy, H);
    put32(cmd, 0x00FFFFFFu);
    put32(cmd + 4, 0x00FFFFFFu);
    BrDlCmdFillRect(&s, cmd);
    check(spy.minx == 1023 && spy.maxx == 1024 &&
          spy.miny == H - 1023 - 1 && spy.maxy == H - 1023,
          "0xF6 masks each corner to ten bits");
}

/* ------------------------------------------------------------------ */
/* 3. 0xF7 -- RGBA5551 expansion                                       */
/* ------------------------------------------------------------------ */
static void test_fillcolour(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[8];
    int fAll = 1;
    unsigned v;

    printf("0xF7 G_SETFILLCOLOR (0x1001E9F0)\n");

    /* Full white with alpha: every channel 0x1F, bit 0 set. */
    setup(&s, &spy, 480);
    put32(cmd, 0xF7000000u); put32(cmd + 4, 0x0000FFFFu);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillR == 0xFF && s.fillG == 0xFF && s.fillB == 0xFF &&
          s.fillA == 0xFF, "0x1F expands to 0xFF on every channel");

    /* Pure red, alpha clear.  Catches a channel being read from the wrong
     * bit range, which a grey test cannot. */
    setup(&s, &spy, 480);
    put32(cmd + 4, 0xF800u);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillR == 0xFF && s.fillG == 0 && s.fillB == 0 && s.fillA == 0,
          "R is bits 15:11, and alpha 0 stays 0");
    setup(&s, &spy, 480);
    put32(cmd + 4, 0x07C0u);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillG == 0xFF && s.fillR == 0 && s.fillB == 0, "G is bits 10:6");
    setup(&s, &spy, 480);
    put32(cmd + 4, 0x003Eu);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillB == 0xFF && s.fillR == 0 && s.fillG == 0, "B is bits 5:1");
    setup(&s, &spy, 480);
    put32(cmd + 4, 0x0001u);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillA == 0xFF && s.fillR == 0 && s.fillG == 0 && s.fillB == 0,
          "A is bit 0, spread to 0xFF -- not 1");

    /* The expansion IDENTITY, over all 32 levels and all three channels:
     * (v << 3) | (v >> 2).  Stated as the formula rather than a table so the
     * assertion cannot be satisfied by a lookup that is wrong in the same
     * way the code is. */
    for (v = 0; v < 32; ++v) {
        uint8_t want = (uint8_t)((v << 3) | (v >> 2));
        setup(&s, &spy, 480);
        put32(cmd + 4, (uint32_t)(v << 11)); BrDlCmdFillColour(&s, cmd);
        if (s.fillR != want) fAll = 0;
        setup(&s, &spy, 480);
        put32(cmd + 4, (uint32_t)(v << 6));  BrDlCmdFillColour(&s, cmd);
        if (s.fillG != want) fAll = 0;
        setup(&s, &spy, 480);
        put32(cmd + 4, (uint32_t)(v << 1));  BrDlCmdFillColour(&s, cmd);
        if (s.fillB != want) fAll = 0;
    }
    check(fAll, "all 32 levels x 3 channels satisfy (v<<3)|(v>>2)");

    /* The HIGH halfword is ignored -- a 32-bit fill colour is two pixels and
     * this build reads one. */
    setup(&s, &spy, 480);
    put32(cmd + 4, 0xFFFF0000u);
    BrDlCmdFillColour(&s, cmd);
    check(s.fillR == 0 && s.fillG == 0 && s.fillB == 0 && s.fillA == 0,
          "the high halfword of w1 is not read");
}

/* ------------------------------------------------------------------ */
/* 4. 0xFA / 0xFB / 0xF8 / 0xFC -- the state setters                   */
/* ------------------------------------------------------------------ */
static void test_setters(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[8];

    printf("0xFA / 0xFB / 0xF8 / 0xFC\n");

    /* 0xFA: 0..255, NO scale.  0xFB: 0..1, scaled by 1/255.  Same bytes. */
    setup(&s, &spy, 480);
    put32(cmd, 0xFA000000u); put32(cmd + 4, 0xFF804020u);
    BrDlCmdPrimColour(&s, cmd);
    check(s.primR == 255.0f && s.primG == 128.0f &&
          s.primB == 64.0f  && s.primA == 32.0f,
          "0xFA stores 0..255 -- prim colour is NOT divided by 255");
    check(spy.cConst == 1 && spy.constColour == 0xFF804020u,
          "0xFA passes the RAW w1 to grConstantColorValue");

    setup(&s, &spy, 480);
    put32(cmd, 0xFB000000u); put32(cmd + 4, 0xFF804020u);
    BrDlCmdEnvColour(&s, cmd);
    check(s.envR == 1.0f, "0xFB scales by 1/255: 0xFF becomes exactly 1.0");
    check(fabs((double)s.envG - 128.0 / 255.0) < 1e-6 &&
          fabs((double)s.envB -  64.0 / 255.0) < 1e-6 &&
          fabs((double)s.envA -  32.0 / 255.0) < 1e-6,
          "0xFB's other three channels scale the same way");
    check(spy.cConst == 0, "0xFB makes no backend call");

    /* Channel ORDER: R is the top byte in both.  A transposed decode would
     * pass the two tests above if the value were symmetric, which is why the
     * fixture above is not. */
    setup(&s, &spy, 480);
    put32(cmd + 4, 0x01020304u);
    BrDlCmdPrimColour(&s, cmd);
    check(s.primR == 1.0f && s.primG == 2.0f && s.primB == 3.0f &&
          s.primA == 4.0f, "0xFA reads R,G,B,A from bits 31:24 down");

    /* 0xF8 stores nothing and calls once with the raw word. */
    setup(&s, &spy, 480);
    put32(cmd, 0xF8000000u); put32(cmd + 4, 0xDEADBEEFu);
    BrDlCmdFogColour(&s, cmd);
    check(spy.cFog == 1 && spy.fogColour == 0xDEADBEEFu,
          "0xF8 is grFogColorValue(w1) and nothing else");

    /* 0xFC latches BEFORE it applies.  The spy reads the two globals from
     * inside the callback, so this fails if the stores were moved after. */
    setup(&s, &spy, 480);
    put32(cmd, 0xFCFFFFFFu); put32(cmd + 4, 0xFFFCF87Cu);
    BrDlCmdSetCombine(&s, cmd);
    check(s.combineW0 == 0xFCFFFFFFu && s.combineW1 == 0xFFFCF87Cu,
          "0xFC latches w0 and w1 into 0x105D17AC / 0x105D17B0");
    check(spy.cCombine == 1 && spy.cbW0 == 0xFCFFFFFFu &&
          spy.cbW1 == 0xFFFCF87Cu, "0xFC calls 0x1001E7A0 with (w0, w1)");
    check(spy.seenW0 == 0xFCFFFFFFu && spy.seenW1 == 0xFFFCF87Cu,
          "and both globals are already set when 0x1001E7A0 is entered");
}

/* ------------------------------------------------------------------ */
/* 5. 0x04 -- decode, transform, outcodes, snap                        */
/* ------------------------------------------------------------------ */

/* One expanded source record: x,y,z,s,t,n0,n1,n2. */
static void put_src(uint8_t *p, float x, float y, float z,
                    float ss, float tt, float a, float b, float c)
{
    putflt(p + 0x00, x);  putflt(p + 0x04, y);  putflt(p + 0x08, z);
    putflt(p + 0x0C, ss); putflt(p + 0x10, tt);
    putflt(p + 0x14, a);  putflt(p + 0x18, b);  putflt(p + 0x1C, c);
}

static void test_vtx(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[8];
    uint8_t src[40 * 0x20];

    printf("0x04 G_VTX (0x10021A20)\n");

    memset(src, 0, sizeof(src));

    /* --- n and v0 come out of F3DEX's fields ------------------------ */
    {
        int i;
        for (i = 0; i < 40; ++i)
            put_src(src + i * 0x20, (float)i, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    /* n = 3 -> bits 15:10 = 3 -> 0x0C00 ;  v0 = 7 -> bits 23:16 */
    put32(cmd, 0x04000000u | (7u << 16) | (3u << 10));
    put32(cmd + 4, 0x1000u);
    BrDlCmdVtx(&s, cmd);
    check(s.cVtxWritten == 3, "n is (w0 >> 10) & 0x3F -- F3DEX packing");
    check(s.aVtx[7].cx == 0.0f && s.aVtx[8].cx == 1.0f && s.aVtx[9].cx == 2.0f,
          "v0 is byte 2 of w0, and the destination runs upward from it");
    check(s.aVtx[6].cx == 0.0f, "nothing is written below v0");

    /* n == 0: no write at all, and the source is never touched. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = NULL;                      /* would refuse if consulted */
    put32(cmd, 0x04000000u | (7u << 16));
    BrDlCmdVtx(&s, cmd);
    check(s.cVtxWritten == 0 && s.cVtxUnresolved == 0,
          "n == 0 skips the loop without resolving the source");

    /* --- the row-vector transform ----------------------------------- */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    put_src(src, 1.0f, 2.0f, 3.0f, 0.25f, 0.5f, 7.0f, 8.0f, 9.0f);
    /* A matrix whose rows are distinguishable, so a transpose fails. */
    s.combined.m[0][0] = 2.0f;  s.combined.m[0][1] = 0.0f;
    s.combined.m[1][0] = 10.0f; s.combined.m[1][1] = 3.0f;
    s.combined.m[2][0] = 100.0f; s.combined.m[2][2] = 5.0f;
    s.combined.m[3][0] = 1000.0f;
    put32(cmd, 0x04000000u | (0u << 16) | (1u << 10));
    put32(cmd + 4, 0x1000u);
    BrDlCmdVtx(&s, cmd);
    /* cx = m[0][0]*x + m[1][0]*y + m[2][0]*z + m[3][0]
     *    = 2 + 20 + 300 + 1000 */
    check(s.aVtx[0].cx == 1322.0f,
          "out = v * M with the translation in ROW 3, not column 3");
    check(s.aVtx[0].s == 0.25f && s.aVtx[0].t == 0.5f,
          "s,t come from source +0x0C / +0x10 verbatim");
    check(s.aVtx[0].n0 == 7.0f && s.aVtx[0].n1 == 8.0f && s.aVtx[0].n2 == 9.0f,
          "the trailing three floats copy verbatim -- this is the UNLIT path");
    /* This fixture lands well outside (cx 1322, cy 6, cz 15, cw 1), so the
     * projection tail never ran and r/g/b are untouched -- which is itself the
     * behaviour asserted further down.  r/g/b are checked in the projection
     * fixture, where the vertex is inside. */
    check(s.aVtx[0].outcode ==
              (BR_DL_CLIP_FAR | BR_DL_CLIP_RIGHT | BR_DL_CLIP_TOP) &&
          s.aVtx[0].r == 0.0f,
          "clip-space fields are written even when the projection is skipped");

    /* --- the seven outcodes ----------------------------------------- */
    {
        /* Identity matrix, so clip space is the source.  Each case puts the
         * vertex outside exactly one plane and expects exactly that bit. */
        struct { float x, y, z, w; int32_t bit; const char *what; } aCase[] = {
            { 0.0f, 0.0f, 0.0f, -1.0f, BR_DL_CLIP_W | BR_DL_CLIP_NEAR |
                                       BR_DL_CLIP_FAR | BR_DL_CLIP_LEFT |
                                       BR_DL_CLIP_RIGHT | BR_DL_CLIP_BOTTOM |
                                       BR_DL_CLIP_TOP, "w < 0 sets all seven" },
            { 0.0f, 0.0f, -2.0f, 1.0f, BR_DL_CLIP_NEAR, "z + w < 0 -> NEAR"   },
            { 0.0f, 0.0f,  2.0f, 1.0f, BR_DL_CLIP_FAR,  "w - z < 0 -> FAR"    },
            { -2.0f, 0.0f, 0.0f, 1.0f, BR_DL_CLIP_LEFT, "x + w < 0 -> LEFT"   },
            {  2.0f, 0.0f, 0.0f, 1.0f, BR_DL_CLIP_RIGHT,"w - x < 0 -> RIGHT"  },
            { 0.0f, -2.0f, 0.0f, 1.0f, BR_DL_CLIP_BOTTOM,"y + w < 0 -> BOTTOM"},
            { 0.0f,  2.0f, 0.0f, 1.0f, BR_DL_CLIP_TOP,  "w - y < 0 -> TOP"    }
        };
        size_t k;
        int fAll = 1;
        for (k = 0; k < sizeof(aCase) / sizeof(aCase[0]); ++k) {
            setup(&s, &spy, 480);
            spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
            /* Feed clip space straight through: identity except that w must
             * come from somewhere, so put it in the translation row. */
            s.combined.m[3][3] = aCase[k].w;
            put_src(src, aCase[k].x, aCase[k].y, aCase[k].z,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            put32(cmd, 0x04000000u | (1u << 10));
            put32(cmd + 4, 0x1000u);
            BrDlCmdVtx(&s, cmd);
            if (s.aVtx[0].outcode != aCase[k].bit) {
                fAll = 0;
                printf("    %s: got %02X want %02X\n", aCase[k].what,
                       (unsigned)s.aVtx[0].outcode, (unsigned)aCase[k].bit);
            }
        }
        check(fAll, "each clip plane sets exactly its own outcode bit");
    }

    /* w == 0 is INSIDE: the test is strictly-less-than, not <=.  br_dl.h's
     * prose says "w <= 0" and the code says otherwise; the code is what the
     * `test ah,1` after `fcomp` does. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    s.combined.m[3][3] = 0.0f;
    put_src(src, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    put32(cmd, 0x04000000u | (1u << 10)); put32(cmd + 4, 0x1000u);
    BrDlCmdVtx(&s, cmd);
    check(s.aVtx[0].outcode == 0,
          "w == 0 sets no bit: the compare is C0, strictly less than");

    /* NaN takes the TRUE side, because an x87 unordered compare also sets
     * C0.  This is the rule CONVENTIONS.md records a live bug against. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    {
        uint32_t qnan = 0x7FC00000u;
        float f;
        memcpy(&f, &qnan, sizeof(f));
        s.combined.m[3][3] = f;
    }
    BrDlCmdVtx(&s, cmd);
    check(s.aVtx[0].outcode == (BR_DL_CLIP_W | BR_DL_CLIP_NEAR |
                                BR_DL_CLIP_FAR | BR_DL_CLIP_LEFT |
                                BR_DL_CLIP_RIGHT | BR_DL_CLIP_BOTTOM |
                                BR_DL_CLIP_TOP),
          "a NaN w sets every outcode bit -- unordered sets C0 too");

    /* --- projection, viewport and the quarter-pixel snap ------------- */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    s.vpScaleX = 160.0f; s.vpTransX = 160.0f;
    s.vpScaleY = 120.0f; s.vpTransY = 120.0f;
    put_src(src, 0.5f, 0.25f, 0.0f, 0.0f, 0.0f, 7.0f, 8.0f, 9.0f);
    s.combined.m[3][3] = 1.0f;
    put32(cmd, 0x04000000u | (1u << 10)); put32(cmd + 4, 0x1000u);
    BrDlCmdVtx(&s, cmd);
    check(s.aVtx[0].oow == 1.0f, "oow is 1/w");
    check(s.aVtx[0].x == 240.0f && s.aVtx[0].y == 150.0f,
          "screen = trans + scale * (clip / w)");
    check(s.cVtxTransformed == 1, "an inside vertex is projected");
    check(s.aVtx[0].r == 7.0f && s.aVtx[0].g == 8.0f && s.aVtx[0].b == 9.0f,
          "r,g,b are the source's trailing three (br_dl.h: not a colour "
          "when unlit)");

    /* The snap: 4 * value, round to nearest, / 4.  A value at an exact half
     * quarter-pixel is the boundary that separates ties-to-even (the x87
     * fistp the original uses) from ties-away (what an `x + 0.5` truncation
     * gives).  0.125 -> 0.5 quarters -> 0 -> 0.0 under ties-to-even;
     * 0.375 -> 1.5 quarters -> 2 -> 0.5 under the same rule.  Both round the
     * OTHER way under ties-away-from-zero, so this one fixture separates the
     * two conventions in both directions. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    s.vpScaleX = 1.0f; s.vpTransX = 0.0f;
    s.vpScaleY = 1.0f; s.vpTransY = 0.0f;
    s.combined.m[3][3] = 1.0f;
    put_src(src, 0.125f, 0.375f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlCmdVtx(&s, cmd);
    check(s.aVtx[0].x == 0.0f && s.aVtx[0].y == 0.5f,
          "the quarter-pixel snap rounds TIES TO EVEN, as fistp does");
    check(s.snapScratch == 2,
          "0x105CE310 is a real global and holds the last fistp result");

    /* An outside vertex is not projected at all: the original's `jne` skips
     * the whole tail, so x/y/oow keep the PREVIOUS occupant's values. */
    setup(&s, &spy, 480);
    spy.pVtxSrc = src; spy.cbVtxSrc = sizeof(src);
    s.aVtx[0].x = 12345.0f;
    s.combined.m[3][3] = 1.0f;
    put_src(src, -9.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    BrDlCmdVtx(&s, cmd);
    check(s.aVtx[0].outcode == BR_DL_CLIP_LEFT && s.aVtx[0].x == 12345.0f &&
          s.cVtxTransformed == 0,
          "an outside vertex keeps the slot's previous screen position");
}

/* ------------------------------------------------------------------ */
/* 6. 0xBF / 0xB1 -- index bytes, order, and the three exits           */
/* ------------------------------------------------------------------ */
static void test_tri(void)
{
    BrDlCmd s;
    Spy spy;
    uint8_t cmd[8];

    printf("0xBF / 0xB1 triangles\n");

    /* WHICH BYTES, AND IN WHICH ORDER.  Distinct indices, so a permutation
     * fails; the vertices are identified by their oow so the spy can name
     * which one arrived where. */
    setup(&s, &spy, 480);
    memset(cmd, 0, 8);
    cmd[3] = 0xBF; cmd[4] = 3; cmd[5] = 7; cmd[6] = 11;
    s.aVtx[3].oow = 3.0f; s.aVtx[7].oow = 7.0f; s.aVtx[11].oow = 11.0f;
    BrDlCmdTri1(&s, cmd);
    check(spy.cDraw == 1, "0xBF with all outcodes zero draws");
    check(spy.a == &s.aVtx[11] && spy.b == &s.aVtx[7] && spy.c == &s.aVtx[3],
          "0xBF is (byte 6, byte 5, byte 4) -- byte 6 FIRST");
    /* Byte 7 is the opcode's neighbour and byte 3 is the opcode; neither may
     * be read as an index. */
    check(s.cTriIn == 1, "0xBF submits exactly one triangle");

    /* texture coordinates are completed for all three, and only for the
     * drawn ones -- tmu0 and tmu1 both, s*scale*oow. */
    setup(&s, &spy, 480);
    s.texScaleS = 2.0f; s.texScaleT = 4.0f;
    s.aVtx[3].oow = 0.5f; s.aVtx[3].s = 3.0f; s.aVtx[3].t = 5.0f;
    BrDlCmdTri1(&s, cmd);
    check(s.aVtx[3].tmu0[0] == 3.0f && s.aVtx[3].tmu1[0] == 3.0f,
          "tmu[0] is s * texScaleS * oow, on BOTH TMUs");
    check(s.aVtx[3].tmu0[1] == 10.0f && s.aVtx[3].tmu1[1] == 10.0f,
          "tmu[1] is t * texScaleT * oow");
    check(s.aVtx[3].tmu0[2] == 0.5f && s.aVtx[3].tmu1[2] == 0.5f,
          "tmu[2] is oow");

    /* The trivial reject is an AND of all three, not an OR: two vertices
     * sharing a bit is NOT enough.
     *
     * THE FIRST FIXTURE HERE IS NOT ENOUGH ON ITS OWN, and that was found by
     * mutation: the original forms `and3 = b & c` and only then tests it
     * against `a`, so replacing that `&` with `|` still gives the right answer
     * whenever the ODD VERTEX OUT is `a` -- which is what a = 0 makes it.  The
     * second fixture puts the odd vertex at `c`, where the two operators
     * disagree, and it is the one that actually pins the AND. */
    setup(&s, &spy, 480);
    s.aVtx[3].outcode = BR_DL_CLIP_LEFT;
    s.aVtx[7].outcode = BR_DL_CLIP_LEFT;
    s.aVtx[11].outcode = 0;
    BrDlCmdTri1(&s, cmd);
    check(s.cTriRejected == 0 && s.cTriClipped == 1,
          "two vertices sharing a plane is clipped, not rejected");

    setup(&s, &spy, 480);
    s.aVtx[11].outcode = BR_DL_CLIP_LEFT;   /* a */
    s.aVtx[7].outcode  = BR_DL_CLIP_LEFT;   /* b */
    s.aVtx[3].outcode  = BR_DL_CLIP_TOP;    /* c -- the odd one out */
    BrDlCmdTri1(&s, cmd);
    check(s.cTriRejected == 0 && s.cTriClipped == 1,
          "a and b sharing a plane that c does not is clipped: the test is "
          "(b & c) & a, not (b | c) & a");

    setup(&s, &spy, 480);
    s.aVtx[11].outcode = BR_DL_CLIP_LEFT;   /* a */
    s.aVtx[7].outcode  = BR_DL_CLIP_TOP;    /* b -- the odd one out */
    s.aVtx[3].outcode  = BR_DL_CLIP_LEFT;   /* c */
    BrDlCmdTri1(&s, cmd);
    check(s.cTriRejected == 0 && s.cTriClipped == 1,
          "and the same with the odd vertex at b");
    setup(&s, &spy, 480);
    s.aVtx[3].outcode = BR_DL_CLIP_LEFT | BR_DL_CLIP_TOP;
    s.aVtx[7].outcode = BR_DL_CLIP_LEFT;
    s.aVtx[11].outcode = BR_DL_CLIP_LEFT | BR_DL_CLIP_FAR;
    BrDlCmdTri1(&s, cmd);
    check(s.cTriRejected == 1 && spy.cClip == 0 && spy.cDraw == 0,
          "all three sharing one plane is rejected without clipping");

    /* The clipper gets the same argument order the drawer does. */
    setup(&s, &spy, 480);
    s.aVtx[11].outcode = BR_DL_CLIP_LEFT;
    BrDlCmdTri1(&s, cmd);
    check(spy.cClip == 1 && spy.a == &s.aVtx[11] && spy.b == &s.aVtx[7] &&
          spy.c == &s.aVtx[3],
          "0x1001EE70 receives (a, b, c) in the same order as grDrawTriangle");

    /* 0xB1: two triangles, from bytes 2,1,0 and 6,5,4.  Note byte 3 is the
     * opcode and byte 7 is unused -- a handler that read 0,1,2 and 4,5,6 in
     * ascending order would pass a symmetric fixture, so these are not. */
    setup(&s, &spy, 480);
    memset(cmd, 0, 8);
    cmd[0] = 1; cmd[1] = 2; cmd[2] = 3; cmd[3] = 0xB1;
    cmd[4] = 4; cmd[5] = 5; cmd[6] = 6; cmd[7] = 0xFF;
    BrDlCmdTri2(&s, cmd);
    check(s.cTriIn == 2 && s.cTriDrawn == 2, "0xB1 submits two triangles");
    check(spy.a == &s.aVtx[6] && spy.b == &s.aVtx[5] && spy.c == &s.aVtx[4],
          "0xB1's SECOND triangle is (byte 6, byte 5, byte 4)");
    setup(&s, &spy, 480);
    s.aVtx[4].outcode = s.aVtx[5].outcode = s.aVtx[6].outcode = BR_DL_CLIP_W;
    BrDlCmdTri2(&s, cmd);
    check(spy.cDraw == 1 && spy.a == &s.aVtx[3] && spy.b == &s.aVtx[2] &&
          spy.c == &s.aVtx[1],
          "0xB1's FIRST triangle is (byte 2, byte 1, byte 0)");
    check(cmd[7] == 0xFF, "byte 7 is not an index and is not touched");
}

int main(void)
{
    test_advance();
    test_fillrect();
    test_fillcolour();
    test_setters();
    test_vtx();
    test_tri();

    printf(g_fail ? "\n1 failures\n" : "\n0 failures\n");
    return g_fail;
}
