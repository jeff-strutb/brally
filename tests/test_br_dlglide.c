/* test_br_dlglide.c -- the seven Glide-state display-list opcodes.
 *
 * WHAT THIS ASSERTS, AND WHY EACH ONE CAN FAIL
 * ----------------------------------------------------------------------
 * Every assertion here was checked by reinstating the bug it guards and
 * confirming the suite goes red; the table is in the report that accompanied
 * this file.  The properties, in the order the original makes them:
 *
 *   1. THE RETURN VALUE OF ALL SEVEN.  A handler that does the right work and
 *      returns the wrong next-pointer desynchronises every command after it,
 *      which is silent and unbounded.  Six return `p + 8`; 0xDC returns
 *      `p + 8*w1`, and that is checked at w1 = 0, 1, 2 and 7 -- including
 *      w1 == 0, where 0x1001E2E0's `lea eax,[esi+ecx*8]` returns p unchanged.
 *
 *   2. THE HOOK ARGUMENTS.  0x118ED1CC takes one argument, the low 24 bits of
 *      w0.  0x118ED1D0 takes two, and the second is w1 UNMASKED -- it is an
 *      address.  Both orders are read off the push sequence, not assumed.
 *
 *   3. A NULL HOOK IS DECLINED AND COUNTED.  These are .data slots that
 *      0x10029B50 fills at run time; before it runs they are zero.  The port
 *      must not call through NULL and must not pretend the call happened.
 *
 *   4. 0xE1's EXACT ARITHMETIC: signed 12-bit corners, w0 carrying the LOWER
 *      right, and the `+1` / `-1` the original applies before calling
 *      0x1001E380.  Checked with a negative corner, which is the case the
 *      unsigned reading gets catastrophically wrong (-8 becomes 4088).
 *
 *   5. 0xE2 AND 0xED ARE DIFFERENT DECODES.  Driven with the SAME two words
 *      through both, and asserted to disagree in a specific, computed way.
 *      This is the property br_dl.c currently gets wrong by routing both to
 *      one function.
 *
 *   6. THE Y FLIP, as an invariant rather than a transcription: for any
 *      scissor whose upper-left is above its lower-right, the resulting
 *      window must have minY <= maxY.  Drop the flip and the window inverts,
 *      because uly < lry and the two swap roles under `H - y`.
 *
 *   7. 0xF2 IS DELEGATED, and the delegation is honest.  This module's entry
 *      decodes nothing and says so; the REAL decode is br_dl.c's
 *      br_dl_settilesize, and it is exercised here through BrDlRun rather
 *      than duplicated -- including the negative fold and the arithmetic
 *      shift that keeps a negative extent negative.
 *
 * WHAT IS DELIBERATELY NOT ASSERTED: anything about 0x1001E380's output.  It
 * is 914 bytes and is not transcribed; the fill handler's hook is where this
 * port stops, and the test checks the arguments handed to that edge, not what
 * lies past it.
 */
#include "br_dlglide.h"
#include "br_dl.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------------ */
/* a recording sink for the four edges                                */
/* ------------------------------------------------------------------ */
typedef struct Rec {
    int      cSelect, cRetarget, cClip, cRect;
    uint32_t selId;
    uint32_t reId, reAddr;
    int32_t  clip[4];
    int32_t  rect[4];
} Rec;

static void on_select(void *pv, uint32_t id)
{ Rec *r = (Rec *)pv; r->cSelect++; r->selId = id; }

static void on_retarget(void *pv, uint32_t id, uint32_t addr)
{ Rec *r = (Rec *)pv; r->cRetarget++; r->reId = id; r->reAddr = addr; }

static void on_clip(void *pv, int32_t a, int32_t b, int32_t c, int32_t d)
{ Rec *r = (Rec *)pv; r->cClip++;
  r->clip[0] = a; r->clip[1] = b; r->clip[2] = c; r->clip[3] = d; }

static void on_rect(void *pv, int32_t a, int32_t b, int32_t c, int32_t d)
{ Rec *r = (Rec *)pv; r->cRect++;
  r->rect[0] = a; r->rect[1] = b; r->rect[2] = c; r->rect[3] = d; }

#define SCREEN_H 480

static Rec g_rec;

static void setup(BrDlGl *pGl)
{
    memset(&g_rec, 0, sizeof(g_rec));
    BrDlGlInit(pGl, SCREEN_H);
    pGl->hook.pUser          = &g_rec;
    pGl->hook.pfnTexSelect   = on_select;
    pGl->hook.pfnTexRetarget = on_retarget;
    pGl->hook.pfnClipWindow  = on_clip;
    pGl->hook.pfnFillRect    = on_rect;
}

/* Commands are host-order words, as the interpreter sees them (the loader has
 * already byte-swapped the list).  Written byte-wise: CONVENTIONS.md forbids
 * overlaying a struct on a display list. */
static void put(uint8_t *p, uint32_t w0, uint32_t w1)
{
    p[0] = (uint8_t)w0;         p[1] = (uint8_t)(w0 >> 8);
    p[2] = (uint8_t)(w0 >> 16); p[3] = (uint8_t)(w0 >> 24);
    p[4] = (uint8_t)w1;         p[5] = (uint8_t)(w1 >> 8);
    p[6] = (uint8_t)(w1 >> 16); p[7] = (uint8_t)(w1 >> 24);
}

static void enddl_fill(uint8_t *p, size_t cb)
{
    size_t i;
    for (i = 0; i + 8 <= cb; i += 8)
        put(p + i, 0xB8000000u, 0u);
}

/* ------------------------------------------------------------------ */
/* 1. the seven slots of 0x100A9A58                                   */
/* ------------------------------------------------------------------ */
static void test_dispatch(void)
{
    /* Read out of orig/BRGlide.dll at 0x100A9A58, slot by slot.  The table has
     * 28 live handlers; these are the seven this module owns, and the other 21
     * must come back NULL so a caller cannot install this module over an
     * opcode it does not implement. */
    static const struct { unsigned op; BrDlGlHandler fn; } aMine[] = {
        { 0xDC, BrDlGlBindTexture  },
        { 0xDD, BrDlGlRetarget     },
        { 0xDF, BrDlGlSet5D17C4    },
        { 0xE1, BrDlGlFillRect     },
        { 0xE2, BrDlGlScissorInt   },
        { 0xED, BrDlGlScissorFrac  },
        { 0xF2, BrDlGlSetTileSize  }
    };
    /* The other 21 live opcodes, which are br_dl.c's. */
    static const unsigned aTheirs[] = {
        0x01, 0x03, 0x04, 0x06, 0xB1, 0xB6, 0xB7, 0xB8, 0xB9, 0xBC,
        0xBD, 0xBF, 0xDE, 0xE3, 0xE4, 0xF6, 0xF7, 0xF8, 0xFA, 0xFB, 0xFC
    };
    size_t i;
    unsigned op;
    int fOk = 1, cNonNull = 0;

    printf("dispatch (0x100A9A58)\n");

    for (i = 0; i < sizeof(aMine) / sizeof(aMine[0]); ++i)
        if (BrDlGlDispatch(aMine[i].op) != aMine[i].fn)
            fOk = 0;
    check(fOk, "each of the seven slots maps to its own handler");

    fOk = 1;
    for (i = 0; i < sizeof(aTheirs) / sizeof(aTheirs[0]); ++i)
        if (BrDlGlDispatch(aTheirs[i]) != NULL)
            fOk = 0;
    check(fOk, "the other 21 live opcodes are NOT claimed by this module");

    for (op = 0; op < 256u; ++op)
        if (BrDlGlDispatch(op) != NULL)
            cNonNull++;
    check(cNonNull == 7, "exactly seven opcodes are claimed, no more");

    /* Cross-check against the other module's independent reading of the same
     * table: everything this module claims must be an opcode the original
     * actually handles, i.e. must not fall to the 0x10021240 skip stub. */
    fOk = 1;
    for (i = 0; i < sizeof(aMine) / sizeof(aMine[0]); ++i)
        if (!BrDlIsHandled(aMine[i].op))
            fOk = 0;
    check(fOk, "all seven are live in br_dl.c's reading of the table too");
    /* ...and the converse, which is the one that could catch a typo'd opcode:
     * an opcode the original leaves on the stub must not be claimed here. */
    fOk = 1;
    for (op = 0; op < 256u; ++op)
        if (!BrDlIsHandled(op) && BrDlGlDispatch(op) != NULL)
            fOk = 0;
    check(fOk, "no unhandled opcode is claimed");
}

/* ------------------------------------------------------------------ */
/* 2. return values -- the property most worth testing                */
/* ------------------------------------------------------------------ */
static void test_returns(void)
{
    BrDlGl gl;
    uint8_t dl[64];
    int fOk;

    printf("return values (the next-command pointer)\n");

    /* --- 0xDC: p + 8*w1, 0x1001E2F9 `lea eax,[esi+ecx*8]` --- */
    fOk = 1;
    {
        static const uint32_t aW1[] = { 1u, 2u, 7u, 13u };
        size_t i;
        for (i = 0; i < sizeof(aW1) / sizeof(aW1[0]); ++i) {
            setup(&gl);
            put(dl, 0xDC000123u, aW1[i]);
            if (BrDlGlBindTexture(&gl, dl) != dl + 8 * aW1[i])
                fOk = 0;
        }
    }
    check(fOk, "0xDC returns p + 8*w1");

    /* w1 == 0 returns p UNCHANGED.  The original then re-executes the same
     * command forever; the arithmetic is preserved and the loop bound is the
     * caller's problem, which is what br_dl.c's cycle guard is for. */
    setup(&gl);
    put(dl, 0xDC000123u, 0u);
    check(BrDlGlBindTexture(&gl, dl) == dl,
          "0xDC with w1 == 0 returns p, not p + 8");

    /* --- the five that return p + 8 --- */
    setup(&gl); put(dl, 0xDD000123u, 0x80101010u);
    check(BrDlGlRetarget(&gl, dl) == dl + 8, "0xDD returns p + 8");

    setup(&gl); put(dl, 0xDF000000u, 0x3F800000u);
    check(BrDlGlSet5D17C4(&gl, dl) == dl + 8, "0xDF returns p + 8");

    setup(&gl); put(dl, 0xE1000000u, 0u);
    check(BrDlGlFillRect(&gl, dl) == dl + 8, "0xE1 returns p + 8");

    setup(&gl); put(dl, 0xE2000000u, 0u);
    check(BrDlGlScissorInt(&gl, dl) == dl + 8, "0xE2 returns p + 8");

    setup(&gl); put(dl, 0xED000000u, 0u);
    check(BrDlGlScissorFrac(&gl, dl) == dl + 8, "0xED returns p + 8");

    setup(&gl); put(dl, 0xF2000000u, 0u);
    check(BrDlGlSetTileSize(&gl, dl) == dl + 8,
          "0xF2 returns p + 8 even though it delegates the decode");
}

/* ------------------------------------------------------------------ */
/* 3. the two hook slots                                              */
/* ------------------------------------------------------------------ */
static void test_hooks(void)
{
    BrDlGl gl;
    uint8_t dl[16];

    printf("hooks 0x118ED1CC / 0x118ED1D0\n");

    /* 0xDC: one argument, `and eax,0xFFFFFF` at 0x1001E2E7.  The opcode byte
     * and everything above bit 23 must be gone. */
    setup(&gl);
    put(dl, 0xDCABCDEFu, 1u);
    BrDlGlBindTexture(&gl, dl);
    check(g_rec.cSelect == 1, "0xDC calls [0x118ED1CC] exactly once");
    check(g_rec.selId == 0x00ABCDEFu,
          "0xDC passes w0 & 0xFFFFFF -- the opcode byte is masked off");
    check(gl.hTexture == 0x00ABCDEFu, "and records the same handle");

    /* 0xDD: two arguments.  0x1001E30A masks w0; NOTHING masks w1, because it
     * is an address.  The order is (id, addr) -- `push eax`(w1) then
     * `push ecx`(id) at 0x1001E310/0x1001E311, cdecl. */
    setup(&gl);
    put(dl, 0xDD00BEEFu, 0xFF123456u);
    BrDlGlRetarget(&gl, dl);
    check(g_rec.cRetarget == 1, "0xDD calls [0x118ED1D0] exactly once");
    check(g_rec.reId == 0x0000BEEFu, "0xDD's first argument is w0 & 0xFFFFFF");
    check(g_rec.reAddr == 0xFF123456u,
          "0xDD's second argument is w1 WHOLE -- an address is not masked");

    /* A NULL slot is the state before 0x10029B50 has run.  It must not be
     * called and must not be silent. */
    BrDlGlInit(&gl, SCREEN_H);
    memset(&g_rec, 0, sizeof(g_rec));
    put(dl, 0xDC000001u, 1u);
    check(BrDlGlBindTexture(&gl, dl) == dl + 8,
          "an uninstalled 0xDC hook still returns the right next-pointer");
    put(dl, 0xDD000001u, 0x1000u);
    BrDlGlRetarget(&gl, dl);
    check(g_rec.cSelect == 0 && g_rec.cRetarget == 0,
          "a NULL hook is never called");
    check(gl.cNullHook == 2,
          "...and both declined calls are counted, not hidden");
    check(gl.cBind == 1 && gl.cRetarget == 1,
          "the commands themselves are still counted");
}

/* ------------------------------------------------------------------ */
/* 4. 0xDF -- 0x1001EB30                                              */
/* ------------------------------------------------------------------ */
static void test_setdf(void)
{
    BrDlGl gl;
    uint8_t dl[16];

    printf("0xDF (0x1001EB30) -- park w1 in 0x105D17C4\n");

    setup(&gl);
    /* 0x3F800000 is 1.0f.  The handler moves a dword; its reader at
     * 0x1002171C does `fld [0x105D17C4]`, so the value is a float. */
    put(dl, 0xDF00FFFFu, 0x3F800000u);
    BrDlGlSet5D17C4(&gl, dl);
    check(gl.w5D17C4 == 0x3F800000u, "0xDF stores w1, not w0");
    check(BrDlGlGet5D17C4(&gl) == 1.0f,
          "and 0x3F800000 reads back as 1.0f through the float view");

    /* w0 is never consulted: two commands with the same w1 and different w0
     * must be indistinguishable. */
    setup(&gl);
    put(dl, 0xDF000000u, 0x40000000u);
    BrDlGlSet5D17C4(&gl, dl);
    check(BrDlGlGet5D17C4(&gl) == 2.0f, "w0 does not affect the stored value");
    check(gl.cSet5D17C4 == 1, "one command, one store");
}

/* ------------------------------------------------------------------ */
/* 5. 0xE1 -- 0x1001E720, fill rect with SIGNED INTEGER corners        */
/* ------------------------------------------------------------------ */
static void test_fillrect(void)
{
    BrDlGl gl;
    uint8_t dl[16];

    printf("0xE1 (0x1001E720) -- fill rect, integer corners\n");

    /* w0 = lower-right, w1 = upper-left -- stock G_FILLRECT packing, and the
     * OPPOSITE way round from G_SETSCISSOR below.
     *
     * lrx = 200, lry = 100, ulx = 40, uly = 20, H = 480:
     *   arg0 = ulx           =  40
     *   arg1 = H - lry - 1   = 480 - 100 - 1 = 379
     *   arg2 = lrx + 1       = 201
     *   arg3 = H - uly       = 460                                        */
    setup(&gl);
    put(dl, 0xE1000000u | (200u << 12) | 100u, (40u << 12) | 20u);
    BrDlGlFillRect(&gl, dl);
    check(g_rec.cRect == 1, "0xE1 calls 0x1001E380 once");
    check(g_rec.rect[0] == 40,  "arg0 is ulx, from w1's HIGH field");
    check(g_rec.rect[1] == 379, "arg1 is H - lry - 1  (flipped, then dec)");
    check(g_rec.rect[2] == 201, "arg2 is lrx + 1      (from w0's HIGH field)");
    check(g_rec.rect[3] == 460, "arg3 is H - uly      (flipped, no adjust)");

    /* NEGATIVE CORNERS.  `shl 20 / sar 20` with no mask, so 0xFF8 is -8 and
     * not 4088.  This is the case that separates the signed reading from the
     * masked one by 4096 pixels. */
    setup(&gl);
    put(dl, 0xE1000000u | (0xFF8u << 12) | 0xFFCu,   /* lrx = -8, lry = -4 */
             (0xFFEu << 12) | 0xFF0u);               /* ulx = -2, uly = -16 */
    BrDlGlFillRect(&gl, dl);
    check(g_rec.rect[0] == -2,  "ulx sign-extends: 0xFFE is -2, not 4094");
    check(g_rec.rect[1] == 483, "H - lry - 1 with lry = -4 is 483");
    check(g_rec.rect[2] == -7,  "lrx + 1 with lrx = -8 is -7, not 4089");
    check(g_rec.rect[3] == 496, "H - uly with uly = -16 is 496");

    /* NO DIVIDE BY FOUR.  0xF6 (0x1001E320) has `sar 0x16` and an `and 0x3FF`
     * where this has `sar 0x14`; 5 in 10.2 would be 1 pixel, and here it is 5.
     * A reading that quartered the corners would give 1 and 2. */
    setup(&gl);
    put(dl, 0xE1000000u | (5u << 12) | 9u, (1u << 12) | 3u);
    BrDlGlFillRect(&gl, dl);
    check(g_rec.rect[0] == 1 && g_rec.rect[2] == 6,
          "0xE1 does NOT divide by four -- corners are whole pixels");
    check(g_rec.rect[1] == SCREEN_H - 9 - 1 && g_rec.rect[3] == SCREEN_H - 3,
          "...on both axes");

    /* The two arguments are a MINIMUM and a MAXIMUM in Glide's bottom-up
     * window -- which is what makes them match the four clamps 0x1001E380
     * opens with.  For any rectangle with ul above/left of lr, that ordering
     * must hold; it does not if the flip is dropped. */
    setup(&gl);
    put(dl, 0xE1000000u | (300u << 12) | 200u, (10u << 12) | 20u);
    BrDlGlFillRect(&gl, dl);
    check(g_rec.rect[0] <= g_rec.rect[2] && g_rec.rect[1] <= g_rec.rect[3],
          "the four arguments come out as (minX, minY, maxX, maxY)");

    /* No hook installed: 0x1001E380 is not transcribed, so the frontier must
     * be counted rather than crossed. */
    BrDlGlInit(&gl, SCREEN_H);
    memset(&g_rec, 0, sizeof(g_rec));
    put(dl, 0xE1000000u, 0u);
    check(BrDlGlFillRect(&gl, dl) == dl + 8 && g_rec.cRect == 0 &&
          gl.cNullHook == 1 && gl.cFillRect == 1,
          "with no 0x1001E380 the command is counted and the edge declined");
}

/* ------------------------------------------------------------------ */
/* 6. 0xE2 / 0xED -- two decodes of one command                       */
/* ------------------------------------------------------------------ */
static void test_scissor(void)
{
    BrDlGl gl;
    uint8_t dl[16];
    int32_t aInt[4], aFrac[4];

    printf("0xE2 (0x1001EBC0) / 0xED (0x1001EB50) -- the clip window\n");

    /* --- 0xE2: plain 12-bit integers, w0 = upper-left, w1 = lower-right ---
     * ulx = 32, uly = 24, lrx = 608, lry = 456, H = 480:
     *   minX = 32          maxX = 608
     *   minY = H - 456 = 24    maxY = H - 24 = 456                        */
    setup(&gl);
    put(dl, 0xE2000000u | (32u << 12) | 24u, (608u << 12) | 456u);
    BrDlGlScissorInt(&gl, dl);
    check(gl.clipMinX == 32,  "0xE2 minX (0x105D17BC) is w0's high 12 bits");
    check(gl.clipMaxY == 456, "0xE2 maxY (0x105CCFE0) is H - uly");
    check(gl.clipMaxX == 608, "0xE2 maxX (0x105D17B8) is w1's high 12 bits");
    check(gl.clipMinY == 24,  "0xE2 minY (0x105D17C0) is H - lry");
    check(g_rec.cClip == 1, "0xE2 calls grClipWindow once");
    check(g_rec.clip[0] == 32 && g_rec.clip[1] == 24 &&
          g_rec.clip[2] == 608 && g_rec.clip[3] == 456,
          "grClipWindow(minx, miny, maxx, maxy) -- that push order");

    /* --- 0xED: the same fields in 10.2 ---
     * Encoding the same pixel rectangle means multiplying each field by four,
     * i.e. shifting the packed value left by two.  The result must be
     * IDENTICAL to the 0xE2 case above, which is what "two conventions, one
     * command" means. */
    setup(&gl);
    put(dl, 0xED000000u | ((32u * 4u) << 12) | (24u * 4u),
             ((608u * 4u) << 12) | (456u * 4u));
    BrDlGlScissorFrac(&gl, dl);
    check(gl.clipMinX == 32 && gl.clipMaxX == 608 &&
          gl.clipMinY == 24 && gl.clipMaxY == 456,
          "0xED in 10.2 lands on the same window as 0xE2 in integers");

    /* --- and the decodes genuinely DIFFER on identical words ---
     * This is the property br_dl.c loses by routing both opcodes to one
     * function.  Feed the same (w0,w1) to both and the answers must not
     * agree: the integer reading of 0x40018 is (0x040, 0x018) = (64, 24)
     * while the 10.2 reading of the same word is (16, 6). */
    setup(&gl);
    put(dl, 0xE2000000u | 0x00040018u, 0x00190164u);
    BrDlGlScissorInt(&gl, dl);
    aInt[0] = gl.clipMinX; aInt[1] = gl.clipMinY;
    aInt[2] = gl.clipMaxX; aInt[3] = gl.clipMaxY;

    setup(&gl);
    put(dl, 0xED000000u | 0x00040018u, 0x00190164u);
    BrDlGlScissorFrac(&gl, dl);
    aFrac[0] = gl.clipMinX; aFrac[1] = gl.clipMinY;
    aFrac[2] = gl.clipMaxX; aFrac[3] = gl.clipMaxY;

    check(aInt[0] == 64 && aFrac[0] == 16,
          "0xE2 reads (w>>12)&0xFFF; 0xED reads (w>>14)&0x3FF");
    check(aInt[3] == SCREEN_H - 0x18 && aFrac[3] == SCREEN_H - (0x18 >> 2),
          "0xE2 reads w&0xFFF; 0xED reads (w>>2)&0x3FF");
    check(memcmp(aInt, aFrac, sizeof(aInt)) != 0,
          "the two opcodes are NOT interchangeable on the same words");

    /* --- 0xE2 is a 12-bit field and 0xED a 10-bit one, so the masks differ
     * too, not only the shifts.  0xFFF survives 0xE2 and is truncated by the
     * 0x3FF mask in 0xED. */
    setup(&gl);
    put(dl, 0xE2000000u | (0xFFFu << 12) | 0u, 0u);
    BrDlGlScissorInt(&gl, dl);
    check(gl.clipMinX == 4095, "0xE2's field is 12 bits wide and unsigned");
    setup(&gl);
    put(dl, 0xED000000u | 0xFFFFFFFFu, 0u);
    BrDlGlScissorFrac(&gl, dl);
    check(gl.clipMinX == 1023, "0xED's field is masked to 10 bits");

    /* --- THE Y FLIP, as an invariant.  For any well-formed scissor -- upper
     * left above and left of lower right -- the window must come out with
     * minY <= maxY.  Without `H -` on both, uly < lry makes minY > maxY and
     * the window is inverted, which no amount of downstream clamping fixes. */
    {
        static const struct { uint32_t ulx, uly, lrx, lry; } aBox[] = {
            {   0u,   0u, 639u, 479u },
            {  10u,  20u, 300u, 240u },
            { 100u, 100u, 101u, 101u },
            {   0u, 200u, 639u, 479u }
        };
        size_t i;
        int fOk = 1;
        for (i = 0; i < sizeof(aBox) / sizeof(aBox[0]); ++i) {
            setup(&gl);
            put(dl, 0xE2000000u | (aBox[i].ulx << 12) | aBox[i].uly,
                     (aBox[i].lrx << 12) | aBox[i].lry);
            BrDlGlScissorInt(&gl, dl);
            if (gl.clipMinX > gl.clipMaxX || gl.clipMinY > gl.clipMaxY)
                fOk = 0;
            /* 0x1001E380 clamps arg0/arg1 UP against the first two and
             * arg2/arg3 DOWN against the last two, so this ordering is what
             * makes the clamp a clip rather than an inversion. */
        }
        check(fOk, "the Y flip keeps minY <= maxY for every well-formed box");
    }

    /* The height is the flip's only other input, and it is 0x100A7518 rather
     * than a constant: the same command against a different window must move
     * the window by exactly the height difference. */
    setup(&gl);
    put(dl, 0xE2000000u | (10u << 12) | 20u, (300u << 12) | 240u);
    BrDlGlScissorInt(&gl, dl);
    aInt[0] = gl.clipMinY; aInt[1] = gl.clipMaxY;
    BrDlGlInit(&gl, SCREEN_H / 2);
    BrDlGlScissorInt(&gl, dl);
    check(gl.clipMinY == aInt[0] - SCREEN_H / 2 &&
          gl.clipMaxY == aInt[1] - SCREEN_H / 2,
          "the flip uses 0x100A7518, not a hard-coded height");
}

/* ------------------------------------------------------------------ */
/* 7. 0xF2 -- delegated here, decoded in br_dl.c                      */
/* ------------------------------------------------------------------ */
/* This module's entry decodes nothing, so the assertion that matters is that
 * it SAYS so -- and that the real decode, br_dl.c's br_dl_settilesize
 * (0x1001EC30), is exercised rather than duplicated.  It is driven through
 * BrDlRun because br_dl_settilesize is static and must stay the one host
 * definition of that address.
 *
 * The disassembly this checks against, 0x1001EC30:
 *     uls   = s12(w0 >> 12)   -> 0x118ED198
 *     ult   = s12(w0)         -> 0x1186C950
 *     lrs   = s12(w1 >> 12)   -> 0x1186C954
 *     lrt   = s12(w1)         -> 0x118EC988
 *     tileW = (lrs - uls + 4) sar 2   -> 0x1186C958
 *     tileH = (lrt - ult + 4) sar 2   -> 0x118ED1AC
 *     return p + 8
 * The fold is `cmp 0x800 / jl / sub 0x1000`, and the two shifts are `sar`. */
static void test_settilesize(void)
{
    BrDlGl gl;
    BrDl   dl;
    uint8_t list[32];
    size_t n;

    printf("0xF2 (0x1001EC30) -- delegated, and the delegate checked\n");

    /* The frontier itself: counted, and honest about doing nothing. */
    setup(&gl);
    put(list, 0xF2000000u | (16u << 12) | 8u, (252u << 12) | 124u);
    check(BrDlGlSetTileSize(&gl, list) == list + 8, "0xF2 advances eight");
    check(gl.cF2Delegated == 1,
          "and reports itself as delegated rather than decoded");
    check(gl.cScissor == 0 && gl.cFillRect == 0 && gl.cNullHook == 0,
          "the delegate does nothing else -- no state is invented");

    /* Now the real thing, through br_dl.c.  uls=16 ult=8 lrs=252 lrt=124:
     *   tileW = (252 - 16 + 4) >> 2 = 60
     *   tileH = (124 -  8 + 4) >> 2 = 30                                  */
    BrDlInit(&dl, 640, 480);
    enddl_fill(list, sizeof(list));
    put(list, 0xF2000000u | (16u << 12) | 8u, (252u << 12) | 124u);
    /* POISON THE SLOT AFTER IT.  Asserting "n == 2" alone would be satisfied
     * by a handler that advanced sixteen and landed on the G_ENDDL, so the
     * next slot carries an observable command instead: G_SETGEOMETRYMODE with
     * a value nothing else could produce.  If 0xF2 does not advance by exactly
     * eight, that command is never executed and geoMode stays zero. */
    put(list + 8,  0xB7000000u, 0x12345678u);   /* G_SETGEOMETRYMODE */
    put(list + 16, 0xB8000000u, 0u);            /* G_ENDDL           */
    n = BrDlRun(&dl, list, sizeof(list));
    check(n == 3, "0xF2, G_SETGEOMETRYMODE, G_ENDDL is three commands");
    check(dl.geoMode == 0x12345678u,
          "the command eight bytes on ran -- 0xF2 advanced by exactly 8");
    check(dl.uls == 16 && dl.ult == 8 && dl.lrs == 252 && dl.lrt == 124,
          "0x1001EC30 unpacks (uls, ult) from w0 and (lrs, lrt) from w1");
    check(dl.tileW == 60 && dl.tileH == 30,
          "tile extents are (lr - ul + 4) >> 2");

    /* The sign fold, `cmp 0x800 / sub 0x1000`: 0xFF8 is -8. */
    BrDlInit(&dl, 640, 480);
    enddl_fill(list, sizeof(list));
    put(list, 0xF2000000u | (0xFF8u << 12) | 0u, (8u << 12) | 0u);
    put(list + 8, 0xB8000000u, 0u);
    BrDlRun(&dl, list, sizeof(list));
    check(dl.uls == -8, "a field at or above 0x800 folds negative");
    check(dl.tileW == 5, "and feeds the extent: (8 - -8 + 4) >> 2 == 5");

    /* 0x7FF is the largest POSITIVE value -- the boundary of the fold. */
    BrDlInit(&dl, 640, 480);
    enddl_fill(list, sizeof(list));
    put(list, 0xF2000000u | (0x7FFu << 12) | 0x800u, 0u);
    put(list + 8, 0xB8000000u, 0u);
    BrDlRun(&dl, list, sizeof(list));
    check(dl.uls == 2047, "0x7FF stays positive -- the fold is >= 0x800");
    check(dl.ult == -2048, "0x800 itself is the first negative value");

    /* An ARITHMETIC shift, so a negative extent stays negative: with
     * lrs = -16 and uls = 16, (-16 - 16 + 4) >> 2 == -28 >> 2 == -7.  A
     * logical shift would make it about a billion. */
    BrDlInit(&dl, 640, 480);
    enddl_fill(list, sizeof(list));
    put(list, 0xF2000000u | (16u << 12) | 0u, (0xFF0u << 12) | 0u);
    put(list + 8, 0xB8000000u, 0u);
    BrDlRun(&dl, list, sizeof(list));
    check(dl.lrs == -16 && dl.tileW == -7,
          "a negative extent stays negative -- the shift is arithmetic");
}

int main(void)
{
    printf("test_br_dlglide -- the seven Glide-state display-list opcodes\n\n");
    test_dispatch();
    test_returns();
    test_hooks();
    test_setdf();
    test_fillrect();
    test_scissor();
    test_settilesize();
    printf("\n%d failures\n", g_fail);
    return g_fail ? 1 : 0;
}
