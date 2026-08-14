/* test_slice2_19.c -- behavioural tests for slice2_19.c.
 *
 * These assert properties of the ORIGINAL code rather than restating the
 * implementation: the half-open unsigned rebase window, the opcodes the
 * display-list walker does and does not follow, the asymmetric one-sided
 * clamps, the Q12 fixed-point interpolation with its floor-shift on
 * negatives, the ping-pong reflect and the reverse-bit handshake, the
 * N64-button-to-engine-bit map, the degenerate single-keyframe bracket, and
 * the exact set of fields the model byte-swapper touches.
 *
 * Every cross-slice callee is stubbed HERE, clearly marked. The vector
 * stand-ins implement br_vec.h's documented semantics so the composition in
 * BrCamFrustumBuild can be checked; they are not decompiled code.
 */
#include "slice2_19.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            g_fail++; \
        } \
    } while (0)

static int approx(float a, float b)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= 1e-4f;
}

/* ================================================================== */
/* Cross-slice stand-ins (TEST ONLY -- not decompiled code)            */
/* ================================================================== */

void BrVec3Add(BrVec3 *o, const BrVec3 *a, const BrVec3 *b)
{ o->x = a->x + b->x; o->y = a->y + b->y; o->z = a->z + b->z; }
void BrVec3Sub(BrVec3 *o, const BrVec3 *a, const BrVec3 *b)
{ o->x = a->x - b->x; o->y = a->y - b->y; o->z = a->z - b->z; }
void BrVec3AddTo(BrVec3 *a, const BrVec3 *b)
{ a->x += b->x; a->y += b->y; a->z += b->z; }
void BrVec3SubFrom(BrVec3 *a, const BrVec3 *b)
{ a->x -= b->x; a->y -= b->y; a->z -= b->z; }
void BrVec3Scale(BrVec3 *o, const BrVec3 *v, float s)
{ o->x = v->x * s; o->y = v->y * s; o->z = v->z * s; }
void BrVec3MulAdd(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float s)
{ o->x = a->x + b->x * s; o->y = a->y + b->y * s; o->z = a->z + b->z * s; }
/* br_vec.h: out = (a - b) * t + b */
void BrVec3Lerp(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float t)
{ o->x = (a->x - b->x) * t + b->x;
  o->y = (a->y - b->y) * t + b->y;
  o->z = (a->z - b->z) * t + b->z; }

/* br_mat.h: SOURCE FIRST. */
void BrMat4Copy(const BrMat4 *pSrc, BrMat4 *pDst) { *pDst = *pSrc; }

static BrMat4 g_stubMulOut;
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{ (void)pA; (void)pB; *pOut = g_stubMulOut; }

static float g_stubSlot[16];
void *BrPoolAlloc(BrPool *pPool) { (void)pPool; return g_stubSlot; }

static uint32_t g_stubSegN64, g_stubSegHost;
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase)
{ (void)pMap; g_stubSegN64 = n64Base; g_stubSegHost = hostBase; }

/* 0x10002240 -- unknown; the test needs only a known monotone function. */
float BrSub10002240(float x) { return x * 2.0f; }

static float g_lookAt[10];
void BrMat4LookAt(BrMat4 *pM, float ex, float ey, float ez,
                  float ax, float ay, float az,
                  float ux, float uy, float uz)
{
    (void)pM;
    g_lookAt[0] = ex; g_lookAt[1] = ey; g_lookAt[2] = ez;
    g_lookAt[3] = ax; g_lookAt[4] = ay; g_lookAt[5] = az;
    g_lookAt[6] = ux; g_lookAt[7] = uy; g_lookAt[8] = uz;
}

static float g_persp[5];
int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm, float fovy,
                       float aspect, float n, float f, float scale)
{
    (void)pM;
    *pPerspNorm = 1;                    /* what the original hardcodes */
    g_persp[0] = fovy; g_persp[1] = aspect;
    g_persp[2] = n;    g_persp[3] = f;   g_persp[4] = scale;
    return 0;
}

static int      g_dlFixupRet;
static uint32_t *g_dlFixupArg;
int BrSub100341B3(uint32_t *pDl, const void *pTable)
{ (void)pTable; g_dlFixupArg = pDl; return g_dlFixupRet; }

static int g_hookRet;
int BrHookIsCurrent(const void *pfn) { (void)pfn; return g_hookRet; }

static const void *g_logLast;
static int g_logCount;
void BrLogPrint(const void *p) { g_logLast = p; g_logCount++; }

static int g_loadCount;
void BrSub10037740(void *pCar, void *pArg) { (void)pCar; (void)pArg; g_loadCount++; }
static int g_postCount;
void BrSub1003551B(void *pCar) { (void)pCar; g_postCount++; }

static void *g_mgrA, *g_mgrB, *g_mgrThis, *g_mgrRet;
void *BrSub100088B0(void *pThis, void *a, void *b)
{ g_mgrThis = pThis; g_mgrA = a; g_mgrB = b; return g_mgrRet; }

static uint32_t *g_vtxSlot;
static int       g_vtxCount;
void BrModelVtxResolve(uint32_t *pSlot, int count)
{ g_vtxSlot = pSlot; g_vtxCount = count; }

static int g_bf80Count, g_dc0Count, g_dc0Arg;
void BrSub1002BF80(uint32_t v) { (void)v; g_bf80Count++; }
void BrSub10074DC0(int n) { g_dc0Arg = n; g_dc0Count++; }

/* ------------------------------------------------------------------ */

static uint32_t g_submitted[16];
static int      g_submitCount;
static void SubmitStub(uint32_t dl)
{ if (g_submitCount < 16) g_submitted[g_submitCount] = dl; g_submitCount++; }

static int g_submitBCount;
static void SubmitBStub(uint32_t p) { (void)p; g_submitBCount++; }

/* The model image uses byte offsets as "addresses"; deref adds the base. */
static unsigned char *g_imgBase;
static int g_fixupCount;
static void ModelFixupStub(uint32_t *pSlot) { (void)pSlot; g_fixupCount++; }
static void *ModelDerefStub(uint32_t v) { return g_imgBase + v; }

/* ================================================================== */
/* 2. Display-list segment fixup                                      */
/* ================================================================== */

static void test_rebase_window(void)
{
    uint32_t w;

    /* Half-open: lo is inside, hi is not. */
    w = 0x1000u; BrDlRebaseWord(&w, 0x1000u, 0x2000u, 0x9000u);
    CHECK(w == 0x9000u);
    w = 0x2000u; BrDlRebaseWord(&w, 0x1000u, 0x2000u, 0x9000u);
    CHECK(w == 0x2000u);                       /* top end excluded */
    w = 0x0FFFu; BrDlRebaseWord(&w, 0x1000u, 0x2000u, 0x9000u);
    CHECK(w == 0x0FFFu);                       /* below: left ALONE, not 0 */

    /* The comparisons are unsigned: a word with the top bit set is ABOVE
     * everything, not below it. */
    w = 0x80000000u; BrDlRebaseWord(&w, 0x1000u, 0x2000u, 0x9000u);
    CHECK(w == 0x80000000u);
    w = 0x80000000u; BrDlRebaseWord(&w, 0x80000000u, 0x90000000u, 0u);
    CHECK(w == 0u);
}

static void test_dl_walk(void)
{
    uint32_t dl[12];
    int i;

    for (i = 0; i < 12; i++) dl[i] = 0;
    dl[0] = 0x04000000u; dl[1] = 0x1010u;      /* G_VTX     -> rebased  */
    dl[2] = 0x06000000u; dl[3] = 0x1010u;      /* G_DL      -> ignored  */
    dl[4] = 0xFD000000u; dl[5] = 0x1010u;      /* G_SETTIMG -> rebased  */
    dl[6] = 0xE7000000u; dl[7] = 0x1010u;      /* other     -> ignored  */
    dl[8] = 0xB8000000u; dl[9] = 0x1010u;      /* G_ENDDL   -> stop     */
    dl[10] = 0x04000000u; dl[11] = 0x1010u;    /* past the end          */

    BrDlRebase(dl, 0x1000u, 0x2000u, 0x9000u);

    CHECK(dl[1] == 0x9010u);
    CHECK(dl[3] == 0x1010u);
    CHECK(dl[5] == 0x9010u);
    CHECK(dl[7] == 0x1010u);
    CHECK(dl[9] == 0x1010u);
    CHECK(dl[11] == 0x1010u);                  /* walk stopped at G_ENDDL */

    BrDlRebase(NULL, 0, 1, 2);                 /* must not crash */
}

static void test_dl_owner(void)
{
    BrDlOwner o;

    /* flags bit 2 set suppresses the global write -- and it stays 0, it is
     * not left at its previous value. */
    g_Br6C666C = 0x55;
    g_Br0B380C = 0;                            /* -> want = 1 */
    memset(&o, 0, sizeof(o));
    o.flags = 4;
    g_dlFixupRet = 0;
    BrDlOwnerFixup(&o);
    CHECK(g_Br6C666C == 0);
    CHECK(o.flags == 4);

    memset(&o, 0, sizeof(o));
    BrDlOwnerFixup(&o);
    CHECK(g_Br6C666C == 1);

    g_Br0B380C = 8;                            /* 2 and 8 both give want=0 */
    memset(&o, 0, sizeof(o));
    BrDlOwnerFixup(&o);
    CHECK(g_Br6C666C == 0);

    g_dlFixupRet = 1;
    memset(&o, 0, sizeof(o));
    BrDlOwnerFixup(&o);
    CHECK((o.flags & 8u) != 0);
}

/* ================================================================== */
/* Bit split                                                          */
/* ================================================================== */

static void test_bit_split(void)
{
    BrBitPair p;
    uint32_t a0 = 0xF0F0F0F0u, b0 = 0x00FFFF00u;

    p.a = a0; p.b = b0;
    BrBitEdgeSplit(&p);

    /* The two outputs partition the ORIGINAL a exactly. */
    CHECK((p.a | p.b) == a0);
    CHECK((p.a & p.b) == 0u);
    CHECK(p.b == (a0 & b0));

    /* Both fields come from the pre-call values, so a second call cannot
     * move anything back: b drains to zero and a is a fixed point. */
    {
        uint32_t a1 = p.a;
        BrBitEdgeSplit(&p);
        CHECK(p.a == a1);
        CHECK(p.b == 0u);
    }
}

/* ================================================================== */
/* One-sided clamps                                                   */
/* ================================================================== */

static void test_accum_clamp(void)
{
    float t[4];

    t[0] = 0.0f;
    BrAccumAddClamp(t, 0, 1.0f);
    CHECK(approx(t[0], 1.0f));

    /* amt is clamped only from ABOVE. */
    t[1] = 0.0f;
    BrAccumAddClamp(t, 1, 100.0f);
    CHECK(approx(t[1], 2.5f));

    /* ...and not at all from below: a big negative passes straight through
     * and the accumulator is free to run arbitrarily negative. */
    t[2] = 0.0f;
    BrAccumAddClamp(t, 2, -100.0f);
    CHECK(approx(t[2], -100.0f));
    BrAccumAddClamp(t, 2, -100.0f);
    CHECK(approx(t[2], -200.0f));

    /* The accumulator saturates only at the top. */
    t[3] = 4.9f;
    BrAccumAddClamp(t, 3, 1.0f);
    CHECK(approx(t[3], 5.0f));
}

/* ================================================================== */
/* 4. Keyframe animation                                              */
/* ================================================================== */

/* One keyframe with cVerts == 1: float t, 3 int16, 3 int8. */
static BrAnimKey *MakeKey(float t, int x, int y, int z, int nx, int ny, int nz)
{
    unsigned char *p = (unsigned char *)malloc(4 + 3 * 2 + 3 * 1);
    int16_t s[3];
    int8_t  c[3];

    memcpy(p, &t, 4);
    s[0] = (int16_t)x; s[1] = (int16_t)y; s[2] = (int16_t)z;
    c[0] = (int8_t)nx; c[1] = (int8_t)ny; c[2] = (int8_t)nz;
    memcpy(p + 4, s, 6);
    memcpy(p + 10, c, 3);
    return (BrAnimKey *)p;
}

static BrAnimTrack *g_track;
static BrAnimVtx    g_vtx[1];
static BrAnimList  *g_list;
static BrAnimSet    g_set;

static void BuildTrack(int hiX, int hiY, int hiZ, int hiN)
{
    if (g_track == NULL) {
        g_track = (BrAnimTrack *)malloc(sizeof(BrAnimTrack) +
                                        3 * sizeof(BrAnimKey *));
        g_list  = (BrAnimList *)malloc(sizeof(BrAnimList) +
                                       3 * sizeof(BrAnimTrack *));
    }
    memset(g_track, 0, sizeof(BrAnimTrack));
    g_track->cVerts = 1;
    g_track->pOut   = g_vtx;
    g_track->cKeys  = 2;
    g_track->tLo    = 0.0f;
    g_track->tHi    = 1.0f;
    g_track->aKeys[0] = MakeKey(0.0f, 0, 0, 0, 0, 0, 0);
    g_track->aKeys[1] = MakeKey(1.0f, hiX, hiY, hiZ, hiN, 0, 0);

    g_list->n = 1;
    g_list->a[0] = g_track;
    g_set.pList = g_list;

    memset(g_vtx, 0, sizeof(g_vtx));
}

static void test_anim_flags(void)
{
    BuildTrack(0, 0, 0, 0);

    g_track->flags = 0;
    BrAnimSetPingPong(&g_set);
    /* GOTCHA: clearBits == 0 becomes ~0, truncated to 0xFFFF, so this sets
     * bits 0 and 1 and clears NOTHING -- including bit 2. */
    g_track->flags = 4;
    BrAnimSetPingPong(&g_set);
    CHECK(g_track->flags == (4u | 1u | 2u));

    g_track->flags = 7;
    BrAnimSetOnce(&g_set);
    CHECK(g_track->flags == 4u);               /* bits 0,1 cleared, 2 kept */

    g_track->flags = 6;
    BrAnimSetLoop(&g_set);
    CHECK(g_track->flags == (4u | 1u));        /* bit 0 set, bit 1 cleared */

    /* A null list is a no-op, not a crash. */
    {
        BrAnimSet empty;
        empty.f00 = 0;
        empty.pList = NULL;
        BrAnimFlagsApply(&empty, 1, 1);
        BrAnimUpdate(&empty);
    }
}

static void test_anim_fixed_point(void)
{
    /* Exact Q12: frac = (int)(0.5 * 4096) = 2048, out = (delta*2048) >> 12. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 1;
    g_track->t = 0.0f;
    g_BrAnimDt = 0.5f;
    g_BrK08F530 = 1.0f / 128.0f;

    BrAnimUpdate(&g_set);

    CHECK(approx(g_track->t, 0.5f));
    CHECK(approx(g_vtx[0].x, 50.0f));
    CHECK(approx(g_vtx[0].y, 100.0f));
    CHECK(approx(g_vtx[0].z, 150.0f));
    CHECK(approx(g_vtx[0].nx, 32.0f / 128.0f));
    /* +0x0C and +0x10 are never written. */
    CHECK(g_vtx[0].f0C == 0.0f && g_vtx[0].f10 == 0.0f);

    /* The shift is ARITHMETIC (sar), so it floors rather than truncating
     * toward zero: half of -1 comes out as -1, not 0. */
    BuildTrack(-1, -3, -4096, 0);
    g_track->flags = 1;
    g_track->t = 0.0f;
    BrAnimUpdate(&g_set);
    CHECK(approx(g_vtx[0].x, -1.0f));          /* (-1*2048)>>12 == -1 */
    CHECK(approx(g_vtx[0].y, -2.0f));          /* (-3*2048)>>12 == -2 */
    CHECK(approx(g_vtx[0].z, -2048.0f));
}

static void test_anim_pingpong_and_reverse(void)
{
    /* Past the end, looping + ping-pong: reflect about tHi and turn the
     * reverse bit ON. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 1u | 2u;
    g_track->t = 0.9f;
    g_BrAnimDt = 0.5f;
    g_BrK08F534 = 0.5f;
    BrAnimUpdate(&g_set);
    CHECK((g_track->flags & 4u) != 0);
    CHECK(approx(g_track->t, 0.6f));           /* 2*tHi - 1.4 */
    CHECK(g_track->iKey == 0);

    /* Running in reverse off the low end with looping on: reflect about tLo
     * and turn the reverse bit back OFF. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 1u | 4u;
    g_track->t = 0.2f;
    BrAnimUpdate(&g_set);
    CHECK((g_track->flags & 4u) == 0);
    CHECK((g_track->flags & 1u) != 0);
    CHECK(approx(g_track->t, 0.3f));
    CHECK(approx(g_vtx[0].x, 29.0f));          /* frac = (int)(0.3*4096) */

    /* Running in reverse off the low end WITHOUT looping: the track is left
     * alone entirely -- time is not clamped back, it just stops being
     * processed. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 4u;
    g_track->t = 0.2f;
    BrAnimUpdate(&g_set);
    CHECK((g_track->flags & 4u) != 0);
    CHECK(approx(g_track->t, -0.3f));
    CHECK(g_vtx[0].x == 0.0f);                 /* nothing written */
}

static void test_anim_degenerate_bracket(void)
{
    /* Past the end and NOT looping: both brackets collapse onto the last
     * keyframe, the parameter divides by zero, and the output is exactly
     * that keyframe because every delta is zero. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 0;
    g_track->t = 0.9f;
    g_BrAnimDt = 0.5f;
    BrAnimUpdate(&g_set);
    CHECK(approx(g_vtx[0].x, 100.0f));
    CHECK(approx(g_vtx[0].y, 200.0f));
    CHECK(approx(g_vtx[0].z, 300.0f));
    CHECK(approx(g_vtx[0].nx, 64.0f / 128.0f));

    /* Before the start, forward: the same collapse onto keyframe 0. */
    BuildTrack(100, 200, 300, 64);
    g_track->flags = 0;
    g_track->t = -10.0f;
    g_BrAnimDt = 0.5f;
    BrAnimUpdate(&g_set);
    CHECK(g_vtx[0].x == 0.0f);
    CHECK(g_vtx[0].y == 0.0f);
}

/* ================================================================== */
/* 5. Controller translation                                          */
/* ================================================================== */

static BrPad    g_pad;
static BrPadRaw g_raw;
static unsigned char g_modeBytes[8];

static void PadReset(void)
{
    memset(&g_pad, 0, sizeof(g_pad));
    memset(&g_raw, 0, sizeof(g_raw));
    memset(g_modeBytes, 0, sizeof(g_modeBytes));
    g_pad.pRaw = &g_raw;
    g_BrPadModeBytes = g_modeBytes;
    g_Br6909B4 = 0;
    g_hookRet = 1;
}

static void PadSetButtons(unsigned n64mask)
{
    g_raw.b0 = (uint8_t)(n64mask & 0xFFu);
    g_raw.b1 = (uint8_t)((n64mask >> 8) & 0xFFu);
}

static void test_pad_button_map(void)
{
    static const struct { unsigned n64; uint32_t out; } map[] = {
        { 0x0800u, BR_PAD_DUP    }, { 0x0400u, BR_PAD_DDOWN  },
        { 0x0200u, BR_PAD_DLEFT  }, { 0x0100u, BR_PAD_DRIGHT },
        { 0x8000u, BR_PAD_A      }, { 0x4000u, BR_PAD_B      },
        { 0x0020u, BR_PAD_L      }, { 0x0010u, BR_PAD_R      },
        { 0x2000u, BR_PAD_Z      }, { 0x1000u, BR_PAD_START  },
        { 0x0008u, BR_PAD_CUP    }, { 0x0001u, BR_PAD_CRIGHT },
        { 0x0004u, BR_PAD_CDOWN  }, { 0x0002u, BR_PAD_CLEFT  }
    };
    size_t i;

    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        PadReset();
        g_hookRet = 0;                 /* keep the derived bits out of it */
        PadSetButtons(map[i].n64);
        BrPadTranslate(&g_pad);
        CHECK(g_pad.buttons == map[i].out);
    }

    /* The map is a bijection over those fourteen bits. */
    PadReset();
    g_hookRet = 0;
    PadSetButtons(0xFFFFu);
    BrPadTranslate(&g_pad);
    CHECK(g_pad.buttons == 0xFF3Fu);
}

static void test_pad_error_neutralises(void)
{
    PadReset();
    PadSetButtons(0xFFFFu);
    g_raw.stickX = 100;
    g_raw.stickY = -100;
    g_raw.status = 8;

    BrPadTranslate(&g_pad);

    /* The RAW record itself is wiped before it is read. */
    CHECK(g_raw.b0 == 0 && g_raw.b1 == 0);
    CHECK(g_raw.stickX == 0 && g_raw.stickY == 0);
    CHECK(g_pad.buttons == 0);
    CHECK(g_pad.f28 == 1);             /* status == 8 is the special one */

    PadReset();
    g_raw.status = 3;
    BrPadTranslate(&g_pad);
    CHECK(g_pad.f28 == 0);
}

static void test_pad_steering(void)
{
    PadReset();
    PadSetButtons(0x0200u);            /* D-left alone */
    BrPadTranslate(&g_pad);
    CHECK(g_pad.steer == (int8_t)-80);

    PadReset();
    PadSetButtons(0x0100u);            /* D-right alone */
    BrPadTranslate(&g_pad);
    CHECK(g_pad.steer == (int8_t)80);

    PadReset();
    PadSetButtons(0x0300u);            /* both -> cancel */
    BrPadTranslate(&g_pad);
    CHECK(g_pad.steer == 0);

    PadReset();
    BrPadTranslate(&g_pad);            /* neither -> also 0 */
    CHECK(g_pad.steer == 0);

    /* Either probe byte with bit 7 set switches to the analog stick. The
     * two bytes are +1 and +7, NOT adjacent. */
    PadReset();
    g_modeBytes[7] = 0x80;
    g_raw.stickX = 33;
    PadSetButtons(0x0200u);
    BrPadTranslate(&g_pad);
    CHECK(g_pad.steer == 33);

    PadReset();
    g_modeBytes[2] = 0x80;             /* +2 is NOT one of the probes */
    g_raw.stickX = 33;
    PadSetButtons(0x0200u);
    BrPadTranslate(&g_pad);
    CHECK(g_pad.steer == (int8_t)-80);
}

static void test_pad_derived_bits_and_ramp(void)
{
    /* All the derived bits are gated on the hook check. */
    PadReset();
    g_hookRet = 0;
    PadSetButtons(0x8000u | 0x4000u | 0x0020u);
    BrPadTranslate(&g_pad);
    CHECK((g_pad.buttons & 0x00FF0000u) == 0);

    PadReset();
    PadSetButtons(0x8000u | 0x4000u);   /* A and B */
    g_raw.stickY = -70;                 /* not past -64... it IS past */
    BrPadTranslate(&g_pad);
    CHECK((g_pad.buttons & BR_PAD_A_D) != 0);
    CHECK((g_pad.buttons & BR_PAD_A_BACK) != 0);
    CHECK((g_pad.buttons & BR_PAD_B_A) != 0);      /* B with A */
    CHECK((g_pad.buttons & BR_PAD_B_ALT) == 0);

    PadReset();
    PadSetButtons(0x4000u);             /* B alone */
    BrPadTranslate(&g_pad);
    CHECK((g_pad.buttons & BR_PAD_B_ALT) != 0);
    CHECK((g_pad.buttons & BR_PAD_B_A) == 0);

    PadReset();
    PadSetButtons(0x8000u);
    g_raw.stickY = -64;                 /* boundary: NOT past, signed < -64 */
    BrPadTranslate(&g_pad);
    CHECK((g_pad.buttons & BR_PAD_A_BACK) == 0);

    /* The ramp: both enables clear -> nothing moves. */
    PadReset();
    g_pad.f34 = 0; g_pad.f3C = 5;
    g_pad.f38 = 0; g_pad.f40 = 5;
    BrPadTranslate(&g_pad);
    CHECK(g_pad.f34 == 0 && g_pad.f38 == 0);

    /* One enable set -> both ramps are considered, each on its own enable. */
    PadReset();
    g_pad.f2C = 1;
    g_pad.f34 = 0; g_pad.f3C = 5;
    g_pad.f38 = 0; g_pad.f40 = 5;
    BrPadTranslate(&g_pad);
    CHECK(g_pad.f34 == 2);
    CHECK(g_pad.f38 == 0);

    /* Step is 2 and the test is `<`, so it overshoots the limit by one. */
    PadReset();
    g_pad.f2C = 1;
    g_pad.f34 = 4; g_pad.f3C = 5;
    BrPadTranslate(&g_pad);
    CHECK(g_pad.f34 == 6);

    /* g_Br6909B4 non-zero freezes the ramp. */
    PadReset();
    g_pad.f2C = 1;
    g_pad.f34 = 0; g_pad.f3C = 5;
    g_Br6909B4 = 1;
    BrPadTranslate(&g_pad);
    CHECK(g_pad.f34 == 0);
    g_Br6909B4 = 0;
}

static void test_pad_axes(void)
{
    PadReset();
    g_BrK08F548 = 1.0f / 80.0f;
    g_raw.stickX = 40;
    g_raw.stickY = -40;
    BrPadTranslate(&g_pad);
    CHECK(approx(g_pad.axisX,  0.5f));
    CHECK(approx(g_pad.axisY, -0.5f));

    /* Saturation, both ends, on all three axes. */
    PadReset();
    g_raw.stickX = 127;
    g_raw.stickY = -128;
    PadSetButtons(0x0200u);            /* steer -80 -> exactly -1 */
    BrPadTranslate(&g_pad);
    CHECK(approx(g_pad.axisX,  1.0f));
    CHECK(approx(g_pad.axisY, -1.0f));
    CHECK(approx(g_pad.axisSteer, -1.0f));
}

/* ================================================================== */
/* 6. Model byte-swap                                                 */
/* ================================================================== */

static void PutBe32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}
static void PutBe16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;
}
static uint32_t Ld32(const unsigned char *p)
{ uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t Ld16(const unsigned char *p)
{ uint16_t v; memcpy(&v, p, 2); return v; }

static void test_model_swap(void)
{
    static uint32_t raw[64];
    unsigned char *img = (unsigned char *)raw;

    memset(raw, 0, sizeof(raw));
    g_imgBase = img;
    g_BrModelFixup = ModelFixupStub;
    g_BrModelDeref = ModelDerefStub;
    g_BrGfxSubmitB = SubmitBStub;
    g_fixupCount = g_bf80Count = g_dc0Count = g_submitBCount = 0;
    g_vtxSlot = NULL; g_vtxCount = -1;

    /* header */
    PutBe16(img + 0x00, 0x1234u);
    PutBe16(img + 0x02, 1u);              /* one record */
    PutBe32(img + 0x04, 0x20u);           /* -> block   */

    /* record 0 at +0x08, stride 0x14 */
    PutBe32(img + 0x08, 0x00ABCDEFu);
    PutBe16(img + 0x0C, 0x1122u);
    PutBe16(img + 0x0E, 0x3344u);
    PutBe32(img + 0x10, 0x55667788u);
    PutBe32(img + 0x14, 0x99AABBCCu);
    PutBe32(img + 0x18, 0xDDEEFF00u);

    /* block at +0x20 */
    PutBe32(img + 0x20, 1u);              /* one item */
    PutBe32(img + 0x24, 0x30u);           /* -> item  */

    /* item at +0x30 */
    PutBe32(img + 0x30, 1u);              /* m = 1  */
    PutBe32(img + 0x34, 0x11u);
    PutBe32(img + 0x38, 0x22u);
    PutBe32(img + 0x3C, 1u);              /* k = 1  */
    PutBe16(img + 0x40, 0xAABBu);
    PutBe16(img + 0x42, 0xCCDDu);
    PutBe32(img + 0x44, 0x01020304u);
    PutBe32(img + 0x48, 0x05060708u);
    PutBe32(img + 0x4C, 0x090A0B0Cu);
    PutBe32(img + 0x50, 0x70u);           /* -> leaf */

    /* leaf at +0x70: a dword then 3*m halfwords */
    PutBe32(img + 0x70, 0xDEADBEEFu);
    PutBe16(img + 0x74, 0x0102u);
    PutBe16(img + 0x76, 0x0304u);
    PutBe16(img + 0x78, 0x0506u);
    PutBe16(img + 0x7A, 0x0708u);         /* one past -- must NOT be touched */

    BrModelSwap(img);

    /* header halfwords are swapped independently */
    CHECK(Ld16(img + 0x00) == 0x1234u);
    CHECK(Ld16(img + 0x02) == 1u);

    /* block / item counts became host-order */
    CHECK(Ld32(img + 0x20) == 1u);
    CHECK(Ld32(img + 0x30) == 1u);
    CHECK(Ld32(img + 0x3C) == 1u);
    CHECK(Ld16(img + 0x40) == 0xAABBu);
    CHECK(Ld16(img + 0x42) == 0xCCDDu);
    CHECK(Ld32(img + 0x44) == 0x01020304u);
    CHECK(Ld32(img + 0x48) == 0x05060708u);
    CHECK(Ld32(img + 0x4C) == 0x090A0B0Cu);

    /* leaf: dword plus exactly 3*m halfwords, no more */
    CHECK(Ld32(img + 0x70) == 0xDEADBEEFu);
    CHECK(Ld16(img + 0x74) == 0x0102u);
    CHECK(Ld16(img + 0x76) == 0x0304u);
    CHECK(Ld16(img + 0x78) == 0x0506u);
    CHECK(img[0x7A] == 0x07 && img[0x7B] == 0x08);   /* untouched */

    /* record 0 */
    CHECK(Ld32(img + 0x08) == 0x00ABCDEFu);
    CHECK(Ld16(img + 0x0C) == 0x1122u);
    CHECK(Ld16(img + 0x0E) == 0x3344u);
    CHECK(Ld32(img + 0x10) == 0x55667788u);
    CHECK(Ld32(img + 0x14) == 0x99AABBCCu);
    CHECK(Ld32(img + 0x18) == 0xDDEEFF00u);

    /* the vertex-cache call gets the SLOT and the item's own count */
    CHECK(g_vtxSlot == (uint32_t *)(img + 0x34));
    CHECK(g_vtxCount == 1);

    /* header +4, block slot, item +4, item +8, leaf slot, record 0 */
    CHECK(g_fixupCount == 6);
    CHECK(g_bf80Count == 1);
    CHECK(g_dc0Count == 1 && g_dc0Arg == 8);
    CHECK(g_submitBCount == 1);
}

static void test_model_swap_null_block(void)
{
    static uint32_t raw[16];
    unsigned char *img = (unsigned char *)raw;

    memset(raw, 0, sizeof(raw));
    g_imgBase = img;
    g_BrModelFixup = ModelFixupStub;
    g_BrModelDeref = ModelDerefStub;
    g_BrGfxSubmitB = SubmitBStub;
    g_fixupCount = g_bf80Count = g_dc0Count = g_submitBCount = 0;

    PutBe16(img + 0x02, 1u);
    PutBe32(img + 0x04, 0u);              /* no block */
    PutBe32(img + 0x08, 0x00ABCDEFu);     /* but a live record */

    BrModelSwap(img);

    /* The record half runs regardless of the block half. */
    CHECK(Ld32(img + 0x08) == 0x00ABCDEFu);
    CHECK(g_fixupCount == 1);
    CHECK(g_submitBCount == 1);

    /* A record whose pointer word is zero is skipped entirely. */
    memset(raw, 0, sizeof(raw));
    g_fixupCount = g_submitBCount = 0;
    PutBe16(img + 0x02, 1u);
    BrModelSwap(img);
    CHECK(g_fixupCount == 0);
    CHECK(g_submitBCount == 0);
}

static void test_model_load_arg_order(void)
{
    static int a, b;
    static uint32_t result[8];

    g_mgrRet = result;
    g_BrModelFixup = ModelFixupStub;
    g_BrModelDeref = ModelDerefStub;
    g_BrGfxSubmitB = SubmitBStub;
    memset(result, 0, sizeof(result));

    /* An all-zero image: BrModelSwap swaps the two header halfwords, finds a
     * null block pointer and a zero record count, and does nothing else. */
    CHECK(BrModelLoad((void *)0x1234, &a, &b) == result);
    CHECK(g_mgrThis == (void *)0x1234);
    CHECK(g_mgrA == &b);                 /* REVERSED */
    CHECK(g_mgrB == &a);
    CHECK(g_stubSegN64 == 0u);
    CHECK(g_stubSegHost == (uint32_t)(uintptr_t)result);
}

/* ================================================================== */
/* 1. Camera                                                          */
/* ================================================================== */

static void test_frustum_corners(void)
{
    BrCamBasis cam;

    memset(&cam, 0, sizeof(cam));
    cam.eye.x = 10.0f; cam.eye.y = 20.0f; cam.eye.z = 30.0f;
    cam.fwd.z = 1.0f;
    cam.right.x = 1.0f;
    cam.up.y = 1.0f;

    g_BrCamMode = 0;
    /* a = BrSub10002240(a2)*a3 = (2*a2)*a3; b = a*a5/a4 */
    BrCamFrustumBuild(&cam, 0.5f, 100.0f, 4.0f, 3.0f);

    CHECK(approx(g_BrCamEye.x, 10.0f));
    CHECK(approx(g_BrCamCentre.z, 130.0f));       /* eye + fwd*100 */
    CHECK(approx(g_BrCamCentreCopy.z, 130.0f));
    CHECK(approx(g_BrCamExtentR.x, 100.0f));      /* a = 1.0*100     */
    CHECK(approx(g_BrCamExtentU.y, 75.0f));       /* b = 100*3/4     */

    /* The four corners are still a parallelogram after the 0.75 pull toward
     * the eye, because that pull is affine. */
    CHECK(approx(g_BrCamCorner0.x + g_BrCamCorner2.x,
                 g_BrCamCorner1.x + g_BrCamCorner3.x));
    CHECK(approx(g_BrCamCorner0.y + g_BrCamCorner2.y,
                 g_BrCamCorner1.y + g_BrCamCorner3.y));

    /* corner0 - corner1 is 2*R scaled by 0.75. */
    CHECK(approx(g_BrCamCorner0.x - g_BrCamCorner1.x, 2.0f * 100.0f * 0.75f));
    /* corner0 - corner3 is 2*U scaled by 0.75. */
    CHECK(approx(g_BrCamCorner0.y - g_BrCamCorner3.y, 2.0f * 75.0f * 0.75f));

    CHECK(approx(g_BrCamDist, 100.0f));
    CHECK(approx(g_BrCamFovIn, 0.5f));

    /* mode 2 halves the vertical extent only. */
    g_BrCamMode = 2;
    BrCamFrustumBuild(&cam, 0.5f, 100.0f, 4.0f, 3.0f);
    CHECK(approx(g_BrCamExtentR.x, 100.0f));
    CHECK(approx(g_BrCamExtentU.y, 37.5f));
    g_BrCamMode = 0;
}

static void test_camera_lookat_and_perspective(void)
{
    BrCamBasis cam;
    static uint32_t gfx[8];

    memset(&cam, 0, sizeof(cam));
    cam.eye.x = 1.0f;  cam.eye.y = 2.0f;  cam.eye.z = 3.0f;
    cam.fwd.x = 10.0f; cam.fwd.y = 20.0f; cam.fwd.z = 30.0f;
    cam.up.y  = 1.0f;

    g_BrK08F518 = 1.0f;
    g_BrK08F51C = 1.0f;
    BrCamMatrixSetup(&cam, 90.0f, 500.0f, 4.0f, 3.0f);

    /* eye, eye+fwd, up -- guLookAtF order. */
    CHECK(approx(g_lookAt[0], 1.0f) && approx(g_lookAt[1], 2.0f) &&
          approx(g_lookAt[2], 3.0f));
    CHECK(approx(g_lookAt[3], 11.0f) && approx(g_lookAt[4], 22.0f) &&
          approx(g_lookAt[5], 33.0f));
    CHECK(approx(g_lookAt[7], 1.0f));

    /* fovy uses a5/a4 while aspect uses a4/a5 -- both in the original. */
    CHECK(approx(g_persp[0], 90.0f * (3.0f / 4.0f)));
    CHECK(approx(g_persp[1], 4.0f / 3.0f));
    CHECK(approx(g_persp[2], 0.8f));            /* hardcoded near */
    CHECK(approx(g_persp[3], 500.0f));
    CHECK(approx(g_persp[4], 1.0f));            /* the 7th argument */
    CHECK(approx(g_BrCamNear, 0.8f));
    CHECK(approx(g_BrCamFar, 500.0f));

    /* The fixed camera: 45 degrees, 4:3, near 10, far 2000, and it emits
     * exactly two display-list commands. */
    g_BrGfxPtr = gfx;
    memset(gfx, 0, sizeof(gfx));
    BrCamMatrixSetupFixed(0.0f, 0.0f);
    CHECK(approx(g_lookAt[0], 512.0f) && approx(g_lookAt[2], 1000.0f));
    CHECK(approx(g_lookAt[5], 0.0f));
    CHECK(approx(g_lookAt[7], 1.0f));
    CHECK(approx(g_persp[0], 45.0f));
    CHECK(approx(g_persp[2], 10.0f) && approx(g_persp[3], 2000.0f));
    CHECK(gfx[0] == 0xBC00000Eu);
    CHECK(gfx[1] == 1u);                        /* the perspNorm halfword */
    CHECK(gfx[2] == 0x01030040u);
    CHECK(g_BrGfxPtr == gfx + 4);
}

static void test_ortho_matrix(void)
{
    static uint32_t gfx[8];
    float x0, y0, x1, y1;

    g_BrGfxPtr = gfx;
    memset(gfx, 0, sizeof(gfx));
    BrCamMatrixSetupOrtho(640.0f, 480.0f);

    /* Row-vector convention (v' = v*M) with v = (x, y, z, 1):
     * the pixel rectangle maps onto [-1,1]. */
    x0 = 0.0f   * g_BrCurMat.m[0][0] + g_BrCurMat.m[3][0];
    y0 = 0.0f   * g_BrCurMat.m[1][1] + g_BrCurMat.m[3][1];
    x1 = 640.0f * g_BrCurMat.m[0][0] + g_BrCurMat.m[3][0];
    y1 = 480.0f * g_BrCurMat.m[1][1] + g_BrCurMat.m[3][1];
    CHECK(approx(x0, -1.0f) && approx(y0, -1.0f));
    CHECK(approx(x1,  1.0f) && approx(y1,  1.0f));

    /* Row 2 is deliberately all zero -- z is discarded. */
    CHECK(g_BrCurMat.m[2][0] == 0.0f && g_BrCurMat.m[2][1] == 0.0f &&
          g_BrCurMat.m[2][2] == 0.0f && g_BrCurMat.m[2][3] == 0.0f);
    CHECK(g_BrCurMat.m[3][3] == 1.0f);
    CHECK(g_BrCurMat.m[3][2] == 0.0f);

    CHECK(gfx[0] == 0xBC00000Eu);
    CHECK(gfx[2] == 0x01030040u);
}

/* ================================================================== */
/* 3. Per-car colour                                                  */
/* ================================================================== */

static BrCarGfx  g_car;
static BrGfxSlot g_slots[8];
static uint16_t  g_words[8][16];

static void CarReset(void)
{
    int i;

    memset(&g_car, 0, sizeof(g_car));
    memset(g_slots, 0, sizeof(g_slots));
    memset(g_words, 0, sizeof(g_words));
    for (i = 0; i < 8; i++) {
        g_slots[i].pWords = g_words[i];
        g_slots[i].f20 = 0x01000000u;      /* bits[27:24] == 1 */
    }
    for (i = 0; i < 12; i++)
        g_car.aSlotIdx[i] = (unsigned char)(i & 7);
    g_car.pSlots = g_slots;
    g_BrCarCount = 1;
    g_Br0AC300 = 0;
    g_Br6C661C = 0;
    g_Br6C6624 = 0;
    g_BrGfxSubmit = SubmitStub;
    g_submitCount = 0;
}

static void test_car_colour(void)
{
    uint16_t packed, back;
    BrRgbSink sink;

    CarReset();
    g_car.cDl = 3;
    g_car.aDl[0] = 0xA1; g_car.aDl[1] = 0xA2; g_car.aDl[2] = 0xA3;

    BrCarGfxSetColour(&g_car, 31, 0, 15);

    /* Stored big-endian: unswapping recovers the RGBA5551 packing. */
    packed = (uint16_t)((31u << 11) | (0u << 6) | (15u << 1));
    back = (uint16_t)(((uint32_t)g_words[0][0] << 8 & 0xFF00u) |
                      ((uint32_t)g_words[0][0] >> 8 & 0xFFu));
    CHECK(back == packed);

    /* The second halfword drops the low bit of every component. */
    packed = (uint16_t)(((31u & 0x1Eu) << 10) | ((0u & 0x1Eu) << 5) |
                        (15u & 0x1Eu));
    back = (uint16_t)(((uint32_t)g_words[0][1] << 8 & 0xFF00u) |
                      ((uint32_t)g_words[0][1] >> 8 & 0xFFu));
    CHECK(back == packed);

    CHECK(g_submitCount == 3);
    CHECK(g_submitted[0] == 0xA1 && g_submitted[2] == 0xA3);

    /* g_BrCarCount == 0 aborts before anything at all happens. */
    CarReset();
    g_car.cDl = 3;
    g_BrCarCount = 0;
    BrCarGfxSetColour(&g_car, 31, 31, 31);
    CHECK(g_submitCount == 0);
    CHECK(g_words[0][0] == 0);

    /* A slot with a wrong type tag is skipped, not written. */
    CarReset();
    g_slots[0].f20 = 0x02000000u;
    BrCarGfxSetColour(&g_car, 31, 31, 31);
    CHECK(g_words[0][0] == 0);
    CHECK(g_words[1][0] != 0);

    /* The alpha bit really is taken from pWords[i] and not from pWords[0]:
     * poke bit 0 of word 5 in the slot used for i == 5 and it shows up in
     * that slot's word 0. */
    CarReset();
    g_words[5][5] = 1;
    BrCarGfxSetColour(&g_car, 0, 0, 0);
    back = (uint16_t)(((uint32_t)g_words[5][0] << 8 & 0xFF00u) |
                      ((uint32_t)g_words[5][0] >> 8 & 0xFFu));
    CHECK((back & 1u) == 1u);
    back = (uint16_t)(((uint32_t)g_words[4][0] << 8 & 0xFF00u) |
                      ((uint32_t)g_words[4][0] >> 8 & 0xFFu));
    CHECK((back & 1u) == 0u);

    /* Read-back is native, so it does NOT see what SetColour wrote -- that
     * asymmetry is the point of this check. */
    CarReset();
    g_words[2][0] = (uint16_t)((31u << 11) | (16u << 6) | (8u << 1));
    memset(&sink, 0, sizeof(sink));
    BrCarGfxReadColour(&sink, &g_car);
    CHECK(sink.r == 0xFF);                       /* 31 -> 0xF8|0x07 */
    CHECK(sink.g == ((16 << 3) | (16 >> 2)));
    CHECK(sink.b == ((8 << 3) | (8 >> 2)));

    /* Null word block: no write at all. */
    CarReset();
    g_slots[2].pWords = NULL;
    memset(&sink, 0xAA, sizeof(sink));
    BrCarGfxReadColour(&sink, &g_car);
    CHECK(sink.r == 0xAA);
}

static void test_car_extra_blocks(void)
{
    CarReset();
    g_car.aDlExtra[0] = 0xB1;
    g_car.aDlExtra[1] = 0xB2;
    g_car.aDlExtra[2] = 0xB3;
    g_car.aDlExtra[3] = 0xB4;

    /* Blocks 1 and 3 copy +0x1E into +0x1A; blocks 2 and 4 copy +0x1C. The
     * last block to run is 4, so +0x1A must equal its +0x1C (0x00C0). */
    BrCarGfxSetColour(&g_car, 0, 0, 0);
    CHECK(g_submitCount == 4);
    CHECK(g_words[3][14] == 0x00C0u);
    CHECK(g_words[3][13] == 0x00C0u);
    CHECK(g_words[3][11] == 0x38E7u);
    CHECK(g_words[3][6]  == 0xFEFFu);

    /* With only block 1 live, +0x1A comes from +0x1E, which the branch on
     * g_Br6C661C selects. */
    CarReset();
    g_car.aDlExtra[0] = 0xB1;
    g_Br6C661C = 1;
    BrCarGfxSetColour(&g_car, 0, 0, 0);
    CHECK(g_words[3][15] == 0x0070u);
    CHECK(g_words[3][13] == 0x0070u);            /* +0x1A <- +0x1E */
    CHECK(g_words[3][14] == 0x0190u);            /* +0x1C unrelated */
    CHECK(g_words[3][10] == 0x8290u);
    CHECK(g_words[3][8]  == 0x8290u);            /* +0x10 <- +0x14 */

    CarReset();
    g_car.aDlExtra[0] = 0xB1;
    BrCarGfxSetColour(&g_car, 0, 0, 0);
    CHECK(g_words[3][15] == 0x0190u);
    CHECK(g_words[3][10] == 0x01A0u);

    /* g_Br0AC300 suppresses the whole second half but not the first. */
    CarReset();
    g_car.aDlExtra[0] = 0xB1;
    g_Br0AC300 = 1;
    BrCarGfxSetColour(&g_car, 31, 31, 31);
    CHECK(g_submitCount == 0);
    CHECK(g_words[3][15] == 0);
    CHECK(g_words[0][0] != 0);                   /* first loop still ran */
    g_Br0AC300 = 0;
}

/* ================================================================== */
/* 7. Odds and ends                                                   */
/* ================================================================== */

static void test_misc(void)
{
    BrVec3 a, b;
    BrPairSlot s;
    static unsigned char cars[3 * 0x15F88];
    void *ptrs[3];
    int marker;

    /* 0x10035C70 takes the DESTINATION first. */
    a.x = 1.0f; a.y = 2.0f; a.z = 3.0f;
    b.x = b.y = b.z = 9.0f;
    BrVec3Copy(&b, &a);
    CHECK(approx(b.x, 1.0f) && approx(b.z, 3.0f));
    CHECK(approx(a.x, 1.0f));

    s.f00 = 7; s.f04 = 7; s.f08 = 7;
    BrPairSlotReset(&s, 42);
    CHECK(s.f00 == 7 && s.f04 == 0 && s.f08 == 42);

    CHECK(BrRet0_10035059() == 0);
    CHECK(BrRet1_1003557B() == 1);
    CHECK(BrRet1_10035B87() == 1);

    /* 0x10035520: flag == 0 loads, flag != 0 only logs -- but the tail runs
     * either way. */
    memset(ptrs, 0, sizeof(ptrs));
    g_loadCount = g_postCount = g_logCount = 0;
    BrCarSlotLoad(cars, ptrs, 1, &marker, 0);
    CHECK(g_loadCount == 1 && g_logCount == 0 && g_postCount == 1);
    CHECK(ptrs[1] == &marker);

    BrCarSlotLoad(cars, ptrs, 2, &marker, 1);
    CHECK(g_loadCount == 1);                    /* NOT loaded */
    CHECK(g_logCount == 1);
    CHECK(g_postCount == 2);                    /* tail still ran */
    CHECK(ptrs[2] == &marker);                  /* still recorded */

    /* 0x10035BA7 ignores its argument and always uses the global. */
    g_logCount = 0;
    BrLogSet(&marker);
    CHECK(g_BrLogArg == &marker);
    CHECK(g_logLast == &marker);
    CHECK(g_logCount == 1);
    BrLogEmit((void *)0xDEAD);
    CHECK(g_logLast == &marker);
}

/* ================================================================== */

int main(void)
{
    test_rebase_window();
    test_dl_walk();
    test_dl_owner();
    test_bit_split();
    test_accum_clamp();

    test_anim_flags();
    test_anim_fixed_point();
    test_anim_pingpong_and_reverse();
    test_anim_degenerate_bracket();

    test_pad_button_map();
    test_pad_error_neutralises();
    test_pad_steering();
    test_pad_derived_bits_and_ramp();
    test_pad_axes();

    test_model_swap();
    test_model_swap_null_block();
    test_model_load_arg_order();

    test_frustum_corners();
    test_camera_lookat_and_perspective();
    test_ortho_matrix();

    test_car_colour();
    test_car_extra_blocks();

    test_misc();

    if (g_fail == 0) {
        printf("test_slice2_19: all checks passed\n");
        return 0;
    }
    printf("test_slice2_19: %d FAILURES\n", g_fail);
    return 1;
}
