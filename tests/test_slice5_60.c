/* test_slice5_60.c -- behaviour tests for port/src/slice5_60.c.
 *
 * Everything asserted here is a property of the ORIGINAL, taken from the
 * disassembly, not from the port: the render-state dirty-bit invariant, the
 * two arms that leave `dirty` stale, the asymmetric cursor clamp, the
 * button-edge asymmetry, that the display-list walkers stop at G_ENDDL and
 * only there, and the substitution column arithmetic.
 *
 * Every cross-slice callee below is a STAND-IN and lives only in this file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "slice5_60.h"

static int g_fails;

static void check(int cond, const char *pszWhat)
{
    if (!cond) {
        printf("FAIL: %s\n", pszWhat);
        g_fails++;
    }
}

/* ====================================================================== */
/* Stand-ins for the cross-slice callees (test-only)                      */
/* ====================================================================== */

/* --- slice4_51 (0x10021560's state) ----------------------------------- */
static BrGbiRectState s_rect;
BrGbiRectState *BrGbiRectGetState(void) { return &s_rect; }

/* --- slice2_19 -------------------------------------------------------- */
int32_t g_Br6C661C;
int32_t g_Br6C6624;
int32_t g_Br6C666C;
BrSegMap *g_BrSegMap;
void  *(*g_BrModelDeref)(uint32_t slot);
void  (*g_BrModelFixup)(uint32_t *pSlot);

/* --- slice1_05 / br_seg ----------------------------------------------- */
static int s_nPtrAdd, s_nVtx, s_nTri1, s_nTri2, s_nSegFixup;
void BrPtrListAdd(BrPtrList *pList, void *pv) { (void)pList; (void)pv; s_nPtrAdd++; }
unsigned BrF3DVtxFixup(const BrSegMap *pMap, BrGfxWords *pCmd)
{
    (void)pMap; s_nVtx++;
    return (pCmd->w0 >> 10) & 0x3Fu;
}
void BrF3DTri1Fixup(void *pCmd) { (void)pCmd; s_nTri1++; }
void BrF3DTri2Fixup(void *pCmd) { (void)pCmd; s_nTri2++; }
void BrSegFixup(const BrSegMap *pMap, uint32_t *pPtr)
{
    (void)pMap; (void)pPtr; s_nSegFixup++;
}

/* --- slice3_39 (input state) ------------------------------------------ */
uint8_t g_BrDikState[BR_DIK_COUNT];
int32_t g_BrBtnRaw[BR_BTN_COUNT];
int32_t g_BrAA3398[7];
int32_t g_BrAA33B8;
int32_t g_BrAA33B4;
static int s_n5FFF0;
void BrMenuSub1005FFF0(void) { s_n5FFF0++; }

/* --- slice2_11 (CD) --------------------------------------------------- */
static int s_nCdPrev, s_nCdNext;
int BrCdTrackPrev(void) { s_nCdPrev++; return 1; }
int BrCdTrackNext(void) { s_nCdNext++; return 1; }

/* --- slice4_50 (clock) ------------------------------------------------ */
static int32_t s_now;
int32_t BrSub10075020(void) { return s_now; }

/* --- slice2_18 (the 0x10008B80 stub) ---------------------------------- */
void BrStub8B80_1p(const void *p0) { (void)p0; }

/* --- slice2_25 -------------------------------------------------------- */
int32_t g_br690A18;

/* --- 0x10071130's two forwards ---------------------------------------- */
static int32_t s_f610Mode = -1, s_f610Arg = -1, s_fE60Arg = -1;
int32_t BrSub10070610(int32_t mode, int32_t arg)
{
    s_f610Mode = mode; s_f610Arg = arg; return 7;
}
int32_t BrSub10070E60(int32_t arg) { s_fE60Arg = arg; return 9; }

/* ====================================================================== */
/* 0x100243D0                                                             */
/* ====================================================================== */

static void TestGbi243D0(void)
{
    BrGfxWords a[4];
    BrGfxWords *p = BrGbiCall100243D0(&a[1]);

    /* The original is `mov eax,[esp+4] / add eax,8`.  One command is eight
     * bytes, so this must be exactly +8 bytes, i.e. the next command. */
    check(p == &a[2], "0x100243D0 advances by exactly one 8-byte command");
    check((size_t)((unsigned char *)p - (unsigned char *)&a[1]) == 8u,
          "0x100243D0 advances by exactly 8 bytes");
}

/* ====================================================================== */
/* 0x10020FA0                                                             */
/* ====================================================================== */

/* Make the block self-consistent: every pending value equals its shadow, so
 * a correct call must leave bit i clear for every index it touches. */
static void RsQuiesce(void)
{
    int i;
    memset(&s_rect, 0, sizeof s_rect);
    for (i = 0; i < BR_GBI_RECT_RS_COUNT; i++) {
        s_rect.aPending[i] = 0x1234u + (uint32_t)i;
        s_rect.aShadow[i]  = 0x1234u + (uint32_t)i;
    }
    s_rect.dirty = 0;
    g_Br4C5184 = 0;
    g_Br0AA720 = 1;
    g_Br0A79D8 = 6u;
}

/* dirty bit i must agree with "pending differs from shadow" for every entry,
 * on every arm that actually stores `dirty`. */
static void RsCheckInvariant(const char *pszWhat)
{
    int i;
    for (i = 0; i < BR_GBI_RECT_RS_COUNT; i++) {
        int fWant = (s_rect.aPending[i] != s_rect.aShadow[i]);
        int fGot  = ((s_rect.dirty >> i) & 1u) != 0;
        if (fWant != fGot) {
            printf("FAIL: %s -- index %d pending %u shadow %u dirty bit %d\n",
                   pszWhat, i, (unsigned)s_rect.aPending[i],
                   (unsigned)s_rect.aShadow[i], fGot);
            g_fails++;
            return;
        }
    }
}

static void TestGbi20FA0(void)
{
    static const uint32_t aW1[] = {
        0x00504F50u, 4u, 0x0C184240u, 0x00504240u, 1u, 0u, 0x0D1849D8u,
        0x00011800u, 0x00001800u, 0x00000800u, 0x00001000u, 0x00000000u,
        0x12340678u, 0x12345678u, 0xFFFFFFFFu
    };
    size_t k;
    uint32_t aBefore[BR_GBI_RECT_RS_COUNT];

    for (k = 0; k < sizeof aW1 / sizeof aW1[0]; k++) {
        if (aW1[k] == 3u)
            continue;               /* the stale-dirty arm, checked below */
        RsQuiesce();
        BrGbiCall10020FA0(aW1[k]);
        RsCheckInvariant("0x10020FA0 dirty bit tracks pending vs shadow");
    }

    /* Indices 4, 8 and 9 (CULLMODE, TEXTUREMAG, TEXTUREMIN) are not written
     * on ANY path. */
    for (k = 0; k < sizeof aW1 / sizeof aW1[0]; k++) {
        RsQuiesce();
        memcpy(aBefore, s_rect.aPending, sizeof aBefore);
        BrGbiCall10020FA0(aW1[k]);
        check(s_rect.aPending[4] == aBefore[4] &&
              s_rect.aPending[8] == aBefore[8] &&
              s_rect.aPending[9] == aBefore[9],
              "0x10020FA0 never touches indices 4, 8 or 9");
    }

    /* The prologue sets aPending[0] = 1 before w1 is examined. */
    RsQuiesce();
    BrGbiCall10020FA0(3u);
    check(s_rect.aPending[0] == 1u, "w1==3 still runs the prologue write");
    check(s_rect.dirty == 0u, "w1==3 leaves `dirty` STALE (no store)");
    check(g_Br4C5184 == 0, "w1==3 clears 0x104C5184");

    /* The second stale-dirty exit: (w1 & 0x1800) && (w1 & 0x10000) with the
     * 0x100AA720 gate clear. */
    RsQuiesce();
    g_Br0AA720 = 0;
    s_rect.dirty = 0x5A5Au;
    BrGbiCall10020FA0(0x00011800u);
    check(s_rect.dirty == 0x5A5Au,
          "0x100AA720 == 0 returns before storing `dirty`");
    check(s_rect.aPending[0] == 1u,
          "...but the prologue write to aPending[0] has already happened");

    /* Same word with the gate set does store, and does reach index 6. */
    RsQuiesce();
    g_Br0AA720 = 1;
    BrGbiCall10020FA0(0x00011800u);
    check(s_rect.aPending[6] == 0x80u,
          "the 0x10000 arm gives index 6 the value 0x80");
    check(g_Br4BBE28 == 7u, "the 0x10000 arm mirrors 7 into 0x104BBE28");

    /* The alpha-blend pair every ordinary arm installs: SRCBLEND 5 /
     * DESTBLEND 6 at indices 2 and 3 (slice4_51.h's ordering). */
    RsQuiesce();
    BrGbiCall10020FA0(0x00504F50u);
    check(s_rect.aPending[2] == 5u && s_rect.aPending[3] == 6u,
          "0x00504F50 installs SRCBLEND 5 / DESTBLEND 6");
    check(s_rect.aPending[5] == 3u && g_Br4C16A0 == 3u,
          "0x00504F50's long tail writes index 5 and mirrors 0x104C16A0");

    /* The short tail (0x0C184240) leaves index 5 and 0x104C16A0 alone. */
    RsQuiesce();
    g_Br4C16A0 = 0xDEADu;
    BrGbiCall10020FA0(0x0C184240u);
    check(s_rect.aPending[5] == 0x1234u + 5u && g_Br4C16A0 == 0xDEADu,
          "0x0C184240's short tail skips index 5 / 0x104C16A0");

    /* 0x104C5184 skips indices 2 and 3 -- and only those. */
    RsQuiesce();
    g_Br4C5184 = 1;
    BrGbiCall10020FA0(0x00504F50u);
    check(s_rect.aPending[2] == 0x1234u + 2u &&
          s_rect.aPending[3] == 0x1234u + 3u,
          "0x104C5184 != 0 skips indices 2 and 3");
    check(s_rect.aPending[6] == 8u && s_rect.aPending[7] == 7u,
          "...but not indices 6 and 7");

    /* The `w1 == 4` arm is the ONLY one that takes index 3's value from
     * 0x100A79D8 instead of the literal 6. */
    RsQuiesce();
    g_Br0A79D8 = 0x99u;
    BrGbiCall10020FA0(4u);
    check(s_rect.aPending[3] == 0x99u, "w1==4 takes index 3 from 0x100A79D8");
    RsQuiesce();
    g_Br0A79D8 = 0x99u;
    BrGbiCall10020FA0(0x00504F50u);
    check(s_rect.aPending[3] == 6u, "w1==0x504F50 uses the literal 6");

    /* 0x0D1849D8 is the only arm that SETS 0x104C5184. */
    RsQuiesce();
    BrGbiCall10020FA0(0x0D1849D8u);
    check(g_Br4C5184 == 1, "0x0D1849D8 sets 0x104C5184");

    /* The bit tests are reached only after the exact-value tests fail:
     * 0x00504240 has bit 0x0800 set but takes the literal arm. */
    RsQuiesce();
    BrGbiCall10020FA0(0x00504240u);
    check(s_rect.aPending[6] == 8u,
          "0x00504240 takes the literal arm, not the 0x1800 bit arm");
    check(g_Br4BBE28 == 7u,
          "...proved by the 7 (not 8) it leaves in 0x104BBE28");

    /* The 0x1800-clear arm is the only one that puts 8 in 0x104BBE28.
     * (0x12345678 would NOT do -- it has bit 0x1000 set.) */
    RsQuiesce();
    BrGbiCall10020FA0(0x12340678u);      /* no 0x1800 bits */
    check(g_Br4BBE28 == 8u && s_rect.aPending[7] == 8u,
          "the (w1 & 0x1800)==0 arm uses 8, not 7, for index 7");
    check(s_rect.aPending[1] == 0u && s_rect.aPending[10] == 0u,
          "...and clears indices 1 and 10");
}

/* ====================================================================== */
/* 0x1002BF80 / BrDlRegister                                              */
/* ====================================================================== */

static void PutBe(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* The original's 32-bit slot IS the pointer.  On a 64-bit host it cannot be,
 * which is exactly why slice2_19.h routes these through g_BrModelDeref; the
 * stand-in below is the trivial one-entry resolver a real host would use. */
static uint32_t  s_derefKey;
static void     *s_derefVal;
static void *DerefOne(uint32_t slot)
{
    return (slot == s_derefKey) ? s_derefVal : NULL;
}

static void TestDlRegister(void)
{
    static uint8_t aDl[6 * 8];
    uint32_t aW[6 * 2];
    uint32_t w;
    int i;

    /* G_VTX, G_TRI1, G_SETTIMG, G_TRI2, G_ENDDL, then one command that must
     * never be looked at. */
    static const uint32_t aSrc[12] = {
        0x04001000u, 0x11223344u,
        0xBF000000u, 0x00010203u,
        0xFD100000u, 0x0A0B0C0Du,
        0xB1000000u, 0x04050607u,
        0xB8000000u, 0x00000000u,
        0x04000000u, 0xDEADBEEFu
    };

    for (i = 0; i < 12; i++)
        PutBe(aDl + 4 * i, aSrc[i]);

    s_nPtrAdd = s_nVtx = s_nTri1 = s_nTri2 = s_nSegFixup = 0;
    g_pBrDlPtrList = NULL;      /* the guarded-NULL path */
    BrDlRegister(aDl);

    memcpy(aW, aDl, sizeof aW);
    check(aW[0] == aSrc[0] && aW[1] == aSrc[1],
          "BrDlRegister byte-reverses both words of every command");
    check(aW[8] == aSrc[8] && aW[9] == aSrc[9],
          "G_ENDDL itself is byte-reversed before the walk stops");

    /* The command AFTER G_ENDDL must still be big-endian, i.e. untouched. */
    memcpy(&w, aDl + 40, sizeof w);
    check(w != aSrc[10], "the walk stops AT G_ENDDL, not after it");
    check(aDl[40] == 0x04 && aDl[41] == 0x00,
          "bytes past G_ENDDL are left big-endian");

    check(s_nVtx == 1, "one G_VTX fixup");
    check(s_nTri1 == 1, "one G_TRI1 fixup");
    check(s_nTri2 == 1, "one G_TRI2 fixup");
    check(s_nSegFixup == 1, "one G_SETTIMG segment fixup");
    check(s_nPtrAdd == 0, "a NULL pointer list is skipped, not crashed on");

    /* A NULL list is a no-op with no dereference. */
    BrDlRegister(NULL);

    /* The u32 entry point resolves through slice2_19's hook and then does
     * exactly the same thing. */
    for (i = 0; i < 12; i++)
        PutBe(aDl + 4 * i, aSrc[i]);
    s_nVtx = s_nTri1 = s_nTri2 = s_nSegFixup = 0;
    s_derefKey = 0xA5A50001u;
    s_derefVal = aDl;
    g_BrModelDeref = DerefOne;
    BrSub1002BF80(s_derefKey);
    check(s_nVtx == 1 && s_nTri1 == 1 && s_nTri2 == 1 && s_nSegFixup == 1,
          "BrSub1002BF80 == BrDlRegister(g_BrModelDeref(v))");

    /* A slot the resolver does not know resolves to NULL, which BrDlRegister
     * treats as "nothing to do" -- as the original does for a null list. */
    BrSub1002BF80(0xDEADBEEFu);

    /* An unwired hook must not crash. */
    g_BrModelDeref = NULL;
    BrSub1002BF80(s_derefKey);

    /* Opcode boundaries: the dispatch window is 0x04..0xFD inclusive, and
     * only 0x04/0xB1/0xB8/0xBF/0xFD do anything.  0x03 and 0xFE are below
     * and above the window; 0xFC is inside it but unhandled. */
    {
        static const uint32_t aBounds[8] = {
            0x03000000u, 0u,
            0xFE000000u, 0u,
            0xFC000000u, 0u,
            0xB8000000u, 0u
        };
        for (i = 0; i < 8; i++)
            PutBe(aDl + 4 * i, aBounds[i]);
        s_nVtx = s_nTri1 = s_nTri2 = s_nSegFixup = 0;
        BrDlRegister(aDl);
        check(s_nVtx + s_nTri1 + s_nTri2 + s_nSegFixup == 0,
              "0x03, 0xFE and 0xFC are all skipped");
        memcpy(&w, aDl + 0, sizeof w);
        check(w == 0x03000000u, "a skipped command is still byte-reversed");
    }
}

/* ====================================================================== */
/* 0x100341B3                                                             */
/* ====================================================================== */

/* Six 32-byte records: {matchW0, matchW1} then three replacement pairs. */
static uint32_t s_aTable[6 * 8];

static void BuildTable(void)
{
    int i, j;
    for (i = 0; i < 6; i++) {
        s_aTable[i * 8 + 0] = 0xB900031Du;
        s_aTable[i * 8 + 1] = 0x1000u + (uint32_t)i;
        for (j = 1; j < 4; j++) {
            s_aTable[i * 8 + j * 2 + 0] = 0xB9000000u + (uint32_t)(j * 0x10 + i);
            s_aTable[i * 8 + j * 2 + 1] = 0x2000u + (uint32_t)(j * 0x10 + i);
        }
    }
}

static void TestSub341B3(void)
{
    uint32_t aDl[8];
    int rc;

    BuildTable();

    /* sel == 1 (both gates clear) => column 1. */
    g_Br6C661C = 0; g_Br6C6624 = 0; g_Br6C6618 = 0; g_Br6C6620 = 0;
    g_Br6C666C = 1;
    aDl[0] = 0xB900031Du; aDl[1] = 0x1000u;      /* record 0 */
    aDl[2] = 0xB8000000u; aDl[3] = 0u;           /* G_ENDDL */
    rc = BrSub100341B3(aDl, s_aTable);
    check(aDl[0] == s_aTable[0 * 8 + 2] && aDl[1] == s_aTable[0 * 8 + 3],
          "sel==1 substitutes column 1");
    check(rc == 0, "record index < 3 does not set the return value");
    check(g_Br6C666C == 0, "0x106C666C is cleared on exit");

    /* sel == 0 (either gate set) => column 2. */
    g_Br6C661C = 1; g_Br6C6624 = 0;
    aDl[0] = 0xB900031Du; aDl[1] = 0x1000u;
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    rc = BrSub100341B3(aDl, s_aTable);
    check(aDl[0] == s_aTable[0 * 8 + 4] && aDl[1] == s_aTable[0 * 8 + 5],
          "sel==0 substitutes column 2");

    /* g_Br6C6618 shifts the column by one more. */
    g_Br6C661C = 0; g_Br6C6624 = 0; g_Br6C6618 = 1;
    aDl[0] = 0xB900031Du; aDl[1] = 0x1000u;
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[0] == s_aTable[0 * 8 + 4],
          "0x106C6618 == 1 shifts the column to 2");
    g_Br6C6618 = 0;

    /* Only record index >= 3 sets the return value. */
    aDl[0] = 0xB900031Du; aDl[1] = 0x1002u;      /* record 2 */
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    check(BrSub100341B3(aDl, s_aTable) == 0, "record 2 returns 0");
    aDl[0] = 0xB900031Du; aDl[1] = 0x1003u;      /* record 3 */
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    check(BrSub100341B3(aDl, s_aTable) == 1, "record 3 returns 1");
    aDl[0] = 0xB900031Du; aDl[1] = 0x1005u;      /* record 5 */
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    check(BrSub100341B3(aDl, s_aTable) == 1, "record 5 returns 1");

    /* A NULL list still clears 0x106C666C and returns 0. */
    g_Br6C666C = 1;
    check(BrSub100341B3(NULL, s_aTable) == 0, "NULL list returns 0");
    check(g_Br6C666C == 0, "NULL list still clears 0x106C666C");

    /* The walk stops at G_ENDDL and only there. */
    aDl[0] = 0xB8000000u; aDl[1] = 0u;
    aDl[2] = 0xB900031Du; aDl[3] = 0x1000u;
    aDl[4] = 0xB8000000u; aDl[5] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[2] == 0xB900031Du && aDl[3] == 0x1000u,
          "nothing past the first G_ENDDL is examined");

    /* The colour arms: g_Br6C6620 arms them, the flag comes from a
     * G_SETCOMBINE that matched one of the two 0x100AA8C8 records, and both
     * conditions are re-tested inside the arm. */
    g_Br6C661C = 0; g_Br6C6624 = 0; g_Br6C6620 = 1; g_Br6C666C = 1;
    aDl[0] = g_aBrAA8C8[1].w0; aDl[1] = g_aBrAA8C8[1].w1;   /* 0xFC.. */
    aDl[2] = 0xFA000000u;      aDl[3] = 0u;
    aDl[4] = 0xFB000000u;      aDl[5] = 0u;
    aDl[6] = 0xB8000000u;      aDl[7] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[3] == BR_DL_PRIMCOLOR_FORCED, "G_SETPRIMCOLOR w1 is forced");
    check(aDl[5] == BR_DL_ENVCOLOR_FORCED,  "G_SETENVCOLOR w1 is forced");

    /* Without a preceding matching G_SETCOMBINE the flag stays 0. */
    g_Br6C666C = 1;
    aDl[0] = 0xFA000000u; aDl[1] = 0x11111111u;
    aDl[2] = 0xB8000000u; aDl[3] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[1] == 0x11111111u, "the colour arm needs the G_SETCOMBINE flag");

    /* g_Br6C666C == 0 stops the flag ever being set -- and note the flag is
     * evaluated with the value 0x106C666C had ON ENTRY, since the clear only
     * happens at the exit. */
    g_Br6C666C = 0;
    aDl[0] = g_aBrAA8C8[0].w0; aDl[1] = g_aBrAA8C8[0].w1;
    aDl[2] = 0xFA000000u;      aDl[3] = 0x22222222u;
    aDl[4] = 0xB8000000u;      aDl[5] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[3] == 0x22222222u, "0x106C666C == 0 disarms the colour arms");

    /* The 0x100AA8B8 substitution needs sel != 0. */
    g_Br6C661C = 0; g_Br6C6624 = 0; g_Br6C6620 = 0;
    aDl[0] = g_aBrAA8B8[0].matchW0; aDl[1] = g_aBrAA8B8[0].matchW1;
    aDl[2] = 0xB8000000u;           aDl[3] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[0] == g_aBrAA8B8[0].newW0 && aDl[1] == g_aBrAA8B8[0].newW1,
          "sel != 0 applies the 0x100AA8B8 substitution");

    g_Br6C661C = 1;                             /* sel == 0 */
    aDl[0] = g_aBrAA8B8[0].matchW0; aDl[1] = g_aBrAA8B8[0].matchW1;
    aDl[2] = 0xB8000000u;           aDl[3] = 0u;
    (void)BrSub100341B3(aDl, s_aTable);
    check(aDl[0] == g_aBrAA8B8[0].matchW0,
          "sel == 0 skips the 0x100AA8B8 substitution");
    g_Br6C661C = 0;

    /* Opcode window: 0xB7 is below it, 0xFD is inside it but unhandled. */
    aDl[0] = 0xB7000000u; aDl[1] = 0x33333333u;
    aDl[2] = 0xFD000000u; aDl[3] = 0x44444444u;
    aDl[4] = 0xB8000000u; aDl[5] = 0u;
    check(BrSub100341B3(aDl, s_aTable) == 0, "0xB7 / 0xFD are skipped");
    check(aDl[1] == 0x33333333u && aDl[3] == 0x44444444u,
          "0xB7 / 0xFD leave their commands alone");
}

/* ====================================================================== */
/* 0x100603A0                                                             */
/* ====================================================================== */

static BrMouseSample s_devSample;
static int32_t       s_devLostTimes;
static int           s_nAcquire, s_nGetState;

static int32_t DevAcquire(BrDInputDev *pThis)
{
    (void)pThis; s_nAcquire++; return 0;
}

static int32_t DevGetState(BrDInputDev *pThis, uint32_t cb, void *pv)
{
    (void)pThis;
    s_nGetState++;
    check(cb == BR_MOUSE_SAMPLE_SIZE, "GetDeviceState is asked for 0x10 bytes");
    if (s_devLostTimes > 0) {
        s_devLostTimes--;
        return (int32_t)BR_DIERR_INPUTLOST;
    }
    memcpy(pv, &s_devSample, sizeof s_devSample);
    return 0;
}

static const BrDInputDevVtbl s_devVtbl = {
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    DevAcquire,
    { NULL },
    DevGetState
};
static BrDInputDev s_dev = { &s_devVtbl };

static void MouseReset(BrMouseState *pMs)
{
    memset(pMs, 0, sizeof *pMs);
    pMs->pDev = &s_dev;
    memset(&s_devSample, 0, sizeof s_devSample);
    memset(g_BrDikState, 0, sizeof g_BrDikState);
    memset(g_BrAA3398, 0, sizeof g_BrAA3398);
    memset(g_BrBtnRaw, 0, sizeof g_BrBtnRaw);
    g_BrAA33B8 = 640; g_BrAA33B4 = 480;
    g_BrAA2844 = 0; g_BrAA2BDC = 0; g_BrAA2BE0 = 0;
    g_BrAA2DAC = 0; g_BrAA2DB4 = 0;
    g_Br0AB3DC = 0; g_BrAA286C = 0; g_BrAA33E8 = 0;
    s_devLostTimes = 0; s_nAcquire = 0; s_nGetState = 0; s_n5FFF0 = 0;
    s_nCdPrev = s_nCdNext = 0;
    s_now = 1000;
}

static void TestSub603A0(void)
{
    BrMouseState ms;

    /* The gate. */
    MouseReset(&ms);
    g_BrAA2844 = 1;
    BrSub100603A0(&ms, NULL);
    check(s_nGetState == 0 && s_n5FFF0 == 0,
          "0x10AA2844 != 0 makes the whole function a no-op");

    /* pArg is genuinely never read: NULL is fine on the live path too. */
    MouseReset(&ms);
    s_devSample.dx = 10; s_devSample.dy = 20; s_devSample.dz = 30;
    BrSub100603A0(&ms, NULL);
    check(ms.x == 10 && ms.y == 20 && ms.z == 30,
          "the three axes accumulate");
    check(s_n5FFF0 == 1, "0x1005FFF0 runs once per poll");
    check(g_pBrAA2A78 == &ms, "`this` is published at 0x10AA2A78 first");

    /* Asymmetric clamp: below zero saturates to 0, but the upper bound is
     * itself reachable (`>= limit` stores `limit`). */
    MouseReset(&ms);
    s_devSample.dx = -5; s_devSample.dy = -5;
    BrSub100603A0(&ms, NULL);
    check(ms.x == 0 && ms.y == 0, "negative cursor clamps to 0");

    MouseReset(&ms);
    s_devSample.dx = 5000; s_devSample.dy = 5000; s_devSample.dz = 5000;
    BrSub100603A0(&ms, NULL);
    check(ms.x == g_BrAA33B8, "x clamps to 0x10AA33B8 inclusive");
    check(ms.y == g_BrAA33B4, "y clamps to 0x10AA33B4 inclusive");
    check(ms.z == 5000, "z is NOT clamped");

    MouseReset(&ms);
    ms.x = 639; s_devSample.dx = 1;
    BrSub100603A0(&ms, NULL);
    check(ms.x == 640, "the limit value itself is reachable");

    /* Buttons are masked to bit 7 only. */
    MouseReset(&ms);
    s_devSample.aBtn[0] = 0xFF; s_devSample.aBtn[1] = 0x7F;
    BrSub100603A0(&ms, NULL);
    check(ms.aBtn[0] == 0x80 && ms.aBtn[1] == 0x00,
          "only bit 7 of each button byte survives");
    check(ms.f4C == 1, "any button down sets f4C");

    /* Press edge: aDown set, aRelease deliberately NOT touched. */
    MouseReset(&ms);
    ms.aRelease[0] = 0x5A;
    s_devSample.aBtn[0] = 0x80;
    BrSub100603A0(&ms, NULL);
    check(ms.aDown[0] == 1, "press sets aDown");
    check(ms.aRelease[0] == 0x5A,
          "the press edge leaves aRelease alone (original's asymmetry)");

    /* Release edge: aRelease set, f4C cleared, g_BrBtnRaw raised. */
    s_devSample.aBtn[0] = 0x00;
    ms.f4C = 1;
    BrSub100603A0(&ms, NULL);
    check(ms.aDown[0] == 0 && ms.aRelease[0] == 1, "release sets aRelease");
    check(ms.f4C == 0, "release clears f4C");
    check(g_BrBtnRaw[0] == 1, "release raises 0x10AA33C0[i]");

    /* Held (no edge): aRelease is cleared again. */
    s_devSample.aBtn[0] = 0x80;
    BrSub100603A0(&ms, NULL);        /* press edge  */
    BrSub100603A0(&ms, NULL);        /* still held  */
    check(ms.aRelease[0] == 0, "a held button clears aRelease");

    /* g_BrBtnRaw is zeroed every poll before the edges are computed. */
    MouseReset(&ms);
    g_BrBtnRaw[2] = 99;
    BrSub100603A0(&ms, NULL);
    check(g_BrBtnRaw[2] == 0, "0x10AA33C0[] is zeroed every poll");

    /* DIERR_INPUTLOST retry: Acquire is called once up front, then twice per
     * retry, and GetDeviceState is retried until it stops failing. */
    MouseReset(&ms);
    s_devLostTimes = 2;
    BrSub100603A0(&ms, NULL);
    check(s_nGetState == 3, "GetDeviceState is retried while INPUTLOST");
    check(s_nAcquire == 1 + 2 * 2, "Acquire runs twice per retry");

    /* The dwell timer arms only ABOVE 0x78, never at it. */
    MouseReset(&ms);
    g_BrAA3398[BR_CURSOR_HELD_MS] = 0;
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - BR_CURSOR_ARM_MS;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA3398[BR_CURSOR_ARMED] == 0, "held == 0x78 does NOT arm");
    MouseReset(&ms);
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - (BR_CURSOR_ARM_MS + 1);
    BrSub100603A0(&ms, NULL);
    check(g_BrAA3398[BR_CURSOR_ARMED] == 1, "held == 0x79 arms");

    /* Armed + DIK_UP steps the 16-bit cursor down and resets the dwell. */
    MouseReset(&ms);
    g_BrAA286C = 5;
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - (BR_CURSOR_ARM_MS + 1);
    g_BrDikState[BR_DIK_UP] = 0x80;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA286C == 4, "DIK_UP decrements 0x10AA286C");
    check(g_Br0AB3DC == 0xFFFFu, "DIK_UP writes -1 into the 16-bit 0x100AB3DC");
    check(g_BrAA3398[BR_CURSOR_HELD_MS] == 0, "a step resets the dwell");

    MouseReset(&ms);
    g_BrAA286C = 0;
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - (BR_CURSOR_ARM_MS + 1);
    g_BrDikState[BR_DIK_DOWN] = 0x80;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA286C == 1, "DIK_DOWN increments 0x10AA286C");
    check(g_Br0AB3DC == 1u, "DIK_DOWN writes +1");

    /* 0x10AA286C wraps as a 16-bit value with no bound. */
    MouseReset(&ms);
    g_BrAA286C = 0;
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - (BR_CURSOR_ARM_MS + 1);
    g_BrDikState[BR_DIK_UP] = 0x80;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA286C == 0xFFFFu, "0x10AA286C is unbounded and 16-bit");

    /* The 0x10AA2DAC request forces aBtn[0] and disarms. */
    MouseReset(&ms);
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - (BR_CURSOR_ARM_MS + 1);
    ms.f4C = 1;
    g_BrAA2DAC = 1;
    BrSub100603A0(&ms, NULL);
    check(ms.aBtn[0] == 1, "0x10AA2DAC synthesises button 0");
    check(g_BrAA3398[BR_CURSOR_ARMED] == 0, "...and disarms the dwell");
    check(g_BrAA286C == 0, "...so the keyboard step does not also run");

    MouseReset(&ms);
    g_BrAA2DB4 = 1;
    BrSub100603A0(&ms, NULL);
    check(ms.aBtn[1] == 1, "0x10AA2DB4 synthesises button 1");
    check(g_Br0AB3DC == 1u, "0x10AA2DB4 writes +1 into 0x100AB3DC");

    /* The four latch/request slots are cleared on every poll. */
    MouseReset(&ms);
    g_BrAA3398[BR_CURSOR_UP_LATCH] = 1;
    g_BrAA3398[BR_CURSOR_DOWN_LATCH] = 1;
    g_BrAA3398[BR_CURSOR_UP_REQ] = 1;
    g_BrAA3398[BR_CURSOR_DOWN_REQ] = 1;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA3398[BR_CURSOR_UP_LATCH] == 0 &&
          g_BrAA3398[BR_CURSOR_DOWN_LATCH] == 0 &&
          g_BrAA3398[BR_CURSOR_UP_REQ] == 0 &&
          g_BrAA3398[BR_CURSOR_DOWN_REQ] == 0,
          "0x10AA3398..0x10AA33A4 are cleared every poll");

    /* CD stepping. */
    MouseReset(&ms);
    g_BrAA2BDC = 1; g_BrAA2BE0 = 1;
    BrSub100603A0(&ms, NULL);
    check(s_nCdPrev == 1 && s_nCdNext == 1,
          "0x10AA2BDC / 0x10AA2BE0 step the CD track");

    /* 0x10AA33E8 timestamps only actual changes. */
    MouseReset(&ms);
    s_devSample.dx = 3;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA33E8 == s_now, "a move timestamps 0x10AA33E8");
    g_BrAA33E8 = 0;
    s_devSample.dx = 0;
    BrSub100603A0(&ms, NULL);
    check(g_BrAA33E8 == 0, "an idle poll does not timestamp");

    /* A NULL device stops the poll but not the dwell bookkeeping. */
    MouseReset(&ms);
    ms.pDev = NULL;
    g_BrAA3398[BR_CURSOR_LAST_MS] = s_now - 200;
    BrSub100603A0(&ms, NULL);
    check(s_nGetState == 0, "a NULL device is not polled");
    check(g_BrAA3398[BR_CURSOR_ARMED] == 1,
          "...but the dwell timer has already run");
    check(s_n5FFF0 == 0, "...and 0x1005FFF0 does not run");
}

/* ====================================================================== */
/* 0x10071130                                                             */
/* ====================================================================== */

static unsigned char s_equip[0x200];
static int32_t       s_equipIndex = -1;

static void *EquipTarget(int32_t index)
{
    s_equipIndex = index;
    return s_equip;
}

static void WriteCfg(const unsigned char *pRec1, const unsigned char *pRec2)
{
    FILE *fp = fopen(BR_CFG_PATH, "wb");
    if (fp == NULL) {
        printf("FAIL: could not create %s\n", BR_CFG_PATH);
        g_fails++;
        return;
    }
    fwrite(pRec1, 1, BR_CFG_REC_SIZE, fp);
    if (pRec2 != NULL)
        fwrite(pRec2, 1, BR_CFG_REC_SIZE, fp);
    fclose(fp);
}

static void TestSub71130(void)
{
    unsigned char aRec1[BR_CFG_REC_SIZE];
    unsigned char aRec2[BR_CFG_REC_SIZE];
    uint32_t v;
    int i;

    /* The three pure forwards, with the mode passed through. */
    s_f610Mode = s_f610Arg = s_fE60Arg = -1;
    BrSub10071130(4, 77);
    check(s_f610Mode == 4 && s_f610Arg == 77, "mode 4 forwards to 0x10070610");
    s_f610Mode = s_f610Arg = -1;
    BrSub10071130(0, 88);
    check(s_f610Mode == 0 && s_f610Arg == 88, "mode 0 forwards to 0x10070610");
    BrSub10071130(1, 99);
    check(s_fE60Arg == 99, "mode 1 forwards to 0x10070E60");

    /* Modes 2 and 3 open BR_CFG_PATH.  With no such file nothing is
     * written and nothing crashes. */
    (void)remove(BR_CFG_PATH);
    memset(g_aBr1782E28, 0xEE, sizeof g_aBr1782E28);
    g_Br0ADF58 = g_Br0ADF5C = g_Br0ADF60 = 0;
    BrSub10071130(2, 0);
    check(g_aBr1782E28[0] == 0xEE && g_Br0ADF58 == 0,
          "a missing config file leaves everything alone");

    for (i = 0; i < BR_CFG_REC_SIZE; i++) {
        aRec1[i] = (unsigned char)i;
        aRec2[i] = (unsigned char)(0x80 + i);
    }

    /* Mode 3 reads 0x100 bytes in one go: one 0x80 record is NOT enough. */
    WriteCfg(aRec1, NULL);
    memset(g_aBr1782E28, 0xEE, sizeof g_aBr1782E28);
    BrSub10071130(3, 0);
    check(g_aBr1782E28[0] == 0x00,
          "mode 3 still copies what it managed to read");

    WriteCfg(aRec1, aRec2);
    memset(g_aBr1782E28, 0xEE, sizeof g_aBr1782E28);
    BrSub10071130(3, 0);
    check(g_aBr1782E28[0] == 0x00 && g_aBr1782E28[BR_CFG_REC_SIZE] == 0x80,
          "mode 3 reads 0x100 bytes");
    check(g_Br0ADF58 == 0, "mode 3 does NOT latch 0x100ADF58");

    /* Mode 2: latch three dwords from record 1, then push five from
     * record 2 into the equipment target. */
    g_BrCarEquipTarget = EquipTarget;
    g_br690A18 = 3;
    memset(s_equip, 0, sizeof s_equip);
    s_equipIndex = -1;
    g_Br0ADF58 = g_Br0ADF5C = g_Br0ADF60 = 0;
    BrSub10071130(2, 0);

    memcpy(&v, aRec1 + 0, sizeof v);
    check(g_Br0ADF58 == v, "mode 2 latches record 1 dword 0");
    memcpy(&v, aRec1 + 4, sizeof v);
    check(g_Br0ADF5C == v, "mode 2 latches record 1 dword 1");
    memcpy(&v, aRec1 + 8, sizeof v);
    check(g_Br0ADF60 == v, "mode 2 latches record 1 dword 2");
    check(s_equipIndex == 3, "the equipment target is chosen by 0x10690A18");

    for (i = 0; i < BR_CAR_EQUIP_COUNT; i++) {
        uint32_t want, got;
        memcpy(&want, aRec2 + 4 * i, sizeof want);
        memcpy(&got, s_equip + BR_CAR_EQUIP_OFF + 4 * i, sizeof got);
        check(want == got, "mode 2 pushes five dwords to target + 0xF8");
    }

    /* One record only: the SECOND read fails, so nothing is pushed -- but
     * the first three dwords have already been latched. */
    WriteCfg(aRec1, NULL);
    memset(s_equip, 0, sizeof s_equip);
    g_Br0ADF58 = 0;
    BrSub10071130(2, 0);
    memcpy(&v, aRec1 + 0, sizeof v);
    check(g_Br0ADF58 == v, "the latch happens before the second read");
    memcpy(&v, s_equip + BR_CAR_EQUIP_OFF, sizeof v);
    check(v == 0, "a short second record pushes nothing");

    /* An unwired equipment hook must not crash. */
    WriteCfg(aRec1, aRec2);
    g_BrCarEquipTarget = NULL;
    BrSub10071130(2, 0);

    (void)remove(BR_CFG_PATH);
}

/* ====================================================================== */

int main(void)
{
    char szDir[] = "/tmp/brd3d60XXXXXX";

    TestGbi243D0();
    TestGbi20FA0();
    TestDlRegister();
    TestSub341B3();
    TestSub603A0();

    /* 0x10071130 opens an absolute, hard-coded path ("c:\RallyConfig.dat",
     * which on a POSIX host is one relative filename).  Run it somewhere
     * disposable so the test never writes into the tree. */
    if (mkdtemp(szDir) != NULL && chdir(szDir) == 0) {
        TestSub71130();
        if (chdir("/") == 0)
            (void)rmdir(szDir);
    } else {
        printf("FAIL: could not make a scratch directory\n");
        g_fails++;
    }

    if (g_fails == 0) {
        printf("slice5_60: all checks passed\n");
        return 0;
    }
    printf("slice5_60: %d check(s) FAILED\n", g_fails);
    return 1;
}
