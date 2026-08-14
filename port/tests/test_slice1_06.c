/* test_slice1_06.c -- behaviour tests for slice1_06.c.
 *
 * These assert properties of the ORIGINAL's behaviour -- its clamps, its
 * reserved values, its scan direction, its asymmetries -- not the shape of
 * the port. Where the original does something surprising the test pins the
 * surprising behaviour, so that "fixing" it fails here.
 */

#include "slice1_06.h"

#include <math.h>
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

/* ------------------------------------------------------------------ 0x10037030 */

static void test_pendlist(void)
{
    BrPendList list;
    uint32_t   dropped = 0;
    int        i;
    int        marker[40];

    memset(&list, 0, sizeof(list));

    for (i = 0; i < BR_PENDLIST_MAX; i++) {
        BrPendListAdd(&list, &marker[i], &dropped);
    }
    CHECK(list.count == BR_PENDLIST_MAX);
    CHECK(dropped == 0u);
    for (i = 0; i < BR_PENDLIST_MAX; i++) {
        CHECK(list.apItems[i] == &marker[i]);
    }

    /* Past capacity: nothing is stored, the drop counter moves, and the
     * item counter STILL moves. Both halves matter. */
    for (i = 0; i < 10; i++) {
        BrPendListAdd(&list, &marker[BR_PENDLIST_MAX + i], &dropped);
    }
    CHECK(dropped == 10u);
    CHECK(list.count == BR_PENDLIST_MAX + 10);
    for (i = 0; i < BR_PENDLIST_MAX; i++) {
        CHECK(list.apItems[i] == &marker[i]);   /* untouched */
    }

    /* count is a request count, so it is not usable as a live index. */
    CHECK(list.count > BR_PENDLIST_MAX);
}

/* ------------------------------------------------------------------ 0x10037070 */

static void test_devrecmatch(void)
{
    BrDevRec recs[8];
    uint8_t  idx[BR_DEVREC_SLOTS];
    int      i;

    memset(recs, 0, sizeof(recs));
    for (i = 0; i < BR_DEVREC_SLOTS; i++) {
        idx[i] = 0;                 /* every slot points at record 0 */
    }

    recs[0].f04 = 0u;               /* record 0 is empty */
    recs[5].f04 = 0xDEADu;
    recs[5].f20 = BR_DEVREC_TYPE_MATCH;

    /* Not reachable while no index entry selects record 5. */
    CHECK(BrDevRecMatch(recs, idx, 0xDEADu) == 0);

    /* The lookup really is indirect: the value only becomes visible once an
     * index slot names its record. */
    idx[7] = 5;
    CHECK(BrDevRecMatch(recs, idx, 0xDEADu) == 1);

    /* The type field is an exact 4-bit match, not a bit test. */
    recs[5].f20 = 0x02000000u;
    CHECK(BrDevRecMatch(recs, idx, 0xDEADu) == 0);
    recs[5].f20 = 0x03000000u;
    CHECK(BrDevRecMatch(recs, idx, 0xDEADu) == 0);
    recs[5].f20 = 0xA1000000u;      /* bits outside the mask are ignored */
    CHECK(BrDevRecMatch(recs, idx, 0xDEADu) == 1);

    /* A record whose f04 is zero is skipped even when the query is zero. */
    recs[5].f04 = 0u;
    CHECK(BrDevRecMatch(recs, idx, 0u) == 0);
}

/* ------------------------------------------------------------------ 0x10037930 */

static void test_keytable(void)
{
    BrKeyEnt   ents[4];
    BrKeyTable t;
    uint32_t   a = 0xA5A5A5A5u, b = 0x5A5A5A5Au;

    memset(ents, 0, sizeof(ents));
    ents[0].key = 1100u; ents[0].a = 10u; ents[0].b = 11u;
    ents[1].key = 1200u; ents[1].a = 20u; ents[1].b = 21u;
    ents[2].key = 1100u; ents[2].a = 30u; ents[2].b = 31u;  /* duplicate key */
    ents[3].key = 1300u; ents[3].a = 40u; ents[3].b = 41u;

    t.aEnts = ents;
    t.count = 4;
    t.bias  = 1000u;

    /* The query is biased: the raw table key must NOT be found. */
    CHECK(BrKeyTableFind(&t, 1200u, &a, &b) == 0);
    CHECK(a == 0xA5A5A5A5u);            /* miss leaves the out-params alone */
    CHECK(b == 0x5A5A5A5Au);

    CHECK(BrKeyTableFind(&t, 200u, &a, &b) == 1);
    CHECK(a == 20u && b == 21u);

    /* Backwards scan: with duplicate keys the LAST entry wins. */
    CHECK(BrKeyTableFind(&t, 100u, &a, &b) == 1);
    CHECK(a == 30u && b == 31u);

    /* Empty and negative counts must not touch the table. */
    t.count = 0;
    CHECK(BrKeyTableFind(&t, 100u, &a, &b) == 0);
    t.count = -3;
    CHECK(BrKeyTableFind(&t, 100u, &a, &b) == 0);
    CHECK(a == 30u && b == 31u);
}

/* ------------------------------------------------------------------ 0x1003B940 */

static BrVec3 v3(float x, float y, float z)
{
    BrVec3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

static void test_tricontains(void)
{
    BrVec3 A   = v3(0.0f, 0.0f, 0.0f);
    BrVec3 B   = v3(2.0f, 0.0f, 0.0f);
    BrVec3 C   = v3(0.0f, 2.0f, 0.0f);
    BrVec3 up  = v3(0.0f, 0.0f, 1.0f);
    BrVec3 dn  = v3(0.0f, 0.0f, -1.0f);
    BrVec3 inP = v3(0.5f, 0.5f, 0.0f);
    BrVec3 out = v3(-1.0f, -1.0f, 0.0f);
    BrVec3 onE = v3(1.0f, 0.0f, 0.0f);      /* midpoint of edge A->B */
    BrVec3 nan = v3(0.0f, 0.0f, (float)NAN);
    BrVec3 A2, B2, C2, P2;
    float  d;

    CHECK(BrTriContainsPoint(&inP, &A, &B, &C, &up) == 1);
    CHECK(BrTriContainsPoint(&out, &A, &B, &C, &up) == 0);

    /* The comparison is `>= 0`, so the boundary counts as inside. Both an
     * edge midpoint and a vertex must pass. */
    CHECK(BrTriContainsPoint(&onE, &A, &B, &C, &up) == 1);
    CHECK(BrTriContainsPoint(&B,   &A, &B, &C, &up) == 1);

    /* Flipping the reference direction flips a strictly-interior point. */
    CHECK(BrTriContainsPoint(&inP, &A, &B, &C, &dn) == 0);
    /* ... and so does reversing the winding, for the same reason. */
    CHECK(BrTriContainsPoint(&inP, &A, &C, &B, &up) == 0);
    /* Reversing both cancels out. */
    CHECK(BrTriContainsPoint(&inP, &A, &C, &B, &dn) == 1);

    /* The predicate is built purely from differences, so it is invariant
     * under translation of the whole configuration. */
    A2 = v3(A.x + 100.0f, A.y - 50.0f, A.z + 7.0f);
    B2 = v3(B.x + 100.0f, B.y - 50.0f, B.z + 7.0f);
    C2 = v3(C.x + 100.0f, C.y - 50.0f, C.z + 7.0f);
    P2 = v3(inP.x + 100.0f, inP.y - 50.0f, inP.z + 7.0f);
    CHECK(BrTriContainsPoint(&P2, &A2, &B2, &C2, &up) == 1);
    P2 = v3(out.x + 100.0f, out.y - 50.0f, out.z + 7.0f);
    CHECK(BrTriContainsPoint(&P2, &A2, &B2, &C2, &up) == 0);

    /* A degenerate triangle yields an all-zero cross product, which passes
     * the `>= 0` test on every edge -- the original reports "inside". */
    CHECK(BrTriContainsPoint(&inP, &A, &A, &A, &up) == 1);

    /* Unordered compares set C0, which the original reads as "less than". */
    CHECK(BrTriContainsPoint(&inP, &A, &B, &C, &nan) == 0);

    /* Sanity on the helper this relies on: the reference direction only
     * enters through a dot product, so scaling it cannot change the sign. */
    d = 1000.0f;
    {
        BrVec3 bigUp = v3(0.0f, 0.0f, d);
        CHECK(BrTriContainsPoint(&inP, &A, &B, &C, &bigUp) == 1);
    }
}

/* ------------------------------------------------------------------ 0x1003D180 */

typedef struct FakeCom {
    BrDPlayObj  obj;
    int       calls;
    uint32_t  cbNeeded;
    int32_t   hrFirst;
    int32_t   hrSecond;
    void     *pvSeen;
} FakeCom;

static int32_t fake_get(void *pThis, void *pParam, void *pvBuf, uint32_t *pcb)
{
    FakeCom *p = (FakeCom *)pThis;

    (void)pParam;
    p->calls++;
    if (pvBuf == NULL) {
        *pcb = p->cbNeeded;
        return p->hrFirst;
    }
    p->pvSeen = pvBuf;
    memset(pvBuf, 0x7E, *pcb);
    return p->hrSecond;
}

static const BrDPlayVtbl g_fakeVtbl = { { 0 }, fake_get };

static void test_comgetalloc(void)
{
    FakeCom f;
    void   *pv;
    int32_t hr;

    /* Normal two-pass flow. */
    memset(&f, 0, sizeof(f));
    f.obj.pVtbl = &g_fakeVtbl;
    f.cbNeeded  = 64u;
    f.hrFirst   = BR_COM_E_BUFFERTOOSMALL;
    f.hrSecond  = 0;
    pv = NULL;
    hr = BrComGetAlloc(&f.obj, NULL, &pv);
    CHECK(hr == 0);
    CHECK(f.calls == 2);
    CHECK(pv != NULL && pv == f.pvSeen);
    CHECK(((unsigned char *)pv)[63] == 0x7Eu);
    free(pv);

    /* GOTCHA: a first call that SUCCEEDS is treated as a failure. The
     * function returns that HRESULT -- 0, which reads as success -- while
     * leaving the out-pointer untouched. Callers must not trust a 0 return
     * without having seeded the out-pointer. */
    memset(&f, 0, sizeof(f));
    f.obj.pVtbl = &g_fakeVtbl;
    f.hrFirst   = 0;
    pv = (void *)0x1;
    hr = BrComGetAlloc(&f.obj, NULL, &pv);
    CHECK(hr == 0);
    CHECK(f.calls == 1);
    CHECK(pv == (void *)0x1);       /* never written */

    /* Any other first-call error is propagated verbatim, unallocated. */
    memset(&f, 0, sizeof(f));
    f.obj.pVtbl = &g_fakeVtbl;
    f.hrFirst   = (int32_t)0x80004005;
    pv = NULL;
    hr = BrComGetAlloc(&f.obj, NULL, &pv);
    CHECK(hr == (int32_t)0x80004005);
    CHECK(f.calls == 1);
    CHECK(pv == NULL);

    /* Second-call failure releases the buffer and reports the error. */
    memset(&f, 0, sizeof(f));
    f.obj.pVtbl = &g_fakeVtbl;
    f.cbNeeded  = 32u;
    f.hrFirst   = BR_COM_E_BUFFERTOOSMALL;
    f.hrSecond  = (int32_t)0x8007000E;
    pv = NULL;
    hr = BrComGetAlloc(&f.obj, NULL, &pv);
    CHECK(hr == (int32_t)0x8007000E);
    CHECK(f.calls == 2);
    CHECK(pv == NULL);

    /* A non-negative second result other than 0 is still success, and is
     * still discarded in favour of 0. */
    memset(&f, 0, sizeof(f));
    f.obj.pVtbl = &g_fakeVtbl;
    f.cbNeeded  = 16u;
    f.hrFirst   = BR_COM_E_BUFFERTOOSMALL;
    f.hrSecond  = 1;
    pv = NULL;
    hr = BrComGetAlloc(&f.obj, NULL, &pv);
    CHECK(hr == 0);
    CHECK(pv != NULL);
    free(pv);
}

/* ------------------------------------------------------------------ 0x1003E1D0 */

static void test_pairbuf(void)
{
    BrPairBuf buf;
    uint32_t  external[BR_PAIRBUF_DWORDS];
    int       i;

    memset(&buf, 0xCD, sizeof(buf));
    buf.pA = NULL;
    buf.pB = NULL;

    CHECK(BrPairBufReset(&buf) == 1);

    /* Null pointers bind to the statics ... */
    CHECK(buf.pA == buf.aStaticA);
    CHECK(buf.pB == buf.aStaticB);
    /* ... and both buffers are cleared. */
    for (i = 0; i < BR_PAIRBUF_DWORDS; i++) {
        CHECK(buf.aStaticA[i] == 0u);
        CHECK(buf.aStaticB[i] == 0u);
    }

    /* The two statics are contiguous in the original (0x10AF9890 +
     * 0x53*4 == 0x10AF99DC); the layout here must preserve that. */
    CHECK(buf.aStaticA + BR_PAIRBUF_DWORDS == buf.aStaticB);

    /* An already-bound pointer is respected, not rebound. */
    memset(external, 0xFF, sizeof(external));
    memset(buf.aStaticA, 0xFF, sizeof(buf.aStaticA));
    buf.pA = external;
    CHECK(BrPairBufReset(&buf) == 1);
    CHECK(buf.pA == external);
    for (i = 0; i < BR_PAIRBUF_DWORDS; i++) {
        CHECK(external[i] == 0u);
    }
    /* ... and the static it did not use is left alone. */
    CHECK(buf.aStaticA[0] == 0xFFFFFFFFu);
}

/* ------------------------------------------------------------------ 0x1003E260 */

typedef struct ErrLog {
    int         lookups;
    uint32_t    lastId;
    const char *pszText;
    const char *pszCaption;
    uint32_t    uType;
    int         exits;
    int         exitCode;
} ErrLog;

/* Records carry a one-byte prefix that the original skips for the body. */
static const char g_str162[] = "\x01" "text-162";
static const char g_str164[] = "\x01" "text-164";
static const char g_strCap[] = "\x01" "caption";

static const char *err_lookup(void *pUser, uint32_t id)
{
    ErrLog *p = (ErrLog *)pUser;

    p->lookups++;
    p->lastId = id;
    if (id == 162u) return g_str162;
    if (id == 164u) return g_str164;
    if (id == BR_ERR_CAPTION_ID) return g_strCap;
    return NULL;
}

static void err_message(void *pUser, const char *pszText,
                        const char *pszCaption, uint32_t uType)
{
    ErrLog *p = (ErrLog *)pUser;

    p->pszText    = pszText;
    p->pszCaption = pszCaption;
    p->uType      = uType;
}

static void err_exit(void *pUser, int code)
{
    ErrLog *p = (ErrLog *)pUser;

    p->exits++;
    p->exitCode = code;
}

static void test_errshow(void)
{
    ErrLog     log;
    BrErrHost  host;

    host.pfnLookup  = err_lookup;
    host.pfnMessage = err_message;
    host.pfnExit    = err_exit;
    host.pUser      = &log;

    /* Entry 0 is fatal and uses string id 162. */
    memset(&log, 0, sizeof(log));
    BrErrShow(&host, 0);
    CHECK(log.pszText != NULL);
    /* The body pointer is lookup()+1; the caption pointer is not adjusted. */
    CHECK(log.pszText == g_str162 + 1);
    CHECK(strcmp(log.pszText, "text-162") == 0);
    CHECK(log.pszCaption == g_strCap);
    CHECK(log.uType == 0u);
    CHECK(log.exits == 1 && log.exitCode == 1);

    /* Entry 2 is non-fatal. */
    memset(&log, 0, sizeof(log));
    BrErrShow(&host, 2);
    CHECK(log.pszText == g_str164 + 1);
    CHECK(log.exits == 0);

    /* Entry 8's fatal flag is 0xFFFFFFFF, not 1; it must still be fatal. */
    CHECK(g_aBrErrTable[8].fFatal != 0);
    CHECK(g_aBrErrTable[8].fFatal != 1);
    memset(&log, 0, sizeof(log));
    BrErrShow(&host, 8);
    CHECK(log.exits == 1);

    /* Out of range: nothing happens at all. */
    memset(&log, 0, sizeof(log));
    BrErrShow(&host, BR_ERR_COUNT);
    CHECK(log.lookups == 0 && log.exits == 0);
    BrErrShow(&host, 1000);
    CHECK(log.lookups == 0 && log.exits == 0);

    /* Table shape: ids run 162..169 then 256. */
    CHECK(g_aBrErrTable[0].idText == 162u);
    CHECK(g_aBrErrTable[7].idText == 169u);
    CHECK(g_aBrErrTable[8].idText == 256u);
}

/* ------------------------------------------------------------------ 0x1003E310 */

static void test_optsave(void)
{
    BrOptState   st;
    BrOptScratch sc;
    int          i;

    /* Distinct, identifiable values so the permutation is checkable. */
    for (i = 0; i < BR_OPT_CFG_COUNT; i++) st.aCfg[i] = 100 + i;
    for (i = 0; i < BR_OPT_SEL_COUNT; i++) st.aSel[i] = 200 + i;

    memset(&sc, 0xEE, sizeof(sc));
    BrOptSave(&sc, &st);

    CHECK(sc.a[0]  == st.aCfg[0]);
    CHECK(sc.a[1]  == st.aSel[0]);
    CHECK(sc.a[2]  == st.aSel[2]);
    CHECK(sc.a[3]  == st.aCfg[1]);
    CHECK(sc.a[4]  == st.aCfg[2]);
    CHECK(sc.a[5]  == st.aCfg[3]);
    CHECK(sc.a[6]  == st.aSel[3]);
    CHECK(sc.a[7]  == st.aCfg[4]);
    CHECK(sc.a[8]  == st.aSel[4]);
    CHECK(sc.a[9]  == st.aSel[5]);
    CHECK(sc.a[10] == st.aCfg[5]);
    CHECK(sc.a[11] == st.aSel[6]);

    /* aSel[1] (0x10AA2A04) is not part of the saved set, and the block is a
     * permutation: every slot distinct, none equal to aSel[1]. */
    for (i = 0; i < BR_OPT_SCRATCH_COUNT; i++) {
        int j;
        CHECK(sc.a[i] != st.aSel[1]);
        for (j = i + 1; j < BR_OPT_SCRATCH_COUNT; j++) {
            CHECK(sc.a[i] != sc.a[j]);
        }
    }
}

/* ------------------------------------------------- 0x1003F2B0 / 0x1003F320 */

static void test_optavail(void)
{
    BrOptCaps c;
    uint32_t  n;

    memset(&c, 0, sizeof(c));

    /* --- 0x1003F2B0 --- */

    /* 12 is a reserved sentinel: it loses even to the force flag. */
    c.fForceAvailA = 1;
    CHECK(BrOptAvailA(&c, 12u) == 0);
    for (n = 0u; n < 16u; n++) {
        if (n != 12u) {
            CHECK(BrOptAvailA(&c, n) == 1);
        }
    }
    c.fForceAvailA = 0;

    /* mode 0, fAlt clear -> maskA. */
    c.mode  = 0;
    c.fAlt  = 0;
    c.maskA = 0x000Au;      /* bits 1 and 3 */
    CHECK(BrOptAvailA(&c, 1u) != 0);
    CHECK(BrOptAvailA(&c, 3u) != 0);
    CHECK(BrOptAvailA(&c, 2u) == 0);

    /* mode 0, fAlt set -> the HIGH half of the shared dword. Setting only
     * the low half must change nothing. */
    c.fAlt     = 1;
    c.maskPair = 0x0000FFFFu;
    for (n = 0u; n < 16u; n++) {
        if (n != 12u) {
            CHECK(BrOptAvailA(&c, n) == 0);
        }
    }
    c.maskPair = 0x00050000u;   /* high half = bits 0 and 2 */
    CHECK(BrOptAvailA(&c, 0u) != 0);
    CHECK(BrOptAvailA(&c, 2u) != 0);
    CHECK(BrOptAvailA(&c, 1u) == 0);

    /* mode != 0: 13 and 14 are forced available when fLowAlwaysB is set. */
    c.mode        = 3;
    c.maskAMode   = 0u;
    c.fLowAlwaysB = 1;
    CHECK(BrOptAvailA(&c, 13u) == 1);
    CHECK(BrOptAvailA(&c, 14u) == 1);
    CHECK(BrOptAvailA(&c, 11u) == 0);
    c.fLowAlwaysB = 0;
    CHECK(BrOptAvailA(&c, 13u) == 0);

    /* --- 0x1003F320 --- */

    memset(&c, 0, sizeof(c));

    /* Index 15 is remapped to 11 in mode 0 ... */
    c.mode  = 0;
    c.maskB = 1u << 11;
    CHECK(BrOptAvailB(&c, 15u) != 0);
    CHECK(BrOptAvailB(&c, 11u) != 0);
    c.maskB = 1u << 15;
    CHECK(BrOptAvailB(&c, 15u) == 0);   /* bit 15 is never consulted */

    /* ... and to 7, not 11, in mode 6. */
    memset(&c, 0, sizeof(c));
    c.mode        = 6;
    c.maskBMode6  = 1u << 7;
    CHECK(BrOptAvailB(&c, 15u) != 0);
    c.maskBMode6  = 1u << 11;
    CHECK(BrOptAvailB(&c, 15u) == 0);

    /* fRebaseB folds the upper 16 indices onto the lower 16. */
    memset(&c, 0, sizeof(c));
    c.mode     = 0;
    c.maskB    = 1u << 3;
    c.fRebaseB = 0;
    CHECK(BrOptAvailB(&c, 19u) == 0);
    c.fRebaseB = 1;
    CHECK(BrOptAvailB(&c, 19u) != 0);
    CHECK(BrOptAvailB(&c, 3u)  != 0);   /* low indices are not rebased */

    /* fAlt rebases too, but only when bit 15 of the LOW half of the shared
     * dword is set -- and it then uses that same low half as the mask. */
    memset(&c, 0, sizeof(c));
    c.mode     = 0;
    c.fAlt     = 1;
    c.maskPair = 0x00008008u;           /* bit 15 flag + bit 3 mask */
    CHECK(BrOptAvailB(&c, 19u) != 0);
    c.maskPair = 0x00000008u;           /* no flag: 19 stays out of range */
    CHECK(BrOptAvailB(&c, 19u) == 0);
    CHECK(BrOptAvailB(&c, 3u)  != 0);

    /* fLowAlways short-circuits every index <= 15, mask or not. */
    memset(&c, 0, sizeof(c));
    c.mode       = 0;
    c.maskB      = 0u;
    c.fLowAlways = 1;
    for (n = 0u; n <= 15u; n++) {
        CHECK(BrOptAvailB(&c, n) == 1);
    }
    CHECK(BrOptAvailB(&c, 16u) == 0);

    /* mode 2 forces the single index held in nAlwaysB. */
    memset(&c, 0, sizeof(c));
    c.mode          = 2;
    c.nAlwaysB      = 9;
    c.maskBDefault  = 0;
    CHECK(BrOptAvailB(&c, 9u) == 1);
    CHECK(BrOptAvailB(&c, 8u) == 0);

    /* The default mask is SIGN-extended: bit 15 set makes every index from
     * 15 up read as available. Zero-extending it would break this. */
    memset(&c, 0, sizeof(c));
    c.mode         = 3;
    c.maskBDefault = (int16_t)0x8000;
    CHECK(BrOptAvailB(&c, 16u) != 0);
    CHECK(BrOptAvailB(&c, 31u) != 0);
    CHECK(BrOptAvailB(&c, 14u) == 0);
    /* 15 is remapped to 11 first, so it reads the (clear) bit 11. */
    CHECK(BrOptAvailB(&c, 15u) == 0);
    c.maskBDefault = (int16_t)0x0001;
    CHECK(BrOptAvailB(&c, 16u) == 0);
}

/* ------------------------------------------------------------------ 0x1005CB90 */

static void test_namelist(void)
{
    BrNameList *p = (BrNameList *)malloc(sizeof(BrNameList));
    int         i;
    int         vtblMarker = 0;

    if (p == NULL) {
        CHECK(0);
        return;
    }
    memset(p, 0xAB, sizeof(*p));

    CHECK(BrNameListInit(p, &vtblMarker, "abc") == p);
    CHECK(p->pVtbl == &vtblMarker);

    for (i = 0; i < BR_NAMELIST_COUNT; i++) {
        CHECK(strcmp(p->asz[i], "abc") == 0);
        /* Everything past the terminator is zeroed. */
        CHECK(p->asz[i][4] == '\0');
        CHECK(p->asz[i][BR_NAMELIST_STRIDE - 1] == '\0');
    }

    /* The stride is what makes 100 * 0x104 fill the block the original
     * zeroes; slots must be exactly that far apart. */
    CHECK(&p->asz[1][0] - &p->asz[0][0] == BR_NAMELIST_STRIDE);

    /* An empty fill leaves every slot an empty string, which is what the
     * retail build actually does (the source buffer is uninitialised data). */
    memset(p, 0xAB, sizeof(*p));
    BrNameListInit(p, NULL, "");
    for (i = 0; i < BR_NAMELIST_COUNT; i++) {
        CHECK(p->asz[i][0] == '\0');
    }

    free(p);
}

/* ------------------------------------------------------------------ 0x1005D440 */

static void test_uiassets(void)
{
    char *apsz[BR_UIASSET_COUNT];
    int   i;

    /* Every path must fit the buffer the original allocates for it -- the
     * original strcpy's with no bound, so this is a real precondition. */
    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        CHECK(g_apszBrUiAssets[i] != NULL);
        CHECK(strlen(g_apszBrUiAssets[i]) < (size_t)BR_UIASSET_PATH_MAX);
    }

    /* Record geometry: 145 records of 0x74 starting 0x70 into the block
     * exactly fill the 0x106D dwords the original zeroes. */
    CHECK(BR_UIASSET_COUNT * BR_UIASSET_STRIDE == 0x106D * 4);
    CHECK(BR_UIASSET_PATH_OFF + 4 == BR_UIASSET_STRIDE);

    CHECK(BrUiAssetPathsInit(apsz) == 0);
    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        CHECK(apsz[i] != NULL);
        CHECK(strcmp(apsz[i], g_apszBrUiAssets[i]) == 0);
        /* calloc'd: the tail of each buffer is zero. */
        CHECK(apsz[i][BR_UIASSET_PATH_MAX - 1] == '\0');
        /* Each entry is its own allocation, not a shared one. */
        if (i > 0) {
            CHECK(apsz[i] != apsz[i - 1]);
        }
    }

    /* Spot-checks against the disassembly, first and last records. */
    CHECK(strcmp(g_apszBrUiAssets[0], "images\\work1a.bmp") == 0);
    CHECK(strcmp(g_apszBrUiAssets[BR_UIASSET_COUNT - 1],
                 "images\\trakQ_.bmp") == 0);

    BrUiAssetPathsFree(apsz);
    for (i = 0; i < BR_UIASSET_COUNT; i++) {
        CHECK(apsz[i] == NULL);
    }

    CHECK(strcmp(g_pszBrRallySeasonDat, "c:\\RallySeason.dat") == 0);
    CHECK(strcmp(g_pszBrRallyGhostDat,  "c:\\RallyGhost.dat") == 0);
}

int main(void)
{
    test_pendlist();
    test_devrecmatch();
    test_keytable();
    test_tricontains();
    test_comgetalloc();
    test_pairbuf();
    test_errshow();
    test_optsave();
    test_optavail();
    test_namelist();
    test_uiassets();

    if (g_fails != 0) {
        printf("test_slice1_06: %d FAILURES\n", g_fails);
        return 1;
    }
    printf("test_slice1_06: all checks passed\n");
    return 0;
}
