/* test_slice2_17.c -- behaviour tests for slice2_17.c
 *
 * The assertions below are properties of the ORIGINAL code (identities,
 * round-trips, documented clamps and documented asymmetries), not
 * transcriptions of what the port happens to compute.
 *
 * This file also carries the stand-ins the port needs to link:
 *   - the cross-slice callees marked XSLICE in slice2_17.c, as recorders;
 *   - the already-verified helpers from br_vecd.c / br_mat.c / slice1_05.c.
 *     Those are NOT part of this packet -- they are reproduced here purely
 *     so the test binary links, and they mirror the committed sources.
 */
#include "slice2_17.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* TEST-ONLY stand-ins for the already-decompiled libraries.           */
/* Mirrors of port/src/br_vecd.c, br_mat.c and slice1_05.c.            */
/* ================================================================== */

double BrVec3dDot(const BrVec3d *pA, const BrVec3d *pB)
{
    double zz = pB->z * pA->z, yy = pB->y * pA->y, xx = pB->x * pA->x;
    return (zz + yy) + xx;
}

double BrVec3dLenSq(const BrVec3d *pV)
{
    double zz = pV->z * pV->z, yy = pV->y * pV->y, xx = pV->x * pV->x;
    return (zz + yy) + xx;
}

double BrVec3dLen(const BrVec3d *pV) { return sqrt(BrVec3dLenSq(pV)); }

BrVec3d *BrVec3dNormalise(BrVec3d *pV)
{
    double len = BrVec3dLen(pV);
    if (len != 0.0) { pV->x /= len; pV->y /= len; pV->z /= len; }
    return pV;
}

void BrVec3dCross(const BrVec3d *pA, const BrVec3d *pB, BrVec3d *pOut)
{
    double x = pA->y * pB->z - pA->z * pB->y;
    double y = pA->z * pB->x - pA->x * pB->z;
    double z = pA->x * pB->y - pA->y * pB->x;
    pOut->x = x; pOut->y = y; pOut->z = z;
}

signed char BrPackNormalByte(double v)
{
    double t = floor(0.5 + 128.0 * v);
    if (t < -128.0) return -128;
    if (t >  127.0) return  127;
    return (signed char)t;
}

void BrMat4Copy(const BrMat4 *pSrc, BrMat4 *pDst) { *pDst = *pSrc; }

void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{
    BrMat4 tmp, *pDst;
    int aliased, i, j;

    if (pA == NULL || pB == NULL) return;
    aliased = (pOut == pA || pOut == pB);
    pDst = aliased ? &tmp : pOut;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j) {
            float s;
            if (aliased) {
                s = pA->m[i][3] * pB->m[3][j] + pA->m[i][1] * pB->m[1][j];
                s = s + pA->m[i][0] * pB->m[0][j];
                s = s + pA->m[i][2] * pB->m[2][j];
            } else {
                s = pA->m[i][2] * pB->m[2][j] + pA->m[i][3] * pB->m[3][j];
                s = s + pA->m[i][0] * pB->m[0][j];
                s = s + pA->m[i][1] * pB->m[1][j];
            }
            pDst->m[i][j] = s;
        }

    if (aliased) *pOut = tmp;
}

void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz)
{
    memset(pM, 0, sizeof *pM);
    pM->m[0][0] = pM->m[1][1] = pM->m[2][2] = pM->m[3][3] = 1.0f;
    pM->m[3][0] = tx; pM->m[3][1] = ty; pM->m[3][2] = tz;
}

/* Only needs to be distinguishable; the real encoding is tested in
 * slice1_05's own suite. */
void BrRdpSetCombineLERP(BrGfxWords *pOut,
                         int a0,  int b0,  int c0,  int d0,
                         int Aa0, int Ab0, int Ac0, int Ad0,
                         int a1,  int b1,  int c1,  int d1,
                         int Aa1, int Ab1, int Ac1, int Ad1)
{
    (void)b0; (void)c0; (void)d0; (void)Aa0; (void)Ab0; (void)Ac0; (void)Ad0;
    (void)b1; (void)c1; (void)d1; (void)Aa1; (void)Ab1; (void)Ac1; (void)Ad1;
    pOut->w0 = 0xFC000000u | (uint32_t)a0;
    pOut->w1 = (uint32_t)a1;
}

/* ================================================================== */
/* TEST-ONLY stand-ins for the XSLICE callees.                        */
/* ================================================================== */

static int g_stubCalls;
static int g_call10060E90;
static int g_call1003563A_arg;
static int g_call1003563A_n;
static int g_call100397C0_n;
static int g_call10034C66_n;
static int g_call10075F10_n;
static int g_call100664C0_n;
static int g_call10072580_last;
static int g_call10072580_n;
static int g_call10042AF0_n;
static int g_call10035BBA_n;
static char g_lastError[128];
static void *g_call100751D0_this;
static int g_atexit_n;
static int g_call10068260_n;
static uint32_t g_call10068260_tag;

void BrStub10008B80(intptr_t a0, ...) { (void)a0; ++g_stubCalls; }
int  BrX10060E90(void) { return ++g_call10060E90 + 100; }
void BrX100751D0(void *pThis) { g_call100751D0_this = pThis; }
void BrX1002C2C0(void) { }
void BrX1003563A(int a0) { g_call1003563A_arg = a0; ++g_call1003563A_n; }
void BrX100397C0(void) { ++g_call100397C0_n; }
void BrX1002C500(void) { }
void BrX10034C66(void (*pfn)(void)) { assert(pfn == BrX1002C500); ++g_call10034C66_n; }
void BrX10075F10(void *pThis) { (void)pThis; ++g_call10075F10_n; }
void BrX100664C0(void *pThis) { (void)pThis; ++g_call100664C0_n; }
void BrX10072580(int a0) { g_call10072580_last = a0; ++g_call10072580_n; }
void BrX10042AF0(void *p, int a1, int a2) { (void)p; (void)a1; (void)a2; ++g_call10042AF0_n; }
void BrX10035BBA(const char *psz)
{
    ++g_call10035BBA_n;
    strncpy(g_lastError, psz, sizeof g_lastError - 1);
    g_lastError[sizeof g_lastError - 1] = '\0';
}
int BrXAtExit(void (*pfn)(void)) { (void)pfn; ++g_atexit_n; return 0; }

static unsigned char g_lightBuf[64];
static BrMat4 g_mtxPool[8];
static int g_mtxNext;
void *BrX10069530(void) { return g_lightBuf; }
void *BrX10069490(void) { return &g_mtxPool[g_mtxNext++ & 7]; }

static unsigned char g_rgbSeen[3];
int BrX10005DE0(void *pOwner, unsigned char *p0, unsigned char *p1, unsigned char *p2)
{
    (void)pOwner;
    *p0 = 0x11; *p1 = 0x22; *p2 = 0x33;
    g_rgbSeen[0] = *p0; g_rgbSeen[1] = *p1; g_rgbSeen[2] = *p2;
    return 0x5A5A;
}
static int g_call10076AE0_arg;
void BrX10076AE0(void *pThis, int a0) { (void)pThis; g_call10076AE0_arg = a0; }
const char *BrX10005E70(void *pOwner) { (void)pOwner; return "car-name"; }
void BrX10068260(int i, uint32_t tag) { (void)i; ++g_call10068260_n; g_call10068260_tag = tag; }

/* ================================================================== */
/* Harness                                                            */
/* ================================================================== */

static int g_fail;

#define CHECK(cond) \
    do { if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fail; } \
    } while (0)

static int nearf(float a, float b, float eps)
{
    float d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

/* ================================================================== */

static uint32_t g_gfx[4096];

static void gfx_reset(void)
{
    memset(g_gfx, 0, sizeof g_gfx);
    BrS17GetState()->pGfx = g_gfx;
}

static size_t gfx_words(void)
{
    return (size_t)(BrS17GetState()->pGfx - g_gfx);
}

/* Count how many emitted commands carry the given opcode byte. */
static int gfx_count_op(uint32_t op)
{
    size_t i, n = gfx_words();
    int c = 0;
    for (i = 0; i + 1 < n; i += 2)
        if ((g_gfx[i] >> 24) == op) ++c;
    return c;
}

/* ================================================================== */
/* 0x100309A0 BrMat4LookAt                                            */
/* ================================================================== */

static float col_dot(const BrMat4 *m, int a, int b)
{
    return m->m[0][a] * m->m[0][b] + m->m[1][a] * m->m[1][b]
         + m->m[2][a] * m->m[2][b];
}

static void test_lookat(void)
{
    BrMat4 m;
    float cx, cy, cz;

    /* A deliberately non-perpendicular `up`: this is where the Boss Game
     * Gram-Schmidt build differs from stock libultra. */
    BrMat4LookAt(&m, 3.0f, 4.0f, 12.0f, 0.0f, 0.0f, 0.0f,
                 0.1f, 1.0f, 0.2f);

    /* All three columns unit length ... */
    CHECK(nearf(col_dot(&m, 0, 0), 1.0f, 1e-5f));
    CHECK(nearf(col_dot(&m, 1, 1), 1.0f, 1e-5f));
    CHECK(nearf(col_dot(&m, 2, 2), 1.0f, 1e-5f));
    /* ... and mutually perpendicular. */
    CHECK(nearf(col_dot(&m, 0, 1), 0.0f, 1e-5f));
    CHECK(nearf(col_dot(&m, 0, 2), 0.0f, 1e-5f));
    CHECK(nearf(col_dot(&m, 1, 2), 0.0f, 1e-5f));

    /* Column 2 is normalise(eye - at). */
    CHECK(nearf(m.m[0][2], 3.0f / 13.0f, 1e-5f));
    CHECK(nearf(m.m[1][2], 4.0f / 13.0f, 1e-5f));
    CHECK(nearf(m.m[2][2], 12.0f / 13.0f, 1e-5f));

    /* Right-handed: column0 == column1 x column2. */
    cx = m.m[1][1] * m.m[2][2] - m.m[2][1] * m.m[1][2];
    cy = m.m[2][1] * m.m[0][2] - m.m[0][1] * m.m[2][2];
    cz = m.m[0][1] * m.m[1][2] - m.m[1][1] * m.m[0][2];
    CHECK(nearf(m.m[0][0], cx, 1e-5f));
    CHECK(nearf(m.m[1][0], cy, 1e-5f));
    CHECK(nearf(m.m[2][0], cz, 1e-5f));

    /* Gram-Schmidt, not cross-product: column 1 stays in the plane spanned
     * by `up` and column 2, i.e. its component perpendicular to that plane
     * is zero. (A stock guLookAtF would fail this only in sign conventions,
     * but the property below is what distinguishes the two builds: column 1
     * has a POSITIVE projection onto the raw up vector.) */
    CHECK(m.m[0][1] * 0.1f + m.m[1][1] * 1.0f + m.m[2][1] * 0.2f > 0.0f);

    /* Translation row is -dot(eye, column). */
    CHECK(nearf(m.m[3][0],
                -(3.0f * m.m[0][0] + 4.0f * m.m[1][0] + 12.0f * m.m[2][0]),
                1e-4f));
    CHECK(nearf(m.m[3][1],
                -(3.0f * m.m[0][1] + 4.0f * m.m[1][1] + 12.0f * m.m[2][1]),
                1e-4f));
    CHECK(nearf(m.m[3][2], -13.0f, 1e-4f));
    CHECK(m.m[0][3] == 0.0f && m.m[1][3] == 0.0f && m.m[2][3] == 0.0f);
    CHECK(m.m[3][3] == 1.0f);

    /* Documented: eye == at is NOT guarded. The zero vector survives
     * normalisation untouched (br_vecd.h), so the whole basis collapses to
     * zero rather than becoming identity or NaN. */
    BrMat4LookAt(&m, 1.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f, 0.0f, 1.0f, 0.0f);
    CHECK(m.m[0][2] == 0.0f && m.m[1][2] == 0.0f && m.m[2][2] == 0.0f);
    CHECK(m.m[3][3] == 1.0f);
}

/* ================================================================== */
/* 0x10030EE0 BrMat4RotateAxis                                        */
/* ================================================================== */

/* v' = v * M, the row-vector convention BrMat4Mul and BrMat4LookAt use. */
static void row_mul(float *out, const float *v, const BrMat4 *m)
{
    int j;
    for (j = 0; j < 3; ++j)
        out[j] = v[0] * m->m[0][j] + v[1] * m->m[1][j] + v[2] * m->m[2][j];
}

static void test_rotate_axis(void)
{
    BrMat4 m, back;
    float axis[3] = { 1.0f, 2.0f, -2.0f };     /* length 3 */
    float v[3] = { 5.0f, -1.0f, 2.0f };
    float r[3], r2[3];
    int i;

    BrMat4RotateAxis(&m, 37.0f, axis[0], axis[1], axis[2]);
    row_mul(r, v, &m);

    /* A rotation preserves length ... */
    CHECK(nearf(r[0]*r[0] + r[1]*r[1] + r[2]*r[2],
                v[0]*v[0] + v[1]*v[1] + v[2]*v[2], 1e-3f));
    /* ... and the component along the axis. */
    CHECK(nearf(r[0]*axis[0] + r[1]*axis[1] + r[2]*axis[2],
                v[0]*axis[0] + v[1]*axis[1] + v[2]*axis[2], 1e-3f));

    /* A point on the axis is fixed. */
    row_mul(r2, axis, &m);
    for (i = 0; i < 3; ++i)
        CHECK(nearf(r2[i], axis[i], 1e-4f));

    /* Rotating back by the same angle about the same axis restores v. */
    BrMat4RotateAxis(&back, -37.0f, axis[0], axis[1], axis[2]);
    row_mul(r2, r, &back);
    for (i = 0; i < 3; ++i)
        CHECK(nearf(r2[i], v[i], 1e-3f));

    /* A full turn is the identity. */
    BrMat4RotateAxis(&m, 360.0f, axis[0], axis[1], axis[2]);
    row_mul(r, v, &m);
    for (i = 0; i < 3; ++i)
        CHECK(nearf(r[i], v[i], 1e-3f));

    /* Degenerate axis -> identity, exactly (the early-out path). */
    BrMat4RotateAxis(&m, 42.0f, 0.0f, 0.0f, 0.0f);
    for (i = 0; i < 4; ++i) {
        int j;
        for (j = 0; j < 4; ++j)
            CHECK(m.m[i][j] == ((i == j) ? 1.0f : 0.0f));
    }

    /* Documented: the x87 C3-only compare treats an unordered result as
     * "equal to zero", so a NaN axis also takes the identity path. */
    {
        float nan_ = (float)strtod("NAN", NULL);
        BrMat4RotateAxis(&m, 42.0f, nan_, nan_, nan_);
        CHECK(m.m[0][0] == 1.0f && m.m[1][1] == 1.0f
              && m.m[2][2] == 1.0f && m.m[3][3] == 1.0f);
        CHECK(m.m[0][1] == 0.0f && m.m[2][0] == 0.0f);
    }
}

/* ================================================================== */
/* 0x10030E20 / 0x10030B50 lights and angles                          */
/* ================================================================== */

static void test_lights(void)
{
    BrMat4 m;
    BrLightPair lights;
    BrSkyAngles ang;
    int i;

    memset(&lights, 0xAB, sizeof lights);
    BrLightDirsFromLookAt(&m, &lights,
                          0.0f, 0.0f, 1.0f,
                          0.0f, 0.0f, 0.0f,
                          0.0f, 1.0f, 0.0f);

    /* dir0 is column 0, dir1 is column 1, both through BrPackNormalByte. */
    for (i = 0; i < 3; ++i) {
        CHECK(lights.dir0[i] == BrPackNormalByte((double)m.m[i][0]));
        CHECK(lights.dir1[i] == BrPackNormalByte((double)m.m[i][1]));
    }
    /* Everything outside the two dir triples is left alone. */
    CHECK(lights.f00[0] == 0xAB && lights.f10[0] == 0xAB);
    CHECK(lights.f0B[0] == 0xAB && lights.f1B[4] == 0xAB);

    /* 1.0 packs to 127, not 128 -- the documented asymmetry. */
    CHECK(BrPackNormalByte(1.0) == 127);
    CHECK(BrPackNormalByte(-1.0) == -128);

    /* Angles: a direction along column 2 gives atan2(0, +1) = 0 and
     * asin(0) = 0, so both land exactly on the half-revolution count. */
    BrLightDirsAndAngles(&m, &lights, &ang,
                         0.0f, 0.0f, 1.0f,      /* eye */
                         0.0f, 0.0f, 0.0f,      /* at  */
                         0.0f, 1.0f, 0.0f,      /* up  */
                         0.0f, 0.0f, 1.0f,      /* A == column 2 */
                         0.0f, 0.0f, 1.0f,      /* B == column 2 */
                         64, 64);
    CHECK(ang.s0 == 0x100);
    CHECK(ang.t0 == 0x100);
    CHECK(ang.s1 == 64 * 4);
    CHECK(ang.t1 == 64 * 4);

    /* A direction along column 1 (the "up" axis) drives asin to +pi/2, a
     * quarter revolution, i.e. t == 1.5 * N. Truncation makes the last
     * count uncertain by one. */
    BrLightDirsAndAngles(&m, &lights, &ang,
                         0.0f, 0.0f, 1.0f,
                         0.0f, 0.0f, 0.0f,
                         0.0f, 1.0f, 0.0f,
                         (float)m.m[0][1], (float)m.m[1][1], (float)m.m[2][1],
                         (float)m.m[0][1], (float)m.m[1][1], (float)m.m[2][1],
                         64, 64);
    CHECK(ang.t0 >= 0x180 - 1 && ang.t0 <= 0x180 + 1);
    CHECK(ang.t1 >= 256 + 128 - 1 && ang.t1 <= 256 + 128 + 1);

    /* The two half-revolution counts really are separate arguments. */
    BrLightDirsAndAngles(&m, &lights, &ang,
                         0.0f, 0.0f, 1.0f,
                         0.0f, 0.0f, 0.0f,
                         0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 1.0f,
                         0.0f, 0.0f, 1.0f,
                         10, 20);
    CHECK(ang.s1 == 40);
    CHECK(ang.t1 == 80);
}

/* ================================================================== */
/* 0x100312A7 BrFloat12MaxAbs                                         */
/* ================================================================== */

static void test_maxabs(void)
{
    float v[12];
    int i;

    for (i = 0; i < 12; ++i) v[i] = 0.0f;
    CHECK(BrFloat12MaxAbs(v) == 0.0f);

    for (i = 0; i < 12; ++i) v[i] = (float)(i + 1);
    CHECK(BrFloat12MaxAbs(v) == 12.0f);

    for (i = 0; i < 12; ++i) v[i] = -(float)(i + 1);
    CHECK(BrFloat12MaxAbs(v) == 12.0f);

    for (i = 0; i < 12; ++i) v[i] = (i & 1) ? (float)i : -(float)(i * 2);
    /* max positive 11, most negative -20 -> 20 */
    CHECK(BrFloat12MaxAbs(v) == 20.0f);

    /* Only the first twelve floats are read. */
    {
        float w[13];
        for (i = 0; i < 12; ++i) w[i] = 1.0f;
        w[12] = 999.0f;
        CHECK(BrFloat12MaxAbs(w) == 1.0f);
    }

    /* Documented: a NaN takes the "negative" branch and then fails the
     * min compare, so it is ignored rather than propagated. */
    for (i = 0; i < 12; ++i) v[i] = 2.0f;
    v[5] = (float)strtod("NAN", NULL);
    CHECK(BrFloat12MaxAbs(v) == 2.0f);
}

/* ================================================================== */
/* 0x10031347 BrTexSizeShift                                          */
/* ================================================================== */

static void test_texsizeshift(void)
{
    static const struct { int size, shift; } tbl[] = {
        { 1, 0 }, { 2, 1 }, { 3, 2 }, { 4, 2 }, { 5, 3 }, { 8, 3 },
        { 9, 4 }, { 16, 4 }, { 17, 5 }, { 32, 5 }, { 33, 6 }, { 64, 6 },
        { 65, 7 }, { 128, 7 }, { 129, 8 }, { 256, 8 }, { 257, 9 },
        { 512, 9 }, { 513, 10 }, { 1024, 10 }
    };
    size_t i;
    int a, b;

    for (i = 0; i < sizeof tbl / sizeof tbl[0]; ++i) {
        a = 0; b = -1;
        BrTexSizeShift(tbl[i].size, &a, &b);
        CHECK(a == 0xFFFF);
        if (b != tbl[i].shift)
            printf("  size %d -> %d, expected %d\n", tbl[i].size, b, tbl[i].shift);
        CHECK(b == tbl[i].shift);
        /* 1 << shift is the smallest power of two >= size. */
        CHECK((1 << b) >= tbl[i].size);
        CHECK(b == 0 || (1 << (b - 1)) < tbl[i].size);
    }

    /* Documented: over 1024 reports an error and leaves *pOut2 untouched,
     * and the message carries size - 1. */
    g_call10035BBA_n = 0;
    a = 0; b = 0x5EED;
    BrTexSizeShift(1025, &a, &b);
    CHECK(g_call10035BBA_n == 1);
    CHECK(b == 0x5EED);
    CHECK(a == 0xFFFF);
    CHECK(strcmp(g_lastError, "ERROR: unhandled texture size: 1024") == 0);
}

/* ================================================================== */
/* 0x1002BF40 BrPtrListContains                                       */
/* ================================================================== */

static void test_ptrlist(void)
{
    static BrPtrList list;
    int a = 1, b = 2, c = 3;

    list.n = 0;
    /* NULL short-circuits to "present" even when the list is empty. */
    CHECK(BrPtrListContains(&list, NULL) == 1);
    CHECK(BrPtrListContains(&list, &a) == 0);

    list.ap[0] = &a;
    list.ap[1] = &b;
    list.n = 2;
    CHECK(BrPtrListContains(&list, &a) == 1);
    CHECK(BrPtrListContains(&list, &b) == 1);
    CHECK(BrPtrListContains(&list, &c) == 0);
    /* Only the first n entries are searched. */
    list.ap[2] = &c;
    CHECK(BrPtrListContains(&list, &c) == 0);
    list.n = 3;
    CHECK(BrPtrListContains(&list, &c) == 1);
}

/* ================================================================== */
/* 0x100314E8 / 0x10031688 fill emitters                              */
/* ================================================================== */

static void test_fills(void)
{
    BrS17State *st = BrS17GetState();
    uint32_t c;

    st->screenW = 320;
    st->screenH = 240;
    st->scaleShift = 0;

    gfx_reset();
    BrGfxClearScreen(0xFF, 0x00, 0x00);
    CHECK(gfx_words() == 7 * 2);

    /* RGBA5551: red only, alpha forced on, duplicated into both halves. */
    c = 0xF800u | 1u;
    CHECK(g_gfx[7] == ((c << 16) | c));           /* the 0xF7 fill colour */
    /* The rectangle covers the whole screen, lower right inclusive. */
    CHECK(g_gfx[8] == (BR_GFX_FILLRECT | ((320u - 1u) << 12) | (240u - 1u)));
    CHECK(g_gfx[9] == 0);
    /* Command bytes bracketing the fill. */
    CHECK((g_gfx[0] >> 24) == 0xE7 && (g_gfx[10] >> 24) == 0xE7);

    /* Alpha bit is unconditional, and only the top 5 bits of each channel
     * survive. */
    gfx_reset();
    BrGfxClearScreen(0, 0, 0);
    CHECK((g_gfx[7] & 0xFFFFu) == 1u);
    gfx_reset();
    BrGfxClearScreen(7, 7, 7);        /* below the top-5-bit threshold */
    CHECK((g_gfx[7] & 0xFFFFu) == 1u);

    /* Unscaled fill rect: plain 12-bit integer corners. */
    gfx_reset();
    BrGfxFillRect(10, 20, 30, 40, 0, 0xFF, 0);
    CHECK(gfx_words() == 7 * 2);
    CHECK(g_gfx[8] == (BR_GFX_FILLRECT | ((10u + 30u - 1u) << 12) | (20u + 40u - 1u)));
    CHECK(g_gfx[9] == ((10u << 12) | 20u));
    CHECK((g_gfx[7] & 0xFFFFu) == (0x07C0u | 1u));

    /* Documented asymmetry: with a scale shift of 1 the upper-left corner
     * is only doubled while the lower-right is doubled AND shifted again. */
    st->scaleShift = 1;
    gfx_reset();
    BrGfxFillRect(10, 20, 30, 40, 0, 0, 0);
    CHECK(g_gfx[9] == ((20u << 12) | 40u));                      /* 2x only  */
    CHECK(g_gfx[8] == (BR_GFX_FILLRECT
                       | (((20u + 60u) * 2u - 1u) << 12)
                       | ((40u + 80u) * 2u - 1u)));              /* 4x       */
    st->scaleShift = 0;
}

/* ================================================================== */
/* 0x10031481 BrGfxEmitTexCmd                                         */
/* ================================================================== */

static void test_emit_tex(void)
{
    unsigned char recs[BR_TEXREC_STRIDE * 2];
    uint32_t w;

    memset(recs, 0, sizeof recs);

    w = 0x12345678u; memcpy(recs + 0x00, &w, 4);
    w = 0u;          memcpy(recs + 0x20, &w, 4);

    w = 0xAABBCCDDu; memcpy(recs + BR_TEXREC_STRIDE + 0x00, &w, 4);
    w = 1u << 20;    memcpy(recs + BR_TEXREC_STRIDE + 0x20, &w, 4);

    gfx_reset();
    BrGfxEmitTexCmd(0, recs);
    CHECK(gfx_words() == 2);
    CHECK(g_gfx[0] == (0xDC000000u | 0x00345678u));
    CHECK(g_gfx[1] == 1u);

    /* Bit 20 of +0x20 suppresses the command entirely. */
    BrGfxEmitTexCmd(1, recs);
    CHECK(gfx_words() == 2);

    /* Any other bit of +0x20 does not. */
    w = ~(1u << 20); memcpy(recs + BR_TEXREC_STRIDE + 0x20, &w, 4);
    BrGfxEmitTexCmd(1, recs);
    CHECK(gfx_words() == 4);
    CHECK(g_gfx[2] == (0xDC000000u | 0x00BBCCDDu));
}

/* ================================================================== */
/* 0x10031190 scratch ring                                            */
/* ================================================================== */

static void test_scratch(void)
{
    static unsigned char ring[BR_SCRATCH_SLOTS * BR_SCRATCH_STRIDE];
    BrS17State *st = BrS17GetState();
    void *first, *p;
    int i;

    st->pScratch = ring;
    st->pScratchWait = ring;
    st->nScratchDepth = 0;
    st->iScratch = 0;
    g_call10042AF0_n = 0;

    /* Records are handed out 0x18 apart and wrap after 32. */
    first = BrScratchRingAlloc();
    CHECK(first == ring + BR_SCRATCH_STRIDE);
    CHECK(st->nScratchDepth == 1);
    for (i = 1; i < BR_SCRATCH_SLOTS; ++i)
        p = BrScratchRingAlloc();
    CHECK(p == ring + 0);                 /* wrapped back to slot 0 */
    CHECK(st->nScratchDepth == BR_SCRATCH_DEPTH);
    CHECK(g_call10042AF0_n == 0);

    /* Documented: once the depth reaches 0x20 the counter STICKS and every
     * further call waits. */
    (void)BrScratchRingAlloc();
    CHECK(st->nScratchDepth == BR_SCRATCH_DEPTH);
    CHECK(g_call10042AF0_n == 1);
    (void)BrScratchRingAlloc();
    CHECK(st->nScratchDepth == BR_SCRATCH_DEPTH);
    CHECK(g_call10042AF0_n == 2);

    /* Draining waits once per outstanding record and leaves depth at 0. */
    g_call10042AF0_n = 0;
    BrScratchRingDrain();
    CHECK(st->nScratchDepth == 0);
    CHECK(g_call10042AF0_n == BR_SCRATCH_DEPTH);
    /* Draining an empty ring is a no-op. */
    BrScratchRingDrain();
    CHECK(g_call10042AF0_n == BR_SCRATCH_DEPTH);

    CHECK(BrScratchRingNull(7, 9) == 0);
}

/* ================================================================== */
/* 0x1002C410 / 0x1002C430 / 0x1002C4A0                               */
/* ================================================================== */

static void test_glue(void)
{
    BrS17State *st = BrS17GetState();
    unsigned char recs[BR_TICKREC_STRIDE * 4];
    unsigned char car[0x1040];
    uint32_t v;
    float f;
    int i;

    /* --- 0x1002C410: decrement +0 while +0x0C is non-zero ---------- */
    memset(recs, 0, sizeof recs);
    for (i = 0; i < 3; ++i) {
        v = 10u + (uint32_t)i; memcpy(recs + i * BR_TICKREC_STRIDE + 0, &v, 4);
        v = 1u;                memcpy(recs + i * BR_TICKREC_STRIDE + 0xC, &v, 4);
    }
    /* record 3 keeps +0x0C == 0, terminating the walk */
    BrS17TimerTick(recs);
    for (i = 0; i < 3; ++i) {
        memcpy(&v, recs + i * BR_TICKREC_STRIDE, 4);
        CHECK(v == 10u + (uint32_t)i - 1u);
    }
    memcpy(&v, recs + 3 * BR_TICKREC_STRIDE, 4);
    CHECK(v == 0u);

    /* Documented: a table whose FIRST +0x0C is zero is left alone. */
    v = 0; memcpy(recs + 0xC, &v, 4);
    v = 99; memcpy(recs + 0, &v, 4);
    BrS17TimerTick(recs);
    memcpy(&v, recs + 0, 4);
    CHECK(v == 99u);

    /* --- 0x1002C430: |(x,y,z)| * 2.24 into +0x1030 ------------------ */
    memset(car, 0, sizeof car);
    v = 1; memcpy(car + 0x730, &v, 4);
    f = 3.0f;  memcpy(car + 0x1E8, &f, 4);
    f = 0.0f;  memcpy(car + 0x1EC, &f, 4);
    f = 4.0f;  memcpy(car + 0x1F0, &f, 4);
    g_call10075F10_n = 0;
    BrCarUpdateSpeedMph(car);
    memcpy(&f, car + 0x1030, 4);
    CHECK(nearf(f, 5.0f * 2.24f, 1e-4f));
    CHECK(g_call10075F10_n == 1);

    /* Gate clear: +0x1030 untouched, but 0x10075F10 still runs. */
    v = 0; memcpy(car + 0x730, &v, 4);
    f = -1.0f; memcpy(car + 0x1030, &f, 4);
    BrCarUpdateSpeedMph(car);
    memcpy(&f, car + 0x1030, 4);
    CHECK(f == -1.0f);
    CHECK(g_call10075F10_n == 2);

    /* --- 0x1002C4A0 -------------------------------------------------- */
    {
        static unsigned char slots[BR_SLOT_STRIDE * 5];
        st->pSlots = slots;
        st->nEntA = 3;
        g_call100664C0_n = 0;
        BrS17SlotsRelease();
        CHECK(g_call100664C0_n == 3);
        st->nEntA = 0;
        BrS17SlotsRelease();
        CHECK(g_call100664C0_n == 3);
    }
}

/* ================================================================== */
/* 0x1002C2D0 / 0x1002C320 / 0x1002C390 / 0x1002C2A0                  */
/* ================================================================== */

static void test_gated(void)
{
    BrS17State *st = BrS17GetState();
    int thisObj;

    /* 0x1002C2D0: nothing happens at all when 0x106909B0 is zero. */
    st->f6909B0 = 0;
    st->f680944 = 0x1234;
    g_call1003563A_n = 0;
    BrS17DrawGated();
    CHECK(g_call1003563A_n == 0);

    /* Non-zero and not -1: the callee runs, 0x106C2CFC is left alone. */
    st->f6909B0 = 5;
    st->f6C2CFC = 0xBEEF;
    BrS17DrawGated();
    CHECK(g_call1003563A_n == 1);
    CHECK(g_call1003563A_arg == 0x1234);
    CHECK(st->f6C2CFC == 0xBEEF);

    /* -1: 0x106C2CFC is zeroed across the call and then restored. */
    st->f6909B0 = -1;
    st->f6C2CFC = 0xCAFE;
    BrS17DrawGated();
    CHECK(g_call1003563A_n == 2);
    CHECK(st->f6C2CFC == 0xCAFE);

    /* 0x1002C320 is entirely gated on 0x106909B4. */
    st->f6909B4 = 1;
    g_stubCalls = 0;
    g_call100397C0_n = 0;
    BrS17DrawFrame();
    CHECK(g_stubCalls == 0);
    CHECK(g_call100397C0_n == 0);

    st->f6909B4 = 0;
    st->f6909B0 = 0;
    BrS17DrawFrame();
    CHECK(g_stubCalls == 3);
    CHECK(g_call100397C0_n == 1);

    /* 0x1002C390 */
    st->f0AA010 = 0;
    st->f6805B8 = 0;
    g_call10034C66_n = 0;
    BrS17SetMode4();
    CHECK(st->f0AA010 == 4);
    CHECK(st->f6805B8 == 2);
    CHECK(g_call10034C66_n == 1);

    /* 0x1002C2A0 forwards the 0x106806B0 object unchanged. */
    st->pThis6806B0 = &thisObj;
    g_call100751D0_this = NULL;
    BrS17Release();
    CHECK(g_call100751D0_this == &thisObj);

    /* 0x1002C2B0 */
    g_atexit_n = 0;
    CHECK(BrS17RegisterAtExit() == 0);
    CHECK(g_atexit_n == 1);
}

/* ================================================================== */
/* 0x1002C210 bank flip                                               */
/* ================================================================== */

static void test_bankflip(void)
{
    BrS17State *st = BrS17GetState();
    static uint32_t hdr[6];
    static unsigned char buf[2 * 3 * 0x800];
    int i;

    st->pBankHdr = hdr;
    st->pBankBuf = buf;
    st->bank = 0;
    memset(hdr, 0xFF, sizeof hdr);
    memset(buf, 0xFF, sizeof buf);
    g_call10060E90 = 0;
    g_stubCalls = 0;

    BrS17BankFlip();
    CHECK(g_stubCalls == 3);
    CHECK(st->bank == 1);                       /* toggled */
    CHECK(st->bank578 == 101);
    CHECK(st->bank57C == 0);
    /* Bank 1's three-dword header cleared, bank 0's untouched. */
    CHECK(hdr[3] == 0 && hdr[4] == 0 && hdr[5] == 0);
    CHECK(hdr[0] == 0xFFFFFFFFu);
    /* Bank 1's three 0x800 sub-buffers each get their first dword cleared. */
    for (i = 0; i < 3; ++i) {
        uint32_t v;
        memcpy(&v, buf + (3 + i) * 0x800, 4);
        CHECK(v == 0);
    }
    memcpy(&i, buf, 4);
    CHECK((uint32_t)i == 0xFFFFFFFFu);

    BrS17BankFlip();
    CHECK(st->bank == 0);
    CHECK(hdr[0] == 0 && hdr[1] == 0 && hdr[2] == 0);
}

/* ================================================================== */
/* 0x10031227 / 0x10031282                                            */
/* ================================================================== */

static void test_resets(void)
{
    BrS17State *st = BrS17GetState();

    st->f6C32CC = st->f6C56DC = st->f6C1178 = 7;
    st->f6C161C = st->f6C1610 = 7;
    st->f6C33B8 = st->f6C06A4 = st->f6C069C = 7;
    BrRenderCountersReset();
    CHECK(st->f6C32CC == 0 && st->f6C56DC == 0 && st->f6C1178 == 0);
    CHECK(st->f6C161C == 0 && st->f6C1610 == 0);
    CHECK(st->f6C33B8 == 0 && st->f6C06A4 == 0 && st->f6C069C == 0);

    st->defaultW = 640;         /* 0x100A81C0 in the shipped DLL */
    st->defaultH = 480;         /* 0x100A81C4                    */
    st->screenW = st->screenH = 0;
    BrScreenSizeInit();
    CHECK(st->screenW == 640 && st->screenH == 480);

    st->defaultW = 800;
    st->defaultH = 600;
    BrScreenSizeApply();
    CHECK(st->screenW == 800 && st->screenH == 600);

    BrTexNoOp();                /* must be callable and do nothing */
}

/* ================================================================== */
/* car table                                                          */
/* ================================================================== */

#define TEST_CARS 3
static unsigned char g_cars[TEST_CARS * BR_CAR_STRIDE];
static unsigned char g_slots[8 * BR_SLOT_STRIDE];
static uint32_t g_s5B0[TEST_CARS], g_s9C0[TEST_CARS], g_s748[TEST_CARS];
static uint32_t g_s728[TEST_CARS], g_s5C8[TEST_CARS], g_s950[TEST_CARS * 12];
static signed char g_tbl[256];

static void car_setup(void)
{
    BrS17State *st = BrS17GetState();
    st->pCars = g_cars;
    st->pSlots = g_slots;
    st->pSave5B0 = g_s5B0;
    st->pSave9C0 = g_s9C0;
    st->pSave748 = g_s748;
    st->pSave728 = g_s728;
    st->pSave5C8 = g_s5C8;
    st->pSave950 = g_s950;
    st->pTblAA210 = g_tbl;
}

static void test_cartable(void)
{
    BrS17State *st = BrS17GetState();
    int owner0, owner1;
    unsigned char *car;
    uint32_t v;

    car_setup();
    memset(g_cars, 0, sizeof g_cars);
    memset(g_slots, 0, sizeof g_slots);
    st->nEntA = 0;
    st->nEntB = 0;
    g_call10068260_n = 0;

    /* --- 0x1002F130: both counters advance, one record each ---------- */
    v = 0xF00Du;
    memcpy(g_cars + 0 * BR_CAR_STRIDE + BR_CAR_OFF_TAG, &v, 4);
    BrCarTableAdd(&owner0);
    CHECK(st->nEntA == 1 && st->nEntB == 1);
    car = g_cars + 0 * BR_CAR_STRIDE;
    CHECK(car[BR_CAR_OFF_RGB + 0] == 0x11);
    CHECK(car[BR_CAR_OFF_RGB + 1] == 0x22);
    CHECK(car[BR_CAR_OFF_RGB + 2] == 0x33);
    CHECK(strcmp((char *)(car + BR_CAR_OFF_NAME), "car-name") == 0);
    CHECK(g_call10076AE0_arg == 0x5A5A);
    CHECK(g_call10068260_n == 1);
    CHECK(g_call10068260_tag == 0xF00Du);
    memcpy(&v, car + BR_CAR_OFF_OWNER, 4);
    CHECK(v == (uint32_t)(uintptr_t)&owner0);

    BrCarTableAdd(&owner1);
    CHECK(st->nEntA == 2 && st->nEntB == 2);
    memcpy(&v, g_cars + 1 * BR_CAR_STRIDE + BR_CAR_OFF_OWNER, 4);
    CHECK(v == (uint32_t)(uintptr_t)&owner1);

    /* --- 0x1002F230: clears +0x0F08 and unhooks matching slots ------- */
    v = 1;
    memcpy(g_cars + 0 * BR_CAR_STRIDE + BR_CAR_OFF_ACTIVE, &v, 4);
    memcpy(g_cars + 1 * BR_CAR_STRIDE + BR_CAR_OFF_ACTIVE, &v, 4);
    v = (uint32_t)(uintptr_t)(g_cars + 1 * BR_CAR_STRIDE);
    memcpy(g_slots + 0 * BR_SLOT_STRIDE + BR_SLOT_OFF_CARPTR, &v, 4);
    v = (uint32_t)(uintptr_t)(g_cars + 0 * BR_CAR_STRIDE);
    memcpy(g_slots + 1 * BR_SLOT_STRIDE + BR_SLOT_OFF_CARPTR, &v, 4);

    g_call10072580_n = 0;
    BrCarTableRemove(&owner1);
    CHECK(g_call10072580_n == 1);
    /* the argument is the record index doubled */
    CHECK(g_call10072580_last == 2);
    memcpy(&v, g_cars + 1 * BR_CAR_STRIDE + BR_CAR_OFF_ACTIVE, 4);
    CHECK(v == 0);
    memcpy(&v, g_cars + 0 * BR_CAR_STRIDE + BR_CAR_OFF_ACTIVE, 4);
    CHECK(v == 1);                       /* the other car is untouched */
    memcpy(&v, g_slots + 0 * BR_SLOT_STRIDE + BR_SLOT_OFF_CARPTR, 4);
    CHECK(v == 0);                       /* matching slot unhooked     */
    memcpy(&v, g_slots + 1 * BR_SLOT_STRIDE + BR_SLOT_OFF_CARPTR, 4);
    CHECK(v != 0);                       /* non-matching slot kept     */

    /* An owner nobody holds removes nothing. */
    g_call10072580_n = 0;
    BrCarTableRemove((void *)(uintptr_t)0xDEADBEEFu);
    CHECK(g_call10072580_n == 0);
}

static void test_carstate(void)
{
    BrS17State *st = BrS17GetState();
    int i, k;

    car_setup();
    memset(g_cars, 0, sizeof g_cars);
    st->nCars = TEST_CARS;
    st->nSaveDwords = 4;
    st->f6909B8 = 0;

    for (i = 0; i < TEST_CARS; ++i) {
        unsigned char *car = g_cars + (size_t)i * BR_CAR_STRIDE;
        uint32_t v;
        v = 0xA000u + (uint32_t)i; memcpy(car + BR_CAR_OFF_SAVE0, &v, 4);
        v = 0xB000u + (uint32_t)i; memcpy(car + BR_CAR_OFF_SAVE1, &v, 4);
        v = 0xC000u + (uint32_t)i; memcpy(car + BR_CAR_OFF_SAVE2, &v, 4);
        v = 0xD000u + (uint32_t)i; memcpy(car + BR_CAR_OFF_SAVE3, &v, 4);
        v = 0xE000u + (uint32_t)i; memcpy(car + BR_CAR_OFF_SAVE4, &v, 4);
        for (k = 0; k < 4; ++k) {
            v = 0x100u * (uint32_t)i + (uint32_t)k;
            memcpy(car + BR_CAR_OFF_SAVEVEC + k * 4, &v, 4);
        }
    }

    BrCarStateSave();
    CHECK(st->f6909B8 == 1);
    for (i = 0; i < TEST_CARS; ++i) {
        CHECK(g_s5B0[i] == 0xA000u + (uint32_t)i);
        CHECK(g_s9C0[i] == 0xB000u + (uint32_t)i);
        CHECK(g_s748[i] == 0xC000u + (uint32_t)i);
        CHECK(g_s728[i] == 0xD000u + (uint32_t)i);
        CHECK(g_s5C8[i] == 0xE000u + (uint32_t)i);
        for (k = 0; k < 4; ++k)
            CHECK(g_s950[i * 12 + k] == 0x100u * (uint32_t)i + (uint32_t)k);
    }

    /* Scribble over the records, then restore. */
    for (i = 0; i < TEST_CARS; ++i)
        memset(g_cars + (size_t)i * BR_CAR_STRIDE + BR_CAR_OFF_SAVEVEC,
               0x5A, 0x60);

    st->f0AA010 = 1;            /* skips the command-block fixup loop */
    BrCarStateRestore();

    for (i = 0; i < TEST_CARS; ++i) {
        unsigned char *car = g_cars + (size_t)i * BR_CAR_STRIDE;
        uint32_t v;
        memcpy(&v, car + BR_CAR_OFF_SAVE0, 4); CHECK(v == 0xA000u + (uint32_t)i);
        memcpy(&v, car + BR_CAR_OFF_SAVE2, 4); CHECK(v == 0xC000u + (uint32_t)i);
        memcpy(&v, car + BR_CAR_OFF_SAVE3, 4); CHECK(v == 0xD000u + (uint32_t)i);
        memcpy(&v, car + BR_CAR_OFF_SAVE4, 4); CHECK(v == 0xE000u + (uint32_t)i);
        for (k = 0; k < 4; ++k) {
            memcpy(&v, car + BR_CAR_OFF_SAVEVEC + k * 4, 4);
            CHECK(v == 0x100u * (uint32_t)i + (uint32_t)k);
        }
        /* DOCUMENTED ASYMMETRY: +0x0FE4 is saved but never restored, so it
         * still holds the scribble. */
        memcpy(&v, car + BR_CAR_OFF_SAVE1, 4);
        CHECK(v == 0x5A5A5A5Au);
    }

    /* Only nSaveDwords dwords move, not the whole 0x30 window. */
    st->nSaveDwords = 0;
    for (i = 0; i < TEST_CARS; ++i)
        memset(g_cars + (size_t)i * BR_CAR_STRIDE + BR_CAR_OFF_SAVEVEC, 0x77, 16);
    BrCarStateRestore();
    {
        uint32_t v;
        memcpy(&v, g_cars + BR_CAR_OFF_SAVEVEC, 4);
        CHECK(v == 0x77777777u);
    }
}

static void test_carstate_fixup(void)
{
    BrS17State *st = BrS17GetState();
    static unsigned char blk[512];
    unsigned char *car;
    uint16_t v16;
    uint32_t v32;
    unsigned n1 = 1, n2 = 2, idx;

    car_setup();
    memset(g_cars, 0, sizeof g_cars);
    memset(blk, 0, sizeof blk);
    st->nCars = 1;
    st->nSaveDwords = 0;
    st->f0AA010 = 0;
    st->f6909B8 = 1;

    blk[4] = (unsigned char)n1;
    blk[5] = (unsigned char)n2;
    car = g_cars;
    memcpy(car + BR_CAR_OFF_CMDPTR, &(unsigned char *){ blk }, sizeof(unsigned char *));

    g_s5C8[0] = 0x0000007Fu;      /* low byte 0x7F, table index 0x7F */
    g_s728[0] = 0xDEADBEEFu;
    g_tbl[0x7F] = -3;

    g_stubCalls = 0;
    BrCarStateRestore();

    idx = n2 + n1 * 4;                          /* 6 */
    /* 1) a sign-extended table byte, at (idx * 2) + 0x1E */
    memcpy(&v16, blk + idx * 2 + 0x1E, 2);
    CHECK(v16 == (uint16_t)(int16_t)-3);
    /* 2) the low byte of the saved dword, at (n2 + n1*4 + 6) BYTES */
    CHECK(blk[n2 + n1 * 4 + 6] == 0x7F);
    /* 3) a dword at (idx + 0x14) * 4 */
    memcpy(&v32, blk + (idx + 0x14) * 4, 4);
    CHECK(v32 == 0xDEADBEEFu);
    /* the read-back is reported through the debug stub */
    CHECK(g_stubCalls == 1);

    /* Gated off when 0x100AA010 is non-zero ... */
    memset(blk, 0, sizeof blk);
    blk[4] = (unsigned char)n1;
    blk[5] = (unsigned char)n2;
    st->f0AA010 = 1;
    g_stubCalls = 0;
    BrCarStateRestore();
    CHECK(g_stubCalls == 0);
    /* ... and when 0x106909B8 is zero. */
    st->f0AA010 = 0;
    st->f6909B8 = 0;
    BrCarStateRestore();
    CHECK(g_stubCalls == 0);
}

/* ================================================================== */
/* 0x1002FB20 prop pass                                               */
/* ================================================================== */

static struct {
    uint16_t f00, count;
    uint32_t f04;
    BrPropItem items[4];
} g_props;

static BrMat4 g_lightMtx, g_transMtx, g_viewMtx;
static const uint32_t g_colTable[4] = {
    0x40404000u, 0x80808000u, 0xC0C0C000u, 0xFFFFFF00u
};

/* Collect the payloads of every 0x06 (branch to display list) command. */
static int collect_dls(uint32_t *out, int cap)
{
    size_t i, n = gfx_words();
    int c = 0;
    for (i = 0; i + 1 < n; i += 2)
        if ((g_gfx[i] >> 24) == 0x06 && c < cap)
            out[c++] = g_gfx[i + 1];
    return c;
}

static void test_props(void)
{
    BrS17State *st = BrS17GetState();
    uint32_t dls[8];
    int n;

    st->pLightMtx = &g_lightMtx;
    st->pTransMtx = &g_transMtx;
    st->pColAA5D0 = g_colTable;
    st->f690A1C = 0;
    st->f6C0258 = 0x11111111u;
    st->f6C0688 = 0x22222222u;
    st->f6C0920 = 0x33333333u;
    st->f6C3364 = 1;
    st->f6C1174 = 1;                /* equal -> the 0x2000 depth word */
    st->f0AA880 = 0;

    memset(&g_props, 0, sizeof g_props);
    memset(&g_viewMtx, 0, sizeof g_viewMtx);
    g_props.count = 4;
    /* item 0: pass 0 (bit 3 clear), valid dl                 */
    g_props.items[0].dl = 0xAA1; g_props.items[0].f04 = 0x00;
    /* item 1: pass 1 (bit 3 set), valid dl                   */
    g_props.items[1].dl = 0xAA2; g_props.items[1].f04 = 0x08;
    /* item 2: pass 0 but dl == 0 -> skipped entirely         */
    g_props.items[2].dl = 0;     g_props.items[2].f04 = 0x00;
    /* item 3: pass 1, valid dl                               */
    g_props.items[3].dl = 0xAA3; g_props.items[3].f04 = 0x08;

    gfx_reset();
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);

    /* Three drawable items, so three 0x06 branches, in PASS order: the
     * bit-3-clear item first, then the two bit-3-set ones. */
    n = collect_dls(dls, 8);
    CHECK(n == 3);
    if (n == 3) {
        CHECK(dls[0] == 0xAA1);
        CHECK(dls[1] == 0xAA2);
        CHECK(dls[2] == 0xAA3);
    }

    /* A count of zero emits the prologue and the trailing 0xBD only. */
    gfx_reset();
    g_props.count = 0;
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);
    CHECK(collect_dls(dls, 8) == 0);
    CHECK(gfx_count_op(0xBD) == 1);

    /* 0x10690A1C selects between two othermode-L words and is cleared. */
    gfx_reset();
    st->f690A1C = 1;
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);
    CHECK(g_gfx[4] == 0xB900031Du && g_gfx[5] == 0x0C192008u);
    CHECK(st->f690A1C == 0);
    gfx_reset();
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);
    CHECK(g_gfx[5] == 0x0C192038u);

    /* f05 bit 2 adds the four 0xBC colour commands twice per item (once
     * from the table, once from the constants). */
    gfx_reset();
    g_props.count = 1;
    g_props.items[0].f05 = 4;
    g_props.items[0].f04 = 2;               /* colour index 2 */
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);
    CHECK(gfx_count_op(0xBC) == 8);
    {
        size_t i, words = gfx_words();
        int sawTable = 0, sawConst = 0;
        for (i = 0; i + 1 < words; i += 2) {
            if (g_gfx[i] == 0xBC00200Au && g_gfx[i + 1] == g_colTable[2])
                sawTable = 1;
            if (g_gfx[i] == 0xBC00200Au && g_gfx[i + 1] == 0x40404000u)
                sawConst = 1;
        }
        CHECK(sawTable);
        CHECK(sawConst);
    }
    g_props.items[0].f05 = 0;

    /* The depth word depends on whether the two globals differ. */
    gfx_reset();
    st->f6C3364 = 1; st->f6C1174 = 2;
    g_props.count = 0;
    BrScenePropsDraw((const BrPropList *)&g_props, &g_viewMtx);
    {
        size_t i, words = gfx_words();
        int found = 0;
        for (i = 0; i + 1 < words; i += 2)
            if (g_gfx[i] == 0xB7000000u && g_gfx[i + 1] == (0x1000u | 0x000A0205u))
                found = 1;
        CHECK(found);
    }
}

/* ================================================================== */

int main(void)
{
    test_lookat();
    test_rotate_axis();
    test_lights();
    test_maxabs();
    test_texsizeshift();
    test_ptrlist();
    test_fills();
    test_emit_tex();
    test_scratch();
    test_glue();
    test_gated();
    test_bankflip();
    test_resets();
    test_cartable();
    test_carstate();
    test_carstate_fixup();
    test_props();

    if (g_fail == 0)
        printf("slice2_17: all tests passed\n");
    else
        printf("slice2_17: %d FAILURES\n", g_fail);
    return g_fail != 0;
}
