/* test_slice5_62.c -- behaviour and invariant tests for slice5_62.c.
 *
 * The build line is
 *   clang -std=c99 -Wall -Wextra -Iport/include \
 *         port/tests/test_slice5_62.c port/src/slice5_62.c -lm -o /tmp/i62
 * so everything slice5_62.c calls but does not define has to live here.  All
 * such definitions are marked STAND-IN and exist in this file and nowhere
 * else, per the contract.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "slice5_62.h"
#include "slice1_02.h"
#include "slice4_50.h"

static int g_fail;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        double d_ = (double)(a) - (double)(b);                             \
        if (!(d_ > -(eps) && d_ < (eps))) {                                \
            printf("FAIL %s:%d: %s ~= %s (%g vs %g)\n", __FILE__, __LINE__,\
                   #a, #b, (double)(a), (double)(b));                      \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

/* ==================================================================== */
/* STAND-INS                                                            */
/* ==================================================================== */

/* STAND-IN for br_pool.c (0x10069490).  Mirrors the real implementation --
 * slice5_62.c only adapts it, so the test needs the real semantics to say
 * anything useful about the adapter. */
void *BrPoolAlloc(BrPool *pPool)
{
    uint32_t index;

    if (pPool->count < BR_POOL_SLOTS_USED) {
        index = pPool->count + pPool->frame * BR_POOL_SLOTS_BANK;
        pPool->count++;
    } else {
        pPool->count++;
        index = BR_POOL_SLOTS_USED + pPool->frame * BR_POOL_SLOTS_BANK;
    }
    return pPool->pBase + (size_t)index * BR_POOL_SLOT_SIZE;
}

/* STAND-IN for br_crt.c (0x1007C8A0).  Copied from the real one so the
 * out-of-range behaviour under test is the project's established one. */
int32_t BrFtolTrunc(float f)
{
    double d = (double)f;

    if (!(d >= -2147483648.0 && d <= 2147483647.0)) {
        return 0;
    }
    return (int32_t)d;
}

/* STAND-IN for slice1_09.c (0x100741B0).  No zero guard, as documented. */
void BrVec4Normalise(BrVec4 *pV)
{
    float k = 1.0f / (float)sqrt((double)(pV->f00 * pV->f00 +
                                          pV->f04 * pV->f04 +
                                          pV->f08 * pV->f08 +
                                          pV->f0C * pV->f0C));
    pV->f00 *= k;
    pV->f04 *= k;
    pV->f08 *= k;
    pV->f0C *= k;
}

/* STAND-INs for slice1_02.c */
static int s_lock, s_unlock;
void BrNetMutexLock(void *hMutex)   { (void)hMutex; ++s_lock;   }
void BrNetMutexUnlock(void *hMutex) { (void)hMutex; ++s_unlock; }

static int32_t s_f02C;
static int32_t s_getSlotArg;
int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{
    (void)pNet;
    s_getSlotArg = slot;
    return s_f02C;
}

/* STAND-INs for the four globals slice4_50.h declares and the send it wraps */
int32_t  g_br094294;
uint8_t  g_brAD0854[3];
int32_t  g_br277B48;
char    *g_brPB4E2E8;

static struct {
    int      n;
    void    *pp;
    int32_t  a1, a2;
    uint8_t  r, g, b;
    int32_t  a6;
    char    *psz;
    int32_t  a8, a9;
} s_send;

void BrNetSend4760(BrDPlay **ppDPlay, int32_t a1, int32_t a2,
                   uint8_t r, uint8_t g, uint8_t b,
                   int32_t a6, char *pszText, int32_t a8, int32_t a9)
{
    ++s_send.n;
    s_send.pp  = (void *)ppDPlay;
    s_send.a1  = a1;   s_send.a2 = a2;
    s_send.r   = r;    s_send.g  = g;   s_send.b = b;
    s_send.a6  = a6;   s_send.psz = pszText;
    s_send.a8  = a8;   s_send.a9 = a9;
}

/* ==================================================================== */
/* 0x100419D0                                                           */
/* ==================================================================== */

static struct {
    int         n;
    void       *pThis;
    void       *p;
    int32_t     a2, a3;
    const void *pv;
} s_dispatch;

static int32_t TargetMethod(void *pThis, void *p, int32_t a2, int32_t a3,
                            const void *pv)
{
    ++s_dispatch.n;
    s_dispatch.pThis = pThis;
    s_dispatch.p     = p;
    s_dispatch.a2    = a2;
    s_dispatch.a3    = a3;
    s_dispatch.pv    = pv;
    return 7;
}

static void test_419D0(void)
{
    BrX419D0State  *pSt = BrX419D0GetState();
    BrX419D0Method  avtbl[BR_X419D0_VTBL_SLOT + 1];
    BrX419D0Target  target;
    BrX419D0Owner   owner;
    /* apObj[1] is the head of a run, so the table needs room behind it. */
    union {
        BrX419D0Table t;
        unsigned char raw[sizeof(BrX419D0Table) +
                          4 * sizeof(BrX419D0Target *)];
    } u;
    BrX419D0Table  *pTable = &u.t;
    int             i;
    int             dummy  = 0;
    int             other  = 0;

    for (i = 0; i < BR_X419D0_VTBL_SLOT; ++i) {
        avtbl[i] = NULL;
    }
    avtbl[BR_X419D0_VTBL_SLOT] = TargetMethod;
    target.apfn = avtbl;

    memset(&u, 0, sizeof u);
    memset(&owner, 0, sizeof owner);
    owner.pTable = pTable;

    pSt->pOwner  = &owner;
    pSt->index   = 0;
    pSt->pvAB558 = &other;

    /* The selected slot is NULL: the original's own guard, and the reason
     * this function can be a complete no-op. */
    pTable->apObj[0] = NULL;
    s_dispatch.n = 0;
    BrExt_100419D0(&dummy);
    CHECK(s_dispatch.n == 0);

    /* Non-NULL slot: dispatch through vtable slot 13 with (p, 1, 1, pv). */
    pTable->apObj[0] = &target;
    BrExt_100419D0(&dummy);
    CHECK(s_dispatch.n == 1);
    CHECK(s_dispatch.pThis == (void *)&target);
    CHECK(s_dispatch.p == (void *)&dummy);
    CHECK(s_dispatch.a2 == 1 && s_dispatch.a3 == 1);
    CHECK(s_dispatch.pv == (const void *)&other);

    /* The index really does pick the slot, and an empty one at that index is
     * still a no-op even though slot 0 is occupied. */
    pSt->index = 2;
    s_dispatch.n = 0;
    BrExt_100419D0(&dummy);
    CHECK(s_dispatch.n == 0);

    pTable->apObj[2] = &target;
    BrExt_100419D0(&dummy);
    CHECK(s_dispatch.n == 1);
    CHECK(s_dispatch.pThis == (void *)&target);

    pSt->pOwner = NULL;
    s_dispatch.n = 0;
    BrExt_100419D0(&dummy);
    CHECK(s_dispatch.n == 0);   /* DEVIATION guard: NULL owner is a no-op */
}

/* ==================================================================== */
/* 0x1005FCF0                                                           */
/* ==================================================================== */

static void test_5FCF0(void)
{
    BrSessLatch *p = BrSessLatchGetState();

    memset(p, 0, sizeof *p);
    p->f094354 = 0x11111111;
    p->f094358 = 0x22222222;
    p->f09435C = 0x33333333;
    p->fAA28A0 = 0x44444444;
    p->fB4E1D0 = 0x55555555;
    p->fAA28A4 = 0x0000AB01;   /* only the low byte is copied / indexes */
    p->fAA28B8 = 0x0000CD00;
    p->fAA27E0 = 0xDEADBEEFu;

    BrSub1005FCF0();

    /* The straight copies, including the two that are byte-narrowed. */
    CHECK(p->fAA27EC == 0x11111111);
    CHECK(p->fAA27F4 == 0x22222222);
    CHECK(p->fAA27F0 == 0x33333333);
    CHECK(p->fAA26F0 == 0x44444444);   /* full dword */
    CHECK(p->fAA27F8 == 0x55555555);
    CHECK(p->fAA26F4 == 0x00);         /* (uint8)0xCD00 */
    CHECK(p->fAA26F5 == 0x01);         /* (uint8)0xAB01 */

    /* Both accumulators come from the two halves of the SAME dword. */
    CHECK(p->fAA2A10 == 0xBEEFu);
    CHECK(p->fAA2A14 == 0xDEADu);

    /* OR-accumulate, never assign: a second call with different bits must
     * add to the accumulators, not replace them. */
    p->fAA27E0 = 0x00010001u;
    BrSub1005FCF0();
    CHECK(p->fAA2A10 == (0xBEEFu | 0x0001u));
    CHECK(p->fAA2A14 == (0xDEADu | 0x0001u));

    /* The lookup uses index 2*(12*fAA28B8 + fAA28A4) and is SKIPPED entirely
     * while f0AA010 is non-zero. */
    p->fAA28B8 = 0;
    p->fAA28A4 = 1;            /* -> bytes 2 and 3 of the table */
    p->f0B380C = 0xFFFFFFFFu;
    p->f22B350 = 0xFFFFFFFFu;
    p->f0AA010 = 1;
    BrSub1005FCF0();
    CHECK(p->f0B380C == 0xFFFFFFFFu);
    CHECK(p->f22B350 == 0xFFFFFFFFu);

    p->f0AA010 = 0;
    BrSub1005FCF0();
    CHECK(p->f0B380C == g_brB3820[2]);
    CHECK(p->f22B350 == g_brB3820[3]);

    /* Row stride is 12 entries of two bytes: (1,0) lands at byte 24. */
    p->fAA28B8 = 1;
    p->fAA28A4 = 0;
    BrSub1005FCF0();
    CHECK(p->f0B380C == g_brB3820[24]);
    CHECK(p->f22B350 == g_brB3820[25]);
}

/* ==================================================================== */
/* 0x1001E7C0                                                           */
/* ==================================================================== */

static void test_1E7C0(void)
{
    BrRasterSel *p = BrRasterSelGetState();

    /* Entry 4: bit 12 wins over bit 13, and "neither" means 1. */
    memset(p, 0, sizeof *p);
    p->geoPrev = 0;
    p->geoCur  = BR_GEO_BIT12 | BR_GEO_BIT13;
    BrGbiGeoModeChanged();
    CHECK(p->aPending[4] == 2);

    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT13;
    BrGbiGeoModeChanged();
    CHECK(p->aPending[4] == 3);

    memset(p, 0, sizeof *p);
    p->geoCur  = BR_GEO_BIT12;
    p->geoPrev = BR_GEO_BIT12;      /* no delta in bits 12/13 */
    BrGbiGeoModeChanged();
    CHECK(p->aPending[4] == 0);     /* untouched */
    CHECK((p->dirty & 0x10u) == 0);

    /* The dirty bit is CLEARED when the shadow already agrees -- it is not a
     * sticky "changed" flag. */
    memset(p, 0, sizeof *p);
    p->geoCur      = BR_GEO_BIT12;
    p->dirty       = 0xFFFFFFFFu;
    p->aShadow[4]  = 2;
    BrGbiGeoModeChanged();
    CHECK((p->dirty & 0x10u) == 0);

    memset(p, 0, sizeof *p);
    p->geoCur      = BR_GEO_BIT12;
    p->aShadow[4]  = 99;
    BrGbiGeoModeChanged();
    CHECK((p->dirty & 0x10u) != 0);

    /* Entry 5 and f4C16A0 always move together, and take 2 / 8. */
    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT0;
    BrGbiGeoModeChanged();
    CHECK(p->aPending[5] == 2 && p->f4C16A0 == 2);
    CHECK((p->dirty & 0x20u) != 0);

    memset(p, 0, sizeof *p);
    p->geoPrev = BR_GEO_BIT0;       /* delta in bit 0, cur has it clear */
    p->geoCur  = 0;
    BrGbiGeoModeChanged();
    CHECK(p->aPending[5] == 8 && p->f4C16A0 == 8);

    /* Bit 18 overrides whatever bit 17 selected, and bit 19 picks which. */
    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT0 | BR_GEO_BIT17;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_10021E80);

    p->f4C0DC0 = 1;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_10022480);

    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT0 | BR_GEO_BIT17 | BR_GEO_BIT18;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_100228F0);   /* bit 19 clear */

    p->geoCur |= BR_GEO_BIT19;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_100231D0);

    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT0;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_10021BD0);

    /* Bit 0 clear: an entirely different pair of slots. */
    memset(p, 0, sizeof *p);
    p->geoCur = 0;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_10023A10);
    CHECK(p->pfn0A7CEC == BR_FN_CEC_1001F2B0);
    CHECK(p->pfn0A7CB4 == BR_FN_CB4_100205F0);

    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT17 | BR_GEO_BIT9;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7A00 == BR_FN_A00_10023CC0);
    CHECK(p->pfn0A7CEC == BR_FN_CEC_1001FBE0);
    CHECK(p->pfn0A7CB4 == BR_FN_CB4_10020860);

    /* Bit 9 chooses the CEC/CB4 pair on the bit-0-set side too, and the two
     * always move together -- never one without the other. */
    memset(p, 0, sizeof *p);
    p->geoCur = BR_GEO_BIT0 | BR_GEO_BIT9;
    BrGbiGeoModeChanged();
    CHECK(p->pfn0A7CEC == BR_FN_CEC_1001CFF0);
    CHECK(p->pfn0A7CB4 == BR_FN_CB4_1001E170);
}

/* ==================================================================== */
/* 0x1001C820                                                           */
/* ==================================================================== */

static void test_1C820(void)
{
    BrRasterSel *p = BrRasterSelGetState();

    /* f4C0DC0 is zeroed on entry no matter what, and only one pair puts it
     * back. */
    memset(p, 0, sizeof *p);
    p->f4C0DC0 = 1;
    BrSub_1001C820(0, 0);
    CHECK(p->f4C0DC0 == 0);

    memset(p, 0, sizeof *p);
    BrSub_1001C820(0xFC317E02u, 0x5FFEF3FAu);
    CHECK(p->f4C0DC0 == 1);
    BrSub_1001C820(0xFC317E02u, 0x51FEF3FAu);   /* the second accepted w1 */
    CHECK(p->f4C0DC0 == 1);

    /* f6C6618 selects between the two handlers on every recognising path. */
    memset(p, 0, sizeof *p);
    p->f6C6618 = 0;
    BrSub_1001C820(0xFCFFFFFFu, 0xFFFE793Cu);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001BC90);

    p->f6C6618 = 1;
    BrSub_1001C820(0xFCFFFFFFu, 0xFFFE793Cu);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001C690);

    /* The two f4C0BB4 pairs, and the CA10/CA90 handler pair they share. */
    memset(p, 0, sizeof *p);
    BrSub_1001C820(0xFCFFFFFFu, 0xFFFCF87Cu);
    CHECK(p->f4C0BB4 == 0xFFFFFFFFu);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001CA10);

    memset(p, 0, sizeof *p);
    BrSub_1001C820(0xFCFFFFFFu, 0xFFFF73B9u);
    CHECK(p->f4C0BB4 == 0xFFFFFFFFu);

    memset(p, 0, sizeof *p);
    p->f6C6618 = 1;
    BrSub_1001C820(0xFC567EACu, 0xFFFFF3F9u);
    CHECK(p->f4C0BB4 == 0xFF000000u);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001CA90);

    memset(p, 0, sizeof *p);
    BrSub_1001C820(0xFC127FFFu, 0xFFFFF838u);
    CHECK(p->f4C0BB4 == 0xFF000000u);

    /* The one path that leaves pfn0A79EC completely alone. */
    memset(p, 0, sizeof *p);
    p->pfn0A79EC = 0xDEADBEEFu;
    p->f6C661C = 0;
    p->f6C6624 = 0;
    BrSub_1001C820(0xFC127E08u, 0xF3FFF2F8u);
    CHECK(p->pfn0A79EC == 0xDEADBEEFu);

    p->f6C6624 = 1;             /* either one being set is enough */
    BrSub_1001C820(0xFC127E08u, 0xF3FFF2F8u);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001BC90);

    /* An unrecognised pair still assigns the handler. */
    memset(p, 0, sizeof *p);
    p->pfn0A79EC = 0xDEADBEEFu;
    p->f6C6618 = 1;
    BrSub_1001C820(0x12345678u, 0x9ABCDEF0u);
    CHECK(p->pfn0A79EC == BR_FN_9EC_1001C690);

    /* The tail swaps pfn0A7A00 in ONE direction only, and leaves any other
     * value untouched. */
    memset(p, 0, sizeof *p);
    p->pfn0A7A00 = BR_FN_A00_10021E80;
    BrSub_1001C820(0xFC317E02u, 0x5FFEF3FAu);        /* sets the flag */
    CHECK(p->pfn0A7A00 == BR_FN_A00_10022480);

    p->pfn0A7A00 = BR_FN_A00_10022480;
    BrSub_1001C820(0, 0);                            /* flag stays 0 */
    CHECK(p->pfn0A7A00 == BR_FN_A00_10021E80);

    p->pfn0A7A00 = BR_FN_A00_10023CC0;
    BrSub_1001C820(0, 0);
    CHECK(p->pfn0A7A00 == BR_FN_A00_10023CC0);       /* untouched */

    /* Round trip: flag-set then flag-clear restores the original value. */
    p->pfn0A7A00 = BR_FN_A00_10021E80;
    BrSub_1001C820(0xFC317E02u, 0x5FFEF3FAu);
    BrSub_1001C820(0, 0);
    CHECK(p->pfn0A7A00 == BR_FN_A00_10021E80);
}

/* ==================================================================== */
/* 0x100020D0                                                           */
/* ==================================================================== */

static void test_20D0(void)
{
    char sz[64];

    BrSub_100020D0(sz, 0.0f);
    CHECK(strcmp(sz, "0:00.00") == 0);

    BrSub_100020D0(sz, 65.43f);
    CHECK(strcmp(sz, "1:05.43") == 0);

    BrSub_100020D0(sz, 59.99f);
    CHECK(strcmp(sz, "0:59.99") == 0);

    BrSub_100020D0(sz, 3599.99f);
    CHECK(strcmp(sz, "59:59.99") == 0);

    /* Minutes are not wrapped at 60 -- there is no hours field. */
    BrSub_100020D0(sz, 3600.0f);
    CHECK(strcmp(sz, "60:00.00") == 0);

    /* Every division truncates toward zero, so a negative duration produces
     * three independently negative fields rather than a clamp. */
    BrSub_100020D0(sz, -1.5f);
    CHECK(strcmp(sz, "0:-1.-50") == 0);
}

/* ==================================================================== */
/* 0x1003289F                                                           */
/* ==================================================================== */

static uint32_t s_dl[16];

static void ScissorReset(BrScissorClamp *p)
{
    memset(s_dl, 0, sizeof s_dl);
    p->minX = 0;
    p->maxX = 320;
    p->minY = 0;
    p->maxY = 240;
    p->doubled = 0;
    p->pDl = s_dl;
}

static void test_3289F(void)
{
    BrScissorClamp *p = BrScissorClampGetState();
    uint32_t x0, y0, x1, y1;

    /* An in-bounds rectangle passes through unchanged, and the cursor
     * advances by exactly two eight-byte commands. */
    ScissorReset(p);
    BrSub_1003289F(10, 20, 100, 50);
    CHECK(p->pDl == s_dl + 4);
    CHECK(s_dl[0] == BR_SCISSOR_TAG_SYNC);
    CHECK(s_dl[1] == 0);
    CHECK((s_dl[2] & 0xFF000000u) == BR_SCISSOR_TAG_RECT);
    CHECK((s_dl[3] & 0xFF000000u) == 0);   /* no tag on the second word */

    x0 = (s_dl[2] >> 12) & 0xFFFu;  y0 = s_dl[2] & 0xFFFu;
    x1 = (s_dl[3] >> 12) & 0xFFFu;  y1 = s_dl[3] & 0xFFFu;
    CHECK(x0 == 10 && y0 == 20 && x1 == 110 && y1 == 70);

    /* Underflow moves the origin AND shortens the extent, so the far corner
     * stays where it was. */
    ScissorReset(p);
    BrSub_1003289F(-30, -40, 100, 100);
    x0 = (s_dl[2] >> 12) & 0xFFFu;  y0 = s_dl[2] & 0xFFFu;
    x1 = (s_dl[3] >> 12) & 0xFFFu;  y1 = s_dl[3] & 0xFFFu;
    CHECK(x0 == 0 && y0 == 0);
    CHECK(x1 == 70 && y1 == 60);

    /* Overflow only shortens the extent. */
    ScissorReset(p);
    BrSub_1003289F(300, 200, 100, 100);
    x0 = (s_dl[2] >> 12) & 0xFFFu;  y0 = s_dl[2] & 0xFFFu;
    x1 = (s_dl[3] >> 12) & 0xFFFu;  y1 = s_dl[3] & 0xFFFu;
    CHECK(x0 == 300 && y0 == 200);
    CHECK(x1 == 320 && y1 == 240);

    /* Entirely off-screen: the extent floors at 0 but the ORIGIN is never
     * re-checked, so a zero-size command is still emitted out of bounds. */
    ScissorReset(p);
    BrSub_1003289F(500, 400, 10, 10);
    x0 = (s_dl[2] >> 12) & 0xFFFu;  y0 = s_dl[2] & 0xFFFu;
    x1 = (s_dl[3] >> 12) & 0xFFFu;  y1 = s_dl[3] & 0xFFFu;
    CHECK(x0 == 500 && y0 == 400);
    CHECK(x1 == x0 && y1 == y0);
    CHECK(p->pDl == s_dl + 4);      /* still emitted */

    /* The doubling happens AFTER the clamp, so the result can exceed the
     * bounds it was just clamped to. */
    ScissorReset(p);
    p->doubled = 1;
    BrSub_1003289F(300, 200, 100, 100);
    x0 = (s_dl[2] >> 12) & 0xFFFu;  y0 = s_dl[2] & 0xFFFu;
    x1 = (s_dl[3] >> 12) & 0xFFFu;  y1 = s_dl[3] & 0xFFFu;
    CHECK(x0 == 600 && y0 == 400);
    CHECK(x1 == 640 && y1 == 480);
    CHECK(x1 > (uint32_t)p->maxX);
}

/* ==================================================================== */
/* 0x10069490                                                           */
/* ==================================================================== */

static void test_69490(void)
{
    static unsigned char buf[BR_POOL_SLOTS_BANK * BR_POOL_SLOT_SIZE * 2];
    BrMat4 *pA, *pB, *pLast;
    int i;

    g_brPool10069490.pBase = buf;
    g_brPool10069490.frame = 0;
    g_brPool10069490.count = 0;

    pA = BrSub_10069490();
    pB = BrSub_10069490();
    CHECK((unsigned char *)pA == buf);
    CHECK((unsigned char *)pB - (unsigned char *)pA == BR_POOL_SLOT_SIZE);

    /* Past the 256th allocation every request aliases the one overflow slot,
     * and the count keeps rising. */
    for (i = 2; i < BR_POOL_SLOTS_USED; ++i) {
        (void)BrSub_10069490();
    }
    pLast = BrSub_10069490();
    CHECK(pLast == (BrMat4 *)(buf + BR_POOL_SLOTS_USED * BR_POOL_SLOT_SIZE));
    CHECK(BrSub_10069490() == pLast);
    CHECK(BrSub_10069490() == pLast);
    CHECK(g_brPool10069490.count == BR_POOL_SLOTS_USED + 3);

    /* Frame 1 starts a bank of 257, not 256. */
    g_brPool10069490.frame = 1;
    g_brPool10069490.count = 0;
    CHECK((unsigned char *)BrSub_10069490() ==
          buf + BR_POOL_SLOTS_BANK * BR_POOL_SLOT_SIZE);
}

/* ==================================================================== */
/* 0x10004FC0                                                           */
/* ==================================================================== */

static BrNetState s_net;

static void KeepReset(BrNetKeepAlive *p)
{
    memset(&s_net, 0, sizeof s_net);
    memset(p, 0, sizeof *p);
    memset(&s_send, 0, sizeof s_send);
    p->pNet    = &s_net;
    p->f22AF18 = 1;
    p->f22AF14 = 1;
    p->f6909E0 = 0;
    p->fACEE50 = 0;
    p->f0BD3E0 = 1;
    s_lock = s_unlock = 0;
}

static void test_4FC0(void)
{
    BrNetKeepAlive *p = BrNetKeepAliveGetState();
    int i;

    /* A counter sitting at zero never starts on its own. */
    KeepReset(p);
    s_net.f1022AAF4 = 0;
    BrNetKeepAliveTick();
    CHECK(s_net.f1022AAF4 == 0);
    CHECK(s_send.n == 0);
    CHECK(s_lock == 1 && s_unlock == 1);   /* the mutex is still taken */

    /* Ticks 1..26 advance and send; the 27th wraps to 0 and does NOT send. */
    KeepReset(p);
    s_net.f1022AAF4 = 1;
    for (i = 0; i < BR_NET_KEEPALIVE_PERIOD - 2; ++i) {
        BrNetKeepAliveTick();
    }
    CHECK(s_net.f1022AAF4 == BR_NET_KEEPALIVE_PERIOD - 1);
    CHECK(s_send.n == BR_NET_KEEPALIVE_PERIOD - 2);
    CHECK(s_net.f1022AF20 == 0);

    BrNetKeepAliveTick();                  /* the wrapping tick */
    CHECK(s_net.f1022AAF4 == 0);
    CHECK(s_net.f1022AF20 == 1);
    CHECK(s_send.n == BR_NET_KEEPALIVE_PERIOD - 2);   /* unchanged */

    /* Each of the four guards suppresses the send on its own. */
    KeepReset(p);  s_net.f1022AAF4 = 1;  p->f22AF18 = 0;
    BrNetKeepAliveTick();  CHECK(s_send.n == 0);

    KeepReset(p);  s_net.f1022AAF4 = 1;  p->f22AF14 = 0;
    BrNetKeepAliveTick();  CHECK(s_send.n == 0);

    KeepReset(p);  s_net.f1022AAF4 = 1;  p->f6909E0 = 1;
    BrNetKeepAliveTick();  CHECK(s_send.n == 0);

    /* fACEE50 < f0BD3E0 is a strict less-than: equal suppresses. */
    KeepReset(p);  s_net.f1022AAF4 = 1;  p->fACEE50 = 5;  p->f0BD3E0 = 5;
    BrNetKeepAliveTick();  CHECK(s_send.n == 0);

    /* The argument shuffle, including the low-byte-only rewrite of a8 and
     * the literal 0 that MSVC's stack reuse supplies as a9. */
    KeepReset(p);
    s_net.f1022AAF4 = 1;
    p->f22B34C = 0x55AA;
    p->p277B40 = (void *)&s_net;
    g_br094294 = 3;
    g_br277B48 = 0x1234;
    g_brPB4E2E8 = (char *)"text";
    g_brAD0854[0] = 0x11; g_brAD0854[1] = 0x22; g_brAD0854[2] = 0x33;
    s_f02C = (int32_t)0x7EFFFFFFu;   /* low byte 0xFF */
    BrNetKeepAliveTick();
    CHECK(s_send.n == 1);
    CHECK(s_send.pp == (void *)&p->p277B40);   /* the ADDRESS of the global */
    CHECK(s_getSlotArg == 3);                  /* the one real argument */
    CHECK(s_send.a1 == 3);
    CHECK(s_send.a2 == 0x55AA);
    CHECK(s_send.r == 0x11 && s_send.g == 0x22 && s_send.b == 0x33);
    CHECK(s_send.a6 == 0x1234);
    CHECK(strcmp(s_send.psz, "text") == 0);
    /* (0xFF & 0xBF) | 0x80 == 0xBF, and the upper 24 bits survive. */
    CHECK((uint32_t)s_send.a8 == 0x7EFFFFBFu);
    CHECK(s_send.a9 == 0);
}

/* ==================================================================== */
/* 0x100765E0                                                           */
/* ==================================================================== */

/* The row-vector rotation matrix of the quaternion (w,x,y,z) -- the same
 * convention br_mat.h and slice3_44.h use.  Building the matrix here and
 * recovering the quaternion is a genuine round trip: nothing in this helper
 * is copied from the function under test. */
static void QuatToMat(BrMat4 *pM, float w, float x, float y, float z)
{
    memset(pM, 0, sizeof *pM);
    pM->m[0][0] = 1.0f - 2.0f * (y * y + z * z);
    pM->m[0][1] = 2.0f * (x * y + w * z);
    pM->m[0][2] = 2.0f * (x * z - w * y);
    pM->m[1][0] = 2.0f * (x * y - w * z);
    pM->m[1][1] = 1.0f - 2.0f * (x * x + z * z);
    pM->m[1][2] = 2.0f * (y * z + w * x);
    pM->m[2][0] = 2.0f * (x * z + w * y);
    pM->m[2][1] = 2.0f * (y * z - w * x);
    pM->m[2][2] = 1.0f - 2.0f * (x * x + y * y);
    pM->m[3][3] = 1.0f;
}

static void RoundTrip(float w, float x, float y, float z, int expectCase)
{
    BrMat4 m;
    BrVec4 q;
    float  len;

    QuatToMat(&m, w, x, y, z);
    BrSub100765E0(&m, &q);

    /* Which branch the discriminants pick -- these are NOT "largest diagonal
     * element", see the header. */
    if (expectCase == 1) { CHECK(!(m.m[0][0] < 0.0f) && !(m.m[1][1] + m.m[2][2] < 0.0f)); }
    if (expectCase == 2) { CHECK(!(m.m[0][0] < 0.0f) &&  (m.m[1][1] + m.m[2][2] < 0.0f)); }
    if (expectCase == 3) { CHECK( (m.m[0][0] < 0.0f) && !(m.m[1][1] < m.m[2][2])); }
    if (expectCase == 4) { CHECK( (m.m[0][0] < 0.0f) &&  (m.m[1][1] < m.m[2][2])); }

    /* Always a unit quaternion: the normalisation is part of the function. */
    len = q.f00 * q.f00 + q.f04 * q.f04 + q.f08 * q.f08 + q.f0C * q.f0C;
    CHECK_NEAR(len, 1.0, 1e-4);

    /* Recovered up to sign, scalar first. */
    if (q.f00 * w + q.f04 * x + q.f08 * y + q.f0C * z < 0.0f) {
        q.f00 = -q.f00; q.f04 = -q.f04; q.f08 = -q.f08; q.f0C = -q.f0C;
    }
    CHECK_NEAR(q.f00, w, 1e-4);
    CHECK_NEAR(q.f04, x, 1e-4);
    CHECK_NEAR(q.f08, y, 1e-4);
    CHECK_NEAR(q.f0C, z, 1e-4);
}

static void test_765E0(void)
{
    const float r1 = 0.316227766f;   /* sqrt(0.1) */
    const float r9 = 0.948683298f;   /* sqrt(0.9) */
    const float h  = 0.707106781f;
    BrMat4 m, mt;
    BrVec4 q, qt;
    int    i, j;

    /* Identity in, identity out. */
    RoundTrip(1.0f, 0.0f, 0.0f, 0.0f, 1);

    /* One quaternion per branch, chosen from the discriminants:
     *   case 1  m00 >= 0, m11 + m22 >= 0
     *   case 2  m00 >= 0, m11 + m22 <  0
     *   case 3  m00 <  0, m11 >= m22
     *   case 4  m00 <  0, m11 <  m22 */
    RoundTrip(h,  h,  0.0f, 0.0f, 1);
    RoundTrip(r1, r9, 0.0f, 0.0f, 2);
    RoundTrip(r1, 0.0f, r9, 0.0f, 3);
    RoundTrip(r1, 0.0f, 0.0f, r9, 4);

    /* A general rotation, still exact. */
    RoundTrip(0.5f, 0.5f, 0.5f, 0.5f, 1);

    /* The off-diagonal signs are the TRANSPOSE convention: feeding the
     * transposed matrix must conjugate the quaternion.  This holds for every
     * case, and it is what pins the sign choice. */
    QuatToMat(&m, 0.5f, 0.5f, 0.5f, 0.5f);
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            mt.m[i][j] = m.m[j][i];
        }
    }
    BrSub100765E0(&m,  &q);
    BrSub100765E0(&mt, &qt);
    CHECK_NEAR(qt.f00,  q.f00, 1e-4);
    CHECK_NEAR(qt.f04, -q.f04, 1e-4);
    CHECK_NEAR(qt.f08, -q.f08, 1e-4);
    CHECK_NEAR(qt.f0C, -q.f0C, 1e-4);

    /* Aliasing: pDst may not overlap pSrc in the original (it reads the
     * matrix all the way through), but the same pDst used twice must be
     * idempotent given the same input. */
    BrSub100765E0(&m, &q);
    BrSub100765E0(&m, &q);
    CHECK_NEAR(q.f00, 0.5, 1e-4);
}

int main(void)
{
    test_419D0();
    test_5FCF0();
    test_1E7C0();
    test_1C820();
    test_20D0();
    test_3289F();
    test_69490();
    test_4FC0();
    test_765E0();

    if (g_fail != 0) {
        printf("%d FAILURE(S)\n", g_fail);
        return 1;
    }
    printf("test_slice5_62: all checks passed\n");
    return 0;
}
