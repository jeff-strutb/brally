/* test_slice3_42.c -- behaviour/invariant tests for slice3_42.
 *
 * Everything asserted here is either a mathematical identity, a round trip,
 * or a boundary that is literally present in the original's control flow.
 * The three "GOTCHA" behaviours the header calls out (Z never summing the
 * children, the stale force vector, the 0x8000 asymmetry in the byte getter)
 * are pinned deliberately: if a later cleanup "fixes" one of them, this file
 * should fail.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "slice3_42.h"

static int g_fail;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            ++g_fail;                                                        \
        }                                                                    \
    } while (0)

#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-5)

/* =====================================================================
 * Stand-ins for everything outside this translation unit.
 * CLEARLY MARKED: none of these is decompiled code.  The two matrix helpers
 * follow br_mat.h / slice3_44.h exactly; the four car-state helpers are
 * deliberately minimal and only need to be self-consistent.
 * ===================================================================== */

void BrMat4MulVec3(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    const float v[3] = { pV->x, pV->y, pV->z };
    float o[3];
    int i, k;
    for (i = 0; i < 3; ++i) {
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] += pM->m[i][k] * v[k];
    }
    pOut->x = o[0]; pOut->y = o[1]; pOut->z = o[2];
}

void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    const float v[3] = { pV->x, pV->y, pV->z };
    float o[3];
    int i, k;
    for (i = 0; i < 3; ++i) {
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] += pM->m[k][i] * v[k];
    }
    pOut->x = o[0]; pOut->y = o[1]; pOut->z = o[2];
}

void BrMat3MulVec3(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{
    const float v[3] = { pV->x, pV->y, pV->z };
    float o[3];
    int i, k;
    for (i = 0; i < 3; ++i) {
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] += pM->m[3 * i + k] * v[k];
    }
    pOut->x = o[0]; pOut->y = o[1]; pOut->z = o[2];
}

/* A minimal wire form: f00 and the position triple, 16 of the 22 bytes.
 * f78 is deliberately NOT carried, matching what slice2_12.h documents about
 * the real 0x10007730. */
void BrCarStatePack(BrCarPacked *pDst, const BrCarState *pSrc)
{
    memset(pDst, 0, sizeof *pDst);
    memcpy(pDst->b +  0, &pSrc->f00, 4);
    memcpy(pDst->b +  4, &pSrc->f10, 4);
    memcpy(pDst->b +  8, &pSrc->f14, 4);
    memcpy(pDst->b + 12, &pSrc->f18, 4);
}

void BrCarStateUnpack(BrCarState *pDst, const BrCarPacked *pSrc)
{
    memcpy(&pDst->f00, pSrc->b +  0, 4);
    memcpy(&pDst->f10, pSrc->b +  4, 4);
    memcpy(&pDst->f14, pSrc->b +  8, 4);
    memcpy(&pDst->f18, pSrc->b + 12, 4);
}

/* A car record is just bytes here; these two move the three position floats
 * between +0x1DC and BrCarState.f10..f18, and f78 <-> +0x0FF4. */
#define TCAR_F32(p, off) (*(float *)(void *)((unsigned char *)(p) + (off)))

void BrCarRecordToState(BrCarState *pDst, void *pCar)
{
    memset(pDst, 0, sizeof *pDst);
    pDst->f10 = TCAR_F32(pCar, 0x1DC);
    pDst->f14 = TCAR_F32(pCar, 0x1E0);
    pDst->f18 = TCAR_F32(pCar, 0x1E4);
    pDst->f00 = 1.0f;
}

void BrCarRecordFromState(void *pCar, const BrCarState *pSrc)
{
    TCAR_F32(pCar, 0x1DC) = pSrc->f10;
    TCAR_F32(pCar, 0x1E0) = pSrc->f14;
    TCAR_F32(pCar, 0x1E4) = pSrc->f18;
    TCAR_F32(pCar, 0x0FF4) = pSrc->f78;
}

/* =====================================================================
 * 1. BrMat4FromCarState (0x100695D0)
 * ===================================================================== */

static void QuatState(BrCarState *pS, float w, float x, float y, float z)
{
    memset(pS, 0, sizeof *pS);
    pS->f00 = w; pS->f04 = x; pS->f08 = y; pS->f0C = z;
}

/* v' = v * M -- the row-vector convention the whole project uses. */
static void RowMul(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM)
{
    const float v[3] = { pV->x, pV->y, pV->z };
    float o[3];
    int i, k;
    for (i = 0; i < 3; ++i) {
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] += v[k] * pM->m[k][i];
    }
    pOut->x = o[0]; pOut->y = o[1]; pOut->z = o[2];
}

static void test_mat4_from_carstate(void)
{
    BrCarState s;
    BrMat4     m, m2;
    BrVec3     v, r;
    int        i, j;
    const float rt = (float)(sqrt(2.0) / 2.0);

    /* Identity quaternion + translation. */
    QuatState(&s, 1.0f, 0.0f, 0.0f, 0.0f);
    s.f10 = 3.0f; s.f14 = -4.0f; s.f18 = 0.5f;
    BrMat4FromCarState(&m, &s);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            CHECK(m.m[i][j] == (i == j ? 1.0f : 0.0f));
    CHECK(m.m[3][0] == 3.0f && m.m[3][1] == -4.0f && m.m[3][2] == 0.5f);
    CHECK(m.m[0][3] == 0.0f && m.m[1][3] == 0.0f && m.m[2][3] == 0.0f);
    CHECK(m.m[3][3] == 1.0f);

    /* 90 degrees about +Z, applied as a ROW vector: x -> y. */
    QuatState(&s, rt, 0.0f, 0.0f, rt);
    BrMat4FromCarState(&m, &s);
    v.x = 1.0f; v.y = 0.0f; v.z = 0.0f;
    RowMul(&r, &v, &m);
    CHECK(NEAR(r.x, 0.0) && NEAR(r.y, 1.0) && NEAR(r.z, 0.0));

    /* The 2/norm scale means a non-unit quaternion gives the SAME rotation.
     * This is the property that distinguishes 0x100695D0 from
     * BrRbBuildMatrix (0x10074450), which does not divide by the norm. */
    QuatState(&s, 3.0f * rt, 0.0f, 0.0f, 3.0f * rt);
    BrMat4FromCarState(&m2, &s);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            CHECK(NEAR(m.m[i][j], m2.m[i][j]));

    /* Rotation matrices preserve length, for an arbitrary quaternion. */
    QuatState(&s, 0.3f, -0.7f, 0.2f, 0.9f);
    BrMat4FromCarState(&m, &s);
    v.x = 1.0f; v.y = 2.0f; v.z = -3.0f;
    RowMul(&r, &v, &m);
    CHECK(NEAR(r.x * r.x + r.y * r.y + r.z * r.z, 1.0 + 4.0 + 9.0));

    /* The rows are mutually orthogonal, i.e. it really is a rotation. */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            const double d = (double)m.m[i][0] * m.m[j][0]
                           + (double)m.m[i][1] * m.m[j][1]
                           + (double)m.m[i][2] * m.m[j][2];
            CHECK(fabs(d - (i == j ? 1.0 : 0.0)) < 1e-5);
        }
    }

    /* Boundary: the explicit norm == 0 guard yields s = 0, i.e. identity. */
    QuatState(&s, 0.0f, 0.0f, 0.0f, 0.0f);
    s.f10 = 7.0f;
    BrMat4FromCarState(&m, &s);
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            CHECK(m.m[i][j] == (i == j ? 1.0f : 0.0f));
    CHECK(m.m[3][0] == 7.0f);
}

/* =====================================================================
 * 2. Control bindings
 * ===================================================================== */

static void test_ctrl(void)
{
    BrCtrlCfg a, b;
    int p, i;
    uint16_t v;

    BrCtrlCfgInit(&a);
    CHECK(a.active == 0);
    CHECK(a.pActive == &a.profile[0]);
    CHECK(a.f7B8 == 640 && a.f7BC == 480 && a.f870 == 1);
    for (p = 0; p < BR_CTRL_PROFILES; ++p)
        CHECK(memcmp(&a.profile[p], &g_BrCtrlDefaults[p],
                     sizeof(BrCtrlProfile)) == 0);

    /* The whole point of 0x10069DE0: pActive is rebuilt, not copied. */
    a.active = 2;
    a.pActive = &a.profile[2];
    a.f3B8[7] = 0xDEADBEEFu;
    memset(&b, 0xAA, sizeof b);
    CHECK(BrCtrlCfgCopy(&b, &a) == &b);
    CHECK(b.active == 2);
    CHECK(b.pActive == &b.profile[2]);
    CHECK(b.f3B8[7] == 0xDEADBEEFu);
    CHECK(memcmp(b.profile, a.profile, sizeof b.profile) == 0);

    /* Out-of-range selectors all fall through to profile 0. */
    BrCtrlCfgInit(&a);
    a.profile[0].e[3][0] = 0x1234;
    BrCtrlCfgLoadDefaults(&a, 99);
    CHECK(a.profile[0].e[3][0] == g_BrCtrlDefaults[0].e[3][0]);
    a.profile[0].e[3][0] = 0x1234;
    BrCtrlCfgLoadDefaults(&a, -1);
    CHECK(a.profile[0].e[3][0] == g_BrCtrlDefaults[0].e[3][0]);
    a.profile[0].e[3][0] = 0x1234;
    BrCtrlCfgLoadDefaults(&a, 0);
    CHECK(a.profile[0].e[3][0] == g_BrCtrlDefaults[0].e[3][0]);

    /* Assign: only hi's byte 1 and lo's byte 0 survive. */
    BrCtrlCfgInit(&a);
    BrCtrlCfgAssign(&a, 0, 4, 0x7EAB12, 0x9955CD);
    CHECK(a.profile[0].e[4][0] == 0xABCD);

    /* Invariant: after an assign, no NON-ZERO alternate of that action may
     * equal any slot-0 value in the profile. */
    BrCtrlCfgInit(&a);
    BrCtrlCfgAssign(&a, 0, 0, 0x0000, 0x004B);
    CHECK(a.profile[0].e[0][0] == 0x004B);
    for (i = 1; i <= 2; ++i) {
        int j;
        v = a.profile[0].e[0][i];
        if (v == 0)
            continue;
        for (j = 0; j < BR_CTRL_ACTIONS; ++j)
            CHECK(a.profile[0].e[j][0] != v);
    }
    /* Concretely: default alternate 1 was 0x004B, which now collides. */
    CHECK(g_BrCtrlDefaults[0].e[0][1] == 0x004B);
    CHECK(a.profile[0].e[0][1] == 0x0000);
    /* ...and alternate 2 (0x00CB) no longer collides, so it survives. */
    CHECK(g_BrCtrlDefaults[0].e[0][2] == 0x00CB);
    CHECK(a.profile[0].e[0][2] == 0x00CB);

    /* Assigning is idempotent: doing it twice changes nothing. */
    {
        BrCtrlCfg c = a;
        BrCtrlCfgAssign(&a, 0, 0, 0x0000, 0x004B);
        CHECK(memcmp(a.profile, c.profile, sizeof a.profile) == 0);
    }

    /* Getters.  BrFn10069BC0 never shifts; BrFn10069C30 shifts only for
     * profiles 1..3 and only above 0x8000. */
    BrCtrlCfgInit(&a);
    CHECK(g_BrCtrlDefaults[1].e[0][0] == 0x8000);
    CHECK(BrFn10069BC0(&a, 1, 0) == 0x8000);
    CHECK(BrFn10069C30(&a, 1, 0) == 0x80);

    CHECK(g_BrCtrlDefaults[1].e[5][0] == 0x0100);
    CHECK(BrFn10069BC0(&a, 1, 5) == 0x0100);
    CHECK(BrFn10069C30(&a, 1, 5) == 0x00);

    CHECK(g_BrCtrlDefaults[0].e[0][0] == 0x00CB);
    CHECK(BrFn10069BC0(&a, 0, 0) == 0x0000);
    CHECK(BrFn10069C30(&a, 0, 0) == 0xCB);

    /* The asymmetry: the same 0x8000 entry read through profile 0 (or any
     * out-of-range selector) yields the LOW byte, because that arm has no
     * 0x8000 test at all. */
    a.profile[0].e[6][0] = 0x8042;
    CHECK(BrFn10069C30(&a, 0,  6) == 0x42);
    CHECK(BrFn10069C30(&a, 77, 6) == 0x42);
    a.profile[1].e[6][0] = 0x8042;
    CHECK(BrFn10069C30(&a, 1,  6) == 0x80);
}

/* =====================================================================
 * 3. Replay recorder
 * ===================================================================== */

static void ResetReplayGlobals(void)
{
    memset(g_BrReplayCount, 0, sizeof g_BrReplayCount);
    memset(g_BrReplayCursor, 0, sizeof g_BrReplayCursor);
    g_BrReplayOn  = 0;
    g_BrX0AA010   = 0;
    g_BrX06909B4  = 0;
    g_BrX06909E0  = 0;
    g_BrX18ABAD0  = 0;
}

static void test_replay(void)
{
    static unsigned char car[0x2B68];
    int i;

    /* --- Reset: the 2/4 session kinds only clear ONE count. ----------- */
    ResetReplayGlobals();
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCount[i] = 5;
    g_BrX0AA010 = 0;
    BrReplayReset();
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        CHECK(g_BrReplayCount[i] == 0);

    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCount[i] = 5;
    g_BrX0AA010 = 4;
    BrReplayReset();
    CHECK(g_BrReplayCount[0] == 0);
    for (i = 1; i < BR_REPLAY_PLAYERS; ++i)
        CHECK(g_BrReplayCount[i] == 5);
    CHECK(g_BrReplayOn == 0);

    /* --- Advance: saturates at BR_REPLAY_FRAMES, and is gated. -------- */
    ResetReplayGlobals();
    g_BrReplayOn = 0;
    g_BrReplayCount[0] = 3;
    BrReplayAdvance();
    CHECK(g_BrReplayCount[0] == 3);        /* recording off -> no-op */

    g_BrReplayOn = 1;
    g_BrX06909B4 = 1;
    BrReplayAdvance();
    CHECK(g_BrReplayCount[0] == 3);        /* suspended -> no-op */

    g_BrX06909B4 = 0;
    BrReplayAdvance();
    CHECK(g_BrReplayCount[0] == 4);
    g_BrReplayCount[0] = BR_REPLAY_FRAMES;
    BrReplayAdvance();
    CHECK(g_BrReplayCount[0] == BR_REPLAY_FRAMES);

    /* --- Record then apply: a full round trip through the ring. ------- */
    ResetReplayGlobals();
    memset(car, 0, sizeof car);
    *(int32_t *)(void *)(car + BR_S42_CAR_OFF_INDEX) = 0;
    g_BrReplayOn = 1;

    for (i = 0; i < 4; ++i) {
        TCAR_F32(car, 0x1DC) = (float)(10 * i);
        TCAR_F32(car, 0x1E0) = (float)(i);
        TCAR_F32(car, 0x1E4) = 0.0f;
        BrReplayRecord(car);
        BrReplayAdvance();
    }
    CHECK(g_BrReplayCount[0] == 4);

    /* Frame 1 replays position 10 and, because 1 < 4-2, also gets the
     * finite-difference velocity (20-10)*30 = 300. */
    g_BrReplayCursor[0] = 1;
    memset(car, 0, sizeof car);
    *(int32_t *)(void *)(car + BR_S42_CAR_OFF_INDEX) = 0;
    BrReplayApplyCar(car);
    CHECK(TCAR_F32(car, 0x1DC) == 10.0f);
    CHECK(TCAR_F32(car, 0x1E0) == 1.0f);
    CHECK(TCAR_F32(car, 0x1E8) == 300.0f);
    CHECK(TCAR_F32(car, 0x1EC) == 30.0f);

    /* Boundary: cursor == count-2 does NOT get a velocity (the test is a
     * strict `<`), so the old value survives. */
    g_BrReplayCursor[0] = 2;
    TCAR_F32(car, 0x1E8) = -1.0f;
    BrReplayApplyCar(car);
    CHECK(TCAR_F32(car, 0x1DC) == 20.0f);
    CHECK(TCAR_F32(car, 0x1E8) == -1.0f);

    /* g_BrX06909E0 == 2 clears the nine flag bytes, and only those nine. */
    g_BrX06909E0 = 2;
    for (i = 0x360; i < 0x370; ++i)
        car[i] = 0x77;
    g_BrReplayCursor[0] = 0;
    BrReplayApplyCar(car);
    CHECK(car[0x360] == 0x77 && car[0x361] == 0x77);
    CHECK(car[0x362] == 0 && car[0x363] == 0);
    CHECK(car[0x364] == 0x77 && car[0x365] == 0x77);   /* skipped */
    for (i = 0x366; i <= 0x36C; ++i)
        CHECK(car[i] == 0);
    CHECK(car[0x36D] == 0x77);
    g_BrX06909E0 = 0;

    /* Recording is gated the same way Advance is. */
    ResetReplayGlobals();
    g_BrReplayOn = 0;
    memset(&g_BrReplayBuf[0], 0x5A, sizeof g_BrReplayBuf[0]);
    BrReplayRecord(car);
    CHECK(g_BrReplayBuf[0].rec.b[0] == 0x5A);

    /* --- Seek: clamps, and the big step overrides the small one. ------ */
    ResetReplayGlobals();
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCount[i] = 100;
    g_BrReplayCursor[0] = 50;
    g_BrX18ABAD0 = 0x00200000u | 0x00800000u;   /* +1 and +10 together */
    BrReplaySeek();
    CHECK(g_BrReplayCursor[0] == 60);
    CHECK(g_BrX06909E0 == 3);

    g_BrReplayCursor[0] = 2;
    g_BrX18ABAD0 = 0x01000000u;                 /* -10 */
    BrReplaySeek();
    CHECK(g_BrReplayCursor[0] == 0);            /* clamped low */

    g_BrReplayCursor[0] = 95;
    g_BrX18ABAD0 = 0x00800000u;                 /* +10 */
    BrReplaySeek();
    CHECK(g_BrReplayCursor[0] == 99);           /* clamped to count-1 */

    /* 0x100000 forces state 1, which forces the step to exactly +1. */
    g_BrReplayCursor[0] = 10;
    g_BrX18ABAD0 = 0x00100000u | 0x00800000u;
    BrReplaySeek();
    CHECK(g_BrX06909E0 == 1);
    CHECK(g_BrReplayCursor[0] == 11);

    /* No bits at all with state 3 demotes to 2 and moves nothing. */
    g_BrX06909E0 = 3;
    g_BrReplayCursor[0] = 10;
    g_BrX18ABAD0 = 0;
    BrReplaySeek();
    CHECK(g_BrX06909E0 == 2);
    CHECK(g_BrReplayCursor[0] == 10);

    /* Rewind zeroes every cursor. */
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        g_BrReplayCursor[i] = 7;
    BrReplayRewind();
    for (i = 0; i < BR_REPLAY_PLAYERS; ++i)
        CHECK(g_BrReplayCursor[i] == 0);
}

/* =====================================================================
 * 4. BrFxClearAll
 * ===================================================================== */

static void test_fx_clear(void)
{
    int i;

    for (i = 0; i < BR_FX_RECORDS; ++i) {
        g_BrFx1750338[i].f00 = 1; g_BrFx1750338[i].f04 = 2;
        g_BrFx1750338[i].f08 = 3; g_BrFx1750338[i].f0C = 4;
        g_BrFx1750338[i].f10 = 5; g_BrFx1750338[i].f14 = 6;
        g_BrFx1750338[i].f18 = 7; g_BrFx1750338[i].f1C = 0xABCD;
    }
    for (i = 0; i < BR_FX_PAIRS; ++i) {
        g_BrFx1754E50[i][0] = 9;
        g_BrFx1754E50[i][1] = 9;
    }
    g_BrX1754E38 = 1; g_BrX17554A0 = 1; g_BrX17554E4 = 1;

    BrFxClearAll();

    for (i = 0; i < BR_FX_RECORDS; ++i) {
        CHECK(g_BrFx1750338[i].f00 == 0 && g_BrFx1750338[i].f18 == 0);
        /* The seven-store / eight-step mismatch: f1C survives. */
        CHECK(g_BrFx1750338[i].f1C == 0xABCD);
    }
    for (i = 0; i < BR_FX_PAIRS; ++i)
        CHECK(g_BrFx1754E50[i][0] == 0 && g_BrFx1754E50[i][1] == 0);
    CHECK(g_BrX1754E38 == 0 && g_BrX17554A0 == 0 && g_BrX17554E4 == 0);
}

/* =====================================================================
 * 5. Rigid body
 * ===================================================================== */

static void IdentityBody(BrRbBodyFull *pB)
{
    int i, j;
    memset(pB, 0, sizeof *pB);
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            pB->m.m[i][j] = (i == j) ? 1.0f : 0.0f;
    pB->mass = 1.0f;
    for (i = 0; i < 9; ++i)
        pB->invInertia.m[i] = (i % 4 == 0) ? 1.0f : 0.0f;
}

static void test_rb_velocity(void)
{
    BrRbBodyFull b, at;
    BrVec3 p, out, out2;

    IdentityBody(&b);
    b.vel.x = 1.0f; b.vel.y = 2.0f; b.vel.z = 3.0f;

    /* No rotation -> the point does not matter. */
    p.x = 100.0f; p.y = -5.0f; p.z = 7.0f;
    BrRbVelAtPoint(&out, &b, &p);
    CHECK(out.x == 1.0f && out.y == 2.0f && out.z == 3.0f);

    /* omega x r, with omega = +Z and r = +X, is +Y. */
    b.vel.x = 0.0f; b.vel.y = 0.0f; b.vel.z = 0.0f;
    b.angVel.z = 1.0f;
    p.x = 1.0f; p.y = 0.0f; p.z = 0.0f;
    BrRbVelAtPoint(&out, &b, &p);
    CHECK(NEAR(out.x, 0.0) && NEAR(out.y, 1.0) && NEAR(out.z, 0.0));

    /* The point-source variant reads pAt->f78, nothing else. */
    IdentityBody(&at);
    at.f78.x = 1.0f; at.f78.y = 0.0f; at.f78.z = 99.0f;
    BrRbVelAtBodyPoint(&out2, &b, &at);
    CHECK(NEAR(out2.x, 0.0) && NEAR(out2.y, 1.0) && NEAR(out2.z, 0.0));

    /* The XY variant drops Z from the point.  With omega = +X the two
     * variants disagree exactly because of that. */
    b.angVel.z = 0.0f;
    b.angVel.x = 1.0f;
    BrRbVelAtBodyPoint(&out, &b, &at);      /* r = (1,0,99) -> (0,-99,0) */
    CHECK(NEAR(out.y, -99.0));
    BrRbVelAtBodyPointXY(&out2, &b, &at);   /* r = (1,0,0)  -> (0,0,0)   */
    CHECK(NEAR(out2.x, 0.0) && NEAR(out2.y, 0.0) && NEAR(out2.z, 0.0));

    /* With identity M the final rotation in the XY variant is a no-op, so it
     * must agree with feeding the z-flattened point to BrRbVelAtPoint. */
    p.x = at.f78.x; p.y = at.f78.y; p.z = 0.0f;
    BrRbVelAtPoint(&out, &b, &p);
    CHECK(NEAR(out.x, out2.x) && NEAR(out.y, out2.y) && NEAR(out.z, out2.z));
}

static void test_rb_forces(void)
{
    BrRbBodyFull b;
    BrRbForce    n0, n1;

    /* --- own forces ---------------------------------------------------- */
    IdentityBody(&b);
    memset(&n0, 0, sizeof n0);
    n0.kind = 0;
    n0.f.x = 5.0f;
    n0.r.y = 1.0f;               /* r x f = (0,1,0) x (5,0,0) = (0,0,-5) */
    b.pForces = &n0;

    b.mode = 0;
    BrRbAccumOwnForces(&b);
    CHECK(b.accel.x == 5.0f);
    CHECK(NEAR(b.angAccel.z, -5.0));

    /* mode 2 suppresses the torque leg entirely. */
    IdentityBody(&b);
    b.pForces = &n0;
    b.mode = 2;
    BrRbAccumOwnForces(&b);
    CHECK(b.accel.x == 5.0f);
    CHECK(b.angAccel.x == 0.0f && b.angAccel.y == 0.0f &&
          b.angAccel.z == 0.0f);

    /* A kind >= 2 node re-uses the PREVIOUS node's vector.  This is the
     * documented stale-slot behaviour; the port makes the first node's value
     * zero instead of uninitialised, but the carry-over is real. */
    IdentityBody(&b);
    memset(&n1, 0, sizeof n1);
    n1.kind = 7;                 /* neither 0 nor 1 */
    n0.pNext = &n1;
    b.pForces = &n0;
    b.mode = 2;
    BrRbAccumOwnForces(&b);
    CHECK(b.accel.x == 10.0f);   /* 5 from n0, 5 again from the stale slot */
    n0.pNext = NULL;

    /* --- child forces -------------------------------------------------- */
    {
        BrRbBodyFull parent, child;
        IdentityBody(&parent);
        IdentityBody(&child);
        memset(&n0, 0, sizeof n0);
        n0.kind = 1;             /* raw, here -- the opposite of above */
        n0.f.x = 4.0f;
        child.pForces = &n0;
        child.m.m[3][0] = 0.0f;
        child.m.m[3][1] = 2.0f;  /* lever arm +2Y */
        child.f1B4 = 0.0f;

        BrRbAccumChildForces(&parent, &child);
        CHECK(child.accel.x == 4.0f);
        CHECK(parent.angAccel.z == 0.0f);   /* f1B4 == 0 -> no torque */

        child.accel.x = 0.0f;
        child.f1B4 = 1.0f;
        BrRbAccumChildForces(&parent, &child);
        CHECK(child.accel.x == 4.0f);
        /* (0,2,0) x (4,0,0) = (0,0,-8) */
        CHECK(NEAR(parent.angAccel.z, -8.0));
    }
}

static void test_rb_solve(void)
{
    BrRbBodyFull b, c[4];
    int k;

    IdentityBody(&b);
    for (k = 0; k < 4; ++k) {
        IdentityBody(&c[k]);
        b.child[k] = &c[k];
        c[k].accel.x = 1.0f;
        c[k].accel.y = 10.0f;
        c[k].accel.z = 100.0f;   /* must NOT reach the result */
    }
    b.mass = 2.0f;
    b.accel.x = 8.0f;
    b.accel.y = 8.0f;
    b.accel.z = 8.0f;

    BrRbSolveAccel(&b);

    /* x: (1+1+1+1 + 8)/2 = 6 ; y: (10*4 + 8)/2 = 24 ;
     * z: 8/2 = 4  -- the children's 400 is dropped.  That asymmetry is the
     * point of this test. */
    CHECK(NEAR(b.accel.x, 6.0));
    CHECK(NEAR(b.accel.y, 24.0));
    CHECK(NEAR(b.accel.z, 4.0));

    /* Identity inertia and identity M leave the angular part alone. */
    IdentityBody(&b);
    for (k = 0; k < 4; ++k) {
        IdentityBody(&c[k]);
        b.child[k] = &c[k];
    }
    b.angAccel.x = 3.0f; b.angAccel.y = -1.0f; b.angAccel.z = 0.5f;
    BrRbSolveAccel(&b);
    CHECK(NEAR(b.angAccel.x, 3.0) && NEAR(b.angAccel.y, -1.0) &&
          NEAR(b.angAccel.z, 0.5));
}

static void test_rb_accum_all(void)
{
    BrRbBodyFull b, c[4];
    BrRbForce    n;
    int k;

    IdentityBody(&b);
    for (k = 0; k < 4; ++k) {
        IdentityBody(&c[k]);
        b.child[k] = &c[k];
        c[k].accel.x = 999.0f;      /* must be cleared */
        c[k].angAccel.x = 42.0f;    /* must NOT be cleared */
    }
    memset(&n, 0, sizeof n);
    n.kind = 0;
    n.f.x = 6.0f;
    b.pForces = &n;
    b.mode = 2;
    b.mass = 3.0f;
    b.accel.y = 123.0f;             /* must be cleared before accumulating */

    BrRbAccumAll(&b);

    for (k = 0; k < 4; ++k) {
        CHECK(c[k].accel.x == 0.0f);
        CHECK(c[k].angAccel.x == 42.0f);
    }
    CHECK(NEAR(b.accel.x, 2.0));    /* 6 / 3 */
    CHECK(NEAR(b.accel.y, 0.0));
}

int main(void)
{
    test_mat4_from_carstate();
    test_ctrl();
    test_replay();
    test_fx_clear();
    test_rb_velocity();
    test_rb_forces();
    test_rb_solve();
    test_rb_accum_all();

    if (g_fail) {
        printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    printf("slice3_42: all checks passed\n");
    return 0;
}
