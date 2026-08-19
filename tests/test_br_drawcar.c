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
#include "slice2_17.h"

#include <math.h>
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

/* ==================================================================== *
 * BrCarVisibilityUpdate (0x10009FC0)
 *
 * The pass reads five slice2_18 globals and calls BrFogFactorAtPoint and
 * BrSpanTestPoint, both in grab-bag objects.  This suite links br_drawcar
 * alone, so it supplies the globals and stubs the two calls (build.sh's
 * header documents that).  BrVec3Dist/BrVec3MulAdd ARE the real br_vec, so
 * the probe point the mode path forms is checked, not assumed.
 * ==================================================================== */
#include "slice2_21.h"   /* BrSpanVolume, BrSpanTestPoint signature */
#include "br_vec.h"

void   *BrG_6C6490;   /* the object the discarded distance is measured to */
void   *BrG_6C2CF8;   /* the player car */
int32_t BrG_6C661C;   /* mode flag */
int32_t BrG_6C6624;   /* mode flag */
int32_t BrG_6C6614;   /* player-translucent override */

/* Globals BrCarDrawBody (0x1000BEB0) adds.  Same rule: this suite links
 * br_drawcar alone, so it supplies their storage. */
void    *BrG_6C3308;                        /* model scratch ptr           */
void    *BrG_0AA838, *BrG_0AA860, *BrG_0AA868;  /* canned DL / light lists */
void    *g_BrMtxSlot;                       /* projection slot (slice2_19) */
uint32_t BrG_6C0258, BrG_6C0688;            /* othermode payloads          */
float    g_4B16A0, g_4B16AC;                /* scene accumulators (slice2_15) */

/* Light-direction block adds. */
BrVec3   BrG_6C0670;                            /* light dir source vec3    */
void BrVec3NormaliseGuard(BrVec3 *pV) { (void)pV; }

/* Specular block stubs. */
void *BrPool16Alloc(void) { return 0; }
void *BrPool32Alloc(void) { return 0; }
void BrLightDirsFromLookAt(BrMat4 *pM, BrLightPair *pL,
    float a, float b, float c, float d, float e, float f,
    float g, float h, float i)
{ (void)pM;(void)pL;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i; }
void BrLightDirsAndAngles(BrMat4 *pM, BrLightPair *pL, BrSkyAngles *pA,
    float a, float b, float c, float d, float e, float f,
    float g, float h, float i, float j, float k, float l,
    float m, float n, float o, int p, int q)
{ (void)pM;(void)pL;(void)pA;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
  (void)g;(void)h;(void)i;(void)j;(void)k;(void)l;(void)m;(void)n;(void)o;(void)p;(void)q; }

/* Post-detail block stubs. */
static int8_t   s_stubRefTbl[16];
static uint32_t s_stubRefColors[16];

/* Globals BrCarDrawVehicle (0x1000A110) adds. */
uint8_t  BrG_6C0260, BrG_6C1614, BrG_6C0200;  /* fog RGB bytes            */
uint8_t  BrG_6C1580, BrG_6C335C, BrG_6C0968;  /* light RGB bytes          */
uint8_t  BrG_6C0960, BrG_6C65BC;               /* dim G/B bytes            */
int32_t  BrG_6C6618;                            /* mode flag                */
int32_t  BrG_6C1174;                            /* cull ref                 */
int32_t  g_brRaceBeginDifficulty;               /* difficulty ref           */
int32_t  g_brRaceBeginNTexSet;                  /* texture set count        */
int32_t  g_brCfgGameMode;                       /* game mode                */

static BrS17State s_s17State;
BrS17State *BrS17GetState(void) { return &s_s17State; }
int32_t BrBootGlobal_ABAA0(void) { return 0; }

/* The texture command is emitted by a grab-bag object; stub it and record
 * the calls BrCarDrawBody / BrCarDrawVehicle make. */
static int         s_texN;
static int         s_texI;
static const void *s_texRecs;
void BrGfxEmitTexCmd(int i, const void *pRecords)
{
    s_texI = i;
    s_texRecs = pRecords;
    ++s_texN;
}

static const BrVec3 *s_fogPt;
static int           s_fogN;
float BrFogFactorAtPoint(const BrVec3 *pPoint)
{
    s_fogPt = pPoint;
    ++s_fogN;
    return 0.5f;
}

static float s_spanX[8], s_spanY[8];
static int   s_spanN;
static int   s_spanReturn;      /* what the stub answers */
int BrSpanTestPoint(const BrSpanVolume *pVol, float x, float y)
{
    (void)pVol;
    if (s_spanN < 8) { s_spanX[s_spanN] = x; s_spanY[s_spanN] = y; }
    ++s_spanN;
    return s_spanReturn;
}

/* One 0x2B68 car record, plus a tiny object for BrG_6C6490. */
static unsigned char s_visCar[0x2B68];
static unsigned char s_ref[0x40];

#define CARF(off) (*(float *)(s_visCar + (off)))
#define CARP(off) (*(void **)(s_visCar + (off)))

static void car_reset(void)
{
    memset(s_visCar, 0, sizeof s_visCar);
    memset(s_ref, 0, sizeof s_ref);
    CARP(BR_CAR_OFF_GUARD) = s_visCar;          /* non-NULL */
    CARF(BR_CAR_OFF_POS + 0) = 10.0f;        /* pos = (10,20,30) */
    CARF(BR_CAR_OFF_POS + 4) = 20.0f;
    CARF(BR_CAR_OFF_POS + 8) = 30.0f;
    CARF(BR_CAR_OFF_MTX + 0) = 1.0f;         /* mtx.m[0] = (1,0,0) */
    *(int32_t *)(s_visCar + BR_CAR_OFF_ICAR) = 3;
    BrG_6C6490 = s_ref;
    BrG_6C2CF8 = (void *)0xD00D;             /* not our car, by default */
    BrG_6C661C = BrG_6C6624 = BrG_6C6614 = 0;
    s_fogN = s_spanN = 0;
    s_spanReturn = 0;
    g_BrCarVisOpaque[3] = g_BrCarVisAny[3] = 77;   /* sentinels */
}

static void test_vis_guard(void)
{
    car_reset();
    CARP(BR_CAR_OFF_GUARD) = NULL;
    BrCarVisibilityUpdate(s_visCar);
    CHECK(s_fogN == 0, "guard: no fog");
    CHECK(s_spanN == 0, "guard: no span");
    CHECK(g_BrCarVisOpaque[3] == 77 && g_BrCarVisAny[3] == 77, "guard: flags untouched");
}

static void test_vis_opaque_visible(void)
{
    car_reset();
    s_spanReturn = 1;                        /* on screen */
    BrCarVisibilityUpdate(s_visCar);
    CHECK(s_fogN == 1 && CARF(BR_CAR_OFF_FOG) == 0.5f, "opaque: fog written");
    CHECK(s_spanN == 1, "opaque: one span test");
    CHECK(s_spanX[0] == 10.0f && s_spanY[0] == 20.0f, "opaque: span at pos.xy");
    CHECK(g_BrCarVisOpaque[3] == 1 && g_BrCarVisAny[3] == 1, "opaque: both flags set");
}

static void test_vis_culled(void)
{
    car_reset();
    s_spanReturn = 0;                        /* off screen */
    BrCarVisibilityUpdate(s_visCar);
    CHECK(g_BrCarVisOpaque[3] == 0 && g_BrCarVisAny[3] == 0, "culled: both zero");
}

static void test_vis_kind2(void)
{
    car_reset();
    s_spanReturn = 1;
    s_visCar[BR_CAR_OFF_KIND] = 2;              /* translucent class */
    BrCarVisibilityUpdate(s_visCar);
    CHECK(g_BrCarVisOpaque[3] == 0 && g_BrCarVisAny[3] == 1, "kind2: only Any set");
}

static void test_vis_mode_probe(void)
{
    car_reset();
    BrG_6C661C = 1;                          /* mode path: probe 6 units aside */
    s_spanReturn = 1;
    BrCarVisibilityUpdate(s_visCar);
    /* probe = pos + 6*mtx.m[0] = (10,20,30)+(6,0,0) = (16,20,..); the first
     * span test uses the probe, computed by the real BrVec3MulAdd. */
    CHECK(s_spanX[0] == 16.0f && s_spanY[0] == 20.0f, "mode: span at probe.xy");
    CHECK(g_BrCarVisOpaque[3] == 1 && g_BrCarVisAny[3] == 1, "mode: visible");
}

static void test_vis_player_translucent(void)
{
    car_reset();
    BrG_6C2CF8 = s_visCar;                      /* this IS the player car */
    CARP(BR_CAR_OFF_ACTIVECAM) = s_visCar + BR_CAR_OFF_CAMA;  /* points at cam A */
    BrCarVisibilityUpdate(s_visCar);
    CHECK(s_spanN == 0, "player: no span test");
    CHECK(g_BrCarVisOpaque[3] == 0 && g_BrCarVisAny[3] == 1, "player-cam: only Any");
}

static void test_vis_player_fallthrough(void)
{
    car_reset();
    BrG_6C2CF8 = s_visCar;
    CARP(BR_CAR_OFF_ACTIVECAM) = (void *)0xBEEF;   /* neither cam frame */
    BrCarVisibilityUpdate(s_visCar);
    CHECK(g_BrCarVisOpaque[3] == 1 && g_BrCarVisAny[3] == 1, "player-nocam: both set");
}

/* ==================================================================== *
 * BrCarDrawBody (0x1000BEB0)
 *
 * The command payloads and the two combiner words are DERIVED FROM THE
 * DISASSEMBLY, and the glare accumulator values are GOLDEN VECTORS FROM
 * tools/x87emu.py executing the real opcode stream of 0x1000BEB0 out of
 * orig/BRGlide.dll -- not this port's output.  Regenerate with the emulator
 * (scratchpad glow_golden.py), never by copying the port's own numbers back.
 *
 * Combiner #1 tokens (a0..Ad1):
 *   {0,0,0,1, 0,0,0,TEXEL0, 0,0,0,1, 0,0,0,TEXEL0}
 * Combiner #2 tokens:
 *   {TEXEL0,0,PRIM,0} x4
 * Both words are the ones-fill shift chain of 0x1002F900 run by hand with
 * the mux table (CC: 0->31 1->6 TEXEL0->1 PRIM->4; AC: 0->7 1->6 TEXEL0->1
 * PRIM->4).  Written as literals so a change to BrRdpSetCombineLERP -- or a
 * wrong token order here -- FAILS rather than moves with the code.
 * ==================================================================== */
#define BODY_COMBINE1_W0  0xFCFFFFFFu
#define BODY_COMBINE1_W1  0xFFFF73B9u
#define BODY_COMBINE2_W0  0xFC121824u
#define BODY_COMBINE2_W1  0xFF33FFFFu

static unsigned char s_bodyCar[0x2B68];
static unsigned char s_bodyModel[0x8040];
static unsigned char s_bodyCam[0x40];
static unsigned char s_texbuf[4];

#define BCARP(off) (*(void **)(s_bodyCar + (off)))

static void body_reset(uint8_t kind, int withBody)
{
    memset(s_bodyCar, 0, sizeof s_bodyCar);
    memset(s_bodyModel, 0, sizeof s_bodyModel);
    memset(s_bodyCam, 0, sizeof s_bodyCam);

    *(int32_t *)(s_bodyCar + BR_CAR_OFF_ICAR) = 3;
    s_bodyCar[BR_CAR_OFF_KIND] = kind;
    BCARP(BR_CAR_OFF_MODEL) = s_bodyModel;
    *(const void **)(s_bodyModel + BR_MODEL_OFF_TEXRECS) = s_texbuf;
    *(uint32_t *)(s_bodyModel + BR_MODEL_OFF_BODYDL) = withBody ? 0x0B0D0000u : 0;

    BrG_6C661C = 1;                 /* mode flag set -> the pass runs        */
    BrG_6C6624 = 0;
    BrG_6C2CF8 = (void *)0xD00D;    /* not our car: non-player, glow runs    */
    BrG_6C6490 = s_bodyCam;         /* glow dereferences its +0 and +0x30    */
    g_BrCarVisOpaque[3] = 1;        /* marked visible for the opaque pass    */

    g_BrCarMtxSlot[3]   = 0x11110000u;
    g_BrCarLightSlot[3] = 0x22220000u;
    g_BrMtxSlot = (void *)0x0A030303u;
    BrG_0AA838  = (void *)0x0A838838u;
    BrG_0AA860  = (void *)0x0A860860u;
    BrG_0AA868  = (void *)0x0A868868u;
    BrG_6C0258  = 0x02580258u;
    BrG_6C0688  = 0x06880688u;
    g_4B16A0 = g_4B16AC = 0.0f;

    s_texN = 0;
    dl_reset();
}

static void test_body_stream(void)
{
    body_reset(0, 1);               /* opaque car, model carries a body list */
    BrCarDrawBody(s_bodyCar);

    CHECK(dl_count() == 29, "body: %d commands, want 29", dl_count());
    if (dl_count() != 29) return;

    CHECK(w0(0)  == 0x01060040u && w1(0)  == 0x11110000u, "body: model mtx");
    CHECK(w0(1)  == 0x01030040u && w1(1)  == 0x0A030303u, "body: proj mtx");
    CHECK(w0(2)  == 0x039E0010u && w1(2)  == 0x22220000u, "body: light 9E");
    CHECK(w0(3)  == 0x03980010u && w1(3)  == 0x22220010u, "body: light 98");
    CHECK(w0(4)  == 0x039A0010u && w1(4)  == 0x22220020u, "body: light 9A");
    CHECK(w0(5)  == 0x039C0010u && w1(5)  == 0x22220030u, "body: light 9C");
    CHECK(w0(6)  == 0x06000000u && w1(6)  == 0x0A838838u, "body: setup DL");
    CHECK(w0(7)  == 0xE7000000u, "body: pipe sync");
    CHECK(w0(8)  == 0xBA001402u && w1(8) == 0, "body: two-cycle");
    CHECK(w0(9)  == 0xBC00000Au, "body: moveword 00");
    CHECK(w0(10) == 0xBC00040Au, "body: moveword 04");
    CHECK(w0(11) == 0xBC00200Au && w1(11) == 0xFFFFFF00u, "body: prim 20");
    CHECK(w0(12) == 0xBC00240Au && w1(12) == 0xFFFFFF00u, "body: prim 24");
    CHECK(w0(13) == BODY_COMBINE1_W0 && w1(13) == BODY_COMBINE1_W1,
          "body: combine1 %08X %08X", w0(13), w1(13));
    CHECK(w0(14) == 0xB900031Du && w1(14) == 0x004049D8u, "body: render mode");
    CHECK(w0(15) == 0xBA000602u && w1(15) == 0x80u, "body: othermode");
    CHECK(w0(16) == 0x06000000u && w1(16) == 0x0B0D0000u, "body: body geom");
    CHECK(w0(17) == 0xE7000000u, "body: sync 2");
    CHECK(w0(18) == 0xBA000602u && w1(18) == 0x06880688u, "body: othermode 2");
    CHECK(w0(19) == 0xE7000000u, "body: tail sync");
    CHECK(w0(20) == 0xBA001402u, "body: tail two-cycle");
    CHECK(w0(21) == 0xBD000000u, "body: pop mtx");
    CHECK(w0(22) == 0xB6000000u && w1(22) == 0x00040000u, "body: clear geom");
    CHECK(w0(23) == 0xBC000002u && w1(23) == 0x80000040u, "body: moveword tail");
    CHECK(w0(24) == 0x03860010u && w1(24) == 0x0A868868u, "body: light 0");
    CHECK(w0(25) == 0x03880010u && w1(25) == 0x0A860860u, "body: ambient");
    CHECK(w0(26) == 0xBA000C02u && w1(26) == 0x02580258u, "body: othermode C02");
    CHECK(w0(27) == 0xBA000E02u && w1(27) == 0, "body: othermode E02");
    CHECK(w0(28) == BODY_COMBINE2_W0 && w1(28) == BODY_COMBINE2_W1,
          "body: combine2 %08X %08X", w0(28), w1(28));

    CHECK(s_texN == 1 && s_texI == 5 && s_texRecs == (const void *)s_texbuf,
          "body: one tex command (5, model texrecs)");
}

static void test_body_no_bodydl(void)
{
    /* 0x1000C147 tests the model's +0x802C: with no body list the G_DL is
     * dropped and ONLY it -- 28 commands, and index 16 is the sync that
     * followed the (absent) body list, not a G_DL. */
    body_reset(0, 0);
    BrCarDrawBody(s_bodyCar);
    CHECK(dl_count() == 28, "body-nogeom: %d commands, want 28", dl_count());
    if (dl_count() == 28)
        CHECK(w0(16) == 0xE7000000u, "body-nogeom: sync where the geom was");
}

static void test_body_guards(void)
{
    /* Each of the four early returns leaves the cursor untouched. */
    body_reset(0, 1); BrG_6C661C = BrG_6C6624 = 0;
    BrCarDrawBody(s_bodyCar);
    CHECK(dl_count() == 0, "guard mode-flags: %d", dl_count());

    body_reset(0, 1); g_BrCarVisOpaque[3] = 0;
    BrCarDrawBody(s_bodyCar);
    CHECK(dl_count() == 0, "guard not-visible: %d", dl_count());

    body_reset(0, 1);
    BrG_6C2CF8 = s_bodyCar;                          /* this IS the player */
    BrG_6C6490 = s_bodyCar + BR_CAR_OFF_CAMSLOT;     /* camera == +0x27C4  */
    BrCarDrawBody(s_bodyCar);
    CHECK(dl_count() == 0, "guard player-camslot: %d", dl_count());

    body_reset(2, 1);                                /* class 2 (translucent) */
    BrCarDrawBody(s_bodyCar);
    CHECK(dl_count() == 0, "guard kind2: %d", dl_count());
}

/* Golden accumulator values from tools/x87emu.py over 0x1000C1C2..0x1000C38A;
 * dist 9, threshold cleared by 0.05 -> 0.05*750/81. */
#define GLOW_GOLDEN  0.462963074f

static void body_setvec(unsigned off, float x, float y, float z)
{
    *(float *)(s_bodyCar + off + 0) = x;
    *(float *)(s_bodyCar + off + 4) = y;
    *(float *)(s_bodyCar + off + 8) = z;
}
static void cam_setvec(unsigned off, float x, float y, float z)
{
    *(float *)(s_bodyCam + off + 0) = x;
    *(float *)(s_bodyCam + off + 4) = y;
    *(float *)(s_bodyCam + off + 8) = z;
}
static void CHECKF(float a, float b, const char *msg)
{
    if (fabsf(a - b) > 1e-4f) {
        printf("FAIL %s  got %.9g want %.9g\n", msg, (double)a, (double)b);
        ++g_fails;
    }
}

static void test_body_glow(void)
{
    /* FRONT glare -> g_4B16AC.  row0=(-1,0,0), pos=0, cam basis=(1,0,0),
     * campos=(-10,0,0): dir=(-9,0,0), unit=(-1,0,0), dot=-1 (<0), align=1. */
    body_reset(0, 1);
    body_setvec(BR_CAR_OFF_MTX,  -1, 0, 0);
    body_setvec(BR_CAR_OFF_ROW2,  0, 0, 1);
    body_setvec(BR_CAR_OFF_POS,   0, 0, 0);
    cam_setvec(0x00,  1, 0, 0);
    cam_setvec(0x30, -10, 0, 0);
    BrCarDrawBody(s_bodyCar);
    CHECKF(g_4B16AC, GLOW_GOLDEN, "glow front -> g_4B16AC");
    CHECKF(g_4B16A0, 0.0f,        "glow front leaves g_4B16A0");

    /* BACK glare -> g_4B16A0.  row0=(1,0,0), cam basis=(1,0,0),
     * campos=(10,0,0): dir=(9,0,0), unit=(1,0,0), dot=1 (>0.95). */
    body_reset(0, 1);
    body_setvec(BR_CAR_OFF_MTX,   1, 0, 0);
    body_setvec(BR_CAR_OFF_ROW2,  0, 0, 1);
    cam_setvec(0x00,  1, 0, 0);
    cam_setvec(0x30, 10, 0, 0);
    BrCarDrawBody(s_bodyCar);
    CHECKF(g_4B16A0, GLOW_GOLDEN, "glow back -> g_4B16A0");
    CHECKF(g_4B16AC, 0.0f,        "glow back leaves g_4B16AC");

    /* NO glare: camera basis orthogonal to the view direction. */
    body_reset(0, 1);
    body_setvec(BR_CAR_OFF_MTX,  1, 0, 0);
    cam_setvec(0x00, 0, 1, 0);
    cam_setvec(0x30, 0, 10, 0);
    BrCarDrawBody(s_bodyCar);
    CHECKF(g_4B16A0, 0.0f, "glow none -> g_4B16A0 clear");
    CHECKF(g_4B16AC, 0.0f, "glow none -> g_4B16AC clear");

    /* The player's own car never glows (the gate at 0x1000C1B5). */
    body_reset(0, 1);
    BrG_6C2CF8 = s_bodyCar;
    body_setvec(BR_CAR_OFF_MTX, -1, 0, 0);
    cam_setvec(0x00,  1, 0, 0);
    cam_setvec(0x30, -10, 0, 0);
    BrCarDrawBody(s_bodyCar);
    CHECKF(g_4B16AC, 0.0f, "glow player: no accumulate");
}

/* ==================================================================== *
 * BrCarDrawVehicle (0x1000A110) -- NOT CLAIMED, partial transcription.
 *
 * This test exercises the function's STRUCTURAL behaviour: the six guard
 * paths (early returns), matrix allocation, the final combiner word
 * (cross-validated against BrCarDrawBody's combiner #2 -- both emit
 * {TEXEL0,0,PRIM,0}x4 = 0xFC121824 / 0xFF33FFFF), and that the wheel
 * call dispatches (early for class 2, late for non-class 2).
 *
 * IT IS NOT A FULL COMMAND-STREAM GOLDEN-BYTES TEST.  A test whose
 * expected values are derived from the same asm reading that produced the
 * body would prove nothing -- see implements-requires-execution.md for
 * the lesson that cost the 66dbe21 revert.  The combiner words are safe
 * because they cross-validate against siblings; the guard tests are safe
 * because they check dl_count() == 0, not specific bytes.
 * ==================================================================== */

static unsigned char s_vehCar[0x2B68];
static unsigned char s_vehPlayer[0x2B68];
static unsigned char s_vehModel[0x8100];
static unsigned char s_vehCam[0x40];
static unsigned char s_vehTexRecs[40];
static unsigned char s_vehTrackFlags[256];
static unsigned char s_vehAuxFlags[4];

#define VCARP(off)  (*(void **)(s_vehCar + (off)))
#define VCARF(off)  (*(float *)(s_vehCar + (off)))

static void veh_reset(uint8_t kind)
{
    memset(s_vehCar, 0, sizeof s_vehCar);
    memset(s_vehModel, 0, sizeof s_vehModel);
    memset(s_vehCam, 0, sizeof s_vehCam);
    memset(s_vehTexRecs, 0, sizeof s_vehTexRecs);
    memset(s_vehTrackFlags, 0, sizeof s_vehTrackFlags);
    memset(s_vehAuxFlags, 0, sizeof s_vehAuxFlags);

    /* Set ALL guard pointers non-NULL. */
    VCARP(BR_CAR_OFF_GUARD) = s_vehCar;
    VCARP(BR_CAR_OFF_P0168) = s_vehCar;
    VCARP(BR_CAR_OFF_P016C) = s_vehCar;
    VCARP(BR_CAR_OFF_P0170) = s_vehCar;
    VCARP(BR_CAR_OFF_P0174) = s_vehCar;
    VCARP(BR_CAR_OFF_U29C0) = s_vehAuxFlags;

    *(int32_t *)(s_vehCar + BR_CAR_OFF_ICAR) = 2;
    s_vehCar[BR_CAR_OFF_KIND] = kind;
    VCARP(BR_CAR_OFF_MODEL) = s_vehModel;
    *(const void **)(s_vehModel + BR_MODEL_OFF_TEXRECS) = s_vehTexRecs;

    /* Position the car at (10,0,0), camera at origin. */
    VCARF(BR_CAR_OFF_POS + 0) = 10.0f;
    /* Identity-ish world matrix row0 for glass dot test. */
    VCARF(BR_CAR_OFF_MTX + 0) = 1.0f;
    VCARF(BR_CAR_OFF_ROW2 + 8) = 1.0f;

    g_BrCarVisAny[2] = 1;
    BrG_6C6490 = s_vehCam;
    memset(s_vehPlayer, 0, sizeof s_vehPlayer);
    BrG_6C2CF8 = s_vehPlayer;
    BrG_6C661C = 0;
    BrG_6C6624 = 0;
    BrG_6C6614 = 0;
    BrG_6C6618 = 0;
    BrG_6C1174 = 0;
    g_brRaceBeginDifficulty = 0;
    g_brRaceBeginNTexSet = 2;
    g_brCfgGameMode = 1;
    g_BrDrawSuppress = 0;
    g_BrDrawLodFloor = 0;
    g_BrDrawReflectEnable = 0;
    g_BrDrawTrackFlags = s_vehTrackFlags;
    g_BrDrawTexBlob = s_vehTexRecs;
    g_BrDrawModelDlHook = 0;
    g_BrDrawByte80 = 0;
    g_BrDrawByte78 = 0;
    g_BrDrawRefIndex = 0;
    g_BrDrawRefTbl = s_stubRefTbl;
    g_BrDrawRefColors = s_stubRefColors;
    BrG_6C0258 = 0;
    BrG_6C0260 = BrG_6C1614 = BrG_6C0200 = 0;
    BrG_6C1580 = BrG_6C335C = BrG_6C0968 = 0;
    BrG_6C0960 = BrG_6C65BC = 0;
    g_BrMtxSlot = (void *)0x0A030303u;
    BrG_0AA838 = (void *)0x0A838838u;
    BrG_0AA860 = (void *)0x0A860860u;
    BrG_0AA868 = (void *)0x0A868868u;
    s_s17State.f6C161C = 0;
    s_nPool = 0;
    s_texN = 0;
    dl_reset();
    BrDrawCarFrontierReset();
    BrDrawCarSetHooks(&s_hooks);
}

static void test_veh_guard_f08(void)
{
    veh_reset(0);
    VCARP(BR_CAR_OFF_GUARD) = 0;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(dl_count() == 0, "veh guard F08: %d", dl_count());
}

static void test_veh_guard_ptrs(void)
{
    int i;
    const unsigned offsets[] = {
        BR_CAR_OFF_P0168, BR_CAR_OFF_P016C,
        BR_CAR_OFF_P0170, BR_CAR_OFF_P0174
    };
    for (i = 0; i < 4; ++i) {
        veh_reset(0);
        VCARP(offsets[i]) = 0;
        BrCarDrawVehicle(s_vehCar, 0);
        CHECK(dl_count() == 0, "veh guard ptr %d: %d", i, dl_count());
    }
}

static void test_veh_guard_visany(void)
{
    veh_reset(0);
    g_BrCarVisAny[2] = 0;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(dl_count() == 0, "veh guard visany: %d", dl_count());
}

static void test_veh_guard_player_selfview(void)
{
    veh_reset(0);
    BrG_6C2CF8 = s_vehCar;
    VCARP(BR_CAR_OFF_ACTIVECAM) = s_vehCar + BR_CAR_OFF_CAMA;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(s_nPool == 2, "veh player-selfview: allocates matrices before guard");
    CHECK(dl_count() == 0, "veh player-selfview: no commands after guard");
}

static void test_veh_produces_output(void)
{
    veh_reset(0);
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(dl_count() > 20, "veh produces output: %d commands", dl_count());
    CHECK(s_nPool == 2, "veh: 2 pool allocations (model + combined), got %d", s_nPool);
}

static void test_veh_matrix_slots(void)
{
    veh_reset(0);
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(g_BrCarMtxSlot[2] != 0, "veh: model matrix slot filled");
    CHECK(g_BrCarLightSlot[2] != 0, "veh: light matrix slot filled");
    CHECK(g_BrCarMtxSlot[2] != g_BrCarLightSlot[2],
          "veh: model and light slots differ");
}

static void test_veh_final_combiner(void)
{
    int n, i;
    veh_reset(0);
    BrCarDrawVehicle(s_vehCar, 0);
    n = dl_count();
    CHECK(n > 4, "veh final combiner: need commands, got %d", n);
    if (n < 4) return;
    /* The LAST combiner in the stream is the final body combiner at 0xBE64:
     * {TEXEL0,0,PRIM,0}x4.  This cross-validates against BrCarDrawBody's
     * combiner #2 (BODY_COMBINE2_W0/W1). */
    for (i = n - 1; i >= 0; --i) {
        if ((w0(i) & 0xFF000000u) == 0xFC000000u) {
            CHECK(w0(i) == BODY_COMBINE2_W0 && w1(i) == BODY_COMBINE2_W1,
                  "veh final combiner: %08X %08X", w0(i), w1(i));
            break;
        }
    }
    CHECK(i >= 0, "veh: found a combiner word in the stream");
}

static void test_veh_body_header(void)
{
    veh_reset(0);
    BrG_6C0258 = 0xAAAAu;
    BrCarDrawVehicle(s_vehCar, 0);

    CHECK(dl_count() > 15, "body header: %d commands", dl_count());
    if (dl_count() < 15) return;

    /* The first two commands after guards pass are the two G_MTX pushes
     * (model matrix then projection slot). Their payloads are the pool
     * addresses, independently derived from the pool allocator. */
    CHECK(w0(0) == 0x01060040u, "body header: model mtx opcode");
    CHECK(w0(1) == 0x01030040u, "body header: proj mtx opcode");
    CHECK(w1(1) == 0x0A030303u, "body header: proj slot from fixture");

    /* After the light-dir and specular blocks, the lights emit. With
     * both mode flags clear, the static Lights1 path runs. */
    /* The body pass combiner at 0xABF8 matches BrCarDrawWheels' plain
     * combiner: {TEXEL0,0,PRIM,0 / 0,0,0,TEXEL0 / 0,0,0,COMBINED / ...}
     * w0 = 0xFC127FFF, same as COMBINE_W0 in the wheels test. */
    {
        int found = 0, i;
        for (i = 0; i < dl_count(); ++i) {
            if (w0(i) == COMBINE_W0 && w1(i) == COMBINE_W1_PLAIN) {
                found = 1;
                break;
            }
        }
        CHECK(found, "body header: body-pass combiner matches wheels plain");
    }

    /* The BA000C02 othermode carries BrG_6C0258, set by the fixture. */
    {
        int found = 0, i;
        for (i = 0; i < dl_count(); ++i) {
            if (w0(i) == 0xBA000C02u && w1(i) == 0xAAAAu) {
                found = 1;
                break;
            }
        }
        CHECK(found, "body header: othermode carries fixture BrG_6C0258");
    }
}

static void test_veh_lod_class(void)
{
    veh_reset(0);
    g_BrDrawClass[2] = -1;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(g_BrDrawClass[2] == 0, "veh lod: dist 10 -> lod 0, got %d",
          g_BrDrawClass[2]);

    veh_reset(0);
    VCARF(BR_CAR_OFF_POS + 0) = 50.0f;
    g_BrDrawClass[2] = -1;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(g_BrDrawClass[2] == 1, "veh lod: dist 50 -> lod 1, got %d",
          g_BrDrawClass[2]);

    veh_reset(0);
    VCARF(BR_CAR_OFF_POS + 0) = 90.0f;
    g_BrDrawClass[2] = -1;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(g_BrDrawClass[2] == 2, "veh lod: dist 90 -> lod 2, got %d",
          g_BrDrawClass[2]);
}

static void test_veh_lod_bias(void)
{
    veh_reset(0);
    g_BrDrawClass[2] = -1;
    BrCarDrawVehicle(s_vehCar, 1);
    CHECK(g_BrDrawClass[2] == 1, "veh lod bias: 0+1=1, got %d",
          g_BrDrawClass[2]);

    veh_reset(0);
    g_BrDrawClass[2] = -1;
    BrCarDrawVehicle(s_vehCar, 5);
    CHECK(g_BrDrawClass[2] == 2, "veh lod bias: clamped to 2, got %d",
          g_BrDrawClass[2]);
}

static void test_veh_model_cost(void)
{
    veh_reset(0);
    *(int32_t *)(s_vehModel + 0x8000) = 42;
    s_s17State.f6C161C = 100;
    BrCarDrawVehicle(s_vehCar, 0);
    CHECK(s_s17State.f6C161C == 142, "veh model cost: 100+42=%d",
          s_s17State.f6C161C);
}

static void test_veh_wheels_late(void)
{
    int n_no_wheel, n_with_wheel;
    /* Non-class-2: wheels happen LATE (after the body/detail passes).
     * With no wheel DL, the wheel call is a no-op and the count is the
     * same as with one -- but the COUNT of commands differs when wheels
     * have geometry. */
    veh_reset(0);
    *(uint32_t *)(s_vehModel + 0x80BC) = 0;
    BrCarDrawVehicle(s_vehCar, 0);
    n_no_wheel = dl_count();

    veh_reset(0);
    *(uint32_t *)(s_vehModel + 0x80BC) = 0xDEADBEEF;
    BrCarDrawVehicle(s_vehCar, 0);
    n_with_wheel = dl_count();
    CHECK(n_with_wheel > n_no_wheel,
          "veh wheels late: with=%d > without=%d", n_with_wheel, n_no_wheel);
}

static void test_veh_wheels_early_class2(void)
{
    int n_no_wheel, n_with_wheel;
    veh_reset(2);
    *(uint32_t *)(s_vehModel + 0x80BC) = 0;
    BrCarDrawVehicle(s_vehCar, 0);
    n_no_wheel = dl_count();

    veh_reset(2);
    *(uint32_t *)(s_vehModel + 0x80BC) = 0xDEADBEEF;
    BrCarDrawVehicle(s_vehCar, 0);
    n_with_wheel = dl_count();
    CHECK(n_with_wheel > n_no_wheel,
          "veh wheels early class2: with=%d > without=%d",
          n_with_wheel, n_no_wheel);
}

int main(void)
{
    test_plain_pass();
    test_kind2_pass();
    test_gate_is_the_whole_function();
    test_alt_list();
    test_frontier_invents_nothing();
    test_mtx_store();
    test_vis_guard();
    test_vis_opaque_visible();
    test_vis_culled();
    test_vis_kind2();
    test_vis_mode_probe();
    test_vis_player_translucent();
    test_vis_player_fallthrough();
    test_body_stream();
    test_body_no_bodydl();
    test_body_guards();
    test_body_glow();
    test_veh_guard_f08();
    test_veh_guard_ptrs();
    test_veh_guard_visany();
    test_veh_guard_player_selfview();
    test_veh_produces_output();
    test_veh_matrix_slots();
    test_veh_final_combiner();
    test_veh_body_header();
    test_veh_lod_class();
    test_veh_lod_bias();
    test_veh_model_cost();
    test_veh_wheels_late();
    test_veh_wheels_early_class2();
    printf("test_br_drawcar: %d failures\n", g_fails);
    return g_fails ? 1 : 0;
}
