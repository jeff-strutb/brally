/* test_slice2_16.c -- behaviour tests for another module's slice of BRD3D.dll.
 *
 * Every assertion below is either a property of the disassembly (a clamp, a
 * ring wrap, a sign extension, an argument order) or a round trip. Nothing
 * here encodes a value that was merely convenient.
 */

#include "slice2_16.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* ================================================================== */
/* Stand-ins for everything outside this packet.                       */
/* These exist ONLY so the test binary links; they are not ports.      */
/* ================================================================== */

static int      g_geoChanged;
static int      g_overflow;
static uint32_t g_fa0Arg;
static int      g_fa0Calls;
static int      g_tileArgs[5];
static int      g_tileCalls;
static int      g_registerId = -1;
static const void *g_registerArg;
static void    *g_releaseArg;
static int      g_releaseCalls;
/* Every release argument in order, not just the last one. The array walker's
 * only observable is WHICH record each call saw, and a single-slot capture
 * cannot tell "walked three records" from "did record 0 three times". */
#define REL_ARG_SLOTS 8
static void    *g_releaseArgs[REL_ARG_SLOTS];

void BrGbiGeoModeChanged(void)          { g_geoChanged++; }
void BrGbiStackOverflow(int code)       { (void)code; g_overflow++; }
BrGfxWords *BrGbiCall100243D0(BrGfxWords *pCmd) { return pCmd + 7; }
void BrGbiCall10020FA0(uint32_t w1)     { g_fa0Arg = w1; g_fa0Calls++; }

void BrGbiCall10021560(int lrs, int lrt, int uls, int ult, int tile)
{
    g_tileArgs[0] = lrs; g_tileArgs[1] = lrt; g_tileArgs[2] = uls;
    g_tileArgs[3] = ult; g_tileArgs[4] = tile;
    g_tileCalls++;
}

BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd) { return pCmd + 5; }

int BrGbiCall10029470(const void *pStage)
{
    g_registerArg = pStage;
    return g_registerId;
}

void BrGbiCall10075330(void *pv)
{
    if (g_releaseCalls >= 0 && g_releaseCalls < REL_ARG_SLOTS)
        g_releaseArgs[g_releaseCalls] = pv;
    g_releaseArg = pv;
    g_releaseCalls++;
}

/* --- slice1_05 / br_seg / br_bits stand-ins ------------------------ */

static const BrMat4 *g_mulA;
static const BrMat4 *g_mulB;
static int           g_mulCalls;

/* Stand-in for 0x100306C0. Treats a NULL left operand as the identity, which
 * is what BrGbiMatrix relies on when the modelview stack is empty. */
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{
    BrMat4 t;
    int i, j, k;

    g_mulA = pA; g_mulB = pB; g_mulCalls++;
    if (pB == NULL)
        return;
    if (pA == NULL) {
        memcpy(pOut, pB, sizeof *pB);
        return;
    }
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (k = 0; k < 4; ++k)
                s += pA->m[i][k] * pB->m[k][j];
            t.m[i][j] = s;
        }
    memcpy(pOut, &t, sizeof t);
}

static int g_combine[16];
static int g_combineCalls;

/* Stand-in for 0x1002F900. */
void BrRdpSetCombineLERP(BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    int v[16];
    v[0]=a0; v[1]=b0; v[2]=c0; v[3]=d0;
    v[4]=Aa0; v[5]=Ab0; v[6]=Ac0; v[7]=Ad0;
    v[8]=a1; v[9]=b1; v[10]=c1; v[11]=d1;
    v[12]=Aa1; v[13]=Ab1; v[14]=Ac1; v[15]=Ad1;
    memcpy(g_combine, v, sizeof v);
    g_combineCalls++;
    pOut->w0 = 0xFC000000u;
    pOut->w1 = 0;
}

/* Stand-in for 0x1002B970. Faithful to br_seg.h's description. */
void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr)
{
    if (*pPtr == 0)
        return;
    if (*pPtr < pMap->n64Base)
        *pPtr = 0;
    else
        *pPtr += pMap->hostBase - pMap->n64Base;
}

/* Stand-in for 0x100383C0. */
void BrSwapVec3(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    int i;
    for (i = 0; i < 12; i += 4) {
        unsigned char t;
        t = p[i]; p[i] = p[i+3]; p[i+3] = t;
        t = p[i+1]; p[i+1] = p[i+2]; p[i+2] = t;
    }
}

/* ================================================================== */
/* 1. Tile size (0x1001CF30 -- opcode 0xF2, NOT the scissor)           */
/* ================================================================== */

static uint32_t pack12(uint32_t hi, uint32_t lo)
{
    return ((hi & 0xFFFu) << 12) | (lo & 0xFFFu);
}

static void test_tilesize(void)
{
    BrGbiState st;
    BrGfxWords cmd[2];

    memset(&st, 0, sizeof st);

    /* A 320x240 tile in 10.2 fixed point. */
    cmd[0].w0 = pack12(0, 0);
    cmd[0].w1 = pack12(320u * 4u - 4u, 240u * 4u - 4u);
    CHECK(BrGbiSetTileSize(&st, cmd) == cmd + 1);
    CHECK(st.tile.uls == 0 && st.tile.ult == 0);
    CHECK(st.tile.tileW == 320 && st.tile.tileH == 240);

    /* 0x7FF is the largest positive 12-bit value, 0x800 the most negative:
     * the sign extension must break exactly there. */
    cmd[0].w0 = pack12(0x7FFu, 0x800u);
    cmd[0].w1 = pack12(0xFFFu, 0u);
    BrGbiSetTileSize(&st, cmd);
    CHECK(st.tile.uls == 0x7FF);
    CHECK(st.tile.ult == -0x800);
    CHECK(st.tile.lrs == -1);
    CHECK(st.tile.lrt == 0);

    /* lrs < uls: the arithmetic shift must keep the extent negative rather
     * than turning it into a huge positive. */
    CHECK(st.tile.tileW < 0);
    /* (0 - (-0x800) + 4) >> 2 */
    CHECK(st.tile.tileH == (0x800 + 4) >> 2);

    /* 0x1001CF30 IS 0x1001EC30 IS br_dl.c's br_dl_settilesize: the D3D and
     * Glide builds hold the same 178-byte function in the same slot 0xF2.
     * This asserts the two ports agree rather than merely coexisting -- the
     * numbers below are br_dl_settilesize's for the same command words, and
     * were computed from the disassembly, not from either port. */
    cmd[0].w0 = pack12(0x004u, 0x008u);      /* uls = 1.0, ult = 2.0 texels */
    cmd[0].w1 = pack12(0x104u, 0x208u);      /* lrs = 65.0, lrt = 130.0     */
    BrGbiSetTileSize(&st, cmd);
    CHECK(st.tile.uls == 4 && st.tile.ult == 8);
    CHECK(st.tile.lrs == 0x104 && st.tile.lrt == 0x208);
    CHECK(st.tile.tileW == ((0x104 - 4 + 4) >> 2));
    CHECK(st.tile.tileH == ((0x208 - 8 + 4) >> 2));
}

/* ================================================================== */
/* 2. Geometry mode                                                    */
/* ================================================================== */

static void test_geometry_mode(void)
{
    BrGbiState st;
    BrGfxWords cmd[1];

    memset(&st, 0, sizeof st);
    g_geoChanged = 0;

    cmd[0].w0 = 0; cmd[0].w1 = 0x00030204u;
    CHECK(BrGbiSetGeometryMode(&st, cmd) == cmd + 1);
    CHECK(st.geo.cur == 0x00030204u);
    CHECK(st.geo.prev == 0);

    /* set then clear the same bits is a round trip on `cur`, and `prev`
     * always holds the value from before the change. */
    CHECK(BrGbiClearGeometryMode(&st, cmd) == cmd + 1);
    CHECK(st.geo.cur == 0);
    CHECK(st.geo.prev == 0x00030204u);
    CHECK(g_geoChanged == 2);
}

/* ================================================================== */
/* 3. Display-list stack                                               */
/* ================================================================== */

static void test_dl_stack(void)
{
    BrGbiState st;
    BrGfxWords cmd[4];
    int        i;

    memset(&st, 0, sizeof st);
    g_overflow = 0;

    cmd[0].w0 = 0x06000000u; cmd[0].w1 = 0x00112230u;
    CHECK((uintptr_t)BrGbiDList(&st, cmd) == cmd[0].w1);
    CHECK(st.dl.n == 1);
    CHECK(st.dl.ap[0] == cmd + 1);

    /* LIFO round trip. */
    CHECK(BrGbiEndDList(&st) == cmd + 1);
    CHECK(st.dl.n == 0);
    /* Empty pop yields NULL, which is what terminates BrGbiRun. */
    CHECK(BrGbiEndDList(&st) == NULL);
    CHECK(st.dl.n == 0);

    /* Any non-zero byte in w0 bits[23:16] suppresses the push but the return
     * value is unchanged. */
    cmd[1].w0 = 0x06010000u; cmd[1].w1 = 0x00445560u;
    CHECK((uintptr_t)BrGbiDList(&st, cmd + 1) == cmd[1].w1);
    CHECK(st.dl.n == 0);

    /* The overflow guard fires on the transition to n == 9, exactly once,
     * and the store still happens. */
    cmd[2].w0 = 0x06000000u; cmd[2].w1 = 0x1000u;
    for (i = 0; i < BR_GBI_DL_STACK_MAX; ++i)
        BrGbiDList(&st, cmd + 2);
    CHECK(st.dl.n == BR_GBI_DL_STACK_MAX);
    CHECK(g_overflow == 1);
}

/* ================================================================== */
/* 4. Matrix stack                                                     */
/* ================================================================== */

static void fill_mat(BrMat4 *pM, float base)
{
    int i, j;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            pM->m[i][j] = base + (float)(i * 4 + j);
}

static void test_matrix_stack(void)
{
    BrGbiState st;
    BrGfxWords cmd[1];
    BrMat4     in;
    int        i;

    memset(&st, 0, sizeof st);
    fill_mat(&in, 100.0f);

    /* Modelview load + push, ten times: the index walks 1..10. */
    cmd[0].w0 = 0x00060000u;    /* load | push, modelview */
    cmd[0].w1 = 0;
    for (i = 1; i <= 10; ++i) {
        CHECK(BrGbiMatrix(&st, cmd, &in) == cmd + 1);
        CHECK(st.mtx.top == i);
    }
    /* Eleventh push wraps 10 -> 0 -> 1 rather than overflowing. */
    BrGbiMatrix(&st, cmd, &in);
    CHECK(st.mtx.top == 1);

    /* Pop is the same ring in reverse: 1 -> 10, not 1 -> 0. */
    BrGbiPopMatrix(&st, cmd);
    CHECK(st.mtx.top == 10);
    for (i = 0; i < 10; ++i)
        BrGbiPopMatrix(&st, cmd);
    /* Ten more pops from 10 walk down to 1 then wrap back to 10. */
    CHECK(st.mtx.top >= 1 && st.mtx.top <= 10);

    /* A pop with the stack marked empty is a no-op. */
    st.mtx.top = 0;
    BrGbiPopMatrix(&st, cmd);
    CHECK(st.mtx.top == 0);

    /* With top == 0 the combine uses NULL for the modelview. */
    memset(&st, 0, sizeof st);
    g_mulCalls = 0;
    cmd[0].w0 = 0x00030000u;    /* projection | load */
    BrGbiMatrix(&st, cmd, &in);
    CHECK(g_mulA == NULL);
    CHECK(g_mulB == BrGbiMtxProj(&st.mtx));
    CHECK(memcmp(BrGbiMtxProj(&st.mtx), &in, sizeof in) == 0);
    /* Projection load ignores G_MTX_PUSH. */
    CHECK(st.mtx.top == 0);

    /* The documented aliasing: stack slot 0 starts 16 bytes into the
     * projection matrix, so writing one is visible through the other. */
    CHECK((char *)BrGbiMtxSlot(&st.mtx, 0) ==
          (char *)BrGbiMtxProj(&st.mtx) + 16);
    BrGbiMtxSlot(&st.mtx, 0)->m[0][0] = -7.5f;
    CHECK(BrGbiMtxProj(&st.mtx)->m[1][0] == -7.5f);
    /* Slot 1 onwards is clear of it. */
    CHECK((char *)BrGbiMtxSlot(&st.mtx, 1) >=
          (char *)BrGbiMtxProj(&st.mtx) + (int)sizeof(BrMat4));
}

/* ================================================================== */
/* 5. Clip codes                                                       */
/* ================================================================== */

static void test_clip_codes(void)
{
    float v[8];
    int   c;

    memset(v, 0, sizeof v);
    v[6] = 1.0f;                 /* w */

    /* Dead centre of the frustum: nothing outside. */
    CHECK(BrGbiClipCodes(v) == 0);

    /* w < 0 is bit 0 and, with all coordinates zero, also trips both sides
     * of every axis -- which is the point of the sign test. */
    v[6] = -1.0f;
    c = BrGbiClipCodes(v);
    CHECK((c & 0x01) != 0);
    v[6] = 1.0f;

    /* The +0x0C coordinate owns bits 2 and 4, and the two bits are mutually
     * exclusive for any finite |coord| > w. */
    v[3] = 2.0f;
    c = BrGbiClipCodes(v);
    CHECK(c == 0x04);
    v[3] = -2.0f;
    c = BrGbiClipCodes(v);
    CHECK(c == 0x02);
    v[3] = 0.0f;

    /* +0x04 owns 8/0x10 and +0x08 owns 0x20/0x40 -- the ORDER that matters. */
    v[1] = 2.0f;
    CHECK(BrGbiClipCodes(v) == 0x10);
    v[1] = 0.0f;
    v[2] = 2.0f;
    CHECK(BrGbiClipCodes(v) == 0x40);
    v[2] = 0.0f;

    /* Exactly on the plane is inside: the test is strict. */
    v[3] = 1.0f;
    CHECK(BrGbiClipCodes(v) == 0);

    /* NaN REJECTS, and nothing tested it -- which is how the wrong polarity
     * survived here while the other copy of this function had it right.
     *
     * `fcomp` sets C0 for less-than OR unordered, so the original clips a
     * vertex with a NaN in any component. Written as `x < 0.0f` the test is
     * false for NaN and the vertex is reported INSIDE, feeding a NaN into the
     * clipper. Every one of the seven planes is checked, because the bug was
     * seven separate comparisons and fixing six would look identical here. */
    {
        float v[7];
        int   k;
        static const int kBit[7] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40 };
        /* which component to poison for each plane: w, z, z, x, x, y, y */
        static const int kIdx[7] = { 6, 3, 3, 1, 1, 2, 2 };
        float nan;
        memcpy(&nan, "\x00\x00\xC0\x7F", 4);

        for (k = 0; k < 7; k++) {
            int f;
            v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f; v[3] = 0.0f;
            v[4] = 0.0f; v[5] = 0.0f; v[6] = 1.0f;      /* comfortably inside */
            CHECK(BrGbiClipCodes(v) == 0);
            v[kIdx[k]] = nan;
            f = BrGbiClipCodes(v);
            CHECK((f & kBit[k]) != 0);   /* a NaN must set this plane's bit */
        }
    }
}

/* ================================================================== */
/* 6. Per-vertex lighting                                              */
/* ================================================================== */

static void test_light_vertex(void)
{
    BrGbiLightState ls;
    float           src[8];
    float           dst[10];
    int             i;

    memset(&ls, 0, sizeof ls);
    memset(src, 0, sizeof src);
    memset(dst, 0, sizeof dst);

    ls.dir[0] = 1.0f; ls.dir[1] = 0.0f; ls.dir[2] = 0.0f;
    for (i = 0; i < 3; ++i) {
        ls.scale[i]   = 0.5f;
        ls.ambient[i] = 0.25f;
        ls.off[i]     = -1.0f - (float)i;
    }

    /* Lighting disabled takes the `off` triple, NOT the ambient one. */
    ls.numLights = 0;
    src[5] = 1.0f;
    BrGbiLightVertex(&ls, src, dst);
    CHECK(dst[7] == -1.0f && dst[8] == -2.0f && dst[9] == -3.0f);

    /* Negative dot falls back to ambient. */
    ls.numLights = 1;
    src[5] = -1.0f;
    BrGbiLightVertex(&ls, src, dst);
    CHECK(dst[7] == 0.25f && dst[8] == 0.25f && dst[9] == 0.25f);

    /* Positive dot scales and offsets, and never exceeds 1.0f. */
    src[5] = 1.0f;
    BrGbiLightVertex(&ls, src, dst);
    CHECK(dst[7] == 0.75f);
    src[5] = 1000.0f;
    BrGbiLightVertex(&ls, src, dst);
    for (i = 7; i < 10; ++i)
        CHECK(dst[i] == 1.0f);

    /* All three source components participate; a direction along the third
     * axis must be picked up from src[7] (the float at +0x1C). */
    ls.dir[0] = 0.0f; ls.dir[2] = 1.0f;
    src[5] = 0.0f; src[7] = 1.0f;
    BrGbiLightVertex(&ls, src, dst);
    CHECK(dst[7] == 0.75f);
}

/* ================================================================== */
/* 7. MOVEMEM / MOVEWORD                                               */
/* ================================================================== */

static void test_movemem_moveword(void)
{
    BrGbiState st;
    BrGfxWords cmd[1];
    uint8_t    src[16];
    BrMat4     m;
    int        i;

    memset(&st, 0, sizeof st);
    for (i = 0; i < 16; ++i)
        src[i] = (uint8_t)(0xA0 + i);

    /* G_MV_L0 (0x86) lands on light slot 0, G_MV_L7 (0x94) on slot 7. */
    cmd[0].w0 = 0x00860010u;    /* index 0x86, length 16 */
    cmd[0].w1 = 0;
    CHECK(BrGbiMoveMem(&st, cmd, src) == cmd + 1);
    CHECK(memcmp(st.lights.aRaw, src, 16) == 0);

    cmd[0].w0 = 0x00940010u;
    BrGbiMoveMem(&st, cmd, src);
    CHECK(memcmp(st.lights.aRaw + 7 * 16, src, 16) == 0);

    /* The odd indices in 0x80..0x9E reach the table's default arm and change
     * nothing but the cursor. */
    memset(&st.lights, 0, sizeof st.lights);
    cmd[0].w0 = 0x00870010u;
    CHECK(BrGbiMoveMem(&st, cmd, src) == cmd + 1);
    CHECK(st.lights.aRaw[0] == 0);

    /* 0x82 and 0x84 latch w1 into two distinct globals. */
    cmd[0].w0 = 0x00820000u; cmd[0].w1 = 0xDEAD0001u;
    BrGbiMoveMem(&st, cmd, src);
    cmd[0].w0 = 0x00840000u; cmd[0].w1 = 0xDEAD0002u;
    BrGbiMoveMem(&st, cmd, src);
    CHECK(st.f1698 == 0xDEAD0001u && st.f169C == 0xDEAD0002u);

    /* 0x9E is the matrix arm. */
    fill_mat(&m, 3.0f);
    cmd[0].w0 = 0x009E0000u;
    BrGbiMoveMem(&st, cmd, &m);
    CHECK(memcmp(&st.mtx.combined, &m, sizeof m) == 0);

    /* Out of range: no dispatch at all. */
    cmd[0].w0 = 0x00A00010u;
    CHECK(BrGbiMoveMem(&st, cmd, src) == cmd + 1);

    /* G_MOVEWORD 0x02 is numlights = (w1 >> 5) & 0xF. */
    cmd[0].w0 = 0x00000002u; cmd[0].w1 = (7u << 5);
    CHECK(BrGbiMoveWord(&st, cmd) == cmd + 1);
    CHECK(st.light.numLights == 7);
    cmd[0].w1 = (0x1Fu << 5);   /* the mask really is four bits */
    BrGbiMoveWord(&st, cmd);
    CHECK(st.light.numLights == 0xF);

    /* G_MOVEWORD 0x0A: offset 0x20 * n picks light n, low nibble picks the
     * first or the second colour triple. */
    memset(&st.lights, 0, sizeof st.lights);
    cmd[0].w0 = 0x0000000Au | ((0x20u * 3u) << 8);
    cmd[0].w1 = 0x112233FFu;
    BrGbiMoveWord(&st, cmd);
    CHECK(st.lights.aRaw[3 * 16 + 0] == 0x11);
    CHECK(st.lights.aRaw[3 * 16 + 1] == 0x22);
    CHECK(st.lights.aRaw[3 * 16 + 2] == 0x33);
    CHECK(st.lights.aRaw[3 * 16 + 4] == 0x00);

    cmd[0].w0 = 0x0000000Au | ((0x20u * 3u + 4u) << 8);
    BrGbiMoveWord(&st, cmd);
    CHECK(st.lights.aRaw[3 * 16 + 4] == 0x11);
    CHECK(st.lights.aRaw[3 * 16 + 6] == 0x33);

    /* Selectors outside 0x02..0x0E, and the sign-extended negatives, are
     * rejected by the range check. */
    st.light.numLights = 5;
    cmd[0].w0 = 0x00000001u; BrGbiMoveWord(&st, cmd);
    cmd[0].w0 = 0x0000000Fu; BrGbiMoveWord(&st, cmd);
    cmd[0].w0 = 0x000000FFu; BrGbiMoveWord(&st, cmd);
    CHECK(st.light.numLights == 5);
}

/* ================================================================== */
/* 8. Tile-rect handlers                                               */
/* ================================================================== */

static void test_tile_rects(void)
{
    BrGbiState st;
    BrGfxWords cmd[4];

    memset(&st, 0, sizeof st);
    memset(cmd, 0, sizeof cmd);

    cmd[0].w0 = pack12(0x010u, 0x020u);           /* uls, ult */
    cmd[0].w1 = (5u << 24) | pack12(0x030u, 0x040u);  /* tile, lrs, lrt */

    g_tileCalls = 0;
    /* The lower-right pair goes first -- an inverted argument order. */
    CHECK(BrGbiTileRect(&st, cmd) == cmd + 3);     /* three commands! */
    CHECK(g_tileCalls == 1);
    CHECK(g_tileArgs[0] == 0x030 && g_tileArgs[1] == 0x040);
    CHECK(g_tileArgs[2] == 0x010 && g_tileArgs[3] == 0x020);
    CHECK(g_tileArgs[4] == 5);

    /* The scaled sibling multiplies every coordinate by four and consumes
     * exactly one command. */
    CHECK(BrGbiTileRectS(&st, cmd) == cmd + 1);
    CHECK(g_tileArgs[0] == 0x030 * 4 && g_tileArgs[1] == 0x040 * 4);
    CHECK(g_tileArgs[2] == 0x010 * 4 && g_tileArgs[3] == 0x020 * 4);
    CHECK(g_tileArgs[4] == 5);
}

/* ================================================================== */
/* 9. The interpreter and the 0x10020F50 dispatcher                    */
/* ================================================================== */

static BrGfxWords *h_step(BrGfxWords *pCmd) { return pCmd + 1; }
static BrGfxWords *h_stop(BrGfxWords *pCmd) { (void)pCmd; return NULL; }

static void test_run(void)
{
    BrGbiHandler table[256];
    BrGfxWords   list[8];
    BrGbiState   st;
    int          i;

    for (i = 0; i < 256; ++i)
        table[i] = h_step;
    table[0xB8] = h_stop;

    for (i = 0; i < 3; ++i) { list[i].w0 = 0x01000000u; list[i].w1 = 0; }
    list[3].w0 = 0xB8000000u; list[3].w1 = 0;

    /* Terminates rather than running off the end. */
    BrGbiRun(table, list);
    /* A NULL start is checked before the first dispatch. */
    BrGbiRun(table, NULL);

    memset(&st, 0, sizeof st);
    g_fa0Calls = 0;
    /* Selector 0 tails into 0x100243D0 (stand-in returns pCmd + 7). */
    list[0].w0 = 0x00000000u;
    CHECK(BrGbiDispatch10020F50(&st, list) == list + 7);
    /* Selector 3 tails into 0x10020F80. */
    list[0].w0 = 0x00030000u; list[0].w1 = 0xCAFE0001u;
    CHECK(BrGbiDispatch10020F50(&st, list) == list + 1);
    CHECK(st.f1694 == 0xCAFE0001u);
    CHECK(g_fa0Calls == 1 && g_fa0Arg == 0xCAFE0001u);
    /* Anything else just steps. Note the byte is SIGN-extended, so 0x83 is
     * -125 and must not be mistaken for 3. */
    list[0].w0 = 0x00830000u;
    CHECK(BrGbiDispatch10020F50(&st, list) == list + 1);
}

/* ================================================================== */
/* 10. Size helpers and the blit thunk                                 */
/* ================================================================== */

static uintptr_t g_blit[15];
static int       g_blitCalls;

static void blit_stub(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
                      uintptr_t pitch, uintptr_t a5, uintptr_t a6,
                      uintptr_t a7, uintptr_t a8, uintptr_t a9,
                      uintptr_t a10, uintptr_t a11, uintptr_t a12,
                      uintptr_t a13, uintptr_t a14)
{
    g_blit[0]=a1;  g_blit[1]=a2;  g_blit[2]=a3;  g_blit[3]=a4;
    g_blit[4]=pitch; g_blit[5]=a5; g_blit[6]=a6; g_blit[7]=a7;
    g_blit[8]=a8;  g_blit[9]=a9;  g_blit[10]=a10; g_blit[11]=a11;
    g_blit[12]=a12; g_blit[13]=a13; g_blit[14]=a14;
    g_blitCalls++;
}

static void test_size_helpers(void)
{
    int n;

    /* ceil(log2) over the whole declared range: 1<<shift is the smallest
     * power of two that is >= n. */
    for (n = 1; n <= 0x80; ++n) {
        int s = BrGbiSizeShift(n);
        CHECK((1 << s) >= n);
        CHECK(s == 0 || (1 << (s - 1)) < n);
    }
    /* Saturates, and negatives collapse to 0 because the compares are
     * signed. */
    CHECK(BrGbiSizeShift(0x81) == 8);
    CHECK(BrGbiSizeShift(1 << 20) == 8);
    CHECK(BrGbiSizeShift(0) == 0);
    CHECK(BrGbiSizeShift(-4) == 0);

    /* Texels per 64-bit word halve as the size code grows, then stick. */
    CHECK(BrGbiTexelsPerWord(0) == 16);
    CHECK(BrGbiTexelsPerWord(1) == 8);
    CHECK(BrGbiTexelsPerWord(2) == 4);
    CHECK(BrGbiTexelsPerWord(3) == 2);
    CHECK(BrGbiTexelsPerWord(99) == 2);
    CHECK(BrGbiTexelsPerWord(-1) == 2);

    /* The thunk inserts the pitch as the FIFTH argument and shifts the rest
     * along; nothing else is reordered. */
    g_blitCalls = 0;
    BrGbiBlit(blit_stub, 101, 102, 40 /* width */, 104,
              2 /* siz */, 106, 107, 108, 109, 110, 111, 112, 113, 114);
    CHECK(g_blitCalls == 1);
    CHECK(g_blit[0] == 101 && g_blit[1] == 102);
    CHECK(g_blit[2] == 40 && g_blit[3] == 104);
    /* width 40 rounds to 64; 64 / 4 texels-per-word = 16 words = 128 bytes */
    CHECK(g_blit[4] == 128);
    CHECK(g_blit[5] == 2);
    CHECK(g_blit[6] == 106 && g_blit[14] == 114);
}

/* ================================================================== */
/* 11. Texture create                                                  */
/* ================================================================== */

static uint32_t g_created[12];
static void    *g_createdSrc;
static int      g_createCalls;

static void *create_stub(void *pSrc, uintptr_t a2, uint32_t w, uint32_t h,
                         uint32_t fmt, uint32_t siz,
                         uint32_t b31, uint32_t b30, uint32_t b29,
                         uint32_t b28, uint32_t a11, uint32_t a12,
                         uint32_t a13, uintptr_t a14)
{
    g_createdSrc = pSrc;
    g_created[0]=(uint32_t)a2; g_created[1]=w; g_created[2]=h;
    g_created[3]=fmt; g_created[4]=siz;
    g_created[5]=b31; g_created[6]=b30; g_created[7]=b29; g_created[8]=b28;
    g_created[9]=a11; g_created[10]=a12; g_created[11]=a13;
    (void)a14;
    g_createCalls++;
    return (void *)(uintptr_t)0x5A5A5A5Au;
}

static void test_tex_create(void)
{
    BrGbiTexRec   rec;
    BrGbiSolidTex solid;
    int           i;

    memset(&rec, 0, sizeof rec);
    g_createCalls = 0;

    /* A NULL texture is an early-out, so this routine never CREATES one. */
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_createCalls == 0);

    rec.pTex = (void *)(uintptr_t)0x1000;
    rec.w = 33; rec.h = 5;
    rec.flags = 0x100000u;      /* the second early-out */
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_createCalls == 0);

    /* bits[27:24] == 1 selects (fmt 0, siz 2); dimensions round up. */
    rec.flags = 0x01000000u;
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_createCalls == 1);
    CHECK(g_created[1] == 64 && g_created[2] == 8);
    CHECK(g_created[3] == 0 && g_created[4] == 2);
    CHECK(rec.pTex == (void *)(uintptr_t)0x5A5A5A5Au);

    /* bits[27:24] == 4 selects (1, 4); anything else selects (2, 0). */
    rec.flags = 0x04000000u;
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_created[3] == 1 && g_created[4] == 4);
    rec.flags = 0x02000000u;
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_created[3] == 2 && g_created[4] == 0);

    /* The four top bits are forwarded individually, in order 31,30,29,28. */
    rec.flags = 0xA0000000u;    /* bits 31 and 29 */
    BrGbiTexCreate(create_stub, &rec, 0);
    CHECK(g_created[5] == 1 && g_created[6] == 0);
    CHECK(g_created[7] == 1 && g_created[8] == 0);

    /* The solid placeholder: 4x4, filled with 0x20 for modes 2 and 3 and
     * 0x80 otherwise, all sixteen bytes. */
    memset(&solid, 0, sizeof solid);
    solid.mode = 2;
    BrGbiSolidTexBuild(create_stub, &solid);
    for (i = 0; i < 16; ++i)
        CHECK(solid.aTexels[i] == 0x20);
    CHECK(g_createdSrc == solid.aTexels);
    CHECK(g_created[1] == 4 && g_created[2] == 4);
    CHECK(g_created[3] == 1 && g_created[4] == 4);
    CHECK(solid.pTex == (void *)(uintptr_t)0x5A5A5A5Au);

    solid.mode = 1;
    BrGbiSolidTexBuild(create_stub, &solid);
    for (i = 0; i < 16; ++i)
        CHECK(solid.aTexels[i] == 0x80);
    solid.mode = 3;
    BrGbiSolidTexBuild(create_stub, &solid);
    CHECK(solid.aTexels[15] == 0x20);
}

/* ================================================================== */
/* 12. Texture-load scan                                               */
/* ================================================================== */

/* Big enough that the deliberately-oversized G_LOADBLOCK below has a real
 * source to read: the port clamps the DESTINATION, not the source, exactly
 * as the original does. */
static uint8_t g_texData[BR_GBI_STAGE_SIZE];

static const void *scan_data(void *pUser, uint32_t addr)
{
    (void)pUser;
    if (addr == 0x0A000000u)
        return g_texData;
    return NULL;
}

static void test_tex_scan(void)
{
    static BrGbiTexScan st;
    BrGfxWords          list[8];
    uint8_t             tlut[512];
    int                 i;

    for (i = 0; i < (int)sizeof g_texData; ++i)
        g_texData[i] = (uint8_t)i;

    memset(&st, 0, sizeof st);
    st.pfnData  = scan_data;
    st.pTlutDst = tlut;

    /* A full load run: SETTIMG, LOADSYNC, LOADBLOCK, TILESYNC, SETTILE,
     * SETTILESIZE, then a triangle to close it, then ENDDL. */
    list[0].w0 = 0xFD000000u | (2u << 19);     /* G_SETTIMG, siz 2 */
    list[0].w1 = 0x0A000000u;
    list[1].w0 = 0xE6000000u; list[1].w1 = 0;  /* G_RDPLOADSYNC */
    list[2].w0 = 0xF3000000u | pack12(0, 0);   /* G_LOADBLOCK */
    list[2].w1 = pack12(31u, 0);               /* 31 - 0 -> 64 bytes */
    list[3].w0 = 0xE8000000u; list[3].w1 = 0;  /* G_RDPTILESYNC */
    list[4].w0 = 0xF5000000u | (3u << 21) | (2u << 19) | (8u << 9) | 0x100u;
    list[4].w1 = (4u << 24) | (1u << 8) | (5u << 4);
    list[5].w0 = 0xF2000000u | pack12(0x10u, 0x20u);
    list[5].w1 = (4u << 24) | pack12(0x30u, 0x40u);
    list[6].w0 = 0xBF000000u; list[6].w1 = 0;  /* G_TRI1 closes the run */
    list[7].w0 = 0xB8000000u; list[7].w1 = 0;  /* G_ENDDL */

    g_registerId = 0x123456;
    BrGbiTexScanRun(&st, list);

    /* The staged bytes are 2*((lrs-uls)+1) and come from the SETTIMG
     * address. */
    CHECK(st.stageLen == 64);
    CHECK(memcmp(st.aStage, g_texData, 64) == 0);
    CHECK(g_registerArg == st.aStage);

    /* The run's first command has been rewritten in place. */
    CHECK(list[0].w0 == (0xDC000000u | 0x123456u));
    /* Length in commands: the run ends at the first non-participating
     * command, which is the triangle at index 6. */
    CHECK(list[0].w1 == 6);
    CHECK(st.state == 0);

    /* The tile record picked up all of G_SETTILE and G_SETTILESIZE. */
    CHECK(st.aTiles[4].fmt == 3);
    CHECK(st.aTiles[4].siz == 2);
    CHECK(st.aTiles[4].line == 8 * 8);
    CHECK(st.aTiles[4].tmem == 0x100);
    CHECK(st.aTiles[4].mirrorS == 1);
    CHECK(st.aTiles[4].maskS == 5);
    CHECK(st.aTiles[4].uls == 0x10 && st.aTiles[4].ult == 0x20);
    CHECK(st.aTiles[4].lrs == 0x30 && st.aTiles[4].lrt == 0x40);
    CHECK(st.maxTile == 4);

    /* A run that is never closed leaves the command alone. */
    memset(&st, 0, sizeof st);
    st.pfnData = scan_data;
    st.pTlutDst = tlut;
    list[0].w0 = 0xFD000000u; list[0].w1 = 0x0A000000u;
    list[1].w0 = 0xB8000000u;
    BrGbiTexScanRun(&st, list);
    CHECK(list[0].w0 == 0xFD000000u);
    CHECK(st.state == 1);

    /* The one recognised G_SETCOMBINE sets a flag; any other clears it. */
    memset(&st, 0, sizeof st);
    st.pfnData = scan_data;
    list[0].w0 = 0xFC50FE04u; list[0].w1 = 0x3FFDF3F8u;
    list[1].w0 = 0xB8000000u;
    BrGbiTexScanRun(&st, list);
    CHECK(st.f575448 == 1);
    list[0].w1 = 0x3FFDF3F9u;
    BrGbiTexScanRun(&st, list);
    CHECK(st.f575448 == 0);

    /* Prim and env colours are four separate bytes, most significant first. */
    memset(&st, 0, sizeof st);
    st.pfnData = scan_data;
    list[0].w0 = 0xFA000000u; list[0].w1 = 0x11223344u;
    list[1].w0 = 0xFB000000u; list[1].w1 = 0x55667788u;
    list[2].w0 = 0xB8000000u;
    BrGbiTexScanRun(&st, list);
    CHECK(st.prim[0] == 0x11 && st.prim[3] == 0x44 && st.f575444 == 1);
    CHECK(st.env[0] == 0x55 && st.env[3] == 0x88 && st.f575440 == 1);

    /* The staging copy is clamped rather than smashing memory when the
     * command asks for more than the buffer holds. */
    memset(&st, 0, sizeof st);
    st.pfnData = scan_data;
    st.state = 2;
    st.timgAddr = 0x0A000000u;
    list[0].w0 = 0xF3000000u | pack12(0, 0);
    list[0].w1 = pack12(0xFFFu, 0);
    BrGbiTexScanLoadBlock(&st, list, g_texData);
    CHECK(st.stageLen == 2u * 0x1000u);
    CHECK(st.state == 3);
}

/* ================================================================== */
/* 13. Fade                                                            */
/* ================================================================== */

static int g_fadeRelease;
static void fade_release(void) { g_fadeRelease++; }

static void test_fade_state(void)
{
    BrFadeState st;

    /* --- refcount --- */
    memset(&st, 0, sizeof st);
    st.pfnRelease = fade_release;
    st.refCount = 2;
    g_fadeRelease = 0;
    CHECK(BrFadeRelease(&st) == 1);
    CHECK(g_fadeRelease == 0);
    CHECK(BrFadeRelease(&st) == 1);
    CHECK(g_fadeRelease == 1);
    /* GOTCHA: the test is `== 0`, so it fires once and never again. */
    BrFadeRelease(&st);
    CHECK(g_fadeRelease == 1);
    CHECK(st.refCount == -1);

    /* --- latch --- */
    memset(&st, 0, sizeof st);
    st.srcC0 = 11; st.srcC4 = 22;
    BrFadeLatch(&st);
    CHECK(st.pos == 11 && st.f5754FC == 22);

    /* --- aiming --- */
    memset(&st, 0, sizeof st);
    st.value = 0.0f;
    BrFadeSetTarget(&st, 1.0f, 4.0f);
    CHECK(st.kick == 1);
    CHECK(st.target == 1.0f);
    CHECK(st.rate == 0.25f);       /* +1 / over */

    /* Aiming backwards from a fully-open wipe with a positive rate arms the
     * bounce instead of retargeting. */
    st.value = 0.5f;
    st.rate  = 0.25f;
    BrFadeSetTarget(&st, 0.0f, 4.0f);
    CHECK(st.bounce == 1);
    CHECK(st.target == 1.0f);      /* untouched */

    /* At value == 1.0f the bounce arm is skipped and it retargets with a
     * negative rate. */
    st.bounce = 0;
    st.value  = 1.0f;
    BrFadeSetTarget(&st, 0.0f, 4.0f);
    CHECK(st.bounce == 0);
    CHECK(st.target == 0.0f);
    CHECK(st.rate == -0.25f);
}

static void test_fade_predicates(void)
{
    BrFadeState st;

    memset(&st, 0, sizeof st);
    st.rate = 1.0f; st.value = 0.5f; st.target = 0.5f;
    CHECK(BrFadeIsClosing(&st) == 0);
    CHECK(BrFadeIsSettled(&st) == 1);
    CHECK(BrFadeIsShut(&st) == 0);

    st.rate = -1.0f;
    CHECK(BrFadeIsClosing(&st) == 1);
    CHECK(BrFadeIsShut(&st) == 0);      /* value is not 0 yet */
    st.value = 0.0f;
    CHECK(BrFadeIsShut(&st) == 1);

    /* The bounce flag overrides all three, in opposite directions. */
    st.bounce = 1;
    CHECK(BrFadeIsShut(&st) == 0);
    CHECK(BrFadeIsClosing(&st) == 1);
    st.target = st.value;
    CHECK(BrFadeIsSettled(&st) == 0);
}

static void test_fade_tick(void)
{
    BrFadeState st;
    int         i;

    memset(&st, 0, sizeof st);
    st.dt   = 0.25f;
    st.span = 400;
    st.width = 320;

    /* Opening: value integrates toward the target and stops exactly on it,
     * never past it. */
    st.value = 0.0f;
    BrFadeSetTarget(&st, 1.0f, 1.0f);   /* rate = +1 */
    for (i = 0; i < 20; ++i) {
        BrFadeTick(&st);
        CHECK(st.value <= 1.0f);
    }
    CHECK(st.value == 1.0f);
    CHECK(BrFadeIsSettled(&st) == 1);
    /* rate > 0 leaves pos2 at zero and pos at span*value, rounded up to a
     * multiple of four. */
    CHECK(st.pos2 == 0);
    CHECK(st.pos == 400);
    CHECK((st.pos & 3) == 0);

    /* Closing: the wipe walks back and clamps at the target. */
    BrFadeSetTarget(&st, 0.0f, 1.0f);   /* rate = -1 from value 1.0 */
    CHECK(st.rate == -1.0f);
    for (i = 0; i < 20; ++i) {
        CHECK(st.value >= 0.0f);
        BrFadeTick(&st);
    }
    CHECK(st.value == 0.0f);
    CHECK(BrFadeIsShut(&st) == 1);

    /* The bounce: arming it while the wipe is still opening makes the tick
     * that lands on the target reverse instead of stopping. */
    memset(&st, 0, sizeof st);
    st.dt = 1.0f;
    st.span = 400;
    st.value = 0.0f; st.target = 1.0f; st.rate = 1.0f;
    st.bounce = 1;
    BrFadeTick(&st);
    CHECK(st.value == 1.0f);
    CHECK(st.rate == -1.0f);
    CHECK(st.target == 0.0f);
    CHECK(st.bounce == 0);

    /* The `kick` flag makes exactly one tick a no-op on `value`. */
    memset(&st, 0, sizeof st);
    st.dt = 1.0f; st.span = 400;
    st.value = 0.25f; st.target = 1.0f; st.rate = 1.0f; st.kick = 1;
    BrFadeTick(&st);
    CHECK(st.value == 0.25f);
    CHECK(st.kick == 0);
    BrFadeTick(&st);
    CHECK(st.value == 1.0f);

    /* The two ramps clamp on their own targets and publish 0..255. */
    memset(&st, 0, sizeof st);
    st.dt = 0.25f;
    st.span = 100;
    BrFadeSetTargetA(&st, 1.0f, 1.0f);
    BrFadeSetTargetB(&st, 1.0f, 1.0f);
    for (i = 0; i < 12; ++i) {
        BrFadeTick(&st);
        CHECK(st.curA <= 1.0f && st.curB <= 1.0f);
    }
    CHECK(st.curA == 1.0f && st.curB == 1.0f);
    CHECK(st.outA == 255 && st.outB == 255);

    /* Ramping back down clamps at the target rather than going negative. */
    BrFadeSetTargetA(&st, 0.0f, 1.0f);
    CHECK(st.rateA == -1.0f);
    for (i = 0; i < 12; ++i)
        BrFadeTick(&st);
    CHECK(st.curA == 0.0f);
    CHECK(st.outA == 0);

    /* Parity selects which of the two history slots a tick writes. */
    memset(&st, 0, sizeof st);
    st.pos = 7; st.pos2 = 9; st.parity = 1; st.rate = 0.0f;
    st.span = 50;
    BrFadeTick(&st);
    CHECK(st.aPos[1] == 7 && st.aPos2[1] == 9);
    CHECK(st.aPos[0] == 0 && st.aPos2[0] == 0);
    /* rate == 0 snaps pos to span and clears pos2. */
    CHECK(st.pos == 50 && st.pos2 == 0);
}

static void test_fade_emit(void)
{
    BrFadeState st;
    BrGfxWords  buf[32];
    uint32_t    recs[BR_FADE_RECT_DWORDS * 2];
    int         i;

    memset(&st, 0, sizeof st);
    memset(buf, 0, sizeof buf);
    memset(recs, 0, sizeof recs);
    st.pCmd = buf;
    st.otherModeH = 0xABCDEF01u;
    st.rectIdx = 1;
    recs[BR_FADE_RECT_DWORDS + 0] = 0x111;
    recs[BR_FADE_RECT_DWORDS + 1] = 0x222;
    recs[BR_FADE_RECT_DWORDS + 2] = 0x333;
    recs[BR_FADE_RECT_DWORDS + 3] = 0x444;

    /* Below the low threshold nothing is emitted at all. */
    BrFadeDrawSprite(&st, recs, 0.05f);
    CHECK(st.pCmd == buf);

    g_combineCalls = 0;
    BrFadeDrawSprite(&st, recs, 0.5f);
    CHECK(st.pCmd == buf + 10);
    CHECK(buf[0].w0 == 0xE7000000u);
    CHECK(buf[1].w0 == 0xBA001402u);
    CHECK(buf[2].w0 == 0xB900031Du && buf[2].w1 == 0x00504340u);
    /* prim colour: white with the alpha in the low byte. */
    CHECK(buf[4].w0 == 0xFA000000u);
    CHECK((buf[4].w1 & 0xFFFFFF00u) == 0xFFFFFF00u);
    CHECK((buf[4].w1 & 0xFFu) == 127u);          /* trunc(0.5 * 255) */
    /* the rectangle command, from record 1's first four dwords */
    CHECK(buf[6].w0 == (0xE1000000u |
                        ((((0x333u + 0x111u) << 12) & 0xFFF000u)) |
                        ((0x444u + 0x222u) & 0xFFFu)));
    CHECK(buf[6].w1 == ((0x111u << 12) | 0x222u));
    /* the tail restores the saved othermode */
    CHECK(buf[8].w0 == 0xE7000000u);
    CHECK(buf[9].w0 == 0xBA000602u && buf[9].w1 == 0xABCDEF01u);
    CHECK(g_combineCalls == 2);
    /* The second combine's four live tokens: d0/Ad0 = 0x3EB, d1/Ad1 = 0x3E8 */
    CHECK(g_combine[3] == 0x3EB && g_combine[7] == 0x3EB);
    CHECK(g_combine[11] == 0x3E8 && g_combine[15] == 0x3E8);
    for (i = 0; i < 16; ++i)
        if (i != 3 && i != 7 && i != 11 && i != 15)
            CHECK(g_combine[i] == 0);

    /* Alpha above the cap is pulled down to 0.7 before it is used. */
    memset(buf, 0, sizeof buf);
    st.pCmd = buf;
    BrFadeDrawSprite(&st, recs, 1.0f);
    CHECK((buf[4].w1 & 0xFFu) == 178u);          /* trunc(0.7f * 255) */

    /* The bar emitter is a no-op while the wipe is fully open. */
    memset(&st, 0, sizeof st);
    st.pCmd = buf;
    st.value = 1.0f;
    BrFadeDrawBars(&st);
    CHECK(st.pCmd == buf);

    /* Otherwise it always brackets its work with a pipe sync. */
    memset(buf, 0, sizeof buf);
    st.value = 0.5f;
    st.span = 200; st.width = 320; st.shift = 2;
    st.pos = 0; st.pos2 = 0; st.bars = 0;
    st.pCmd = buf;
    BrFadeDrawBars(&st);
    CHECK(buf[0].w0 == 0xE7000000u);
    CHECK(st.pCmd[-1].w0 == 0xE7000000u);
    CHECK(buf[4].w0 == 0xE2000000u);
    CHECK(buf[4].w1 == (((uint32_t)(320 << 2) & 0xFFFu) |
                        (((uint32_t)(200 << 2) & 0xFFFu) << 12)));
    CHECK(buf[5].w0 == 0xFA00FFFFu);

    /* `bars` is a countdown: each emitting pass spends one. */
    memset(buf, 0, sizeof buf);
    st.pCmd = buf;
    st.bars = 2;
    st.pos = 0;
    BrFadeDrawBars(&st);
    CHECK(st.bars == 1);
}

/* ================================================================== */
/* 14. .rca swap and fixup helpers                                     */
/* ================================================================== */

static void test_swaps(void)
{
    uint8_t a[32], b[32];
    int     i;

    for (i = 0; i < 32; ++i)
        a[i] = (uint8_t)(i * 7 + 1);
    memcpy(b, a, sizeof a);

    /* Copy exactly 32 bytes, destination first. */
    memset(b, 0, sizeof b);
    BrCopy8Words(b, a);
    CHECK(memcmp(a, b, 32) == 0);

    /* Every swapper is its own inverse. */
    BrSwapU16Array(a, 16);
    CHECK(memcmp(a, b, 32) != 0);
    BrSwapU16Array(a, 16);
    CHECK(memcmp(a, b, 32) == 0);

    /* THESE ROUND TRIPS CANNOT FAIL ON THEIR OWN, and the equivalence audit
     * caught it: applying an involution twice and comparing to the input
     * passes for a NO-OP, and for any wrong-but-symmetric permutation. The
     * BrSwapU16Array block above does assert "it changed" after a single
     * application; that assertion was simply not written for these two.
     *
     * A round trip is still worth keeping -- it catches an asymmetric bug --
     * but it has to be paired with a single-application check against an
     * independently-computed expectation, or it certifies nothing. */
    BrSwapU16x4Array(a, 4);
    CHECK(memcmp(a, b, 32) != 0);            /* it actually did something */
    {   /* and it did the RIGHT thing: 16 independent u16 swaps, computed
         * here byte-wise rather than by calling the function under test */
        unsigned char exp[32]; int k;
        memcpy(exp, b, 32);
        for (k = 0; k < 32; k += 2) {
            unsigned char t = exp[k]; exp[k] = exp[k+1]; exp[k+1] = t;
        }
        CHECK(memcmp(a, exp, 32) == 0);
    }
    BrSwapU16x4Array(a, 4);
    CHECK(memcmp(a, b, 32) == 0);            /* and it round-trips */

    BrSwapVec3Array(a, 2);
    CHECK(memcmp(a, b, 24) != 0);            /* same treatment */
    {   /* six 32-bit lanes, each byte-reversed */
        unsigned char exp[24]; int k;
        memcpy(exp, b, 24);
        for (k = 0; k < 24; k += 4) {
            unsigned char t;
            t = exp[k];   exp[k]   = exp[k+3]; exp[k+3] = t;
            t = exp[k+1]; exp[k+1] = exp[k+2]; exp[k+2] = t;
        }
        CHECK(memcmp(a, exp, 24) == 0);
    }
    BrSwapVec3Array(a, 2);
    CHECK(memcmp(a, b, 24) == 0);

    /* A single 8-byte record: four independent u16 swaps, and the 8-byte
     * form agrees with the array form. */
    BrSwapU16x4(a);
    BrSwapU16Array(b, 4);
    CHECK(memcmp(a, b, 8) == 0);
    BrSwapU16x4(a);
    BrSwapU16Array(b, 4);

    /* Non-positive counts do nothing. */
    BrSwapU16Array(a, 0);
    BrSwapU16Array(a, -3);
    BrSwapU16x4Array(a, -1);
    BrSwapVec3Array(a, 0);
    CHECK(memcmp(a, b, 32) == 0);
}

static void test_swap_mesh(void)
{
    uint8_t mesh[8 + 2 * 12];
    int     i;

    memset(mesh, 0, sizeof mesh);
    mesh[0] = 0xAA; mesh[1] = 0xBB;           /* untouched */
    mesh[2] = 0x00; mesh[3] = 0x02;           /* big-endian count 2 */
    mesh[4] = 0x01; mesh[5] = 0x02; mesh[6] = 0x03; mesh[7] = 0x04;
    for (i = 0; i < 24; ++i)
        mesh[8 + i] = (uint8_t)(i + 1);

    BrRcaSwapMesh(mesh);

    CHECK(mesh[0] == 0xAA && mesh[1] == 0xBB);
    /* count byte-swapped */
    CHECK(mesh[2] == 0x02 && mesh[3] == 0x00);
    /* the u32 at +4 reversed */
    CHECK(mesh[4] == 0x04 && mesh[5] == 0x03 &&
          mesh[6] == 0x02 && mesh[7] == 0x01);
    /* six u32s reversed in place, stride 12 across two entries */
    for (i = 0; i < 6; ++i) {
        const uint8_t *p = mesh + 8 + i * 4;
        CHECK(p[0] == (uint8_t)(i * 4 + 4));
        CHECK(p[3] == (uint8_t)(i * 4 + 1));
    }
    /* NULL is checked. */
    BrRcaSwapMesh(NULL);
}

/* --- 0x1002BAA0 ---------------------------------------------------- */

static uint8_t  g_dstA[0x400];
static uint8_t  g_dstB[0x400];
static uint8_t  g_mesh[8 + 3 * 12];
static uint8_t  g_blob[0x800];

static void *rca_resolve(void *pUser, uint32_t addr)
{
    (void)pUser;
    switch (addr) {
    case 0x00010000u: return g_dstA;
    case 0x00020000u: return g_dstB;
    case 0x00030000u: return g_mesh;
    default:          return NULL;
    }
}

/* Store a u32 in big-endian order, which is how the .rca payload holds it. */
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static void test_rca_fixup(void)
{
    BrSegMap    seg;
    BrRcaFixup  ctx;
    uint8_t     rec[BR_RCA_REC_SIZE];
    int         i;

    for (i = 0; i < (int)sizeof g_blob; ++i)
        g_blob[i] = (uint8_t)(i ^ 0x5A);

    seg.n64Base  = 0;
    seg.hostBase = 0;      /* identity fixup, so the addresses stay as written */

    memset(&ctx, 0, sizeof ctx);
    ctx.pSeg = &seg;
    ctx.enable = 1;
    ctx.pBlob = g_blob;
    ctx.pfnResolve = rca_resolve;

    /* --- bit 20 clear: +0x08 is a 12-bit blob index scaled by 32 --- */
    memset(rec, 0, sizeof rec);
    put_be32(rec + 0x00, 0x00010000u);
    put_be32(rec + 0x04, 0x00020000u);
    put_be32(rec + 0x08, 0x00000003u);    /* index 3 -> blob + 96 */
    rec[0x0C] = 0x12; rec[0x0D] = 0x34;
    put_be32(rec + 0x20, 0x02000000u);    /* bits[27:24] == 2 -> length 0x200 */

    memset(g_dstB, 0, sizeof g_dstB);
    g_releaseCalls = 0;
    BrRcaFixupRecord(&ctx, rec);

    /* The record's own words came out byte-swapped. */
    CHECK(rec[0x0C] == 0x34 && rec[0x0D] == 0x12);
    CHECK(memcmp(g_dstB, g_blob + 3 * 32, 0x200) == 0);
    CHECK(g_releaseCalls == 1 && g_releaseArg == g_dstB);

    /* bits[27:24] == 1 is the ONLY value that shortens the copy to 0x20. */
    memset(g_dstB, 0, sizeof g_dstB);
    put_be32(rec + 0x00, 0x00010000u);
    put_be32(rec + 0x04, 0x00020000u);
    put_be32(rec + 0x08, 0x00000003u);
    put_be32(rec + 0x20, 0x01000000u);
    BrRcaFixupRecord(&ctx, rec);
    CHECK(memcmp(g_dstB, g_blob + 3 * 32, 0x20) == 0);
    CHECK(g_dstB[0x20] == 0);

    /* --- bit 20 set: +0x08 points at a mesh header --- */
    memset(g_mesh, 0, sizeof g_mesh);
    g_mesh[2] = 0x00; g_mesh[3] = 0x03;               /* count 3 */
    put_be32(g_mesh + 0x08, 0x00000000u);             /* entry 0 dword 0 */
    put_be32(g_mesh + 0x0C, 64u);                     /* entry 0 dword 1 */
    put_be32(g_mesh + 0x10, 128u);                    /* entry 0 dword 2 */

    memset(rec, 0, sizeof rec);
    put_be32(rec + 0x00, 0x00010000u);
    put_be32(rec + 0x04, 0x00020000u);
    put_be32(rec + 0x08, 0x00030000u);
    /* bit 20 set, bits[27:24] == 2 -> 0x200, low 18 bits = first copy size */
    put_be32(rec + 0x20, 0x02100000u | 0x40u);

    memset(g_dstA, 0, sizeof g_dstA);
    memset(g_dstB, 0, sizeof g_dstB);
    g_releaseCalls = 0;
    BrRcaFixupRecord(&ctx, rec);

    CHECK(memcmp(g_dstA, g_blob + 64, 0x40) == 0);
    CHECK(g_dstA[0x40] == 0);
    CHECK(memcmp(g_dstB, g_blob + 128, 0x200) == 0);
    CHECK(g_releaseCalls == 1);

    /* A -1 offset suppresses the copy but still releases. */
    memset(g_mesh, 0, sizeof g_mesh);
    g_mesh[3] = 0x03;
    put_be32(g_mesh + 0x0C, 0xFFFFFFFFu);
    put_be32(g_mesh + 0x10, 0xFFFFFFFFu);
    memset(rec, 0, sizeof rec);
    put_be32(rec + 0x00, 0x00010000u);
    put_be32(rec + 0x04, 0x00020000u);
    put_be32(rec + 0x08, 0x00030000u);
    put_be32(rec + 0x20, 0x02100000u | 0x40u);
    memset(g_dstA, 0, sizeof g_dstA);
    memset(g_dstB, 0, sizeof g_dstB);
    g_releaseCalls = 0;
    BrRcaFixupRecord(&ctx, rec);
    CHECK(g_dstA[0] == 0 && g_dstB[0] == 0);
    CHECK(g_releaseCalls == 1);

    /* enable == 0 skips every copy and goes straight to the release. */
    memset(rec, 0, sizeof rec);
    put_be32(rec + 0x04, 0x00020000u);
    put_be32(rec + 0x20, 0x02100000u | 0x40u);
    ctx.enable = 0;
    memset(g_dstB, 0, sizeof g_dstB);
    g_releaseCalls = 0;
    BrRcaFixupRecord(&ctx, rec);
    CHECK(g_dstB[0] == 0);
    CHECK(g_releaseCalls == 1);

    /* The array wrapper walks 0x24 bytes at a time and ignores counts <= 0.
     *
     * THE FIXTURE IS THE TEST HERE.  This block used to be three records
     * memset to ZERO with `g_releaseCalls == 3` as its only assertion, and
     * three identical zero records make "walk the array" and "do record 0
     * three times" produce exactly the same observation -- so deleting
     * `p += BR_RCA_REC_SIZE` from the walker left the suite green.  The
     * records below are all different, and each one's own +0x04 is checked
     * against the release call it produced, in order.
     *
     * With enable == 0 the record path is short: swap +0x00 and +0x04,
     * rebase both (identity here, seg base 0), swap the six u16 at
     * +0x0C..+0x17 and the dword at +0x20, then release with the VALUE at
     * +0x04.  So the release argument identifies the record uniquely and
     * the +0x0C..+0x0D swap proves the record itself was touched. */
    ctx.enable = 0;
    g_releaseCalls = 0;
    {
        uint8_t arr[BR_RCA_REC_SIZE * 3];
        int     r;

        memset(arr, 0, sizeof arr);
        memset(g_releaseArgs, 0, sizeof g_releaseArgs);
        for (r = 0; r < 3; ++r) {
            uint8_t *p = arr + (size_t)r * BR_RCA_REC_SIZE;
            put_be32(p + 0x00, 0x00410000u + (uint32_t)r);
            put_be32(p + 0x04, 0x00420000u + (uint32_t)r);
            p[0x0C] = (uint8_t)(0x10 + r);
            p[0x0D] = (uint8_t)(0xA0 + r);
        }

        BrRcaFixupArray(&ctx, arr, 3);
        CHECK(g_releaseCalls == 3);
        /* call n saw record n -- this is the stride */
        for (r = 0; r < 3; ++r) {
            CHECK(g_releaseArgs[r]
                  == (void *)(uintptr_t)(0x00420000u + (uint32_t)r));
        }
        /* ...and every record was byte-swapped in place exactly once */
        for (r = 0; r < 3; ++r) {
            const uint8_t *p = arr + (size_t)r * BR_RCA_REC_SIZE;
            CHECK(p[0x0C] == (uint8_t)(0xA0 + r));
            CHECK(p[0x0D] == (uint8_t)(0x10 + r));
        }

        BrRcaFixupArray(&ctx, arr, 0);
        BrRcaFixupArray(&ctx, arr, -2);
        CHECK(g_releaseCalls == 3);
    }
}

/* ================================================================== */

int main(void)
{
    test_tilesize();
    test_geometry_mode();
    test_dl_stack();
    test_matrix_stack();
    test_clip_codes();
    test_light_vertex();
    test_movemem_moveword();
    test_tile_rects();
    test_run();
    test_size_helpers();
    test_tex_create();
    test_tex_scan();
    test_fade_state();
    test_fade_predicates();
    test_fade_tick();
    test_fade_emit();
    test_swaps();
    test_swap_mesh();
    test_rca_fixup();

    if (g_fail != 0) {
        printf("test_slice2_16: %d FAILURE(S)\n", g_fail);
        return 1;
    }
    printf("test_slice2_16: all checks passed\n");
    return 0;
}
