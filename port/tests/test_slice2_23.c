/* test_slice2_23.c -- behaviour tests for slice2_23.c.
 *
 * These pin properties of the ORIGINAL: its sentinels, its asymmetric
 * defaults, its dead guards, the order in which it dispatches, and two
 * outright bugs it contains. Where the original does something surprising
 * the test asserts the surprise, so that "tidying" it fails here.
 */

#include "slice2_23.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_fails++;                                                       \
        }                                                                    \
    } while (0)

/* ==========================================================================
 * Cross-slice stand-ins.  NOT part of the port -- integration drops
 * these when the real 0x10074030 / 0x1003E070 / 0x1005FFD0 / 0x1003D210 /
 * 0x10069BC0 / 0x10069C30 land.
 * ========================================================================== */

static const char *g_pszStr    = "<str>";
static int32_t     g_lastStrId = -1;
static int32_t     g_ret5FFD0  = -1;
static int         g_n3E070    = 0;
static int         g_n3D210    = 0;

const char *BrStrGet(int32_t id)
{
    static char sz[32];
    g_lastStrId = id;
    sprintf(sz, "S%d", (int)id);
    g_pszStr = sz;
    return sz;
}

void BrFn1003E070(void)     { g_n3E070++; }
int32_t BrFn1005FFD0(void)  { return g_ret5FFD0; }

void BrFn1003D210(int32_t a, int32_t b, int32_t c)
{
    (void)a; (void)b; (void)c;
    g_n3D210++;
}

/* Binding tables the 0x10040330 stand-ins answer from, keyed by the record
 * key. `kind` is echoed into the answer so a wrong kind cannot pass. */
static int32_t g_bindA[64];
static uint8_t g_bindB[64];
static int32_t g_bindKind = 0;

int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key)
{
    (void)pThis;
    g_bindKind = kind;
    return (key < 64u) ? g_bindA[key] : 0;
}

uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key)
{
    (void)pThis;
    g_bindKind = kind;
    return (key < 64u) ? g_bindB[key] : (uint8_t)0;
}

/* br_state.c already defines this (0x1003E080).  Stand-in so the standalone
 * test binary links; DELETE when br_state.c joins the link line. */
int BrIsAnyActive(const BrActiveFlags *pFlags)
{
    if (pFlags->a0 || pFlags->a1 || pFlags->a2 || pFlags->a3) {
        return 1;
    }
    if (pFlags->override) {
        return 0;
    }
    return (pFlags->a5 || pFlags->a6 || pFlags->a7 || pFlags->a8) ? 1 : 0;
}

/* ==========================================================================
 * A fake menu object
 * ========================================================================== */

static BrUiObj g_obj[BR_UI_OBJ_MIN_SIZE];

static int g_n04, g_n08, g_n10, g_n2C, g_n20, g_n24, g_nApplyCb;
static int g_f14ret;
static int g_seq[64];
static int g_nSeq;
static int g_selOffer;         /* what f20 returns */
static int g_selLastArg;
static int g_selCommitArg;

/* Object-vtable slot 0x14 message log. */
typedef struct MsgRec { int32_t msg, a, b; } MsgRec;
static MsgRec g_aMsg[64];
static int    g_nMsg;

static void note(int tag) { if (g_nSeq < 64) { g_seq[g_nSeq++] = tag; } }

static void it_f04(BrUiObj *p) { (void)p; g_n04++; note(4);  }
static void it_f08(BrUiObj *p) { (void)p; g_n08++; note(8);  }
static void it_f10(BrUiObj *p) { (void)p; g_n10++; note(16); }
static void it_f2C(BrUiObj *p) { (void)p; g_n2C++; note(44); }
static int32_t it_f14(BrUiObj *p) { (void)p; note(20); return g_f14ret; }

static int32_t sel_f20(BrUiObj *p, int32_t v)
{
    (void)p; g_n20++; g_selLastArg = v; return g_selOffer;
}
static void sel_f24(BrUiObj *p, int32_t v)
{
    (void)p; g_n24++; g_selCommitArg = v;
}

static const BrUiWidgetVtbl g_itemVtbl = {
    NULL, it_f04, it_f08, NULL, it_f10, it_f14,
    NULL, NULL, sel_f20, sel_f24, NULL, it_f2C
};

static void obj_f14(BrUiObj *p, int32_t msg, int32_t a, int32_t b)
{
    (void)p;
    if (g_nMsg < 64) {
        g_aMsg[g_nMsg].msg = msg;
        g_aMsg[g_nMsg].a   = a;
        g_aMsg[g_nMsg].b   = b;
        g_nMsg++;
    }
}

static const BrUiObjVtbl g_objVtbl = { NULL, NULL, NULL, NULL, NULL, obj_f14 };

static void on_apply(BrUiObj *p) { (void)p; g_nApplyCb++; }

static void reset_counters(void)
{
    g_n04 = g_n08 = g_n10 = g_n2C = g_n20 = g_n24 = g_nApplyCb = 0;
    g_nSeq = 0;
    g_nMsg = 0;
    g_n3E070 = 0;
    g_n3D210 = 0;
    g_lastStrId = -1;
}

static BrUiObj *fresh_obj(void)
{
    memset(g_obj, 0, sizeof(g_obj));
    BrUiStPtr(g_obj, BR_UI_OFF_VTBL, (void *)(size_t)&g_objVtbl);
    BrUiStPtr(g_obj, BR_UI_OFF_SEL,  (void *)(size_t)&g_itemVtbl);
    BrUiStPtr(BrUiItem(g_obj, 0), BR_UI_ITEM_OFF_VTBL,
              (void *)(size_t)&g_itemVtbl);
    BrUiStPtr(g_obj, BR_UI_OFF_ONAPPLY, (void *)(size_t)on_apply);
    reset_counters();
    return g_obj;
}

/* A globals block with every table pointed somewhere legal. */
static int32_t  g_tab[64];
static uint8_t  g_tabB[256];
static int16_t  g_tab16[64];
static int8_t   g_tab8[64];
static uint32_t g_ab334[2 * BR_UI_AB334_COUNT];
static int32_t  g_conflict[BR_UI_AB334_COUNT];
static void    *g_bd2a8[16];
static unsigned char g_ent[8];
static char     g_bufs[10][256];
static BrUiObj  g_flagObjs[3][64];

static BrUiGlobals g_G;

static void fresh_globals(void)
{
    int i;

    memset(&g_G, 0, sizeof(g_G));
    memset(g_tab, 0, sizeof(g_tab));
    memset(g_tabB, 0, sizeof(g_tabB));
    memset(g_tab16, 0, sizeof(g_tab16));
    memset(g_tab8, 0, sizeof(g_tab8));
    memset(g_conflict, 0, sizeof(g_conflict));
    memset(g_bufs, 0, sizeof(g_bufs));
    memset(g_flagObjs, 0, sizeof(g_flagObjs));
    memset(g_ent, 0, sizeof(g_ent));

    for (i = 0; i < 64; i++) {
        g_tab[i]   = 100 + i;
        g_tab16[i] = (int16_t)(200 + i);
        g_tab8[i]  = (int8_t)(i - 5);
    }
    for (i = 0; i < BR_UI_AB334_COUNT; i++) {
        g_ab334[2 * i]     = (uint32_t)i;   /* key == index */
        g_ab334[2 * i + 1] = 0u;
    }
    for (i = 0; i < 16; i++) {
        g_bd2a8[i] = g_ent;
    }

    g_G.tAC308 = g_tab;  g_G.tAC348 = g_tab;  g_G.tAC358 = g_tab;
    g_G.tAC368 = g_tab;  g_G.tAC3A8 = g_tab;  g_G.tAC3B0 = g_tab;
    g_G.tAC3E0 = g_tab;  g_G.tAC3F0 = g_tab;  g_G.tAC400 = g_tab;
    g_G.tAC408 = g_tab;  g_G.tAC410 = g_tab;  g_G.tAC418 = g_tab;
    g_G.tAC5A8 = g_tab;
    g_G.tB3820 = g_tabB;
    g_G.tAA26E8 = g_tab8;
    g_G.tA9D068 = g_tab16;
    g_G.tBD2A8 = (void * const *)g_bd2a8;
    g_G.aAB334 = g_ab334;
    g_G.aA9D5C0 = g_conflict;
    g_G.pAA29A8 = g_flagObjs[0];
    g_G.pAA29BC = g_flagObjs[1];
    g_G.pAA29E8 = g_flagObjs[2];
    g_G.szB4E2E8 = g_bufs[0];
    g_G.szA9CDF0 = g_bufs[1];
    g_G.szB4E1E4 = g_bufs[2];
    g_G.szB4E740 = g_bufs[3];
    g_G.szB4E760 = g_bufs[4];
    g_G.szA9DD28 = g_bufs[5];
    g_G.szA9D018 = g_bufs[6];
    g_G.sz39B720 = g_bufs[7];
    g_G.sz0AD300 = g_bufs[8];
}

/* ==========================================================================
 * 0x1003DC10
 * ========================================================================== */

static void test_dperr(void)
{
    CHECK(strcmp(BrDPlayErrName(0), "DP_OK") == 0);
    CHECK(strcmp(BrDPlayErrName((int32_t)0x8877001Eu),
                 "DPERR_BUFFERTOOSMALL") == 0);
    /* The boundary entries of every level of the original's search tree. */
    CHECK(strcmp(BrDPlayErrName((int32_t)0x8000000Au), "DPERR_PENDING") == 0);
    CHECK(strcmp(BrDPlayErrName((int32_t)0x80004001u),
                 "DPERR_UNSUPPORTED") == 0);
    CHECK(strcmp(BrDPlayErrName((int32_t)0x88770820u),
                 "DPERR_LOGONDENIED") == 0);
    CHECK(strcmp(BrDPlayErrName((int32_t)0x887707F8u),
                 "DPERR_CANTLOADSECURITYPACKAGE") == 0);

    /* Unknown -> "0x%08X": uppercase hex, zero padded to eight digits. */
    CHECK(strcmp(BrDPlayErrName((int32_t)0x88770999u), "0x88770999") == 0);
    CHECK(strcmp(BrDPlayErrName(1), "0x00000001") == 0);
    CHECK(strcmp(BrDPlayErrName((int32_t)0xDEADBEEFu), "0xDEADBEEF") == 0);

    /* One shared buffer: the previous unknown-code answer is destroyed by
     * the next call. That is the original's contract, not an accident. */
    {
        const char *p = BrDPlayErrName(2);
        CHECK(strcmp(p, "0x00000002") == 0);
        (void)BrDPlayErrName(3);
        CHECK(strcmp(p, "0x00000003") == 0);
    }
    /* Named entries are static and therefore stable across calls. */
    {
        const char *p = BrDPlayErrName((int32_t)0x887700F0u);
        (void)BrDPlayErrName(0x12345);
        CHECK(strcmp(p, "DPERR_TIMEOUT") == 0);
    }
}

/* ==========================================================================
 * 0x1003DFC0 / 0x1003E010 / 0x1003E040 / 0x1003E0E0
 * ========================================================================== */

static void test_small(void)
{
    BrStartupState st;
    int            marker = 0;
    BrActiveFlags  flags;

    memset(&st, 0xAA, sizeof(st));
    BrUiFn1003DFC0(&st, &marker);
    CHECK(st.g0B380C == 0 && st.g22B350 == 0 && st.g22B34C == 0);
    CHECK(st.g094350 == 1 && st.g094354 == 1 && st.g094358 == 1);
    CHECK(st.g09435C == 2);
    CHECK(st.gB4E1D0 == 0);
    CHECK(st.gB4E1D4 == &marker);

    fresh_globals();
    BrUiFn1003E010(&g_G);
    CHECK(g_G.gAA27E0 == (int16_t)0x0102);
    CHECK(g_G.gAA2598 == 0x102);
    CHECK(g_G.gAA27E2 == 0);        /* the sibling word is untouched */
    BrUiFn1003E040(&g_G);
    CHECK(g_G.gAA27E2 == (int16_t)0x0037);
    CHECK(g_G.gA9D010 == 0x37);
    CHECK(g_G.gAA27E0 == (int16_t)0x0102);

    memset(&flags, 0, sizeof(flags));

    /* `jge`: a return of exactly 0 from 0x1005FFD0 already answers yes. */
    g_ret5FFD0 = 0;
    CHECK(BrUiFn1003E0E0(&flags) == 1);
    g_ret5FFD0 = 5;
    CHECK(BrUiFn1003E0E0(&flags) == 1);

    g_ret5FFD0 = -1;
    CHECK(BrUiFn1003E0E0(&flags) == 0);
    flags.a5 = 1;
    CHECK(BrUiFn1003E0E0(&flags) == 1);
    /* br_state.h's documented inversion still governs the second test. */
    flags.override = 1;
    CHECK(BrUiFn1003E0E0(&flags) == 0);
    g_ret5FFD0 = 0;
    CHECK(BrUiFn1003E0E0(&flags) == 1);   /* first test short-circuits it */
    g_ret5FFD0 = -1;
}

/* ==========================================================================
 * 0x1003EE50
 * ========================================================================== */

static void test_apply(void)
{
    BrUiObj *pObj;

    /* I420 == 0: f04, then f10, return 0, nothing else runs. */
    fresh_globals();
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 0u);
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 0);
    CHECK(g_n04 == 1 && g_n10 == 1);
    CHECK(g_nSeq == 2 && g_seq[0] == 4 && g_seq[1] == 16);
    CHECK(g_n3E070 == 0 && g_nApplyCb == 0);

    /* I420 != 0, f14 positive, flag bit 1 clear: the confirm block is
     * skipped and I420 survives. */
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 1;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 0 && g_nApplyCb == 0);
    CHECK(BrUiLd32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420) == 7u);
    CHECK(g_nSeq == 3 && g_seq[0] == 4 && g_seq[1] == 20 && g_seq[2] == 16);

    /* Same, but flag bit 1 SET -> the confirm block runs anyway. */
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    BrUiSt32(pObj, BR_UI_OFF_FLAGS, 0xFF00FFFFu);   /* bit 1 set */
    g_f14ret = 1;
    g_G.gAA28D8 = 9;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 1 && g_nApplyCb == 1);
    CHECK(g_G.gAA28D8 == 0);
    CHECK(BrUiLd32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420) == 0u);
    /* Only bit 1 is cleared; the other 31 bits survive the byte-wide mask. */
    CHECK(BrUiLd32(pObj, BR_UI_OFF_FLAGS) == (0xFF00FFFFu & ~2u));

    /* f14's result is read as a SIGNED BYTE: 0x100 has a zero low byte and
     * therefore counts as "not positive". */
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 0x100;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 1);

    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 0xFF;                       /* -1 as a signed byte */
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 1);

    /* ...but 0x101 has a positive low byte and is "yes". */
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 0x101;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 0);

    /* gAA285C non-zero suppresses the three clears but NOT the callbacks. */
    pObj = fresh_obj();
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 0;
    g_G.gAA285C = 1;
    g_G.gAA28D8 = 9;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_G.gAA28D8 == 9);
    CHECK(BrUiLd32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420) == 7u);
    CHECK(g_n3E070 == 1 && g_nApplyCb == 1);
    g_G.gAA285C = 0;

    /* A null +0x10 callback is skipped without disturbing the rest. */
    pObj = fresh_obj();
    BrUiStPtr(pObj, BR_UI_OFF_ONAPPLY, NULL);
    BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_I420, 7u);
    g_f14ret = 0;
    CHECK(BrUiItemApply(pObj, 0, &g_G) == 1);
    CHECK(g_n3E070 == 1 && g_nApplyCb == 0 && g_n10 == 1);
}

/* ==========================================================================
 * Geometry and code callbacks
 * ========================================================================== */

static void test_scalar_callbacks(void)
{
    BrUiObj *pObj;
    int32_t  n;

    fresh_globals();
    pObj = fresh_obj();

    for (n = -3; n < 5; n++) {
        g_G.g0AC65C = n;
        CHECK(BrUiFn1003E920(pObj, &g_G) == 1);
        CHECK(BrUiLdF(pObj, BR_UI_OFF_F3C) == (float)(11 * n + 0x3D));
    }

    /* 0x1003EA40 picks its count with g0AB3D8 and the two are NOT
     * interchangeable. */
    g_G.gB4E708 = 3u;
    g_G.gB4E70C = 10u;
    g_G.g0AB3D8 = 1;
    CHECK(BrUiFn1003EA40(pObj, &g_G) == 1);
    CHECK(BrUiLdF(pObj, BR_UI_OFF_F3C) == (float)(8 * 3 + 0x4A));
    g_G.g0AB3D8 = 0;
    CHECK(BrUiFn1003EA40(pObj, &g_G) == 1);
    CHECK(BrUiLdF(pObj, BR_UI_OFF_F3C) == (float)(8 * 10 + 0x4A));

    /* 0x1003E950: SET -> 0x68, CLEAR -> 0x69, and both destinations move. */
    g_G.g0AB3D8 = 1;
    CHECK(BrUiCode1003E950(pObj, &g_G) == 1);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W2A40) == 0x68);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x68);
    g_G.g0AB3D8 = 0;
    CHECK(BrUiCode1003E950(pObj, &g_G) == 1);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W2A40) == 0x69);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x69);
}

static void test_code_callbacks(void)
{
    BrUiObj *pObj;
    int32_t  i;

    fresh_globals();
    pObj = fresh_obj();

    /* --- 0x1003F440 --- */
    {
        static const int16_t aExp[6] = { 0x73, 0x72, 0x71, 0x70, 0x6F, -1 };
        for (i = 1; i <= 6; i++) {
            g_G.gAA26F0 = i;
            BrUiSt16(pObj, BR_UI_OFF_W1E20C, 0x1234);
            CHECK(BrUiCode1003F440(pObj, &g_G) == 1);
            CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == aExp[i - 1]);
        }
        /* gAA26F0 < 0: NEITHER stage runs, the field keeps its old value. */
        g_G.gAA26F0 = -1;
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, 0x1234);
        CHECK(BrUiCode1003F440(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x1234);
    }
    {
        static const int16_t aExp[7] = { 0x47, 0x49, 0x4B, 0x4D, 0x4D,
                                         0x4D, -1 };
        g_G.gAA26F0 = 0;
        for (i = 1; i <= 7; i++) {
            g_G.gAA26F4 = i;
            CHECK(BrUiCode1003F440(pObj, &g_G) == 1);
            CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == aExp[i - 1]);
        }
        /* Only the low byte of gAA26F4 matters. */
        g_G.gAA26F4 = 0x7F00 | 1;
        CHECK(BrUiCode1003F440(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x47);
    }

    /* --- 0x1003F540: the first stage has no case for 1 --- */
    {
        static const int16_t aExp[5] = { -1, 0x6D, 0x6E, 0x6C, -1 };
        for (i = 1; i <= 5; i++) {
            g_G.gAA26F0 = i;
            CHECK(BrUiCode1003F540(pObj, &g_G) == 1);
            CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == aExp[i - 1]);
        }
        g_G.gAA26F0 = 0;
        {
            static const int16_t aExp2[5] = { 0x48, 0x4A, 0x4C, -1, -1 };
            for (i = 1; i <= 5; i++) {
                g_G.gAA26F4 = i;
                CHECK(BrUiCode1003F540(pObj, &g_G) == 1);
                CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == aExp2[i - 1]);
            }
        }
        g_G.gAA26F0 = -5;
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, 0x0777);
        CHECK(BrUiCode1003F540(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x0777);
    }

    /* --- 0x1003F5E0 / 0x1003F680 disagree about their defaults --- */
    {
        static const int16_t a5E0[5] = { 0x56, 0x57, 0x59, 0x5B, 0x5D };
        static const int16_t a680[5] = { -1, 0x58, 0x5A, 0x5C, 0x5E };
        for (i = 0; i < 5; i++) {
            g_G.gAA2A18 = (uint32_t)i;
            CHECK(BrUiCode1003F5E0(pObj, &g_G) == 1);
            CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == a5E0[i]);
            CHECK(BrUiCode1003F680(pObj, &g_G) == 1);
            CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == a680[i]);
        }
        g_G.gAA2A18 = 5u;
        CHECK(BrUiCode1003F5E0(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x56);   /* == index 0 */
        CHECK(BrUiCode1003F680(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == -1);
        /* Compared unsigned: 0xFFFFFFFF is out of range, not "-1 < 4". */
        g_G.gAA2A18 = 0xFFFFFFFFu;
        CHECK(BrUiCode1003F5E0(pObj, &g_G) == 1);
        CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x56);
    }

    /* --- 0x1003F720: the one callback that can fail --- */
    g_G.gAA2904 = 4; g_G.gAA2964 = 4; g_G.gAA28E8 = 0;
    BrUiSt16(pObj, BR_UI_OFF_W1E20C, 0x0BAD);
    CHECK(BrUiCode1003F720(pObj, &g_G) == -2);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x0BAD);   /* nothing written */

    g_G.gAA28E8 = 1;
    g_G.g0AC654 = 3;
    g_tab[3] = (int32_t)0x11110042;      /* only the low word is read */
    CHECK(BrUiCode1003F720(pObj, &g_G) == 1);
    CHECK(BrUiLd16(pObj, BR_UI_OFF_W1E20C) == 0x0042);
    g_G.gAA2964 = 5;                     /* differing pair also proceeds */
    g_G.gAA28E8 = 0;
    CHECK(BrUiCode1003F720(pObj, &g_G) == 1);
}

/* ==========================================================================
 * Poll family
 * ========================================================================== */

static void test_poll(void)
{
    BrUiObj *pObj;

    fresh_globals();
    pObj = fresh_obj();

    g_G.g0AB3F4 = 4;
    g_selOffer  = 9;
    CHECK(BrUiPoll1003EAE0(pObj, &g_G) == 1);
    CHECK(g_selLastArg == 4 && g_G.g0AB3F4 == 9);

    /* A negative answer means "no change". */
    g_selOffer = -1;
    CHECK(BrUiPoll1003EAE0(pObj, &g_G) == 1);
    CHECK(g_G.g0AB3F4 == 9);

    /* 0 is accepted -- the guard is `jl`, not `jle`. */
    g_selOffer = 0;
    CHECK(BrUiPoll1003EAE0(pObj, &g_G) == 1);
    CHECK(g_G.g0AB3F4 == 0);

    /* 0x1003EBC0 discards the answer entirely. */
    g_G.gAA2880 = 2;
    g_selOffer  = 7;
    CHECK(BrUiPoll1003EBC0(pObj, &g_G) == 1);
    CHECK(g_selLastArg == 2 && g_G.gAA2880 == 2);
    /* ...where its near-twin 0x1003EB90 stores. */
    CHECK(BrUiPoll1003EB90(pObj, &g_G) == 1);
    CHECK(g_G.gAA2880 == 7);

    /* 0x1003EB10 == 0x1003EC30, and f24 only fires when gAA28D8 is set. */
    g_G.g0AB3F4 = 1;
    g_selOffer  = 6;
    g_G.gAA28D8 = 0;
    g_n24 = 0;
    CHECK(BrUiPoll1003EB10(pObj, &g_G) == 1);
    CHECK(g_G.g0AB3F4 == 6 && g_n24 == 0);
    g_G.gAA28D8 = 1;
    CHECK(BrUiPoll1003EC30(pObj, &g_G) == 1);
    CHECK(g_n24 == 1 && g_selCommitArg == 6);
    /* Rejected offer: the commit still runs, with the OLD value. */
    g_selOffer = -3;
    g_n24 = 0;
    CHECK(BrUiPoll1003EB10(pObj, &g_G) == 1);
    CHECK(g_G.g0AB3F4 == 6 && g_n24 == 1 && g_selCommitArg == 6);

    /* 0x1003EBE0 reads the +0x3C98 table with the item stride. */
    g_G.gAA2880 = 0;
    g_selOffer  = 2;
    BrUiStPtr(pObj, BR_UI_OFF_TBL3C98 + 2u * BR_UI_ITEM_STRIDE, (void *)&g_G);
    CHECK(BrUiPoll1003EBE0(pObj, &g_G) == 1);
    CHECK(g_G.gAA2880 == 2);
    CHECK(g_G.g0AB3E0 == (void *)&g_G);

    /* 0x1003EE20 clamps only what it SENDS. */
    g_selOffer = -1;
    g_G.gAA2A34 = 5;
    CHECK(BrUiPoll1003EE20(pObj, &g_G) == 1);
    CHECK(g_selLastArg == 5 && g_G.gAA2A34 == 5);
    g_G.gAA2A34 = 12;
    CHECK(BrUiPoll1003EE20(pObj, &g_G) == 1);
    CHECK(g_selLastArg == -1 && g_G.gAA2A34 == 12);
    g_G.gAA2A34 = -4;
    CHECK(BrUiPoll1003EE20(pObj, &g_G) == 1);
    CHECK(g_selLastArg == -1 && g_G.gAA2A34 == -4);
    g_G.gAA2A34 = 11;
    CHECK(BrUiPoll1003EE20(pObj, &g_G) == 1);
    CHECK(g_selLastArg == 11);
    /* An accepted answer is stored WITHOUT being re-clamped. */
    g_selOffer  = 99;
    g_G.gAA2A34 = 3;
    CHECK(BrUiPoll1003EE20(pObj, &g_G) == 1);
    CHECK(g_G.gAA2A34 == 99);
}

/* ==========================================================================
 * Draw callbacks
 * ========================================================================== */

static void test_draw(void)
{
    BrUiObj *pObj;

    fresh_globals();
    pObj = fresh_obj();

    /* __ftol truncates toward zero, so 10.9 -> 10 and -3.9 -> -3. */
    BrUiStF(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F410, 10.9f);
    BrUiStF(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F414, 20.9f);
    BrUiSt16(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_W40A, 32);
    CHECK(BrUiDraw1003E7A0(pObj) == 1);
    /* n == 32/16 + 1 == 3 */
    CHECK(g_nMsg == 5);
    CHECK(g_aMsg[0].msg == 0x3D && g_aMsg[0].a == 7 - 8 && g_aMsg[0].b == 8);
    CHECK(g_aMsg[1].msg == 0x3B && g_aMsg[1].a == 7);
    CHECK(g_aMsg[2].msg == 0x3B && g_aMsg[2].a == 7 + 16);
    CHECK(g_aMsg[3].msg == 0x3B && g_aMsg[3].a == 7 + 32);
    CHECK(g_aMsg[4].msg == 0x3C && g_aMsg[4].a == 7 + 48);

    /* The division truncates toward zero: -17/16 == -1, so n == 0 and the
     * middle run vanishes while the tail still lands on x0. */
    reset_counters();
    BrUiSt16(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_W40A, -17);
    CHECK(BrUiDraw1003E7A0(pObj) == 1);
    CHECK(g_nMsg == 2);
    CHECK(g_aMsg[0].msg == 0x3D && g_aMsg[1].msg == 0x3C);
    CHECK(g_aMsg[1].a == 7);

    /* W40A in [0,15] still yields exactly one 0x3B. */
    reset_counters();
    BrUiSt16(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_W40A, 15);
    CHECK(BrUiDraw1003E7A0(pObj) == 1);
    CHECK(g_nMsg == 3 && g_aMsg[1].msg == 0x3B && g_aMsg[2].a == 7 + 16);

    /* 0x1003E980 / 0x1003E9E0 differ only in which counter they read. */
    reset_counters();
    BrUiStF(pObj, BR_UI_OFF_F3C, 5.0f);
    BrUiStF(pObj, BR_UI_OFF_F40, 1.0f);
    g_G.gB4E708 = 2u;
    g_G.gB4E70C = 0u;
    CHECK(BrUiDraw1003E980(pObj, &g_G) == 1);
    CHECK(g_nMsg == 3);
    CHECK(g_aMsg[0].msg == 0x74 && g_aMsg[0].a == 5 && g_aMsg[0].b == 0x14);
    CHECK(g_aMsg[1].msg == 0x75 && g_aMsg[1].a == 5);
    CHECK(g_aMsg[2].msg == 0x75 && g_aMsg[2].a == 5 + 12);

    reset_counters();
    CHECK(BrUiDraw1003E9E0(pObj, &g_G) == 1);
    CHECK(g_nMsg == 1 && g_aMsg[0].msg == 0x74);   /* gB4E70C is 0 */
}

/* ==========================================================================
 * Text family
 * ========================================================================== */

static void test_text(void)
{
    BrUiObj *pObj;

    fresh_globals();
    pObj = fresh_obj();

    /* 0x1003E840: both globals zero selects 0x51, either one non-zero
     * selects 0x0C, and this member does NOT run the apply path. */
    CHECK(BrUiText1003E840(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x51);
    CHECK(strcmp(BrUiItemText(pObj, 0), "S81") == 0);
    CHECK(g_n04 == 1 && g_n10 == 0);      /* apply never ran */
    g_G.g220B20 = 1;
    CHECK(BrUiText1003E840(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x0C);
    g_G.g220B20 = 0;
    g_G.g0AA010 = 1;
    CHECK(BrUiText1003E840(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x0C);
    g_G.g0AA010 = 0;

    /* A representative table-driven member: f04 fires TWICE (once directly,
     * once inside BrUiItemApply) and the apply path is reached. */
    pObj = fresh_obj();
    g_G.gAA287C = 3;
    g_tab[3] = 0x21;
    CHECK(BrUiText1003FC40(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x21);
    CHECK(strcmp(BrUiItemText(pObj, 0), "S33") == 0);
    CHECK(g_n04 == 2 && g_n10 == 1);

    /* 0x1003F760's index handling is a WRAP, not a clamp. */
    pObj = fresh_obj();
    g_G.gAA2904 = 1; g_G.gAA2964 = 2;    /* not solo */
    g_G.g0AC654 = 0x11;
    g_tab[1] = 0x55;
    CHECK(BrUiText1003F760(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x55);          /* 0x11 - 0x10 == 1 */
    g_G.g0AC654 = 0x0F;
    g_tab[15] = 0x56;
    CHECK(BrUiText1003F760(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x56);          /* 0x0F is left alone */
    /* Solo short-circuits to a fixed id. */
    g_G.gAA2964 = 1; g_G.gAA28E8 = 0;
    CHECK(BrUiText1003F760(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x14);

    /* 0x1003FCB0's fallback id when g18ABDBC is zero. */
    pObj = fresh_obj();
    g_G.g18ABDBC = 0;
    CHECK(BrUiText1003FCB0(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x74);
    g_G.g18ABDBC = 1;
    g_G.gAA2A1C = 2;
    g_tab[2] = 0x31;
    CHECK(BrUiText1003FCB0(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x31);

    /* 0x1003F8D0 picks its B08 marker from the conflict array, and the
     * other branch leaves B08 alone. */
    pObj = fresh_obj();
    g_G.gAA2850 = 1;
    g_G.gAA2840 = 4;
    g_conflict[4] = 1;
    CHECK(BrUiText1003F8D0(pObj, &g_G) == 1);
    CHECK(BrUiItem(pObj, 0)[BR_UI_ITEM_OFF_B08] == 4);
    g_conflict[4] = 0;
    CHECK(BrUiText1003F8D0(pObj, &g_G) == 1);
    CHECK(BrUiItem(pObj, 0)[BR_UI_ITEM_OFF_B08] == 1);
    g_G.gAA2850 = 0;
    strcpy(g_bufs[8], "raw");
    BrUiItem(pObj, 0)[BR_UI_ITEM_OFF_B08] = 0x77;
    CHECK(BrUiText1003F8D0(pObj, &g_G) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "raw") == 0);
    CHECK(BrUiItem(pObj, 0)[BR_UI_ITEM_OFF_B08] == 0x77);   /* untouched */

    /* 0x1003FE80's solo path restores F414 exactly (-8 then -(-8)). */
    pObj = fresh_obj();
    g_G.gAA2904 = 1; g_G.gAA2964 = 1; g_G.gAA28E8 = 0;
    BrUiStF(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F414, 100.0f);
    CHECK(BrUiText1003FE80(pObj, &g_G) == 1);
    CHECK(BrUiLdF(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F414) == 100.0f);
    CHECK(g_lastStrId == 0x1C);

    /* 0x1003FA00's byte-0/byte-1 split of the 0x100B3820 records. */
    pObj = fresh_obj();
    g_G.gAA2904 = 1; g_G.gAA2964 = 2;    /* not solo */
    g_G.g0AA010 = 0;
    g_G.gAA28A8 = 1;
    g_G.gAA28AC = 1;
    g_G.gAA28B8 = 2;                     /* k == 1 + 12*2 == 25 */
    g_tabB[2 * 25]     = 6;
    g_tabB[2 * 25 + 1] = 7;
    g_tab[6] = 0x60;
    g_tab[7] = 0x70;
    g_ent[4] = 0;                        /* the 0x10 bit is clear */
    CHECK(BrUiText1003FA00(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x60);          /* byte 0 */
    CHECK(BrUiText1003FE80(pObj, &g_G) == 1);
    CHECK(g_lastStrId == 0x70);          /* byte 1 of the SAME record */

    /* With the 0x10 bit set 0x1003FA00 emits the extra 0xB0 caption first
     * and then still finishes with the scratch string. */
    pObj = fresh_obj();
    g_G.gAA2904 = 1; g_G.gAA2964 = 2;
    g_ent[4] = 0x10;
    CHECK(BrUiText1003FA00(pObj, &g_G) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "S96") == 0);   /* id 0x60 */
    CHECK(g_n04 == 4);                                  /* two full rounds */
}

/* ==========================================================================
 * Text read-back callbacks
 * ========================================================================== */

static void test_readback(void)
{
    BrUiObj *pObj;

    fresh_globals();
    pObj = fresh_obj();

    /* The flag clear is gated on a NON-EMPTY caption. */
    BrUiSt32(g_G.pAA29A8, BR_UI_OFF_FLAGS, 0xFFFFFFFFu);
    BrUiItemText(pObj, 0)[0] = '\0';
    CHECK(BrUiFn1003EF60(pObj, &g_G) == 1);
    CHECK(BrUiLd32(g_G.pAA29A8, BR_UI_OFF_FLAGS) == 0xFFFFFFFFu);
    strcpy(BrUiItemText(pObj, 0), "x");
    CHECK(BrUiFn1003EF60(pObj, &g_G) == 1);
    CHECK(BrUiLd32(g_G.pAA29A8, BR_UI_OFF_FLAGS) == (0xFFFFFFFFu & ~0x10u));

    /* 0x1003EEF0 applies first, then mirrors the caption out. */
    pObj = fresh_obj();
    strcpy(BrUiItemText(pObj, 0), "hello");
    CHECK(BrUiFn1003EEF0(pObj, &g_G) == 1);
    CHECK(strcmp(g_G.szB4E2E8, "hello") == 0);
    CHECK(g_n04 == 1 && g_n10 == 1);

    /* 0x1003EF90's mirror into szB4E1E4 lives INSIDE the differs-branch, so
     * a second identical call leaves a stale mirror in place. */
    pObj = fresh_obj();
    strcpy(BrUiItemText(pObj, 0), "abc");
    CHECK(BrUiFn1003EF90(pObj, &g_G) == 1);
    CHECK(strcmp(g_G.szA9CDF0, "abc") == 0);
    CHECK(strcmp(g_G.szB4E1E4, "abc") == 0);
    strcpy(g_G.szB4E1E4, "STALE");
    CHECK(BrUiFn1003EF90(pObj, &g_G) == 1);      /* caption unchanged */
    CHECK(strcmp(g_G.szB4E1E4, "STALE") == 0);   /* mirror not refreshed */

    /* 0x1003F170 overwrites both the global and the caption from sz39B720
     * AFTER calling 0x1003D210, and never runs the apply path. */
    pObj = fresh_obj();
    strcpy(BrUiItemText(pObj, 0), "typed");
    strcpy(g_G.sz39B720, "canon");
    CHECK(BrUiFn1003F170(pObj, &g_G) == 1);
    CHECK(g_n3D210 == 1);
    CHECK(strcmp(g_G.szA9DD28, "canon") == 0);
    CHECK(strcmp(BrUiItemText(pObj, 0), "canon") == 0);
    CHECK(g_n04 == 0 && g_n10 == 0);
}

/* ==========================================================================
 * 0x10040040
 * ========================================================================== */

static BrCfgRec g_t0[BR_CFG_T0_COUNT];
static BrCfgRec g_t1[BR_CFG_T1_COUNT];
static BrCfgRec g_t3[BR_CFG_T3_COUNT];
static BrCfgTables g_T;

static void build_tables(void)
{
    int i;

    for (i = 0; i < BR_CFG_T0_COUNT; i++) {
        g_t0[i].key = (uint32_t)(1000 + i);
        sprintf(g_t0[i].szText, "T0.%d", i);
    }
    for (i = 0; i < BR_CFG_T1_COUNT; i++) {
        g_t1[i].key = (uint32_t)(2000 + i);
        sprintf(g_t1[i].szText, "T1.%d", i);
    }
    for (i = 0; i < BR_CFG_T3_COUNT; i++) {
        g_t3[i].key = (uint32_t)(3000 + i);
        sprintf(g_t3[i].szText, "T3.%d", i);
    }
    g_T.aT0 = g_t0;
    g_T.aT1 = g_t1;
    g_T.aT3 = g_t3;
}

static void test_lookup(void)
{
    build_tables();

    CHECK(sizeof(BrCfgRec) == 0x24);

    CHECK(BrCfgLookupIndex(&g_T, 0, 1005u) == 5);
    CHECK(BrCfgLookupIndex(&g_T, 0, 1119u) == BR_CFG_T0_COUNT - 1);
    CHECK(BrCfgLookupIndex(&g_T, 3, 3009u) == BR_CFG_T3_COUNT - 1);

    /* Types 1 and 2 are the SAME table -- that is in the jump table. */
    CHECK(BrCfgLookupIndex(&g_T, 1, 2005u) == 5);
    CHECK(BrCfgLookupIndex(&g_T, 2, 2005u) == 5);
    CHECK(BrCfgLookupIndex(&g_T, 1, 3005u) == 0);   /* not in that table */

    /* No match and "found at index 0" are the same answer. */
    CHECK(BrCfgLookupIndex(&g_T, 0, 999999u) == 0);
    CHECK(BrCfgLookupIndex(&g_T, 0, 1000u) == 0);

    /* The type is range-checked UNSIGNED. */
    CHECK(BrCfgLookupIndex(&g_T, 4, 1005u) == 0);
    CHECK(BrCfgLookupIndex(&g_T, -1, 1005u) == 0);
}

/* ==========================================================================
 * 0x100400E0
 * ========================================================================== */

static void test_400E0(void)
{
    BrUiObj *pObj;

    fresh_globals();
    build_tables();
    pObj = fresh_obj();

    /* gAA2844 non-zero short-circuits to string id 0xB2. */
    g_G.gAA2844 = 1;
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(g_lastStrId == 0xB2);
    CHECK(g_n04 == 2);

    /* gAA2A0C out of range: NO copy at all, but f04 + apply still run. */
    pObj = fresh_obj();
    g_G.gAA2844 = 0;
    g_G.gAA2A0C = 4;
    strcpy(BrUiItemText(pObj, 0), "PRESERVED");
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "PRESERVED") == 0);
    CHECK(g_lastStrId == -1);           /* the string table was not touched */
    CHECK(g_n04 == 2 && g_n10 == 1);

    /* kind 0 goes straight to table 0 without ever asking 0x10069BC0. */
    pObj = fresh_obj();
    g_G.gAA2A0C = 0;
    g_G.gAA2840 = 3;                     /* record key 3 */
    memset(g_bindB, 0, sizeof(g_bindB));
    memset(g_bindA, 0, sizeof(g_bindA));
    g_bindB[3] = 7;                      /* 0x10069C30 answers 7 */
    g_t0[9].key = 7u;                    /* which lives at index 9 of T0 */
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "T0.9") == 0);

    /* kind 3 with a positive 0x10069BC0 uses the small table. */
    pObj = fresh_obj();
    g_G.gAA2A0C = 3;
    g_bindA[3] = 1;
    g_bindB[3] = 5;
    g_t3[2].key = 5u;
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "T3.2") == 0);
    CHECK(g_bindKind == 3);

    /* ...and with a zero 0x10069BC0 it falls back to table 0 with the SAME
     * key, which is the surprising half. */
    pObj = fresh_obj();
    g_bindA[3] = 0;
    g_t0[11].key = 5u;
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(strcmp(BrUiItemText(pObj, 0), "T0.11") == 0);

    /* Both answers zero -> string id 0xB1. */
    pObj = fresh_obj();
    g_bindA[3] = 0;
    g_bindB[3] = 0;
    CHECK(BrUiText100400E0(pObj, &g_G, &g_T, NULL) == 1);
    CHECK(g_lastStrId == 0xB1);
}

/* ==========================================================================
 * 0x10040330
 * ========================================================================== */

static void test_conflicts(void)
{
    fresh_globals();
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));

    /* Nothing bound anywhere: no conflict, every flag zero. */
    CHECK(BrCfgFindConflicts(&g_G, 1, NULL) == 0);
    {
        int i, any = 0;
        for (i = 0; i < BR_UI_AB334_COUNT; i++) { any |= g_conflict[i]; }
        CHECK(any == 0);
    }

    /* Records 0 and 1 collide. Pass 0 marks BOTH -- and then pass 1 zeroes
     * flag 1 on its way in and never re-finds it. The asymmetric result is
     * the original's bug and is pinned here deliberately. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    g_bindA[0] = 5; g_bindB[0] = 7;
    g_bindA[1] = 5; g_bindB[1] = 7;
    CHECK(BrCfgFindConflicts(&g_G, 2, NULL) == 1);
    CHECK(g_conflict[0] == 1);
    CHECK(g_conflict[1] == 0);
    CHECK(g_bindKind == 2);

    /* The same asymmetry at the top end, where the last pass additionally
     * skips its (empty) inner loop. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    g_bindA[19] = 3; g_bindB[19] = 4;
    g_bindA[20] = 3; g_bindB[20] = 4;
    CHECK(BrCfgFindConflicts(&g_G, 0, NULL) == 1);
    CHECK(g_conflict[19] == 1);
    CHECK(g_conflict[20] == 0);

    /* Keys 0x0C/0x0D/0x0E are invisible as the SECOND member of a pair while
     * i < 12: record 0 versus record 12 (key 0x0C) is not a conflict. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    g_bindA[0]  = 8; g_bindB[0]  = 9;
    g_bindA[12] = 8; g_bindB[12] = 9;
    CHECK(BrCfgFindConflicts(&g_G, 1, NULL) == 0);

    /* ...but from i == 12 the filter is off, so 12 versus 13 (key 0x0D)
     * does count. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    g_bindA[12] = 8; g_bindB[12] = 9;
    g_bindA[13] = 8; g_bindB[13] = 9;
    CHECK(BrCfgFindConflicts(&g_G, 1, NULL) == 1);
    CHECK(g_conflict[12] == 1);
    CHECK(g_conflict[13] == 0);

    /* Two records agreeing on (0, 0) are "unbound", never a conflict. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    CHECK(BrCfgFindConflicts(&g_G, 1, NULL) == 0);

    /* Agreement on only ONE of the two answers is not enough. */
    memset(g_bindA, 0, sizeof(g_bindA));
    memset(g_bindB, 0, sizeof(g_bindB));
    g_bindA[2] = 4; g_bindB[2] = 1;
    g_bindA[3] = 4; g_bindB[3] = 2;
    CHECK(BrCfgFindConflicts(&g_G, 1, NULL) == 0);
}

int main(void)
{
    test_dperr();
    test_small();
    test_apply();
    test_scalar_callbacks();
    test_code_callbacks();
    test_poll();
    test_draw();
    test_text();
    test_readback();
    test_lookup();
    test_400E0();
    test_conflicts();

    if (g_fails != 0) {
        printf("%d FAILURE(S)\n", g_fails);
        return 1;
    }
    printf("slice2_23: all checks passed\n");
    return 0;
}
