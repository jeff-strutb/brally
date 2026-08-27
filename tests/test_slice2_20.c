/* test_slice2_20.c -- behaviour tests for slice2_20.c.
 *
 * The tests below assert properties of the original, not of my transcription:
 * byte-swap involutions, the deliberate off-by-one loop bounds, the tag-5
 * count carry-over, the CI4/CI8 palette-size split, and -- where testdata/ is
 * present -- structural facts read straight out of the retail ce.rca/bb.rca.
 *
 * Everything under "TEST STAND-INS" is scaffolding, not decompiled code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "slice2_20.h"
#include "br_seg.h"
#include "br_bits.h"
#include "br_vec.h"

static int g_cFail;
static int g_cRun;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        ++g_cRun;                                                             \
        if (!(cond)) {                                                        \
            ++g_cFail;                                                        \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                       \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * TEST STAND-INS
 *
 * br_seg.c / br_bits.c / br_vec.c are not linked by this file's build line,
 * so their three routines are mirrored here.  Everything else is a stub that
 * only records that it was called.
 * ========================================================================== */

/* mirrors port/src/br_seg.c */
void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr)
{
    uint32_t v = *pPtr;
    if (v == 0) return;
    if (v < pMap->n64Base) { *pPtr = 0; return; }
    *pPtr = v - pMap->n64Base + pMap->hostBase;
}
/* slice2_20.o calls the flat-globals BrSegPtrFixup (owned by slice2_16, whose
 * link closure pulls the fade/CD chain); stand it in with identity bases -- the
 * module paths this test reaches rebase against an identity segment map. */
static int32_t s_segN64Base_stub, s_segHostBase_stub;
void BrSegPtrFixup(uint32_t *p)
{
    if (*p == 0) return;
    if ((int32_t)*p < s_segN64Base_stub) { *p = 0; return; }
    *p = (uint32_t)(s_segHostBase_stub - s_segN64Base_stub) + *p;
}
uint32_t BrSegResolve(const BrSegMap *pMap, uint32_t a)
{
    if (a == 0 || a < pMap->n64Base) return 0;
    return a - pMap->n64Base + pMap->hostBase;
}
void BrSegSetBases(BrSegMap *pMap, uint32_t n64Base, uint32_t hostBase)
{
    pMap->n64Base = n64Base;
    pMap->hostBase = hostBase;
}

/* mirrors port/src/br_bits.c: three consecutive u32s, byte-reversed */
static int g_cSwapVec3;
void BrSwapVec3(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    int i;
    ++g_cSwapVec3;
    for (i = 0; i < 3; ++i) {
        unsigned char t;
        t = p[0]; p[0] = p[3]; p[3] = t;
        t = p[1]; p[1] = p[2]; p[2] = t;
        p += 4;
    }
}

/* mirrors the three br_vec.c routines this slice calls */
void BrVec3Scale(BrVec3 *o, const BrVec3 *v, float s)
{ o->x = v->x * s; o->y = v->y * s; o->z = v->z * s; }
void BrVec3Sub(BrVec3 *o, const BrVec3 *a, const BrVec3 *b)
{ o->x = a->x - b->x; o->y = a->y - b->y; o->z = a->z - b->z; }
void BrVec3MulAdd(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float s)
{ o->x = a->x + b->x * s; o->y = a->y + b->y * s; o->z = a->z + b->z * s; }

/* -- stubs ---------------------------------------------------------------- */

static void SwapU16Range(void *pv, int n)
{
    unsigned char *p = (unsigned char *)pv;
    int i;
    for (i = 0; i < n; ++i, p += 2) {
        unsigned char t = p[0]; p[0] = p[1]; p[1] = t;
    }
}

void BrSegSetFlag(uint32_t v) { (void)v; }
void BrSwapU16Array(void *pv, int n) { if (pv && n > 0) SwapU16Range(pv, n); }
void BrSwapRec8Array(void *pv, int n) { (void)pv; (void)n; }
void BrSwapVec3Array(void *pv, int n)
{
    unsigned char *p = (unsigned char *)pv;
    int i;
    if (!p) return;
    for (i = 0; i < n; ++i, p += 12) BrSwapVec3(p);
}

/* PARTIAL stand-in for 0x1002BA80/0x1002BAA0: the real routine swaps and
 * rebases a whole 0x24-byte record.  Only fields +0x00 and +0x04 are handled
 * here, which is all BrRcaFixup goes on to read. */
static BrSegMap s_testSeg;
void BrSwapRec24Array(void *pv, int n)
{
    unsigned char *p = (unsigned char *)pv;
    int i, k;
    if (!p) return;
    for (i = 0; i < n; ++i, p += 0x24) {
        for (k = 0; k < 8; k += 4) {
            uint32_t v;
            unsigned char t;
            t = p[k+0]; p[k+0] = p[k+3]; p[k+3] = t;
            t = p[k+1]; p[k+1] = p[k+2]; p[k+2] = t;
            memcpy(&v, p + k, 4);
            BrSegFixup(&s_testSeg, &v);
            memcpy(p + k, &v, 4);
        }
    }
}

static int g_cDlRegister;
int  BrDlIsRegistered(const void *pv) { return pv == NULL ? 1 : 0; }
void BrDlRegister(void *pv) { (void)pv; ++g_cDlRegister; }
void BrSub10074DC0(int n) { (void)n; }
void BrSub10074E00(void) { }
void BrSub1003445A(void *pv) { (void)pv; }
static int g_cInit35BD1;
void BrSub10035BD1(void) { ++g_cInit35BD1; }
void BrSub10061010(int a, int b) { (void)a; (void)b; }
/* Captures the path BrTrackLoadHandling built, so the extension it appends
 * can be asserted on. It used to discard it, which is why the build's `.hnd`
 * / `.hnt` divergence had no coverage at all. */
static char g_szLastHandlingPath[512];
static int  g_cHandlingLoads;
void BrSub10037990(const char *p)
{
    ++g_cHandlingLoads;
    g_szLastHandlingPath[0] = '\0';
    if (p != NULL) {
        strncpy(g_szLastHandlingPath, p, sizeof g_szLastHandlingPath - 1);
        g_szLastHandlingPath[sizeof g_szLastHandlingPath - 1] = '\0';
    }
}

static float g_fLenResult = 1.0f;
float BrVec3Len(const BrVec3 *v) { (void)v; return g_fLenResult; }

static int g_iRandValue;
int BrRand(void) { return g_iRandValue; }

void *BrChkFRead(void *d, size_t s, size_t c, FILE **f)
{ if (f && *f) { size_t r = fread(d, s, c, *f); (void)r; } return d; }
int   BrChkFileExists(const char *p) { (void)p; return 1; }
FILE **BrChkFReadOpen(const char *p) { (void)p; return NULL; }
int   BrChkFileSize(FILE **f) { (void)f; return 0; }
void  BrChkFClose(FILE **f) { (void)f; }
/* THIS USED TO BE `{ (void)f; d[0] = 0; return 0; }` -- a stand-in that threw
 * the formatted string away. Anything asserting on a path this function built
 * would have passed vacuously against an empty buffer, which is exactly the
 * "fixture leaves the deciding value zeroed" failure CONVENTIONS.md lists. It
 * formats for real now. */
int   BrSprintf(char *d, const char *f, ...)
{
    va_list ap;
    int     n;
    va_start(ap, f);
    n = vsprintf(d, f, ap);
    va_end(ap);
    return n;
}
void  BrFatal(const char *m) { (void)m; }

static int g_c18AA084;
static uint32_t Pfn084(uint32_t a, uint32_t b, void *c)
{ (void)a; (void)b; (void)c; return 0xA5A50000u + (uint32_t)(++g_c18AA084); }
static void Pfn0C4(void *p) { (void)p; }
static void Pfn0C8(void *p, int f) { (void)p; (void)f; }
static void Pfn0CC(void *p, int n) { (void)p; (void)n; }

uint32_t (*g_pfn18AA084)(uint32_t, uint32_t, void *) = Pfn084;
void (*g_pfn18AA0C4)(void *) = Pfn0C4;
void (*g_pfn18AA0C8)(void *, int) = Pfn0C8;
void (*g_pfn18AA0CC)(void *, int) = Pfn0CC;

void    *g_p6C7C3C;
int      g_i6C661C;
int      g_i6C6624;
int      g_i0AC300;
int      g_i4BBE08;
int      g_i0B8C90;
int      g_i10AA3444;
int      g_i10AA3460;
uint8_t  g_ab0C12A0[16];
const char *const g_apszCarFiles[]   = { "ce", "bb" };
const char *const g_apszTrackFiles[] = { "desert.pod" };
uint32_t g_a220B20[0x46];
BrPoolNode g_aPoolNodes[8];
uint16_t g_uPoolFree;
uint16_t g_uPoolHead;
float    g_f6C2CFC = 0.1f;

/* ==========================================================================
 * helpers
 * ========================================================================== */

static void Put32BE(void *pv, uint32_t v)
{
    unsigned char *p = (unsigned char *)pv;
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >>  8); p[3] = (unsigned char)v;
}
static uint32_t Get32(const void *pv)
{ uint32_t v; memcpy(&v, pv, 4); return v; }
static uint16_t Get16(const void *pv)
{ uint16_t v; memcpy(&v, pv, 2); return v; }

/* Install the segment bases the way BrRcaFixup does, then point the image
 * window at a caller-supplied buffer.  s_testSeg mirrors what the module
 * installed so the BrSwapRec24Array stand-in can rebase too. */
#define TEST_N64_BASE 0x803C8000u
static void SetupImage(void *pvImage, size_t cb)
{
    static unsigned char abDummy[0x8200];
    memset(abDummy, 0, sizeof abDummy);
    BrRcaFixup(abDummy, sizeof abDummy);       /* installs the bases */
    g_BrLoad.pImage  = (uint8_t *)pvImage;
    g_BrLoad.uBase32 = BR_LOAD_BASE32;
    g_BrLoad.cbImage = cb;
    BrSegSetBases(&s_testSeg, TEST_N64_BASE, BR_LOAD_BASE32);
}
/* The value a field must hold BEFORE fixup to point at image offset `off`. */
static uint32_t N64At(size_t off) { return TEST_N64_BASE + (uint32_t)off; }
/* ...and the value it holds AFTER fixup. */
static void PutHostAt(void *pv, size_t off)
{
    uint32_t v = BR_LOAD_BASE32 + (uint32_t)off;
    memcpy(pv, &v, 4);
}

/* ==========================================================================
 * tests
 * ========================================================================== */

static void TestSwapPrimitives(void)
{
    unsigned char a[4] = { 0x12, 0x34, 0x56, 0x78 };
    unsigned char b[4];
    unsigned char w[2] = { 0xAB, 0xCD };

    CHECK(BrRead32BE(a) == 0x12345678u, "BrRead32BE");

    memcpy(b, a, 4);
    BrSwap4(b);
    CHECK(memcmp(a, b, 4) != 0, "BrSwap4 must change a non-palindrome");
    CHECK(Get32(b) == BrRead32BE(a), "BrSwap4 == BrRead32BE on this host");
    BrSwap4(b);
    CHECK(memcmp(a, b, 4) == 0, "BrSwap4 twice is the identity");

    BrSwap2(w);
    CHECK(w[0] == 0xCD && w[1] == 0xAB, "BrSwap2");
    BrSwap2(w);
    CHECK(w[0] == 0xAB && w[1] == 0xCD, "BrSwap2 twice is the identity");
}

static void TestSwapRec28(void)
{
    unsigned char rec[0x28], copy[0x28];
    int i;

    for (i = 0; i < 0x28; ++i) rec[i] = (unsigned char)(i * 7 + 1);
    memcpy(copy, rec, sizeof rec);

    BrSwapRec28(rec);
    /* 0x28 bytes = ten dwords, but only 0x00..0x23 and 0x24 are touched --
     * which together is all ten, so the whole record must be reversed
     * dword-wise and nothing beyond it. */
    for (i = 0; i < 0x28; i += 4) {
        CHECK(rec[i + 0] == copy[i + 3] && rec[i + 3] == copy[i + 0]
           && rec[i + 1] == copy[i + 2] && rec[i + 2] == copy[i + 1],
              "BrSwapRec28 dword at +0x%02x", i);
    }
    BrSwapRec28(rec);
    CHECK(memcmp(rec, copy, sizeof rec) == 0, "BrSwapRec28 is an involution");
}

static void TestF08FromMax(void)
{
    static unsigned char image[0x4000];
    unsigned char hdr[0x100];
    unsigned char *pSeg = image + 0x0000;   /* needs 0x2002 bytes */
    unsigned char *pArr = image + 0x3000;
    uint16_t n = 4;
    int i;

    memset(image, 0, sizeof image);
    memset(hdr, 0, sizeof hdr);
    SetupImage(image, sizeof image);

    /* n = 4 entries; entry 0 is the big one and must be ignored.  The count
     * is read host-native -- it has already been swapped by this point. */
    memcpy(pSeg + 0x2000, &n, 2);
    for (i = 0; i < 4; ++i) {
        uint16_t v = (uint16_t)((i == 0) ? 900 : (i * 10));
        memcpy(pArr + i * 2, &v, 2);
    }
    /* BrTrackF08FromMax runs after the header has been fixed up, so these
     * fields already hold rebased values. */
    PutHostAt(hdr + 0x24, 0x0000);
    PutHostAt(hdr + 0x20, 0x3000);

    BrTrackF08FromMax(hdr);
    CHECK(Get32(hdr + 8) == 31u,
          "F08 = 1 + max over indices 1..n-1 (got %u)", Get32(hdr + 8));

    /* All-zero table: index 0 skipped, running max starts at 0 -> result 1. */
    memset(pArr, 0, 8);
    BrTrackF08FromMax(hdr);
    CHECK(Get32(hdr + 8) == 1u, "F08 is never below 1");
}

static void TestFixupNodeLoopBound(void)
{
    static unsigned char image[0x2000];
    unsigned char *pNode = image + 0x100;

    memset(image, 0, sizeof image);
    SetupImage(image, sizeof image);

    /* count = 0 at +0x14.  The original still walks the record at +0x18 and
     * one more at +0x40, because the loop test is `<=`. */
    g_cSwapVec3 = 0;
    BrTrackFixupNode(pNode);
    CHECK(Get16(pNode + 0x14) == 0, "node count decodes to 0");
    CHECK(g_cSwapVec3 == 6,
          "count 0 still processes two 0x28 records (6 Vec3s), got %d",
          g_cSwapVec3);

    /* count = 2 -> the +0x18 record plus three more. */
    memset(image, 0, sizeof image);
    pNode[0x14] = 0; pNode[0x15] = 2;               /* big-endian 2 */
    g_cSwapVec3 = 0;
    BrTrackFixupNode(pNode);
    CHECK(g_cSwapVec3 == 12,
          "count 2 processes count+2 records (12 Vec3s), got %d", g_cSwapVec3);
}

static void TestFixupCmds(void)
{
    static unsigned char image[0x4000];
    unsigned char *h = image + 0x1000;      /* the header lives in the image */
    unsigned char *pVtx = image + 0x0000;
    unsigned char *pRec;

    memset(image, 0, sizeof image);
    SetupImage(image, sizeof image);

    Put32BE(h + 0x224, 4);                  /* four command records */

    /* record 0: tag 0 -- swap +0x00 and +0x04 */
    pRec = h + 0x164 + 0 * 0x0C;
    Put32BE(pRec + 0x00, 0x11223344u);
    Put32BE(pRec + 0x04, 0x55667788u);
    pRec[8] = 0;

    /* record 1: tag 3 -- swap +0x00 only */
    pRec = h + 0x164 + 1 * 0x0C;
    Put32BE(pRec + 0x00, 0x11223344u);
    Put32BE(pRec + 0x04, 0x55667788u);
    pRec[8] = 3;

    /* record 2: tag 4 -- pointer + count, swaps 2 Vec3s at image+0 */
    pRec = h + 0x164 + 2 * 0x0C;
    Put32BE(pRec + 0x00, N64At(0));
    Put32BE(pRec + 0x04, 2);
    pRec[8] = 4;

    /* record 3: tag 5 -- same pointer, NO count of its own */
    pRec = h + 0x164 + 3 * 0x0C;
    Put32BE(pRec + 0x00, N64At(0));
    pRec[8] = 5;

    g_cSwapVec3 = 0;
    BrTrackFixupCmds(h);

    pRec = h + 0x164 + 0 * 0x0C;
    CHECK(Get32(pRec + 0x00) == 0x11223344u, "tag 0 swaps +0x00");
    CHECK(Get32(pRec + 0x04) == 0x55667788u, "tag 0 swaps +0x04");

    pRec = h + 0x164 + 1 * 0x0C;
    CHECK(Get32(pRec + 0x00) == 0x11223344u, "tag 3 swaps +0x00");
    CHECK(BrRead32BE(pRec + 0x04) == 0x55667788u,
          "tag 3 leaves +0x04 big-endian");

    CHECK(g_cSwapVec3 == 4,
          "tag 5 inherits the tag-4 count: expected 2+2 Vec3s, got %d",
          g_cSwapVec3);
    (void)pVtx;

    /* A lone tag 5 with no preceding tag 4 has a zero count and does nothing.
     * Also check tag 8 (out of range) is ignored entirely. */
    memset(image, 0, sizeof image);
    Put32BE(h + 0x224, 2);
    pRec = h + 0x164 + 0 * 0x0C;
    Put32BE(pRec + 0x00, N64At(0));
    pRec[8] = 5;
    pRec = h + 0x164 + 1 * 0x0C;
    Put32BE(pRec + 0x00, 0x11223344u);
    pRec[8] = 8;

    g_cSwapVec3 = 0;
    BrTrackFixupCmds(h);
    CHECK(g_cSwapVec3 == 0, "tag 5 with no prior tag 4 swaps nothing");
    CHECK(BrRead32BE(h + 0x164 + 0x0C) == 0x11223344u,
          "tag 8 is ignored, +0x00 left untouched");
}

static void TestTexCopyPaletteSize(void)
{
    static unsigned char image[0x1000];
    static unsigned char tex[0x400];
    static unsigned char flags[0x24 * 2];
    unsigned char *pTable = image + 0x100;
    unsigned char *pDesc  = image + 0x800;
    uint32_t v;
    int i;

    for (i = 0; i < (int)sizeof tex; ++i) tex[i] = (unsigned char)(i & 0xFF);

    memset(image, 0, sizeof image);
    memset(flags, 0, sizeof flags);
    SetupImage(image, sizeof image);
    g_BrLoad.pTexBase  = tex;
    g_BrLoad.cbTexBase = sizeof tex;
    g_BrLoad.pTexFlags = flags;

    /* One record: dst tex at +0x200, dst palette at +0x300, desc at +0x400. */
    v = BR_LOAD_BASE32 + 0x200; memcpy(pTable + 0x00, &v, 4);
    v = BR_LOAD_BASE32 + 0x300; memcpy(pTable + 0x04, &v, 4);
    v = BR_LOAD_BASE32 + 0x800; memcpy(pTable + 0x08, &v, 4);
    v = 0x00100000u | 0x10u;    memcpy(pTable + 0x20, &v, 4);

    v = 2;  memcpy(pDesc + 0x02, &v, 2);
    v = 0xFFFFFFFFu; memcpy(pDesc + 0x08, &v, 4);
    v = 0x00; memcpy(pDesc + 0x0C, &v, 4);      /* texture at tex+0    */
    v = 0x40; memcpy(pDesc + 0x10, &v, 4);      /* palette at tex+0x40 */

    /* CI4: the parallel field's 0xF000000 nibble is exactly 0x1000000. */
    v = 0x01000000u; memcpy(flags + 0x20, &v, 4);
    memset(image + 0x300, 0xEE, 0x400);
    BrTexCopyRecords(pTable, 1);
    CHECK(memcmp(image + 0x200, tex, 0x10) == 0, "texture bytes copied");
    CHECK(memcmp(image + 0x300, tex + 0x40, 0x20) == 0, "CI4 palette copied");
    CHECK(image[0x300 + 0x20] == 0xEE, "CI4 palette is exactly 0x20 bytes");

    /* Anything else in that nibble means CI8 -> 0x200 bytes. */
    v = 0x02000000u; memcpy(flags + 0x20, &v, 4);
    memset(image + 0x300, 0xEE, 0x400);
    BrTexCopyRecords(pTable, 1);
    CHECK(memcmp(image + 0x300, tex + 0x40, 0x200) == 0, "CI8 palette copied");
    CHECK(image[0x300 + 0x200] == 0xEE, "CI8 palette is exactly 0x200 bytes");

    /* Guards: clearing the 0x100000 bit skips the record entirely. */
    v = 0x10u; memcpy(pTable + 0x20, &v, 4);
    memset(image + 0x200, 0x11, 0x10);
    BrTexCopyRecords(pTable, 1);
    CHECK(image[0x200] == 0x11, "no 0x100000 bit -> record skipped");

    g_BrLoad.pTexBase = NULL;
    g_BrLoad.pTexFlags = NULL;
    g_BrLoad.cbTexBase = 0;
}

static void TestInit220B20(void)
{
    size_t i;
    for (i = 0; i < 0x46; ++i) g_a220B20[i] = 0xDEADBEEFu;
    g_cInit35BD1 = 0;
    BrInit220B20();
    CHECK(g_a220B20[0] == 8u, "first dword is 8");
    for (i = 1; i < 0x46; ++i)
        if (g_a220B20[i] != 0) { CHECK(0, "dword %u not cleared", (unsigned)i); break; }
    CHECK(g_cInit35BD1 == 1, "0x10035BD1 called once");
}

static void TestPoolEmit(void)
{
    static union { double dAlign; unsigned char b[0x1100]; } uThis;
    int i;

    memset(&uThis, 0, sizeof uThis);
    memset(g_aPoolNodes, 0, sizeof g_aPoolNodes);

    /* free list 1 -> 2 -> 3 -> 0 */
    g_uPoolFree = 1;
    g_aPoolNodes[1].uNext = 2;
    g_aPoolNodes[2].uNext = 3;
    g_aPoolNodes[3].uNext = 0;
    g_uPoolHead = 0;

    g_iRandValue = 0;
    g_fLenResult = 1.0f;
    g_f6C2CFC = 0.1f;

    /* delta per call is ((0/65536) + 0.001*0 + 1.0) * 0.1 = 0.1, so the
     * 0.25 threshold is only crossed on the third call. */
    BrPoolEmit(uThis.b);
    CHECK(g_uPoolFree == 1, "no emit below the threshold (1st)");
    BrPoolEmit(uThis.b);
    CHECK(g_uPoolFree == 1, "no emit below the threshold (2nd)");
    BrPoolEmit(uThis.b);
    CHECK(g_uPoolFree == 2, "3rd call pops node 1 off the free list");
    CHECK(g_uPoolHead == 1, "node 1 becomes the live head");
    CHECK(g_aPoolNodes[1].uNext == 0, "node 1 links to the old live head");
    CHECK(g_aPoolNodes[1].b1F == 0xFF, "b1F is forced to 0xFF");
    /* fU = 0*0.1 - (-1.0) = 1.0; 1.0/(len + fU)*255 = 127.5 -> 127 */
    CHECK(g_aPoolNodes[1].b1E == 127,
          "b1E = trunc(255 / (len + fU)) = 127, got %u",
          (unsigned)g_aPoolNodes[1].b1E);
    {
        float f;
        memcpy(&f, uThis.b + 0x105C, 4);
        CHECK(f == 0.0f, "the timer is reset on emit");
    }

    /* Drain: three nodes available, so exactly three emits are possible. */
    for (i = 0; i < 30; ++i) BrPoolEmit(uThis.b);
    CHECK(g_uPoolFree == 0, "free list drains to the 0 sentinel");
    CHECK(g_uPoolHead == 3, "last node taken becomes the live head");
    CHECK(g_aPoolNodes[3].uNext == 2 && g_aPoolNodes[2].uNext == 1,
          "the live list chains in reverse order of allocation");
}

/* --------------------------------------------------------------------------
 * Retail data.  Skipped, loudly, if testdata/ is not next to us.
 * -------------------------------------------------------------------------- */

static unsigned char *LoadFile(const char *pszName, size_t *pcb)
{
    static const char *const apszDirs[] = {
        "testdata/", "../testdata/", "../../testdata/"
    };
    size_t i;
    for (i = 0; i < sizeof apszDirs / sizeof apszDirs[0]; ++i) {
        char szPath[256];
        FILE *f;
        long cb;
        unsigned char *p;
        snprintf(szPath, sizeof szPath, "%s%s", apszDirs[i], pszName);
        f = fopen(szPath, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); cb = ftell(f); fseek(f, 0, SEEK_SET);
        if (cb <= 0) { fclose(f); continue; }
        p = (unsigned char *)malloc((size_t)cb);
        if (!p) { fclose(f); continue; }
        if (fread(p, 1, (size_t)cb, f) != (size_t)cb) {
            free(p); fclose(f); continue;
        }
        fclose(f);
        *pcb = (size_t)cb;
        return p;
    }
    return NULL;
}

static void TestRealRca(const char *pszName)
{
    size_t cb = 0;
    unsigned char *pFile = LoadFile(pszName, &cb);
    unsigned char *pData;
    uint32_t uRaw90;
    int i, cLive = 0;

    if (!pFile) {
        printf("  (skipped %s -- testdata/ not found)\n", pszName);
        return;
    }
    CHECK(cb > 0x8200, "%s is big enough to hold the N64 struct", pszName);
    CHECK(memcmp(pFile, "RCar", 4) == 0, "%s starts with the RCar magic",
          pszName);

    pData = pFile + 0x8000;
    uRaw90 = BrRead32BE(pData + 0x90);

    /* Every pointer slot in the file must be either 0 or a 0x803C.... N64
     * address -- that is what makes 0x803C8000 the right base. */
    for (i = 0; i < 30; ++i) {
        uint32_t v = BrRead32BE(pData + 0x18 + i * 4);
        if (v != 0) {
            ++cLive;
            CHECK(v >= 0x803C8000u,
                  "%s slot %d (0x%08x) is above the N64 base", pszName, i, v);
        }
    }
    CHECK(cLive > 0, "%s has at least one live display list", pszName);

    g_i0AC300 = 1;                       /* take the "no surfaces" branch */
    BrRcaFixup(pFile, cb);

    {
        uint32_t cEnt = Get32(pData + 0x10);
        unsigned char *pTbl = (unsigned char *)BrLoadResolve(Get32(pData + 0x14));
        CHECK(cEnt > 0 && cEnt < 0x400u,
              "%s: the +0x14 table entry count is plausible (0x%x)",
              pszName, cEnt);
        CHECK(pTbl != NULL,
              "%s: the +0x14 table resolves inside the file", pszName);
        if (pTbl != NULL) {
            CHECK((size_t)(pTbl - pData) + cEnt * 0x24u <= cb - 0x8000u,
                  "%s: the whole 0x24-stride table fits in the file", pszName);
        }
    }

    for (i = 0; i < 30; ++i) {
        uint32_t v = Get32(pData + 0x18 + i * 4);
        CHECK(v == 0 || BrLoadResolve(v) != NULL,
              "%s slot %d resolves or is null", pszName, i);
    }

    /* The +0x90 / +0x94 crossing: +0x90 is byte-swapped but never rebased, so
     * it still holds the raw N64 address afterwards. */
    CHECK(Get32(pData + 0x90) == uRaw90,
          "%s: +0x90 is swapped but not rebased (0x%08x vs 0x%08x)",
          pszName, Get32(pData + 0x90), uRaw90);
    if (uRaw90 >= 0x803C8000u) {
        CHECK(BrLoadResolve(Get32(pData + 0x90)) == NULL,
              "%s: the unrebased +0x90 does not resolve", pszName);
    }

    /* The zero branch must have blanked the five handle slots. */
    for (i = 0x80; i <= 0x90; i += 4)
        CHECK(Get32(pFile + i) == 0, "%s: handle slot +0x%x cleared",
              pszName, i);

    free(pFile);
}

/* 0x10031140 (Glide) / 0x10037A90 (D3D)  BrTrackLoadHandling
 *
 * THE TWO BUILDS APPEND DIFFERENT EXTENSIONS and the port had the wrong one.
 * Glide 0x1003117B loads 0x100AA338 == ".hnt"; D3D 0x10037AC9 loads
 * 0x100AABA8 == ".hnd"; neither literal appears in the other image. The disc
 * ships testdata/tracks/desert.hnt and coast.hnt and no .hnd at all, so the
 * reference build is also the one that agrees with the data.
 *
 * The assertion is against the file the extracted assets actually contain,
 * not against the port's own macro, so it cannot pass by agreeing with
 * itself -- and a literal is used rather than BR_TRACK_HANDLING_EXT for the
 * same reason. */
static void TestTrackHandlingExt(void)
{
    const char *p;

    g_cHandlingLoads = 0;
    BrTrackLoadHandling(0);   /* g_apszTrackFiles[0] == "desert.pod" */

    /* NOTE (added by the LEAVE-return pass, not by this test's author): this
     * file's CHECK is `CHECK(cond, fmt, ...)` and expands `printf(fmt, ...)`,
     * so a bare `CHECK(cond)` does not compile under -std=c99. The seven
     * assertions below arrived without messages and broke ./build.sh; the
     * conditions are untouched and only the messages were added. */
    CHECK(g_cHandlingLoads == 1, "loads=%d, want 1", g_cHandlingLoads);
    CHECK(strcmp(g_szLastHandlingPath, "tracks/desert.hnt") == 0,
          "path=\"%s\", want \"tracks/desert.hnt\"", g_szLastHandlingPath);

    /* Spelled out separately so a failure says WHICH half is wrong: the
     * "tracks/" + name join, or the extension swap. */
    CHECK(strncmp(g_szLastHandlingPath, "tracks/", 7) == 0,
          "no \"tracks/\" prefix: \"%s\"", g_szLastHandlingPath);
    p = strrchr(g_szLastHandlingPath, '.');
    CHECK(p != NULL && strcmp(p, ".hnt") == 0,
          "extension is \"%s\", want \".hnt\"", (p != NULL) ? p : "(none)");

    /* The original replaces the extension in place rather than appending, so
     * exactly one dot survives -- ".pod" must be gone, not merely followed. */
    CHECK(strstr(g_szLastHandlingPath, ".pod") == NULL,
          "\".pod\" survives in \"%s\"", g_szLastHandlingPath);
    CHECK(strchr(g_szLastHandlingPath, '.') == p,
          "more than one dot in \"%s\"", g_szLastHandlingPath);
}

int main(void)
{
    printf("slice2_20 tests\n");
    TestTrackHandlingExt();
    TestSwapPrimitives();
    TestSwapRec28();
    TestF08FromMax();
    TestFixupNodeLoopBound();
    TestFixupCmds();
    TestTexCopyPaletteSize();
    TestInit220B20();
    TestPoolEmit();
    TestRealRca("ce.rca");
    TestRealRca("bb.rca");

    printf("%d checks, %d failures\n", g_cRun, g_cFail);
    return g_cFail != 0;
}
