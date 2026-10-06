/* test_slice1_08.c -- behaviour tests for the pass-08 slice.
 *
 * The DirectSound side is exercised against a mock buffer whose vtable has
 * the same shape as the real one, so what is being checked is the call
 * SEQUENCE, the ARGUMENT VALUES the engine computes, and the error handling
 * -- which is all that was recovered from the original.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice1_08.h"

static int g_fail = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                               \
    do {                                                                    \
        double da_ = (double)(a), db_ = (double)(b);                        \
        if (!(fabs(da_ - db_) <= (eps))) {                                  \
            printf("FAIL %s:%d: %s (%g) !~ %s (%g)\n", __FILE__, __LINE__,  \
                   #a, da_, #b, db_);                                       \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

/* ================================================================== */
/* 0x1006C9A0                                                          */
/* ================================================================== */

static float dot3(const BrVec3 *a, const BrVec3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static void test_plane_eval(void)
{
    BrVec3 n  = { 0.267261f, 0.534522f, 0.801784f };  /* (1,2,3)/|.| */
    BrVec3 p0 = { 4.0f, -1.0f, 0.5f };
    BrVec3 p  = { -2.0f, 7.0f, 3.25f };
    BrVec3 q  = { 0.0f, 0.0f, 0.0f };
    BrVec3 pq;
    float  d  = -dot3(&n, &p0);

    /* a point on the plane evaluates to zero */
    CHECK_NEAR(BrPlaneEval(&n, d, &p0), 0.0, 1e-5);

    /* the value is the signed distance along the (unit) normal */
    pq.x = p.x - p0.x; pq.y = p.y - p0.y; pq.z = p.z - p0.z;
    CHECK_NEAR(BrPlaneEval(&n, d, &p), dot3(&n, &pq), 1e-4);

    /* differences do not depend on d at all */
    CHECK_NEAR(BrPlaneEval(&n, d, &p) - BrPlaneEval(&n, d, &q),
               BrPlaneEval(&n, 100.0f, &p) - BrPlaneEval(&n, 100.0f, &q),
               1e-4);

    /* shifting d shifts the result by exactly that much */
    CHECK_NEAR(BrPlaneEval(&n, d + 2.5f, &p) - BrPlaneEval(&n, d, &p),
               2.5, 1e-4);

    /* the origin evaluates to d */
    CHECK_NEAR(BrPlaneEval(&n, d, &q), d, 1e-5);
}

/* ================================================================== */
/* mock IDirectSoundBuffer                                             */
/* ================================================================== */

typedef struct Mock {
    BrDSBuffer base;

    int      nRelease, nStop, nPlay, nSetPos, nGetStatus, nGetCaps;
    int      nLock, nUnlock, nSetPan, nSetVolume, nSetFreq;

    int32_t  lastPan, lastVolume;
    uint32_t lastFreq, lastPlayFlags, lastUnlockBytes;

    uint32_t status;      /* what GetStatus reports */
    uint32_t capsFlags;   /* what GetCaps reports */

    int32_t  hrLock, hrSetVolume, hrSetPan, hrGetCaps;
    int32_t  hrGetStatus, hrPlay, hrSetPos, hrStop;
    int32_t  hrUnlock[2]; /* per-call script; index by nUnlock */

    unsigned char storage[64];
} Mock;

static int32_t mk_release(BrDSBuffer *p)
{ ((Mock *)p)->nRelease++; return 0; }

static int32_t mk_getcaps(BrDSBuffer *p, BrDSBCaps *c)
{
    Mock *m = (Mock *)p;
    m->nGetCaps++;
    /* the engine must have set dwSize before calling */
    if (c->dwSize != 0x14u) { printf("FAIL: DSBCAPS dwSize %u\n", c->dwSize); ++g_fail; }
    c->dwFlags = m->capsFlags;
    return m->hrGetCaps;
}

static int32_t mk_getstatus(BrDSBuffer *p, uint32_t *s)
{
    Mock *m = (Mock *)p;
    m->nGetStatus++;
    *s = m->status;
    return m->hrGetStatus;
}

static int32_t mk_lock(BrDSBuffer *p, uint32_t off, uint32_t bytes,
                       void **pp1, uint32_t *pn1, void **pp2, uint32_t *pn2,
                       uint32_t flags)
{
    Mock *m = (Mock *)p;
    m->nLock++;
    (void)off; (void)flags;
    if (bytes > sizeof(m->storage)) { printf("FAIL: lock too big\n"); ++g_fail; }
    *pp1 = m->storage;
    *pn1 = bytes;
    *pp2 = NULL;
    *pn2 = 0;
    return m->hrLock;
}

static int32_t mk_play(BrDSBuffer *p, uint32_t a, uint32_t b, uint32_t f)
{
    Mock *m = (Mock *)p;
    m->nPlay++;
    m->lastPlayFlags = f;
    (void)a; (void)b;
    return m->hrPlay;
}

static int32_t mk_setpos(BrDSBuffer *p, uint32_t pos)
{
    Mock *m = (Mock *)p;
    m->nSetPos++;
    (void)pos;
    return m->hrSetPos;
}

static int32_t mk_setvolume(BrDSBuffer *p, int32_t v)
{
    Mock *m = (Mock *)p;
    m->nSetVolume++;
    m->lastVolume = v;
    return m->hrSetVolume;
}

static int32_t mk_setpan(BrDSBuffer *p, int32_t v)
{
    Mock *m = (Mock *)p;
    m->nSetPan++;
    m->lastPan = v;
    return m->hrSetPan;
}

static int32_t mk_setfreq(BrDSBuffer *p, uint32_t v)
{
    Mock *m = (Mock *)p;
    m->nSetFreq++;
    m->lastFreq = v;
    return 0;
}

static int32_t mk_stop(BrDSBuffer *p)
{ Mock *m = (Mock *)p; m->nStop++; return m->hrStop; }

static int32_t mk_unlock(BrDSBuffer *p, void *p1, uint32_t n1,
                         void *p2, uint32_t n2)
{
    Mock *m = (Mock *)p;
    int32_t hr = m->hrUnlock[(m->nUnlock < 2) ? m->nUnlock : 1];
    m->nUnlock++;
    m->lastUnlockBytes = n1;
    (void)p1; (void)p2; (void)n2;
    return hr;
}

static const BrDSBufferVtbl g_mockVtbl = {
    NULL, NULL, mk_release, mk_getcaps,
    NULL, NULL, NULL, NULL, NULL,
    mk_getstatus, NULL, mk_lock, mk_play, mk_setpos,
    NULL, mk_setvolume, mk_setpan, mk_setfreq, mk_stop, mk_unlock, NULL
};

static void mock_init(Mock *m)
{
    memset(m, 0, sizeof(*m));
    m->base.pVtbl = &g_mockVtbl;
}

/* mock IDirectSound ------------------------------------------------- */

typedef struct MockDS {
    BrDSound       base;
    BrDSBufferDesc lastDesc;
    int            nCreate;
    int32_t        hrCreate;
    BrDSBuffer    *pGive;
} MockDS;

static int32_t mkds_create(BrDSound *p, const BrDSBufferDesc *desc,
                           BrDSBuffer **pp, void *outer)
{
    MockDS *m = (MockDS *)p;
    m->nCreate++;
    m->lastDesc = *desc;
    (void)outer;
    if (desc->dwSize != 0x14u) { printf("FAIL: DSBUFFERDESC dwSize\n"); ++g_fail; }
    *pp = (m->hrCreate == 0) ? m->pGive : NULL;
    return m->hrCreate;
}

static const BrDSoundVtbl g_mockDSVtbl = { NULL, NULL, NULL, mkds_create };

/* ================================================================== */

static void voice_init(BrSndVoice *v, Mock *m)
{
    memset(v, 0, sizeof(*v));
    v->pBuf = &m->base;
}

/* ---- 0x10072490 : pan mapping ---- */
static void test_pan(void)
{
    Mock m; BrSndVoice v;

    mock_init(&m); voice_init(&v, &m);

    v.f10 = 400; CHECK(BrSndVoiceApplyPan(&v) == 0); CHECK(m.lastPan == 0);
    v.f10 = 0;   BrSndVoiceApplyPan(&v);             CHECK(m.lastPan == -4000);
    v.f10 = 800; BrSndVoiceApplyPan(&v);             CHECK(m.lastPan == 4000);
    /* strictly monotonic and symmetric about 400 */
    v.f10 = 300; BrSndVoiceApplyPan(&v);
    CHECK(m.lastPan == -1000);
    v.f10 = 500; BrSndVoiceApplyPan(&v);
    CHECK(m.lastPan == 1000);
    CHECK(m.nSetPan == 5);
}

/* ---- 0x100724D0 : volume mapping ---- */
static void test_volume(void)
{
    Mock m; BrSndVoice v;
    uint8_t save = BrSndMasterVolume;

    mock_init(&m); voice_init(&v, &m);

    /* master 0 is a hard mute: DSBVOLUME_MIN, and f14 is not consulted */
    BrSndMasterVolume = 0;
    v.f14 = 400;
    CHECK(BrSndVoiceApplyVolume(&v) == 0);
    CHECK(m.lastVolume == BR_DSBVOLUME_MIN);

    /* master 255 is unity: 400 -> 0 dB attenuation */
    BrSndMasterVolume = 255;
    v.f14 = 400; BrSndVoiceApplyVolume(&v); CHECK(m.lastVolume == 0);
    v.f14 = 0;   BrSndVoiceApplyVolume(&v); CHECK(m.lastVolume == -4000);
    v.f14 = 800; BrSndVoiceApplyVolume(&v); CHECK(m.lastVolume == 4000);

    /* the /255 is an integer truncation, not a round */
    BrSndMasterVolume = 128;
    v.f14 = 1;   BrSndVoiceApplyVolume(&v); CHECK(m.lastVolume == -4000);
    v.f14 = 400; BrSndVoiceApplyVolume(&v);
    CHECK(m.lastVolume == ((400 * 128) / 255 - 400) * 10);

    /* master scaling is monotonic in master for a fixed f14 */
    v.f14 = 400;
    BrSndMasterVolume = 200; BrSndVoiceApplyVolume(&v);
    {
        int32_t at200 = m.lastVolume;
        BrSndMasterVolume = 255; BrSndVoiceApplyVolume(&v);
        CHECK(at200 < m.lastVolume);
    }

    BrSndMasterVolume = save;
}

/* ---- 0x100724B0 ---- */
static void test_freq(void)
{
    Mock m; BrSndVoice v;
    mock_init(&m); voice_init(&v, &m);
    v.f0C = 22050;
    CHECK(BrSndVoiceApplyFreq(&v) == 0);
    CHECK(m.nSetFreq == 1 && m.lastFreq == 22050u);
}

/* ---- 0x10072820 ---- */
static void test_levels(void)
{
    Mock m; BrSndVoice v;
    BrDSound *saveDS = BrSndPDS;
    void     *saveG  = BrSndG18290FC;
    int32_t   saveE  = BrSndG0B5DE8;
    MockDS    ds;
    uint8_t   saveVol = BrSndMasterVolume;

    memset(&ds, 0, sizeof(ds));
    ds.base.pVtbl = &g_mockDSVtbl;
    BrSndPDS      = &ds.base;
    BrSndG18290FC = &ds;
    BrSndG0B5DE8  = 1;
    BrSndMasterVolume = 255;

    mock_init(&m); voice_init(&v, &m);

    /* balanced -> dead centre */
    CHECK(BrSndVoiceSetLevels(&v, 0x00100010u) == 1);
    CHECK(v.f10 == 400);
    CHECK(v.f14 == (16 * 400) / 32);

    /* all in the high half -> hard left; all in the low half -> hard right */
    CHECK(BrSndVoiceSetLevels(&v, 0x00200000u) == 1);
    CHECK(v.f10 == 0);
    CHECK(v.f14 == 400);
    CHECK(BrSndVoiceSetLevels(&v, 0x00000020u) == 1);
    CHECK(v.f10 == 800);
    CHECK(v.f14 == 400);

    /* both halves clamp at 0x20, so anything above behaves identically */
    CHECK(BrSndVoiceSetLevels(&v, 0xFFFF0000u) == 1);
    CHECK(v.f10 == 0 && v.f14 == 400);

    /* pan always lands inside [0,800] and volume inside [0,400] */
    {
        uint32_t hi, lo;
        for (hi = 0; hi <= 0x24u; hi += 3u) {
            for (lo = 0; lo <= 0x24u; lo += 3u) {
                CHECK(BrSndVoiceSetLevels(&v, (hi << 16) | lo) == 1);
                CHECK(v.f10 >= 0 && v.f10 <= 800);
                CHECK(v.f14 >= 0 && v.f14 <= 400);
            }
        }
    }

    /* zero on both sides: centre, silent, and the divide is skipped */
    v.f10 = -1; v.f14 = -1;
    CHECK(BrSndVoiceSetLevels(&v, 0u) == 1);
    CHECK(v.f10 == 400 && v.f14 == 0);

    /* a failing DirectSound call turns the INVERTED result to 0 */
    m.hrSetPan = 1;
    CHECK(BrSndVoiceSetLevels(&v, 0x00100010u) == 0);
    m.hrSetPan = 0;

    /* NULL voice is a failure (0), but a disabled mixer is a success (1) */
    CHECK(BrSndVoiceSetLevels(NULL, 0u) == 0);
    BrSndPDS = NULL;
    CHECK(BrSndVoiceSetLevels(NULL, 0u) == 1);

    BrSndPDS = saveDS;
    BrSndG18290FC = saveG;
    BrSndG0B5DE8 = saveE;
    BrSndMasterVolume = saveVol;
}

/* ---- 0x10072A00 / 0x100729E0 ---- */
static void test_start_stop(void)
{
    Mock m; BrSndVoice v;

    mock_init(&m); voice_init(&v, &m);

    /* not playing -> Play, and f1C latches */
    m.status = 0;
    CHECK(BrSndVoiceStart(&v) == 0);
    CHECK(m.nPlay == 1 && m.nSetPos == 0);
    CHECK(v.f1C == 1);
    CHECK(m.lastPlayFlags == 0u);

    /* loop flag reaches Play as DSBPLAY_LOOPING */
    v.f1C = 0;
    CHECK(BrSndVoiceSetLoopAndStart(&v, 7) == 0);
    CHECK(v.f18 == 7);
    CHECK(m.lastPlayFlags == 1u);   /* normalised to 0/1, not passed through */

    /* already playing -> rewind only, and f1C is NOT touched */
    v.f1C = 0;
    m.status = BR_DSBSTATUS_PLAYING;
    CHECK(BrSndVoiceStart(&v) == 0);
    CHECK(m.nSetPos == 1);
    CHECK(v.f1C == 0);

    /* a failed Play leaves f1C clear */
    m.status = 0;
    m.hrPlay = 9;
    CHECK(BrSndVoiceStart(&v) == 9);
    CHECK(v.f1C == 0);
    m.hrPlay = 0;

    /* Stop is a no-op unless f1C is set */
    m.nStop = 0;
    v.f1C = 0;
    CHECK(BrSndVoiceStop(&v) == 0);
    CHECK(m.nStop == 0);

    v.f1C = 1;
    CHECK(BrSndVoiceStop(&v) == 0);
    CHECK(m.nStop == 1 && v.f1C == 0);

    /* a failed Stop leaves the voice marked started */
    v.f1C = 1;
    m.hrStop = 4;
    CHECK(BrSndVoiceStop(&v) == 4);
    CHECK(v.f1C == 1);
    m.hrStop = 0;
}

/* ---- 0x10072520 ---- */
static void test_release(void)
{
    Mock m; BrSndVoice v;
    mock_init(&m); voice_init(&v, &m);

    CHECK(BrSndVoiceRelease(&v) == 0);
    CHECK(m.nRelease == 1 && v.pBuf == NULL);
    /* idempotent: a second call must not release again */
    CHECK(BrSndVoiceRelease(&v) == 0);
    CHECK(m.nRelease == 1);
}

/* ---- 0x10072450 / 0x10072BF0 ---- */
static void test_chain(void)
{
    Mock  m[4];
    BrSndVoice v[4];
    int i;

    for (i = 0; i < 4; ++i) { mock_init(&m[i]); voice_init(&v[i], &m[i]); }

    /* appending preserves insertion order */
    v[1].pNext = (BrSndVoice *)0xDEAD;  /* must be overwritten */
    v[1].f1C   = 1;                     /* must be cleared */
    CHECK(BrSndVoiceAppend(&v[0], &v[1]) == 0);
    CHECK(v[1].pNext == NULL && v[1].f1C == 0);
    BrSndVoiceAppend(&v[0], &v[2]);
    BrSndVoiceAppend(&v[0], &v[3]);
    CHECK(v[0].pNext == &v[1]);
    CHECK(v[1].pNext == &v[2]);
    CHECK(v[2].pNext == &v[3]);
    CHECK(v[3].pNext == NULL);

    /* stopping the chain skips the head and hits every other node once */
    for (i = 0; i < 4; ++i) v[i].f1C = 1;
    CHECK(BrSndVoiceStopChain(&v[0]) == 0);
    CHECK(m[0].nStop == 0 && v[0].f1C == 1);
    for (i = 1; i < 4; ++i) {
        CHECK(m[i].nStop == 1);
        CHECK(v[i].f1C == 0);
    }
}

/* ---- 0x10072C20 ---- */
static void *g_freed[16];
static int   g_nFreed;

static void trap_free(void *p) { if (g_nFreed < 16) g_freed[g_nFreed++] = p; }

static void test_free_chain(void)
{
    Mock  m[3];
    BrSndVoice v[3];
    BrSndFreeFn save = BrSndFreeHook;
    int i;

    for (i = 0; i < 3; ++i) {
        mock_init(&m[i]);
        voice_init(&v[i], &m[i]);
        v[i].pData   = (void *)((char *)&v[i] + 1);   /* distinct markers */
        v[i].pFormat = (void *)((char *)&v[i] + 2);
    }
    BrSndVoiceAppend(&v[0], &v[1]);
    BrSndVoiceAppend(&v[0], &v[2]);

    g_nFreed = 0;
    BrSndFreeHook = trap_free;
    CHECK(BrSndVoiceFreeChain(&v[0]) == 0);
    BrSndFreeHook = save;

    /* the head is detached but neither released nor freed */
    CHECK(v[0].pNext == NULL);
    CHECK(m[0].nRelease == 0);

    /* every other node: buffer released, then format, data, node freed */
    CHECK(m[1].nRelease == 1 && m[2].nRelease == 1);
    CHECK(g_nFreed == 6);
    CHECK(g_freed[0] == v[1].pFormat);
    CHECK(g_freed[1] == v[1].pData);
    CHECK(g_freed[2] == &v[1]);
    CHECK(g_freed[3] == v[2].pFormat);
    CHECK(g_freed[4] == v[2].pData);
    CHECK(g_freed[5] == &v[2]);
}

/* ---- 0x100722D0 ---- */
static void test_create(void)
{
    Mock       m;
    MockDS     ds;
    BrSndVoice v;
    BrDSound  *saveDS = BrSndPDS;
    unsigned char sample[16];
    int i;

    for (i = 0; i < 16; ++i) sample[i] = (unsigned char)(i * 7 + 1);

    mock_init(&m);
    memset(&ds, 0, sizeof(ds));
    ds.base.pVtbl = &g_mockDSVtbl;
    ds.pGive = &m.base;
    BrSndPDS = &ds.base;

    memset(&v, 0, sizeof(v));
    v.pData      = sample;
    v.nDataBytes = sizeof(sample);
    v.pFormat    = (void *)&ds;      /* just a marker */

    /* happy path */
    m.capsFlags = BR_DSBCAPS_LOCHARDWARE;
    CHECK(BrSndVoiceCreate(&v) == 0);
    CHECK(v.pBuf == &m.base);
    CHECK(v.f24 == 1);
    CHECK(m.nLock == 1 && m.nUnlock == 1);
    CHECK(m.lastUnlockBytes == sizeof(sample));   /* nDataBytes, not nLock1 */
    CHECK(memcmp(m.storage, sample, sizeof(sample)) == 0);
    CHECK(m.nSetVolume == 1 && m.lastVolume == 0);
    CHECK(m.nSetPan == 1 && m.lastPan == 0);
    CHECK(ds.lastDesc.dwFlags == BR_SND_DESC_FLAGS);
    CHECK(ds.lastDesc.dwBufferBytes == sizeof(sample));
    CHECK(ds.lastDesc.lpwfxFormat == v.pFormat);

    /* f28 selects the alternate flag word; caps without LOCHARDWARE -> 0 */
    mock_init(&m);
    ds.pGive = &m.base;
    m.capsFlags = 0;
    v.f28 = 1;
    v.f24 = 99;
    CHECK(BrSndVoiceCreate(&v) == 0);
    CHECK(ds.lastDesc.dwFlags == BR_SND_DESC_FLAGS_ALT);
    CHECK(v.f24 == 0);
    v.f28 = 0;

    /* a failure after the lock is released is reported and the buffer freed */
    mock_init(&m);
    ds.pGive = &m.base;
    m.hrSetVolume = 3;
    CHECK(BrSndVoiceCreate(&v) == 3);
    CHECK(v.pBuf == NULL);
    CHECK(m.nRelease == 1);
    CHECK(m.nUnlock == 1);      /* not unlocked twice */

    /* GOTCHA reproduced: when the FIRST Unlock fails the retry's HRESULT
     * replaces it, so a failed create can still report success -- but the
     * buffer really is gone. */
    mock_init(&m);
    ds.pGive = &m.base;
    m.hrUnlock[0] = 5;
    m.hrUnlock[1] = 0;
    CHECK(BrSndVoiceCreate(&v) == 0);
    CHECK(m.nUnlock == 2);
    CHECK(v.pBuf == NULL);
    CHECK(m.nRelease == 1);

    /* CreateSoundBuffer itself failing: nothing to unlock or release */
    mock_init(&m);
    ds.pGive = &m.base;
    ds.hrCreate = 8;
    CHECK(BrSndVoiceCreate(&v) == 8);
    CHECK(m.nLock == 0 && m.nUnlock == 0 && m.nRelease == 0);
    CHECK(v.pBuf == NULL);
    ds.hrCreate = 0;

    BrSndPDS = saveDS;
}

/* ---- 0x10072A90 / 0x10072A70 / 0x10072AF0 ---- */
static void test_play_table(void)
{
    Mock       m;
    MockDS     ds;
    BrSndVoice v;
    BrDSound  *saveDS = BrSndPDS;
    void      *saveG  = BrSndG18290FC;

    mock_init(&m);
    voice_init(&v, &m);
    memset(&ds, 0, sizeof(ds));
    ds.base.pVtbl = &g_mockDSVtbl;
    BrSndPDS = &ds.base;
    BrSndG18290FC = &ds;

    BrSndVoices[1 + 3 * BR_SND_SLOTS_PER_GROUP] = &v;

    /* the slot-1 wrapper reaches the same entry the full form does */
    CHECK(BrSndPlayGroup(3, 0x00100010u, 0) == 1);
    CHECK(m.nPlay == 1);
    CHECK(v.f10 == 400);
    CHECK(v.f18 == 0);

    /* the two-argument wrapper pins loop to 0 */
    v.f18 = 1;
    CHECK(BrSndPlaySimple(3, 0x00200000u) == 1);
    CHECK(v.f18 == 0);
    CHECK(v.f10 == 0);

    /* explicit slot/group indexing */
    BrSndVoices[5 + 2 * BR_SND_SLOTS_PER_GROUP] = &v;
    CHECK(BrSndPlayEx(2, 5, 0x00000020u, 1) == 1);
    CHECK(v.f18 == 1 && v.f10 == 800);

    /* a failed Play makes the inverted result 0 */
    m.hrPlay = 2;
    CHECK(BrSndPlayEx(2, 5, 0u, 0) == 0);
    m.hrPlay = 0;

    /* disabled sound is a silent success and must not touch the voice */
    m.nPlay = 0;
    BrSndPDS = NULL;
    CHECK(BrSndPlaySimple(3, 0u) == 1);
    CHECK(m.nPlay == 0);

    BrSndPDS = saveDS;
    BrSndG18290FC = saveG;
    BrSndVoices[1 + 3 * BR_SND_SLOTS_PER_GROUP] = NULL;
    BrSndVoices[5 + 2 * BR_SND_SLOTS_PER_GROUP] = NULL;
}

/* ---- 0x10073060 ---- */
static void test_bank_reset(void)
{
    BrSndBank bank;
    int i;

    for (i = 0; i < BR_SND_BANK_SLOTS; ++i) {
        bank.aName0[i] = i + 1;
        bank.aName1[i] = i + 100;
        bank.aName2[i] = i + 200;
    }
    BrSndBankReset(&bank);
    for (i = 0; i < BR_SND_BANK_SLOTS; ++i) {
        CHECK(bank.aName0[i] == 0);
        CHECK(bank.aName1[i] == 0);
        CHECK(bank.aName2[i] == 0);
    }
}

int main(void)
{
    test_plane_eval();
    test_pan();
    test_volume();
    test_freq();
    test_levels();
    test_start_stop();
    test_release();
    test_chain();
    test_free_chain();
    test_create();
    test_play_table();
    test_bank_reset();

    if (g_fail != 0) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("slice1_08: all checks passed\n");
    return 0;
}
