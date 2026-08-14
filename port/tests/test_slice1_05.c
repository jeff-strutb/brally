/* test_slice1_05.c -- behaviour/invariant tests for slice1_05.c
 *
 * Build:
 *   clang -std=c99 -Wall -Wextra -Iport/include \
 *         port/tests/test_slice1_05.c port/src/slice1_05.c -lm -o /tmp/t05
 *
 * NOTE ON LINKING: BrF3DVtxFixup (0x1002C150) calls BrSegFixup, which lives
 * in br_seg.c and is deliberately NOT duplicated in slice1_05.c. So that the
 * command line above links on its own, this file supplies a stand-in whose
 * behaviour is copied from br_seg.h's documented contract. If this test is
 * ever linked against br_seg.o, delete the stand-in below.
 */

#include <stdio.h>
#include <string.h>

#include "slice1_05.h"

static int g_fail;
static int g_checks;

#define CHECK(cond) do {                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_fail;                                                      \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                  \
    } while (0)

/* ---- stand-in for br_seg.c's BrSegFixup (0x1002B970) ------------------ */
void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr)
{
    if (*pPtr == 0)
        return;
    if (*pPtr < pMap->n64Base)
        *pPtr = 0;
    else
        *pPtr += (pMap->hostBase - pMap->n64Base);
}

/* ---- big static state ------------------------------------------------- */
static BrVtxCache g_cache;
static float      g_arena[4096];
static BrPeer     g_peers[BR_PEER_COUNT];
static BrPtrList  g_list;

/* ====================================================================== */
/* 0x1002BDD0  BrVtxSwap                                                  */
/* ====================================================================== */
static void test_vtx_swap(void)
{
    unsigned char v[2 * BR_VTX_SRC_SIZE];
    unsigned char orig[2 * BR_VTX_SRC_SIZE];
    int i;

    for (i = 0; i < (int)sizeof v; ++i)
        v[i] = (unsigned char)(i * 7 + 1);
    memcpy(orig, v, sizeof v);

    /* Round trip: swapping twice restores the input exactly. */
    BrVtxSwap(v, 2);
    CHECK(memcmp(v, orig, sizeof v) != 0);
    BrVtxSwap(v, 2);
    CHECK(memcmp(v, orig, sizeof v) == 0);

    /* Only 0x00..0x0B move; 0x0C..0x0F are untouched. */
    BrVtxSwap(v, 2);
    for (i = 0; i < 2; ++i) {
        int b = i * BR_VTX_SRC_SIZE;
        int k;
        for (k = 0; k < 12; k += 2) {
            CHECK(v[b + k]     == orig[b + k + 1]);
            CHECK(v[b + k + 1] == orig[b + k]);
        }
        for (k = 12; k < 16; ++k)
            CHECK(v[b + k] == orig[b + k]);
    }

    /* Non-positive counts do nothing. */
    memcpy(v, orig, sizeof v);
    BrVtxSwap(v, 0);
    BrVtxSwap(v, -3);
    CHECK(memcmp(v, orig, sizeof v) == 0);
}

/* ====================================================================== */
/* 0x1002BE30  BrVtxExpand                                                */
/* ====================================================================== */

/* One N64 Vtx, big-endian as it sits in the .rca payload:
 *   x=100  y=-200  z=300  flag=0x1234  s=64  t=-32  bytes 0x40,0x81,0x00,0x7F */
static const unsigned char k_vtx_be[BR_VTX_SRC_SIZE] = {
    0x00, 0x64,  0xFF, 0x38,  0x01, 0x2C,  0x12, 0x34,
    0x00, 0x40,  0xFF, 0xE0,  0x40, 0x81,  0x00, 0x7F
};

static void reset_cache(void)
{
    memset(&g_cache, 0, sizeof g_cache);
    memset(g_arena, 0, sizeof g_arena);
    g_cache.pCursor = g_arena;
}

static void test_vtx_expand(void)
{
    unsigned char v[BR_VTX_SRC_SIZE];
    float *p;

    reset_cache();
    memcpy(v, k_vtx_be, sizeof v);
    BrVtxSwap(v, 1);

    p = BrVtxExpand(&g_cache, v, 1);

    CHECK(p == g_arena);                        /* returns the OLD cursor */
    CHECK(g_cache.pCursor == g_arena + BR_VTX_OUT_FLOATS);
    CHECK(g_cache.nVerts == 1);

    CHECK(p[0] ==  100.0f);
    CHECK(p[1] == -200.0f);
    CHECK(p[2] ==  300.0f);
    CHECK(p[3] ==   64.0f);                     /* s, from offset 0x08 */
    CHECK(p[4] ==  -32.0f);                     /* t, from offset 0x0A */
    CHECK(p[5] ==  64.0f  * BR_VTX_NORMAL_SCALE);
    CHECK(p[6] == -127.0f * BR_VTX_NORMAL_SCALE);
    CHECK(p[7] ==   0.0f);

    /* 1/128 exactly: 0x40 -> 0.5, 0x81 -> -0.9921875 */
    CHECK(p[5] == 0.5f);
    CHECK(p[6] == -0.9921875f);

    /* count <= 0: cursor returned unchanged, nothing written or counted. */
    p = BrVtxExpand(&g_cache, v, 0);
    CHECK(p == g_arena + BR_VTX_OUT_FLOATS);
    CHECK(g_cache.pCursor == g_arena + BR_VTX_OUT_FLOATS);
    CHECK(g_cache.nVerts == 1);
    p = BrVtxExpand(&g_cache, v, -5);
    CHECK(p == g_arena + BR_VTX_OUT_FLOATS);
    CHECK(g_cache.nVerts == 1);
}

/* ====================================================================== */
/* 0x1002BD50 / 0x1002BF00  cache behaviour                               */
/* ====================================================================== */
static void test_vtx_cache(void)
{
    unsigned char v[4 * BR_VTX_SRC_SIZE];
    unsigned char afterFirst[sizeof v];
    void *pv;
    float *pFirst;
    int i;

    reset_cache();
    for (i = 0; i < 4; ++i)
        memcpy(v + i * BR_VTX_SRC_SIZE, k_vtx_be, BR_VTX_SRC_SIZE);

    pv = v;
    BrVtxCacheResolve(&g_cache, &pv, 4);
    CHECK(pv == g_arena);
    CHECK(g_cache.nEntries == 1);
    CHECK(g_cache.nVerts == 4);
    pFirst = (float *)pv;
    CHECK(pFirst[0] == 100.0f);
    memcpy(afterFirst, v, sizeof v);

    /* Hit: same pointer AND same count -> same block, no second swap, no
     * new entry, no extra vertices expanded. */
    pv = v;
    BrVtxCacheResolve(&g_cache, &pv, 4);
    CHECK(pv == (void *)pFirst);
    CHECK(g_cache.nEntries == 1);
    CHECK(g_cache.nVerts == 4);
    CHECK(memcmp(v, afterFirst, sizeof v) == 0);

    /* Miss on a different count for the SAME memory. This is the documented
     * hazard: the source gets byte-swapped a second time. */
    pv = v;
    BrVtxCacheResolve(&g_cache, &pv, 2);
    CHECK(g_cache.nEntries == 2);
    CHECK(memcmp(v, afterFirst, sizeof v) != 0);
    CHECK(memcmp(v, k_vtx_be, BR_VTX_SRC_SIZE) == 0);   /* back to big-endian */

    /* Bail-outs leave the caller's slot alone. */
    pv = NULL;
    BrVtxCacheResolve(&g_cache, &pv, 4);
    CHECK(pv == NULL);
    pv = v;
    BrVtxCacheResolve(&g_cache, &pv, 0);
    CHECK(pv == (void *)v);
    CHECK(g_cache.nEntries == 2);

    /* Insert drops silently once full; it never grows past the cap. */
    reset_cache();
    for (i = 0; i < BR_VTX_CACHE_MAX + 16; ++i)
        BrVtxCacheInsert(&g_cache, (void *)&g_arena[i & 63], i, g_arena);
    CHECK(g_cache.nEntries == BR_VTX_CACHE_MAX);
}

/* ====================================================================== */
/* 0x1002C150 / 0x1002C190 / 0x1002C1B0                                   */
/* ====================================================================== */
static void test_f3d(void)
{
    BrSegMap map;
    BrGfxWords cmd;
    unsigned char raw[8];
    unsigned n;

    map.n64Base  = 0x0F000000u;
    map.hostBase = 0x20000000u;

    /* F3DEX G_VTX: w0 = 0x01<<24 | n<<10 | ((v0+n)*2)<<1, n = 12, v0 = 0 */
    cmd.w0 = 0x01000000u | (12u << 10) | (24u << 1);
    cmd.w1 = 0x06001234u;                       /* segment 6, offset 0x1234 */

    n = BrF3DVtxFixup(&map, &cmd);
    CHECK(n == 12);
    /* prefix taken from the base, low 24 bits kept, then rebased */
    CHECK(cmd.w1 == 0x20001234u);

    /* Below the base -> BrSegFixup zeroes it. */
    cmd.w0 = 0x01000000u | (3u << 10);
    cmd.w1 = 0x06000010u;
    map.n64Base  = 0x0F000000u;                 /* prefix 0x0F, so 0x0F000010 */
    map.hostBase = 0x20000000u;
    n = BrF3DVtxFixup(&map, &cmd);
    CHECK(n == 3);
    CHECK(cmd.w1 == 0x20000010u);

    /* The count field really is 6 bits at bit 10. */
    cmd.w0 = 0xFFFFFFFFu;
    cmd.w1 = 0;
    CHECK(BrF3DVtxFixup(&map, &cmd) == 0x3F);
    /* A zero w1 does NOT stay null: the prefix is merged in before
     * BrSegFixup runs, so w1 becomes the segment base and then rebases to
     * hostBase. BrSegFixup's null-stays-null rule is unreachable here. */
    CHECK(cmd.w1 == 0x20000000u);

    /* G_TRI1: only bytes 4,5,6 change, and they are halved (rounding down). */
    memset(raw, 0, sizeof raw);
    raw[0] = 0xB1; raw[1] = 0x11; raw[2] = 0x22; raw[3] = 0x33;
    raw[4] = 0x08; raw[5] = 0x0A; raw[6] = 0x0D; raw[7] = 0x44;
    BrF3DTri1Fixup(raw);
    CHECK(raw[0] == 0xB1 && raw[1] == 0x11 && raw[2] == 0x22 && raw[3] == 0x33);
    CHECK(raw[4] == 0x04);
    CHECK(raw[5] == 0x05);
    CHECK(raw[6] == 0x06);                      /* 0x0D >> 1, rounds down */
    CHECK(raw[7] == 0x44);

    /* G_TRI2: bytes 0,1,2 and 4,5,6; 3 and 7 untouched. */
    raw[0] = 0x02; raw[1] = 0x04; raw[2] = 0x06; raw[3] = 0xB1;
    raw[4] = 0x08; raw[5] = 0x0A; raw[6] = 0x0C; raw[7] = 0x55;
    BrF3DTri2Fixup(raw);
    CHECK(raw[0] == 0x01 && raw[1] == 0x02 && raw[2] == 0x03);
    CHECK(raw[3] == 0xB1);
    CHECK(raw[4] == 0x04 && raw[5] == 0x05 && raw[6] == 0x06);
    CHECK(raw[7] == 0x55);

    /* Every F3DEX index byte is even, so halving is exact for real data and
     * halving twice is NOT idempotent -- guard against applying it twice. */
    raw[4] = 0x08;
    BrF3DTri1Fixup(raw);
    BrF3DTri1Fixup(raw);
    CHECK(raw[4] == 0x02);
}

/* ====================================================================== */
/* 0x1002FAF0 / 0x1002FAC0 / 0x1002F900  RDP combiner                     */
/* ====================================================================== */
static void test_combine(void)
{
    BrGfxWords c;
    uint32_t w0, w1;

    /* The literals and the one asymmetric token. */
    CHECK(BrRdpCCMux(0) == 31);
    CHECK(BrRdpACMux(0) == 7);
    CHECK(BrRdpCCMux(1) == 6);
    CHECK(BrRdpACMux(1) == 6);
    CHECK(BrRdpCCMux(1000) == 0);
    CHECK(BrRdpACMux(1000) == 0);
    CHECK(BrRdpCCMux(1013) == 13);      /* LOD_FRACTION, colour */
    CHECK(BrRdpACMux(1013) == 0);       /* LOD_FRACTION, alpha  */
    /* 1013 is the only token where the two disagree other than 0. */
    CHECK(BrRdpCCMux(1005) == 5 && BrRdpACMux(1005) == 5);

    /* Distinct value in every field, so each one is identifiable. */
    BrRdpSetCombineLERP(&c,
        1003, 1005, 1017, 1002,     /* a0  b0  c0  d0  */
        1001, 1002, 1003, 1004,     /* Aa0 Ab0 Ac0 Ad0 */
        1009, 1006, 1021, 1005,     /* a1  b1  c1  d1  */
        1005, 1006, 1007, 1001);    /* Aa1 Ab1 Ac1 Ad1 */

    w0 = 0xFC000000u
       | (3u  << 20) | (17u << 15) | (1u << 12) | (3u << 9)
       | (9u  <<  5) | 21u;
    w1 = (5u  << 28) | (6u  << 24) | (5u << 21) | (7u << 18)
       | (2u  << 15) | (2u  << 12) | (4u <<  9) | (5u <<  6)
       | (6u  <<  3) | 1u;

    CHECK(c.w0 == w0);
    CHECK(c.w1 == w1);

    /* The command byte always comes out as 0xFC regardless of a0. */
    CHECK((c.w0 >> 24) == 0xFCu);
    BrRdpSetCombineLERP(&c, 0, 0, 0, 0, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0, 0);
    CHECK((c.w0 >> 24) == 0xFCu);
    /* a0 = token 0 -> 31, truncated to 4 bits -> 15 */
    CHECK(((c.w0 >> 20) & 0xFu) == 15u);
    /* c0 = 31 fits its 5 bits */
    CHECK(((c.w0 >> 15) & 0x1Fu) == 31u);
    /* Aa0 = 7 fits its 3 bits */
    CHECK(((c.w0 >> 12) & 0x7u) == 7u);

    /* b0 is the unmasked field: only its low 4 bits survive. */
    BrRdpSetCombineLERP(&c, 1000, 1000 + 0x35, 1000, 1000, 1000, 1000, 1000, 1000,
                            1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000);
    CHECK(((c.w1 >> 28) & 0xFu) == 5u);         /* 0x35 & 0xF */
    CHECK((c.w1 & 0x0FFFFFFFu) == 0u);          /* nothing leaked downward */
}

/* ====================================================================== */
/* 0x100306C0 / 0x10031140  matrices                                      */
/* ====================================================================== */
static int mat_eq(const BrMat4 *a, const BrMat4 *b)
{
    int i, j;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            if (a->m[i][j] != b->m[i][j])
                return 0;
    return 1;
}

static void test_matrix(void)
{
    BrMat4 id, a, b, r, r2;
    int i, j;

    BrMat4Translate(&id, 0.0f, 0.0f, 0.0f);

    /* Layout: identity plus the translation in ROW 3 (row-vector form). */
    BrMat4Translate(&a, 1.0f, 2.0f, 3.0f);
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j) {
            float want = (i == j) ? 1.0f : 0.0f;
            if (i == 3 && j < 3)
                want = (float)(j + 1);
            CHECK(a.m[i][j] == want);
        }

    /* I * M == M and M * I == M. */
    BrMat4Mul(&id, &a, &r);
    CHECK(mat_eq(&r, &a));
    BrMat4Mul(&a, &id, &r);
    CHECK(mat_eq(&r, &a));

    /* Translations compose by addition under this convention. */
    BrMat4Translate(&a, 1.0f, 2.0f, 3.0f);
    BrMat4Translate(&b, 10.0f, 20.0f, 30.0f);
    BrMat4Mul(&a, &b, &r);
    BrMat4Translate(&r2, 11.0f, 22.0f, 33.0f);
    CHECK(mat_eq(&r, &r2));

    /* Aliasing: out == a and out == b must match the non-aliased result.
     * (Exact here because every product/sum is exactly representable.) */
    BrMat4Mul(&a, &b, &r);
    r2 = a;
    BrMat4Mul(&r2, &b, &r2);
    CHECK(mat_eq(&r2, &r));
    r2 = b;
    BrMat4Mul(&a, &r2, &r2);
    CHECK(mat_eq(&r2, &r));

    /* Null inputs leave the destination untouched. */
    r = a;
    BrMat4Mul(NULL, &b, &r);
    CHECK(mat_eq(&r, &a));
    BrMat4Mul(&a, NULL, &r);
    CHECK(mat_eq(&r, &a));

    /* Full 4x4: the fourth row and column do participate. */
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j) {
            a.m[i][j] = (float)(i * 4 + j + 1);
            b.m[i][j] = (float)((j == i) ? 2 : 0);
        }
    BrMat4Mul(&a, &b, &r);              /* b = 2I, so r = 2a */
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            CHECK(r.m[i][j] == 2.0f * a.m[i][j]);
}

/* ====================================================================== */
/* 0x1002F460  BrSelLookup                                                */
/* ====================================================================== */
static void test_sel(void)
{
    static const unsigned char table[24][2] = {
        {0,50},{1,51},{2,52},{3,53},{4,54},{5,55},
        {6,56},{7,57},{8,58},{9,59},{10,60},{11,61},
        {0,70},{1,71},{2,72},{3,73},{4,74},{5,75},
        {6,76},{7,77},{8,78},{9,79},{10,80},{11,81}
    };
    BrSelInput in;
    int a, b;

    memset(&in, 0, sizeof in);

    /* index = f04 * 12 + f05 */
    in.f04 = 1; in.f05 = 3;             /* -> 15 */
    BrSelLookup(&in, table, &a, &b);
    CHECK(a == 3);
    CHECK(b == 73);

    /* Fold on: values >= 6 go down by 6 ... */
    in.f00 = 1;
    in.f04 = 0; in.f05 = 9;             /* value 9 */
    BrSelLookup(&in, table, &a, &b);
    CHECK(a == 3);
    CHECK(b == 59);                     /* second output NOT folded */

    /* ... and values < 6 go up by 6. */
    in.f04 = 0; in.f05 = 2;             /* value 2 */
    BrSelLookup(&in, table, &a, &b);
    CHECK(a == 8);
    CHECK(b == 52);

    /* Folding twice returns the original: it is an involution on 0..11. */
    {
        int once, twice;
        in.f00 = 0; in.f04 = 0; in.f05 = 4;
        BrSelLookup(&in, table, &once, &b);
        in.f00 = 1;
        BrSelLookup(&in, table, &twice, &b);
        CHECK(twice == once + 6);
        CHECK(((twice >= 6) ? twice - 6 : twice + 6) == once);
    }

    /* Only bit 0 of f00 selects the fold. */
    in.f00 = 0x02; in.f04 = 0; in.f05 = 2;
    BrSelLookup(&in, table, &a, &b);
    CHECK(a == 2);
    in.f00 = 0x03;
    BrSelLookup(&in, table, &a, &b);
    CHECK(a == 8);
}

/* ====================================================================== */
/* 0x10036030  BrPeerFind                                                 */
/* ====================================================================== */
static void test_peer(void)
{
    int i;

    memset(g_peers, 0, sizeof g_peers);

    /* id == 1 short-circuits to record 0 whatever the table holds. */
    CHECK(BrPeerFind(g_peers, 1) == 0);

    /* Empty table: the first free record is 1, never 0. */
    CHECK(BrPeerFind(g_peers, 77) == 1);

    /* Record 0 is never returned by the scans, even when it matches. */
    g_peers[0].f2C = 2;
    g_peers[0].f04 = 77;
    CHECK(BrPeerFind(g_peers, 77) == 1);        /* still the first free slot */

    /* An occupied record with a matching id wins over the free scan. */
    g_peers[5].f2C = 3;
    g_peers[5].f04 = 77;
    CHECK(BrPeerFind(g_peers, 77) == 5);
    CHECK(BrPeerFind(g_peers, 78) == 1);        /* no match -> first free */

    /* Only the low 6 bits count as state: 0x40 has them all clear, so the
     * record still reads as free. */
    g_peers[1].f2C = 0x40;
    g_peers[1].f04 = 78;
    CHECK(BrPeerFind(g_peers, 78) == 1);        /* matched the free scan, not
                                                 * the in-use scan */
    g_peers[1].f2C = 0x41;
    CHECK(BrPeerFind(g_peers, 78) == 1);        /* now in use and matching */

    /* Table full and nothing matching -> -1. */
    for (i = 0; i < BR_PEER_COUNT; ++i) {
        g_peers[i].f2C = 1;
        g_peers[i].f04 = 1000u + (uint32_t)i;
    }
    CHECK(BrPeerFind(g_peers, 99) == -1);
    CHECK(BrPeerFind(g_peers, 1000u + 15u) == 15);
    /* record 0's id is still unreachable through the scan */
    CHECK(BrPeerFind(g_peers, 1000u) == -1);
}

/* ====================================================================== */
/* 0x10035FE0  BrEntInit                                                  */
/* ====================================================================== */
static void test_ent(void)
{
    static BrEnt ents[4];
    static BrEntRec recs[4];
    int i;

    for (i = 0; i < 4; ++i) {
        ents[i].f2C = 0xAAAAAAAAu;
        ents[i].f30 = 0xBBBBBBBBu;
        ents[i].f44 = 0xCCCCCCCCu;
        BrEntInit(&ents[i], ents, recs);
    }

    for (i = 0; i < 4; ++i) {
        CHECK(ents[i].f2C == 0);
        CHECK(ents[i].f30 == 0);
        CHECK(ents[i].f44 == 0);
        CHECK(ents[i].f154 == i);
        CHECK(ents[i].f158 == &recs[i]);
    }

    /* The record array's stride really is 6 bytes. */
    CHECK(sizeof(BrEntRec) == 6);
    CHECK((size_t)((unsigned char *)ents[3].f158 -
                   (unsigned char *)ents[0].f158) == 18u);
}

/* ====================================================================== */
/* small setters and lists                                                */
/* ====================================================================== */
static int g_hookCalls;
static void hook_cb(void) { ++g_hookCalls; }

static void test_misc(void)
{
    BrCursorPair pair;
    BrHooks hooks;
    int dummyA, dummyB;
    int i;

    /* 0x1002B280: one value into both fields. */
    pair.f10 = NULL;
    pair.f18 = (void *)&pair;
    BrCursorPairSet(&pair, &dummyA);
    CHECK(pair.f10 == (void *)&dummyA);
    CHECK(pair.f18 == (void *)&dummyA);
    CHECK(pair.f10 == pair.f18);

    /* 0x1002C1F0: append, then drop silently once full. */
    memset(&g_list, 0, sizeof g_list);
    BrPtrListAdd(&g_list, &dummyA);
    BrPtrListAdd(&g_list, &dummyB);
    CHECK(g_list.n == 2);
    CHECK(g_list.ap[0] == (void *)&dummyA);
    CHECK(g_list.ap[1] == (void *)&dummyB);
    for (i = 2; i < BR_PTRLIST_MAX + 8; ++i)
        BrPtrListAdd(&g_list, &dummyA);
    CHECK(g_list.n == BR_PTRLIST_MAX);
    CHECK(g_list.ap[0] == (void *)&dummyA);     /* head not overwritten */
    CHECK(g_list.ap[1] == (void *)&dummyB);

    /* 0x10034C32..0x10034CA8 */
    memset(&hooks, 0, sizeof hooks);
    BrHookNopA();
    BrHookNopB();
    BrHookSetA(&hooks, &dummyA);
    BrHookSetB(&hooks, &dummyB);
    CHECK(hooks.pfA == (void *)&dummyA);
    CHECK(hooks.pfB == (void *)&dummyB);
    CHECK(hooks.pfnC == NULL);

    g_hookCalls = 0;
    BrHookSetC(&hooks, hook_cb);
    BrHookCallC(&hooks);
    BrHookCallC(&hooks);
    CHECK(g_hookCalls == 2);

    /* The stubbed pair: the source argument is ignored, the flag is set and
     * the global is returned unchanged. */
    hooks.g0938 = 0x1234u;
    hooks.g3300 = 0x5678u;
    hooks.f7C44 = 0;
    CHECK(BrHookTakeA(&hooks, &dummyA) == 0x1234u);
    CHECK(hooks.f7C44 == 1);
    CHECK(hooks.g0938 == 0x1234u);              /* nothing was copied in */
    hooks.f7C44 = 0;
    CHECK(BrHookTakeB(&hooks, NULL) == 0x5678u);
    CHECK(hooks.f7C44 == 1);
    CHECK(hooks.g3300 == 0x5678u);
}

int main(void)
{
    test_vtx_swap();
    test_vtx_expand();
    test_vtx_cache();
    test_f3d();
    test_combine();
    test_matrix();
    test_sel();
    test_peer();
    test_ent();
    test_misc();

    printf("slice1_05: %d checks, %d failures\n", g_checks, g_fail);
    return g_fail != 0;
}
