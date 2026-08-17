/* test_br_drawcar.c -- BrCarDrawWheels (0x10009C10) and BrGuMtxStore
 * (0x10029E50).
 *
 * EVERY EXPECTED VALUE HERE IS DERIVED FROM THE DISASSEMBLY, not from the
 * port.  The two combiner words in particular are computed by hand from
 * 0x1001CF90's shift chain and 0x1001D150/0x1001D180's token tables, and
 * are written as literals so that a change to BrRdpSetCombineLERP would
 * FAIL this suite rather than move with it:
 *
 *   tokens          a0=TEXEL0 c0=PRIM Aa0=0 Ac0=0 a1=0 c1=0
 *   mux             CC(TEXEL0)=1 CC(PRIM)=4 AC(0)=7 CC(0)=31
 *   w0 chain        x=(1&0xF)|0xFFFFFFC0=0xFFFFFFC1
 *                   <<5 |4  = 0xFFFFF824
 *                   <<3 |7  = 0xFFFFC127
 *                   <<3 |7  = 0xFFFE093F
 *                   <<4 |15 = 0xFFE093FF
 *                   <<5 |31 = 0xFC127FFF
 *
 * The 0xFC opcode byte is never OR'd in -- it falls out of the ones-fill,
 * exactly as slice1_05.h records.
 */
#include "br_drawcar.h"
#include "slice1_05.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c, ...) do { if (!(c)) { \
        printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); \
        printf("\n"); ++g_fails; } } while (0)

/* ---- the display-list cursor -------------------------------------- *
 * slice2_18.c owns 0x106C0680 (== Glide 0x106E7710).  This suite links
 * br_drawcar alone, so it supplies the storage itself -- build.sh's header
 * documents that tests may stand in for a dependency.               */
uint32_t *BrG_6C0680;

static uint32_t s_dl[4096];

static void dl_reset(void) { memset(s_dl, 0, sizeof(s_dl)); BrG_6C0680 = s_dl; }
static int  dl_count(void) { return (int)(BrG_6C0680 - s_dl) / 2; }
static uint32_t w0(int i)  { return s_dl[2 * i]; }
static uint32_t w1(int i)  { return s_dl[2 * i + 1]; }

/* ---- the matrix pool, with the ORIGINAL's address arithmetic ------- *
 * 0x10062500 hands back 0x10B1CF20 + 0x40 * (n + 257 * buffer).  The
 * arena's own extent confirms it: 514 matrices of 0x40 bytes from
 * 0x10B1CF20 ends at 0x10B24FA0, which is exactly where the counter
 * 0x10B24FA0 lives.                                                  */
#define POOL_BASE 0x10B1CF20u
static BrMat4  s_pool[16];
static int     s_nPool;
static BrMat4 *pool_alloc(void)     { return &s_pool[s_nPool++ & 15]; }
static uint32_t pool_addr(const BrMat4 *pM)
{
    return POOL_BASE + 0x40u * (uint32_t)(pM - s_pool);
}

static const BrDrawCarHooks s_hooks = { pool_alloc, pool_addr };

/* ---- fixtures ------------------------------------------------------ */
static BrCarView    s_car;
static BrModelView  s_model;

static void fixture(uint8_t kind)
{
    int i_fixture;
    memset(&s_car, 0, sizeof(s_car));
    memset(&s_model, 0, sizeof(s_model));
    s_car.bKind = kind;
    /* Make the four wheel transforms DISTINGUISHABLE.  With all four
     * zeroed, "pass i uses aWheel[i]" cannot be tested at all: a
     * transcription that read aWheel[0] four times produced a
     * byte-identical command stream and survived the mutation table. */
    for (i_fixture = 0; i_fixture < 4; ++i_fixture)
        s_car.aWheel[i_fixture].m[0][0] = 1.0f + (float)i_fixture;
    s_model.dlWheel    = 0x0BADF00Du;
    s_model.dlWheelAlt = 0x0DEFACEDu;
    g_BrDrawRenderMode = 0x0C080000u;
    g_BrDrawFogAlpha   = 0x1234;      /* only the low byte may survive  */
    g_BrDrawWheelAlt   = 0;
    s_nPool = 0;
    dl_reset();
    BrDrawCarFrontierReset();
    BrDrawCarSetHooks(&s_hooks);
}

/* The combiner words, computed above from the shift chain. */
#define COMBINE_W0      0xFC127FFFu
/* w1 differs between the two arms only in Aa1/Ac1/Ad1:
 *   plain  Aa1=0 -> AC 7, Ac1=0 -> AC 7, Ad1=COMBINED -> AC 0
 *   class2 Aa1=COMBINED -> AC 0, Ac1=SHADE -> AC 5, Ad1=0 -> AC 7
 * Both chains start b0=CC(0)=31 unmasked, exactly as slice1_05.h records. */
#define COMBINE_W1_PLAIN 0xFFFFF238u
#define COMBINE_W1_KIND2 0xFF17F23Fu

/* Opcode of a command word: byte 3, per CONVENTIONS.md. */
static uint32_t op(int i) { return w0(i) >> 24; }

/* ------------------------------------------------------------------ */
static void test_plain_pass(void)
{
    int i, base;
    fixture(1);
    BrCarDrawWheels(&s_car, &s_model);

    /* 17 commands per pass, four passes.  Counted off the disassembly:
     * pipesync, othermode_H, combine, othermode_L, G_MTX, four G_MOVEMEM,
     * G_TEXTURE, cleargeom, tilesync, three G_SETTILE, G_DL, G_POPMTX. */
    CHECK(dl_count() == 68, "plain: %d commands, want 68", dl_count());
    if (dl_count() != 68) return;

    for (i = 0; i < 4; ++i) {
        base = i * 17;
        CHECK(w0(base + 0) == 0xE7000000u && w1(base + 0) == 0,
              "pass %d: pipe sync", i);
        CHECK(w0(base + 1) == 0xBA001402u && w1(base + 1) == 0x00100000u,
              "pass %d: two-cycle", i);
        /* THE ORDER IS THE POINT: the non-class-2 arm emits the combiner
         * FIRST and the render mode after it (0x10009D47 then 0x10009D60). */
        CHECK(w0(base + 2) == COMBINE_W0 && w1(base + 2) == COMBINE_W1_PLAIN,
              "pass %d: combine %08X %08X", i, w0(base + 2), w1(base + 2));
        CHECK(w0(base + 3) == 0xB900031Du
              && w1(base + 3) == (0x0C080000u | 0x00112230u),
              "pass %d: render mode %08X", i, w1(base + 3));
        CHECK(op(base + 4) == 0x01, "pass %d: G_MTX", i);
        CHECK(w0(base + 4) == 0x01060040u, "pass %d: PUSH|LOAD", i);

        /* The four 16-byte blocks of the combined matrix, in the
         * original's order 0x9E 0x98 0x9A 0x9C at +0 +0x10 +0x20 +0x30. */
        CHECK(w0(base + 5) == 0x039E0010u, "pass %d: movemem 9E", i);
        CHECK(w0(base + 6) == 0x03980010u, "pass %d: movemem 98", i);
        CHECK(w0(base + 7) == 0x039A0010u, "pass %d: movemem 9A", i);
        CHECK(w0(base + 8) == 0x039C0010u, "pass %d: movemem 9C", i);
        CHECK(w1(base + 6) == w1(base + 5) + 0x10u
              && w1(base + 7) == w1(base + 5) + 0x20u
              && w1(base + 8) == w1(base + 5) + 0x30u,
              "pass %d: movemem strides", i);

        CHECK(w0(base + 9) == 0xBB000001u && w1(base + 9) == 0xFFFFFFFFu,
              "pass %d: texture on", i);
        CHECK(w0(base + 10) == 0xB6000000u && w1(base + 10) == 0x000C0000u,
              "pass %d: clear texgen", i);
        CHECK(w0(base + 11) == 0xE8000000u, "pass %d: tile sync", i);
        CHECK(w0(base + 12) == 0xF5100000u && w1(base + 12) == 0x07000000u,
              "pass %d: tile 0", i);
        CHECK(w0(base + 13) == 0xF50001F0u && w1(base + 13) == 0x06000000u,
              "pass %d: tile 1", i);
        CHECK(w0(base + 14) == 0xF5000100u && w1(base + 14) == 0x05000000u,
              "pass %d: tile 2", i);
        CHECK(w0(base + 15) == 0x06000000u && w1(base + 15) == 0x0BADF00Du,
              "pass %d: G_DL %08X", i, w1(base + 15));
        CHECK(w0(base + 16) == 0xBD000000u, "pass %d: pop", i);
    }

    /* Each pass allocates TWO matrices, and the model matrix is not the
     * combined one -- a single shared slot would make the car and its
     * wheels move together. */
    CHECK(s_nPool == 8, "8 pool allocations, got %d", s_nPool);
    CHECK(w1(4) != w1(5), "model and combined matrices must differ");
    /* Four passes, four DIFFERENT model matrix SLOTS. */
    CHECK(w1(4) != w1(21) && w1(21) != w1(38) && w1(38) != w1(55),
          "each wheel needs its own matrix slot");

    /* ...and each slot must carry ITS OWN wheel.  The cursor advances by
     * 0x40 per pass in the original (`add edi, 0x40` at 0x10009F88), so
     * pass i scales aWheel[i]; the fixture makes m[0][0] = 1+i and the
     * 1/255 scale carries that straight through to the stored matrix.
     * Slots 0, 2, 4, 6 are the model matrices -- each pass allocates the
     * model matrix first and the combined one second. */
    for (i = 0; i < 4; ++i)
        CHECK(s_pool[i * 2].m[0][0] > (float)i * 0.0039f
              && s_pool[i * 2].m[0][0] < (float)(i + 2) * 0.0039f,
              "pass %d stored m00 %g, want about %g", i,
              (double)s_pool[i * 2].m[0][0], (double)(1.0f + (float)i) * 0.003921569);
}

static void test_kind2_pass(void)
{
    int i, base;
    fixture(2);
    BrCarDrawWheels(&s_car, &s_model);

    /* Class 2 adds the render mode and an env colour BEFORE the combiner,
     * so 18 commands, and the render mode is at index 2, not after. */
    CHECK(dl_count() == 72, "kind2: %d commands, want 72", dl_count());
    if (dl_count() != 72) return;

    for (i = 0; i < 4; ++i) {
        base = i * 18;
        CHECK(w0(base + 0) == 0xE7000000u, "pass %d: pipe sync", i);
        CHECK(w0(base + 1) == 0xBA001402u, "pass %d: two-cycle", i);
        CHECK(w0(base + 2) == 0xB900031Du
              && w1(base + 2) == (0x0C080000u | 0x00104A50u),
              "pass %d: class-2 render mode %08X", i, w1(base + 2));
        /* G_SETENVCOLOR carries ONLY the low byte of the fog alpha; the
         * original masks with 0xFF and leaves RGB zero. */
        CHECK(w0(base + 3) == 0xFB000000u && w1(base + 3) == 0x34u,
              "pass %d: env colour %08X", i, w1(base + 3));
        CHECK(w0(base + 4) == COMBINE_W0 && w1(base + 4) == COMBINE_W1_KIND2,
              "pass %d: class-2 combine %08X %08X", i,
              w0(base + 4), w1(base + 4));
        CHECK(w0(base + 5) == 0x01060040u, "pass %d: G_MTX", i);
        CHECK(w0(base + 17) == 0xBD000000u, "pass %d: pop", i);
    }
}

static void test_gate_is_the_whole_function(void)
{
    /* 0x10009C19 tests the model's +0x80BC ONCE, before the loop, and
     * returns without touching the cursor.  A gate that only skipped the
     * G_DL would leave 64 commands behind. */
    fixture(1);
    s_model.dlWheel = 0;
    BrCarDrawWheels(&s_car, &s_model);
    CHECK(dl_count() == 0, "gated: %d commands, want 0", dl_count());
    CHECK(s_nPool == 0, "gated: allocated %d matrices, want 0", s_nPool);
}

static void test_alt_list(void)
{
    int i;
    fixture(1);
    g_BrDrawWheelAlt = 1;
    BrCarDrawWheels(&s_car, &s_model);
    CHECK(dl_count() == 68, "alt: %d commands", dl_count());
    for (i = 0; i < 4 && dl_count() == 68; ++i)
        CHECK(w1(i * 17 + 15) == 0x0DEFACEDu,
              "alt pass %d: %08X", i, w1(i * 17 + 15));

    /* The alt arm has its OWN null test, and it drops only the G_DL --
     * the rest of the pass still runs.  16 commands, not 0 and not 17. */
    fixture(1);
    g_BrDrawWheelAlt = 1;
    s_model.dlWheelAlt = 0;
    BrCarDrawWheels(&s_car, &s_model);
    CHECK(dl_count() == 64, "alt-null: %d commands, want 64", dl_count());
    if (dl_count() == 64)
        CHECK(w0(15) == 0xBD000000u, "alt-null: pop follows the tiles");
}

static void test_frontier_invents_nothing(void)
{
    /* With no pool hook installed the matrices are UNAVAILABLE.  The
     * commands are still emitted -- the original emits them unconditionally
     * -- but the address word is zero and the reach is counted.  Nothing
     * downstream is told a matrix was allocated. */
    fixture(1);
    BrDrawCarSetHooks(0);
    BrCarDrawWheels(&s_car, &s_model);
    CHECK(dl_count() == 68, "frontier: %d commands", dl_count());
    CHECK(BrDrawCarFrontierHits() == 8,
          "frontier: %d reaches, want 8", BrDrawCarFrontierHits());
    if (dl_count() == 68) {
        CHECK(w1(4) == 0, "frontier: no address invented for G_MTX");
        CHECK(w1(5) == 0 && w1(6) == 0,
              "frontier: no address invented for the light blocks");
    }
    BrDrawCarSetHooks(&s_hooks);
}

static void test_mtx_store(void)
{
    uint32_t src[20], dst[20];
    int i;
    for (i = 0; i < 20; ++i) { src[i] = 0xA0000000u + (uint32_t)i; dst[i] = 0; }
    BrGuMtxStore(src, dst);
    for (i = 0; i < 16; ++i)
        CHECK(dst[i] == src[i], "mtx store dword %d", i);
    /* SIXTEEN, not twenty: the nested 4x4 loops walk one cursor. */
    CHECK(dst[16] == 0 && dst[17] == 0, "mtx store must stop at 16 dwords");
    /* Argument order is (source, destination) -- the opposite of memcpy. */
    CHECK(src[0] == 0xA0000000u, "mtx store must not write its source");
}

int main(void)
{
    test_plain_pass();
    test_kind2_pass();
    test_gate_is_the_whole_function();
    test_alt_list();
    test_frontier_invents_nothing();
    test_mtx_store();
    printf("test_br_drawcar: %d failures\n", g_fails);
    return g_fails ? 1 : 0;
}
