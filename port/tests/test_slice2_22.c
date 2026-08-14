/* test_slice2_22.c -- behaviour/invariant tests for the ported part of the
 * DirectPlay module (0x1003BD50..0x1003DBC0).
 *
 * Cross-slice functions this packet calls are STUBBED HERE ONLY (see the
 * "STAND-IN" block); the real implementations live in slice1_03 / slice1_06. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "slice2_22.h"
#include "slice1_03.h"
#include "slice1_06.h"

static int g_fails = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));         \
            ++g_fails;                                                     \
        }                                                                  \
    } while (0)

/* =====================================================================
 * STAND-IN implementations of cross-slice callees. Test file only.
 * ===================================================================== */

/* STAND-IN for 0x1000C4D0 (slice1_03 BrComCallLocked68 = DirectPlay Send). */
typedef struct SendRec {
    int      nCalls;
    void    *pThis;
    uint32_t idFrom, idTo, flags, cbData;
    uint32_t aData[4];
} SendRec;
static SendRec g_send;

int BrComCallLocked68(BrComObj *pThis, void *a2, void *a3, void *a4,
                      void *a5, void *a6)
{
    uint32_t cb = (uint32_t)(uintptr_t)a6;
    ++g_send.nCalls;
    g_send.pThis  = pThis;
    g_send.idFrom = (uint32_t)(uintptr_t)a2;
    g_send.idTo   = (uint32_t)(uintptr_t)a3;
    g_send.flags  = (uint32_t)(uintptr_t)a4;
    g_send.cbData = cb;
    memset(g_send.aData, 0xEE, sizeof g_send.aData);
    if (cb <= sizeof g_send.aData)
        memcpy(g_send.aData, a5, cb);
    return 0x2000;                     /* distinctive success-ish HRESULT */
}

/* STAND-IN for 0x10003580 (the local-delivery side channel of tag 8). */
typedef struct LocalRec {
    int      nCalls;
    void    *pv1;
    uint32_t cbData, idFrom;
    int32_t  a5;
    uint32_t aData[4];
} LocalRec;
static LocalRec g_local;

void BrAppMsg107(void *pv1, const void *pData, uint32_t cbData,
                 uint32_t idFrom, int32_t a5);
void BrAppMsg107(void *pv1, const void *pData, uint32_t cbData,
                 uint32_t idFrom, int32_t a5)
{
    ++g_local.nCalls;
    g_local.pv1    = pv1;
    g_local.cbData = cbData;
    g_local.idFrom = idFrom;
    g_local.a5     = a5;
    memset(g_local.aData, 0xEE, sizeof g_local.aData);
    if (cbData <= sizeof g_local.aData)
        memcpy(g_local.aData, pData, cbData);
}

/* STAND-IN for 0x1003F320 (slice1_06 BrOptAvailB). The real one decodes a
 * pile of masks; here BrOptCaps::maskB is simply a 32-bit availability set,
 * which is enough to exercise the caller's traversal. */
int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n)
{
    if (n > 31u) return 0;
    return (int32_t)((pCaps->maskB >> n) & 1u);
}

/* The other slice1_06 symbols are not called; provide nothing for them. */

/* =====================================================================
 * 1. 0x1003BD50
 * ===================================================================== */

static void test_rand(void)
{
    uint32_t s, prev;
    int i;

    /* 0 is absorbing. */
    s = 0;
    CHECK(BrDPlayRandStep(&s) == 0 && s == 0, "rand: 0 must be absorbing");

    /* Matches an independent wide-arithmetic reference, and never leaves the
     * 27-bit range. */
    s = 1u;
    for (i = 0; i < 4000; ++i) {
        uint32_t ref = (uint32_t)(((uint64_t)s * 16807ull) & 0x07FFFFFFull);
        uint32_t got = BrDPlayRandStep(&s);
        CHECK(got == ref, "rand: disagrees with 64-bit reference");
        CHECK(got == s, "rand: return value must be the new state");
        CHECK(s < (1u << 27), "rand: state escaped 27 bits");
        if (got != ref) break;
    }

    /* It is NOT the minimal-standard generator: after two steps from seed 1
     * the true Lehmer sequence is 16807^2 mod (2^31-1) = 282475249, which is
     * outside the 27-bit range this one can ever produce. */
    s = 1u;
    (void)BrDPlayRandStep(&s);
    (void)BrDPlayRandStep(&s);
    CHECK(s != 282475249u, "rand: must not equal minimal-standard Lehmer");
    CHECK(s == ((16807u * 16807u) & 0x07FFFFFFu),
          "rand: two steps from 1 must be 16807^2 mod 2^27");

    /* Odd multiplier modulo a power of two is a bijection, so the step must
     * be injective: distinct states can never merge. */
    {
        uint32_t a = 0x0123456u, b = 0x0123457u;
        uint32_t ra, rb;
        ra = BrDPlayRandStep(&a);
        rb = BrDPlayRandStep(&b);
        CHECK(ra != rb, "rand: step must be injective");
    }

    /* No fixed point other than 0 anywhere in a long run from a live seed. */
    s = 0x07FFFFFFu;
    prev = s;
    for (i = 0; i < 2000; ++i) {
        uint32_t got = BrDPlayRandStep(&s);
        CHECK(got != prev || got == 0, "rand: unexpected fixed point");
        prev = got;
    }
}

/* =====================================================================
 * 2. service-provider table
 * ===================================================================== */

static void fill_known(uint8_t aKnown[BR_DPLAY_SP_KNOWN][16])
{
    int i, j;
    for (i = 0; i < BR_DPLAY_SP_KNOWN; ++i)
        for (j = 0; j < 16; ++j)
            aKnown[i][j] = (uint8_t)(0xA0 + i * 16 + j);
}

static void test_sp_classify(void)
{
    uint8_t aKnown[BR_DPLAY_SP_KNOWN][16];
    uint8_t aOther[16];
    int seen[4];
    int i;

    fill_known(aKnown);
    memset(aOther, 0x5A, sizeof aOther);

    /* The documented, deliberately non-identity mapping. */
    CHECK(BrDPlaySpClassify(aKnown[0], aKnown) == 0, "classify[0] != 0");
    CHECK(BrDPlaySpClassify(aKnown[1], aKnown) == 1, "classify[1] != 1");
    CHECK(BrDPlaySpClassify(aKnown[2], aKnown) == 3, "classify[2] != 3");
    CHECK(BrDPlaySpClassify(aKnown[3], aKnown) == 2, "classify[3] != 2");

    /* Whatever the mapping is, it must be a permutation of 0..3 -- otherwise
     * two providers would share a table row. */
    memset(seen, 0, sizeof seen);
    for (i = 0; i < BR_DPLAY_SP_KNOWN; ++i) {
        int r = BrDPlaySpClassify(aKnown[i], aKnown);
        CHECK(r >= 0 && r < 4, "classify out of range");
        if (r >= 0 && r < 4) ++seen[r];
    }
    for (i = 0; i < 4; ++i)
        CHECK(seen[i] == 1, "classify: not a permutation of 0..3");

    CHECK(BrDPlaySpClassify(aOther, aKnown) == -1, "classify: unknown != -1");

    /* Only the 16 GUID bytes matter. */
    {
        uint8_t aNear[16];
        memcpy(aNear, aKnown[1], 16);
        aNear[15] ^= 1u;
        CHECK(BrDPlaySpClassify(aNear, aKnown) == -1,
              "classify: one differing byte must not match");
    }
}

static void test_sp_store(void)
{
    static BrDPlaySp aTable[BR_DPLAY_SP_MAX];
    uint8_t aKnown[BR_DPLAY_SP_KNOWN][16];
    uint8_t aOther[16];
    uint8_t *pGuid = NULL;
    uint32_t aBlob[3];
    int i;

    fill_known(aKnown);
    memset(aOther, 0x5A, sizeof aOther);
    memset(aTable, 0, sizeof aTable);
    aBlob[0] = 0x11111111u; aBlob[1] = 0x22222222u; aBlob[2] = 0x33333333u;

    /* aKnown[2] must land in ROW 3, and nowhere else. */
    CHECK(BrDPlaySpEnumConn(aTable, aKnown[2], aKnown, "modem",
                            aBlob, (uint32_t)sizeof aBlob) == 1,
          "EnumConn must always return 1");
    CHECK(memcmp(aTable[3].aGuid, aKnown[2], 16) == 0,
          "EnumConn: guid landed in the wrong row");
    CHECK(strcmp(aTable[3].aName, "modem") == 0, "EnumConn: name not stored");
    CHECK(aTable[3].cbConn == sizeof aBlob, "EnumConn: size not stored");
    CHECK(aTable[3].pConn != NULL &&
          memcmp(aTable[3].pConn, aBlob, sizeof aBlob) == 0,
          "EnumConn: blob not copied");
    for (i = 0; i < BR_DPLAY_SP_MAX; ++i)
        if (i != 3)
            CHECK(aTable[i].pConn == NULL && aTable[i].cbConn == 0,
                  "EnumConn: touched a row it should not have");

    /* The tail of the fixed-width name field must be zeroed, so the field is
     * always a usable C string. */
    for (i = (int)strlen("modem"); i < BR_DPLAY_SP_NAMELEN; ++i)
        CHECK(aTable[3].aName[i] == 0, "EnumConn: name tail not zeroed");

    /* Re-storing a shorter name must not leave the old tail behind. */
    CHECK(BrDPlaySpEnumConn(aTable, aKnown[2], aKnown, "x",
                            aBlob, 4u) == 1, "EnumConn rc");
    CHECK(strcmp(aTable[3].aName, "x") == 0, "EnumConn: name not replaced");
    CHECK(aTable[3].cbConn == 4u, "EnumConn: size not replaced");

    /* An unrecognised GUID must write nothing at all, and still return 1. */
    {
        BrDPlaySp aBefore[BR_DPLAY_SP_MAX];
        memcpy(aBefore, aTable, sizeof aTable);
        CHECK(BrDPlaySpEnumConn(aTable, aOther, aKnown, "nope",
                                aBlob, 4u) == 1,
              "EnumConn: unknown guid must still return 1");
        CHECK(memcmp(aBefore, aTable, sizeof aTable) == 0,
              "EnumConn: unknown guid must not modify the table");
    }

    /* SelectedGuid points at the GUID FIELD of the row, not the row. */
    CHECK(BrDPlaySpSelectedGuid(aTable, 3u, &pGuid) == 0,
          "SelectedGuid must return 0");
    CHECK(pGuid == aTable[3].aGuid, "SelectedGuid: wrong pointer");
    CHECK((size_t)(pGuid - (uint8_t *)&aTable[3]) == 0xC8,
          "SelectedGuid: guid is not at row+0xC8");
    /* The original's stride is 0xE0; that only holds where a pointer is four
     * bytes wide. What must hold everywhere is the field ORDER and the GUID's
     * offset, which is what every call site derives its arithmetic from. */
    if (sizeof(void *) == 4)
        CHECK(sizeof(BrDPlaySp) == 0xE0, "BrDPlaySp stride must be 0xE0 (32-bit)");
    CHECK((size_t)((uint8_t *)&aTable[0].cbConn - (uint8_t *)&aTable[0]) == 0xD8,
          "cbConn is not at row+0xD8");

    /* FreeAll nulls every pointer but deliberately leaves cbConn alone. */
    BrDPlaySpFreeAll(aTable);
    for (i = 0; i < BR_DPLAY_SP_MAX; ++i)
        CHECK(aTable[i].pConn == NULL, "FreeAll: pointer not cleared");
    CHECK(aTable[3].cbConn == 4u, "FreeAll: must not clear cbConn");
    BrDPlaySpFreeAll(aTable);     /* idempotent, must not double free */
}

/* =====================================================================
 * 3. slot table
 * ===================================================================== */

static void slots_init(BrSlotTable *pT)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        pT->aSlots[i].id = BR_SLOT_EMPTY;
        pT->aSlots[i].a  = 0;
        pT->aSlots[i].b  = 0;
    }
    pT->count = 0;
}

static int slots_find(const BrSlotTable *pT, int id)
{
    int i;
    for (i = 0; i < BR_SLOT_COUNT; ++i)
        if (pT->aSlots[i].id == id) return i;
    return -1;
}

static void test_slots(void)
{
    BrSlotTable t;
    int i;

    /* First free row is claimed, and id 0 is a legal key. */
    slots_init(&t);
    BrDPlaySlotTouch(&t, 0);
    CHECK(t.aSlots[0].id == 0 && t.aSlots[0].b == 1,
          "Touch: id 0 must be a valid key in row 0");
    CHECK(t.aSlots[0].a == 0, "Touch: a must be 0 for id != 1");

    /* id == 1 is the reserved value that also sets `a`. */
    slots_init(&t);
    BrDPlaySlotTouch(&t, 1);
    CHECK(t.aSlots[0].id == 1 && t.aSlots[0].a == 1 && t.aSlots[0].b == 1,
          "Touch: id 1 must set a as well");
    BrDPlaySlotTouch(&t, 2);
    CHECK(t.aSlots[1].id == 2 && t.aSlots[1].a == 0,
          "Touch: id 2 must not set a");

    /* Touching an existing id must only re-mark it, never duplicate it. */
    slots_init(&t);
    BrDPlaySlotTouch(&t, 7);
    t.aSlots[0].b = 0;
    t.aSlots[0].a = 9;                 /* a must be left alone on re-touch */
    BrDPlaySlotTouch(&t, 7);
    CHECK(t.aSlots[0].b == 1, "Touch: re-touch must set b");
    CHECK(t.aSlots[0].a == 9, "Touch: re-touch must not rewrite a");
    CHECK(t.aSlots[1].id == BR_SLOT_EMPTY, "Touch: duplicated an id");

    /* FIRST free row wins, even when a later one is also free. */
    slots_init(&t);
    for (i = 0; i < BR_SLOT_COUNT; ++i) BrDPlaySlotTouch(&t, 100 + i);
    t.aSlots[5].id = BR_SLOT_EMPTY;
    t.aSlots[2].id = BR_SLOT_EMPTY;
    BrDPlaySlotTouch(&t, 999);
    CHECK(t.aSlots[2].id == 999, "Touch: must claim the lowest free row");
    CHECK(t.aSlots[5].id == BR_SLOT_EMPTY, "Touch: claimed the wrong row");

    /* Full table: a new id is silently dropped, nothing is evicted. */
    slots_init(&t);
    for (i = 0; i < BR_SLOT_COUNT; ++i) BrDPlaySlotTouch(&t, 200 + i);
    BrDPlaySlotTouch(&t, 777);
    CHECK(slots_find(&t, 777) < 0, "Touch: full table must drop the new id");
    for (i = 0; i < BR_SLOT_COUNT; ++i)
        CHECK(t.aSlots[i].id == 200 + i, "Touch: full table must not evict");

    /* Mark-and-sweep round trip: clear, re-touch a subset, purge. Exactly the
     * touched subset survives. */
    slots_init(&t);
    for (i = 0; i < BR_SLOT_COUNT; ++i) BrDPlaySlotTouch(&t, 300 + i);
    BrDPlaySlotsClearMarks(&t);
    for (i = 0; i < BR_SLOT_COUNT; ++i)
        CHECK(t.aSlots[i].b == 0 && t.aSlots[i].id == 300 + i,
              "ClearMarks: must clear b and only b");
    BrDPlaySlotTouch(&t, 301);
    BrDPlaySlotTouch(&t, 305);
    BrDPlaySlotsPurge(&t);
    CHECK(slots_find(&t, 301) >= 0 && slots_find(&t, 305) >= 0,
          "Purge: dropped a marked row");
    for (i = 0; i < BR_SLOT_COUNT; ++i) {
        int id = 300 + i;
        if (id != 301 && id != 305)
            CHECK(slots_find(&t, id) < 0, "Purge: kept an unmarked row");
    }
    /* Surviving rows keep their mark -- purge does not clear b. */
    CHECK(t.aSlots[slots_find(&t, 301)].b == 1,
          "Purge: must not clear b on survivors");

    /* Purge then re-touch must reuse the freed rows. */
    BrDPlaySlotsClearMarks(&t);
    BrDPlaySlotsPurge(&t);
    for (i = 0; i < BR_SLOT_COUNT; ++i)
        CHECK(t.aSlots[i].id == BR_SLOT_EMPTY && t.aSlots[i].a == 0,
              "Purge: empty sweep must free everything");
}

/* =====================================================================
 * 4. senders
 * ===================================================================== */

static BrComObj g_iface;   /* only its address matters to the stub */

static void link_init(BrDPlayLink *pL)
{
    pL->pIface = &g_iface;
    pL->f04    = NULL;
    pL->f08    = 0x1234u;
    pL->f0C    = 0;
}

static void test_senders(void)
{
    BrDPlayLink link;
    BrDPlayLink dead;
    int rc;

    link_init(&link);

    /* Envelope shape, shared by all six pair senders. */
    memset(&g_send, 0, sizeof g_send);
    rc = BrDPlaySendTag2(&link, 0, 0xDEADBEEFu);
    CHECK(g_send.nCalls == 1, "Tag2: must send once");
    CHECK(rc == 0x2000, "Tag2: must return the transport result");
    CHECK(g_send.pThis == &g_iface, "Tag2: wrong interface");
    CHECK(g_send.idFrom == 0x1234u, "Tag2: idFrom must be link->f08");
    CHECK(g_send.idTo == 0u, "Tag2: idTo must be 0 (all players)");
    CHECK(g_send.flags == 1u, "Tag2: flags must be 1 (guaranteed)");
    CHECK(g_send.cbData == 8u, "Tag2: payload must be 8 bytes");
    CHECK(g_send.aData[0] == BR_DPLAY_TAG2, "Tag2: wrong tag");
    CHECK(g_send.aData[1] == 0xDEADBEEFu, "Tag2: wrong value");

    /* Each wrapper differs only in its tag. */
    memset(&g_send, 0, sizeof g_send);
    BrDPlaySendTag4(&link, 0, 4u);
    CHECK(g_send.aData[0] == BR_DPLAY_TAG4 && g_send.aData[1] == 4u,
          "Tag4 payload");
    memset(&g_send, 0, sizeof g_send);
    BrDPlaySendTag5(&link, 0, 5u);
    CHECK(g_send.aData[0] == BR_DPLAY_TAG5 && g_send.aData[1] == 5u,
          "Tag5 payload");
    memset(&g_send, 0, sizeof g_send);
    BrDPlaySendTag6(&link, 6u);
    CHECK(g_send.aData[0] == BR_DPLAY_TAG6 && g_send.aData[1] == 6u,
          "Tag6 payload");
    memset(&g_send, 0, sizeof g_send);
    BrDPlaySendTag7(&link, 7u);
    CHECK(g_send.aData[0] == BR_DPLAY_TAG7 && g_send.aData[1] == 7u,
          "Tag7 payload");

    /* Tag 3 carries no value; it still sends 8 bytes. */
    memset(&g_send, 0, sizeof g_send);
    BrDPlaySendTag3(&link, 0);
    CHECK(g_send.cbData == 8u, "Tag3: must still send 8 bytes");
    CHECK(g_send.aData[0] == BR_DPLAY_TAG3, "Tag3: wrong tag");
    CHECK(g_send.aData[1] == 0u, "Tag3: second dword must be 0 (DEVIATION)");

    /* Null guards: no send, result 0. */
    memset(&g_send, 0, sizeof g_send);
    CHECK(BrDPlaySendTag2(NULL, 0, 1u) == 0, "Tag2(NULL) must return 0");
    dead = link; dead.pIface = NULL;
    CHECK(BrDPlaySendTag2(&dead, 0, 1u) == 0, "Tag2(no iface) must return 0");
    CHECK(BrDPlaySendTag8(&dead, 1u, 2u) == 0, "Tag8(no iface) must return 0");
    CHECK(BrDPlaySendTag6(NULL, 1u) == 0, "Tag6(NULL) must return 0");
    CHECK(g_send.nCalls == 0, "null guards must not transmit");

    /* THE ASYMMETRY: the gate suppresses 2/3/4/5 but not 6/7/8. */
    memset(&g_send, 0, sizeof g_send);
    CHECK(BrDPlaySendTag2(&link, 1, 1u) == 0, "Tag2 must be gated");
    CHECK(BrDPlaySendTag3(&link, 1) == 0, "Tag3 must be gated");
    CHECK(BrDPlaySendTag4(&link, 1, 1u) == 0, "Tag4 must be gated");
    CHECK(BrDPlaySendTag5(&link, 1, 1u) == 0, "Tag5 must be gated");
    CHECK(g_send.nCalls == 0, "gate must suppress tags 2/3/4/5");
    BrDPlaySendTag6(&link, 1u);
    BrDPlaySendTag7(&link, 1u);
    BrDPlaySendTag8(&link, 1u, 2u);
    CHECK(g_send.nCalls == 3, "tags 6/7/8 must ignore the gate");

    /* Tag6Self: DPID 0 means "no player". */
    memset(&g_send, 0, sizeof g_send);
    link.f08 = 0;
    BrDPlaySendTag6Self(&link);
    CHECK(g_send.nCalls == 0, "Tag6Self must not send with DPID 0");
    BrDPlaySendTag6Self(NULL);
    CHECK(g_send.nCalls == 0, "Tag6Self(NULL) must not send");
    link.f08 = 0x99u;
    BrDPlaySendTag6Self(&link);
    CHECK(g_send.nCalls == 1, "Tag6Self must send");
    CHECK(g_send.aData[0] == BR_DPLAY_TAG6 && g_send.aData[1] == 0x99u,
          "Tag6Self must carry our own DPID as the value");
    CHECK(g_send.idFrom == 0x99u, "Tag6Self: idFrom must also be the DPID");

    /* Tag 8: twelve bytes, three dwords, and the local side channel. */
    link_init(&link);
    memset(&g_send, 0, sizeof g_send);
    memset(&g_local, 0, sizeof g_local);
    rc = BrDPlaySendTag8(&link, 0xAAu, 0xBBu);
    CHECK(rc == 0x2000, "Tag8 rc");
    CHECK(g_send.cbData == 12u, "Tag8: payload must be 12 bytes");
    CHECK(g_send.aData[0] == BR_DPLAY_TAG8, "Tag8: wrong tag");
    CHECK(g_send.aData[1] == 0xAAu && g_send.aData[2] == 0xBBu,
          "Tag8: wrong operands");
    CHECK(g_local.nCalls == 0, "Tag8: f0C == 0 must not deliver locally");

    memset(&g_send, 0, sizeof g_send);
    memset(&g_local, 0, sizeof g_local);
    link.f0C = 1;
    BrDPlaySendTag8(&link, 0xCCu, 0xDDu);
    CHECK(g_local.nCalls == 1, "Tag8: f0C != 0 must deliver locally too");
    CHECK(g_send.nCalls == 1, "Tag8: local delivery must not replace the send");
    CHECK(g_local.cbData == 12u, "Tag8 local: size");
    CHECK(g_local.idFrom == link.f08, "Tag8 local: idFrom");
    CHECK(g_local.a5 == 1, "Tag8 local: trailing argument is a literal 1");
    CHECK(memcmp(g_local.aData, g_send.aData, 12) == 0,
          "Tag8: local and remote payloads must be identical");
}

/* =====================================================================
 * 5. 0x1003CE80 tail fragment
 * ===================================================================== */

static void test_advance(void)
{
    BrOptCaps caps;
    int32_t idx;

    memset(&caps, 0, sizeof caps);

    /* An already-available index is left untouched. */
    caps.maskB = (1u << 5) | (1u << 9);
    idx = 5;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 5, "Advance: available start must not move");

    /* Otherwise walk forward to the next available one. */
    idx = 6;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 9, "Advance: must stop at the next available index");

    /* Wrap 31 -> 0 (and prove 31 itself is a legal index, not the wrap). */
    caps.maskB = (1u << 0) | (1u << 31);
    idx = 31;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 31, "Advance: 31 must be a legal, testable index");
    idx = 30;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 31, "Advance: must reach 31 before wrapping");
    caps.maskB = (1u << 0);
    idx = 20;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 0, "Advance: must wrap past 31 to 0");

    /* Nothing available anywhere: the index comes back to where it started,
     * with no failure signal. */
    caps.maskB = 0;
    idx = 13;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 13, "Advance: exhausted search must land back on start");

    /* Only the start is available, but the start is tested first, so it is
     * returned rather than searched past. */
    caps.maskB = (1u << 4);
    idx = 4;
    BrDPlayAdvanceAvail(&caps, &idx);
    CHECK(idx == 4, "Advance: sole available start must be kept");

    /* Sweeping every start position must always land on an available index
     * whenever at least one exists. */
    {
        int start;
        caps.maskB = (1u << 2) | (1u << 17) | (1u << 29);
        for (start = 0; start <= 31; ++start) {
            idx = start;
            BrDPlayAdvanceAvail(&caps, &idx);
            CHECK(idx >= 0 && idx <= 31, "Advance: index out of range");
            CHECK(((caps.maskB >> idx) & 1u) != 0,
                  "Advance: landed on an unavailable index");
        }
    }
}

int main(void)
{
    test_rand();
    test_sp_classify();
    test_sp_store();
    test_slots();
    test_senders();
    test_advance();

    printf("slice2_22: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails != 0;
}
