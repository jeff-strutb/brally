/* test_br_mix.c -- the mixer, asserted on its SAMPLES.
 *
 * Audio is the easiest subsystem in a port to declare working without
 * evidence: it runs, something comes out of the speaker, nobody can say what.
 * So nothing here listens to anything.  Every assertion is a measurable
 * property of the int16 stream BrMixRender produces:
 *
 *   - a 440 Hz reference tone resampled by SetFrequency comes back at the
 *     frequency the ratio predicts, measured by counting zero crossings;
 *   - a hard-left pan puts the energy in one channel and NONE in the other;
 *   - silence is bit-exact zero, and so is DSBVOLUME_MIN;
 *   - the gain law is DirectSound's, checked at the decade points where the
 *     answer is known in advance (-2000 cB is exactly a tenth);
 *   - a .wav round-trips through the writer and the parser unchanged.
 *
 * No device is opened and no file from the disc is needed.
 */
#include "br_mix.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------- fixtures */

/* A mono 16-bit sine at `rate` Hz sampling, `hz` Hz tone, `ms` long. */
static void make_tone(BrMixWaveFormat *pFmt, void **ppPcm, uint32_t *pcb,
                      uint32_t rate, double hz, int ms)
{
    uint32_t n = (uint32_t)((double)rate * ms / 1000.0);
    int16_t *p = (int16_t *)malloc(n * sizeof(int16_t));
    uint32_t i;

    for (i = 0; i < n; ++i)
        p[i] = (int16_t)(28000.0 * sin(2.0 * 3.14159265358979323846
                                       * hz * (double)i / (double)rate));

    pFmt->wFormatTag      = 1;
    pFmt->nChannels       = 1;
    pFmt->nSamplesPerSec  = rate;
    pFmt->nBlockAlign     = 2;
    pFmt->nAvgBytesPerSec = rate * 2;
    pFmt->wBitsPerSample  = 16;

    *ppPcm = p;
    *pcb   = n * (uint32_t)sizeof(int16_t);
}

/* Create a buffer through the DirectSound interface, exactly as
 * BrSndVoiceCreate does: CreateSoundBuffer, Lock, copy, Unlock. */
static BrDSBuffer *upload(BrDSound *pDev, const BrMixWaveFormat *pFmt,
                          const void *pPcm, uint32_t cb)
{
    BrDSBufferDesc desc;
    BrDSBuffer    *pBuf = NULL;
    void          *p1 = NULL, *p2 = NULL;
    uint32_t       n1 = 0, n2 = 0;

    desc.dwSize        = 0x14u;
    desc.dwFlags       = BR_SND_DESC_FLAGS;
    desc.dwBufferBytes = cb;
    desc.dwReserved    = 0;
    desc.lpwfxFormat   = (void *)(size_t)(const void *)pFmt;

    if (pDev->pVtbl->CreateSoundBuffer(pDev, &desc, &pBuf, NULL) != 0)
        return NULL;
    if (pBuf->pVtbl->Lock(pBuf, 0, cb, &p1, &n1, &p2, &n2, 0) != 0)
        return NULL;
    memcpy(p1, pPcm, cb);
    pBuf->pVtbl->Unlock(pBuf, p1, cb, NULL, 0);
    return pBuf;
}

/* Count sign changes in one channel; the tone frequency is half that per
 * second.  Crude on purpose -- it needs no FFT and it cannot be fooled by a
 * mixer that plays the right samples at the wrong rate, which is exactly the
 * bug it is here to catch. */
static double measured_hz(const int16_t *pPcm, int cFrames, int ch)
{
    int i, cross = 0, prev = 0;

    for (i = 0; i < cFrames; ++i) {
        int s = pPcm[i * 2 + ch];
        int sign = (s > 200) ? 1 : ((s < -200) ? -1 : 0);
        if (sign != 0) {
            if (prev != 0 && sign != prev)
                cross++;
            prev = sign;
        }
    }
    return (double)cross * BR_MIX_RATE / (2.0 * (double)cFrames);
}

static double rms(const int16_t *pPcm, int cFrames, int ch)
{
    double a = 0.0;
    int i;

    for (i = 0; i < cFrames; ++i) {
        double s = (double)pPcm[i * 2 + ch];
        a += s * s;
    }
    return sqrt(a / (double)cFrames);
}

static long peak(const int16_t *pPcm, int cFrames, int ch)
{
    long m = 0;
    int i;

    for (i = 0; i < cFrames; ++i) {
        long s = pPcm[i * 2 + ch];
        if (s < 0) s = -s;
        if (s > m) m = s;
    }
    return m;
}

/* ------------------------------------------------------------- the gain law */

static void test_gain_law(void)
{
    printf("gain law\n");

    check(BrMixGainFromCentibels(0) == 1.0, "0 cB is unity");
    check(fabs(BrMixGainFromCentibels(-2000) - 0.1) < 1e-12,
          "-2000 cB (-20 dB) is exactly a tenth");
    check(fabs(BrMixGainFromCentibels(-4000) - 0.01) < 1e-12,
          "-4000 cB is a hundredth -- the extreme BrSndVoiceSetLevels reaches");
    check(BrMixGainFromCentibels(BR_MIX_VOLUME_MIN) == 0.0,
          "DSBVOLUME_MIN is SILENCE, not 10^-5, so a mute is bit-exact");
    check(BrMixGainFromCentibels(5000) == 1.0,
          "positive attenuation does not amplify");

    /* The value BrSndVoiceApplyVolume produces at the engine's neutral 400
     * with a full master volume: (400*255/255 - 400)*10 == 0. */
    check(BrMixGainFromCentibels((400 - 400) * 10) == 1.0,
          "the engine's neutral 400 is unity gain");
}

/* --------------------------------------------------------------- silence */

static void test_silence(void)
{
    BrMix   *pMix = BrMixCreate();
    int16_t  aOut[512 * 2];
    int      i, nonzero = 0;

    printf("silence\n");

    memset(aOut, 0x5A, sizeof(aOut));
    BrMixRender(pMix, aOut, 512);
    for (i = 0; i < 512 * 2; ++i)
        if (aOut[i] != 0) nonzero++;
    check(nonzero == 0,
          "a mixer with no buffers renders bit-exact zero, not stale memory");

    {
        BrMixWaveFormat fmt;
        void           *pPcm;
        uint32_t        cb;
        BrDSBuffer     *pBuf;

        make_tone(&fmt, &pPcm, &cb, 22050, 440.0, 100);
        pBuf = upload(BrMixDevice(pMix), &fmt, pPcm, cb);
        check(pBuf != NULL, "a buffer uploads through the DirectSound calls");

        /* Created but never Played: DirectSound buffers do not sound until
         * Play, and BrSndVoiceCreate does not call it. */
        memset(aOut, 0x5A, sizeof(aOut));
        BrMixRender(pMix, aOut, 512);
        nonzero = 0;
        for (i = 0; i < 512 * 2; ++i) if (aOut[i] != 0) nonzero++;
        check(nonzero == 0, "an uploaded but unplayed buffer is silent");

        /* Played at DSBVOLUME_MIN: still bit-exact silence. */
        pBuf->pVtbl->SetVolume(pBuf, BR_DSBVOLUME_MIN);
        pBuf->pVtbl->Play(pBuf, 0, 0, 1);
        BrMixRender(pMix, aOut, 512);
        nonzero = 0;
        for (i = 0; i < 512 * 2; ++i) if (aOut[i] != 0) nonzero++;
        check(nonzero == 0,
              "a voice at DSBVOLUME_MIN is silent -- the hard-mute path is real");

        free(pPcm);
    }
    BrMixDestroy(pMix);
}

/* ------------------------------------------------------------ resampling */

static void test_resample(void)
{
    BrMix          *pMix = BrMixCreate();
    BrMixWaveFormat fmt;
    void           *pPcm;
    uint32_t        cb;
    BrDSBuffer     *pBuf;
    int16_t        *pOut;
    const int       cFrames = BR_MIX_RATE;      /* one second */
    double          f;

    printf("resampling -- the pitch the port asked for is the pitch that comes out\n");

    /* A 440 Hz reference recorded at the mixer's own rate. */
    make_tone(&fmt, &pPcm, &cb, 22050, 440.0, 1000);
    pBuf = upload(BrMixDevice(pMix), &fmt, pPcm, cb);
    pOut = (int16_t *)malloc((size_t)cFrames * 2 * sizeof(int16_t));

    /* SetFrequency(0) is DSBFREQUENCY_ORIGINAL: play it as recorded. */
    pBuf->pVtbl->SetFrequency(pBuf, 0);
    pBuf->pVtbl->SetCurrentPosition(pBuf, 0);
    pBuf->pVtbl->Play(pBuf, 0, 0, 1);
    BrMixRender(pMix, pOut, cFrames);
    f = measured_hz(pOut, cFrames, 0);
    printf("    original rate -> %.1f Hz\n", f);
    check(fabs(f - 440.0) < 3.0, "at its own rate a 440 Hz tone measures 440 Hz");

    /* Double the frequency: DirectSound plays the same samples twice as fast,
     * so the tone doubles. */
    pBuf->pVtbl->SetFrequency(pBuf, 44100);
    pBuf->pVtbl->SetCurrentPosition(pBuf, 0);
    BrMixRender(pMix, pOut, cFrames);
    f = measured_hz(pOut, cFrames, 0);
    printf("    SetFrequency(44100) -> %.1f Hz\n", f);
    check(fabs(f - 880.0) < 6.0, "SetFrequency(2x) doubles the tone");

    /* Half. */
    pBuf->pVtbl->SetFrequency(pBuf, 11025);
    pBuf->pVtbl->SetCurrentPosition(pBuf, 0);
    BrMixRender(pMix, pOut, cFrames);
    f = measured_hz(pOut, cFrames, 0);
    printf("    SetFrequency(11025) -> %.1f Hz\n", f);
    check(fabs(f - 220.0) < 3.0, "SetFrequency(0.5x) halves the tone");

    /* A rate the ratio arithmetic would actually produce: 3/2. */
    pBuf->pVtbl->SetFrequency(pBuf, 33075);
    pBuf->pVtbl->SetCurrentPosition(pBuf, 0);
    BrMixRender(pMix, pOut, cFrames);
    f = measured_hz(pOut, cFrames, 0);
    printf("    SetFrequency(33075) -> %.1f Hz\n", f);
    check(fabs(f - 660.0) < 5.0, "and a non-power-of-two ratio lands where it should");

    free(pOut);
    free(pPcm);
    BrMixDestroy(pMix);
}

/* ------------------------------------------------------------------- pan */

static void test_pan(void)
{
    BrMix          *pMix = BrMixCreate();
    BrMixWaveFormat fmt;
    void           *pPcm;
    uint32_t        cb;
    BrDSBuffer     *pBuf;
    const int       cFrames = 8192;
    int16_t        *pOut;
    double          l, r;

    printf("pan -- and which side is left\n");

    make_tone(&fmt, &pPcm, &cb, 22050, 440.0, 1000);
    pBuf = upload(BrMixDevice(pMix), &fmt, pPcm, cb);
    pOut = (int16_t *)malloc((size_t)cFrames * 2 * sizeof(int16_t));
    pBuf->pVtbl->Play(pBuf, 0, 0, 1);

    /* Centre. */
    pBuf->pVtbl->SetPan(pBuf, 0);
    BrMixRender(pMix, pOut, cFrames);
    l = rms(pOut, cFrames, 0);
    r = rms(pOut, cFrames, 1);
    check(l > 1000.0 && fabs(l - r) < 1.0, "a centred voice is equal in both channels");

    /* DSBPAN_LEFT: energy in the LEFT channel and NONE in the right.  This is
     * the assertion that makes br_sfx.h's finding -- BrSndPan's pGainA is
     * physically left -- an audible fact rather than a deduction. */
    pBuf->pVtbl->SetPan(pBuf, BR_MIX_PAN_LEFT);
    BrMixRender(pMix, pOut, cFrames);
    l = rms(pOut, cFrames, 0);
    r = rms(pOut, cFrames, 1);
    printf("    DSBPAN_LEFT  L rms %.1f, R rms %.1f, R peak %ld\n",
           l, r, peak(pOut, cFrames, 1));
    check(l > 1000.0, "full left keeps the left channel");
    check(peak(pOut, cFrames, 1) == 0,
          "full left puts EXACTLY ZERO in the right channel");

    pBuf->pVtbl->SetPan(pBuf, BR_MIX_PAN_RIGHT);
    BrMixRender(pMix, pOut, cFrames);
    check(peak(pOut, cFrames, 0) == 0 && rms(pOut, cFrames, 1) > 1000.0,
          "...and full right is the mirror of it");

    /* The extreme the engine can actually reach: BrSndVoiceSetLevels drives
     * f10 to 0 or 800, so SetPan sees +-4000, which is -40 dB rather than
     * silence.  Both facts are worth pinning: the law is DirectSound's, and
     * the game never uses all of it. */
    pBuf->pVtbl->SetPan(pBuf, (0 - 400) * 10);
    BrMixRender(pMix, pOut, cFrames);
    l = rms(pOut, cFrames, 0);
    r = rms(pOut, cFrames, 1);
    printf("    engine hard left (f10=0 -> %d cB)  L %.1f  R %.1f  ratio %.1f\n",
           (0 - 400) * 10, l, r, (r > 0.0) ? l / r : 0.0);
    check(r > 0.0 && l / r > 90.0,
          "the engine's own hard left is 100:1, not silence -- it only reaches -4000 cB");

    free(pOut);
    free(pPcm);
    BrMixDestroy(pMix);
}

/* ------------------------------------------------------- buffer semantics */

static void test_buffer_semantics(void)
{
    BrMix          *pMix = BrMixCreate();
    BrMixWaveFormat fmt;
    void           *pPcm;
    uint32_t        cb;
    BrDSBuffer     *pBuf;
    int16_t         aOut[4096 * 2];
    uint32_t        status = 0;
    BrDSBCaps       caps;

    printf("buffer semantics the port depends on\n");

    /* A 50 ms sample, so one render of 4096 frames (186 ms) outlasts it. */
    make_tone(&fmt, &pPcm, &cb, 22050, 440.0, 50);
    pBuf = upload(BrMixDevice(pMix), &fmt, pPcm, cb);

    caps.dwSize = 0x14u;
    pBuf->pVtbl->GetCaps(pBuf, &caps);
    check((caps.dwFlags & BR_DSBCAPS_LOCHARDWARE) == 0,
          "GetCaps reports a SOFTWARE buffer, so the voice's f24 comes back 0");

    pBuf->pVtbl->GetStatus(pBuf, &status);
    check(status == 0, "a fresh buffer does not report DSBSTATUS_PLAYING");

    pBuf->pVtbl->Play(pBuf, 0, 0, 0);          /* no DSBPLAY_LOOPING */
    pBuf->pVtbl->GetStatus(pBuf, &status);
    check((status & BR_DSBSTATUS_PLAYING) != 0,
          "...and does after Play -- which is the branch BrSndVoiceStart reads");

    BrMixRender(pMix, aOut, 4096);
    pBuf->pVtbl->GetStatus(pBuf, &status);
    check(status == 0, "a one-shot stops itself at the end of the sample");
    check(peak(aOut, 4096, 0) > 1000, "...having actually produced samples first");

    /* And can be fired again without a rewind, because the cursor went back
     * to the start.  0x10072A00 does NOT rewind on this path, so if the
     * cursor stayed at the end every repeated one-shot would be silent. */
    pBuf->pVtbl->Play(pBuf, 0, 0, 0);
    BrMixRender(pMix, aOut, 4096);
    check(peak(aOut, 4096, 0) > 1000,
          "a one-shot replayed without SetCurrentPosition is audible again");

    /* Looping outlives the sample. */
    pBuf->pVtbl->Play(pBuf, 0, 0, 1);
    BrMixRender(pMix, aOut, 4096);
    pBuf->pVtbl->GetStatus(pBuf, &status);
    check((status & BR_DSBSTATUS_PLAYING) != 0 && peak(aOut, 4096, 0) > 1000,
          "DSBPLAY_LOOPING keeps going past the end of the sample");

    /* Stop keeps the cursor; only SetCurrentPosition rewinds. */
    pBuf->pVtbl->Stop(pBuf);
    pBuf->pVtbl->GetStatus(pBuf, &status);
    check(status == 0, "Stop clears DSBSTATUS_PLAYING");
    BrMixRender(pMix, aOut, 4096);
    check(peak(aOut, 4096, 0) == 0, "...and a stopped buffer contributes nothing");

    check(BrMixBufferCount(pMix) == 1, "one live buffer");
    pBuf->pVtbl->Release(pBuf);
    check(BrMixBufferCount(pMix) == 0, "Release unlinks it from the mix");

    free(pPcm);
    BrMixDestroy(pMix);
}

/* -------------------------------------------------------------- mixing */

static void test_sum(void)
{
    BrMix          *pMix = BrMixCreate();
    BrMixWaveFormat fmt;
    void           *pPcm;
    uint32_t        cb;
    BrDSBuffer     *pA, *pB;
    const int       cFrames = 4096;
    int16_t        *pOut;
    double          one, two;

    printf("summing\n");

    make_tone(&fmt, &pPcm, &cb, 22050, 440.0, 1000);
    pOut = (int16_t *)malloc((size_t)cFrames * 2 * sizeof(int16_t));

    pA = upload(BrMixDevice(pMix), &fmt, pPcm, cb);
    pA->pVtbl->SetVolume(pA, -2000);          /* a tenth, so two fit */
    pA->pVtbl->Play(pA, 0, 0, 1);
    BrMixRender(pMix, pOut, cFrames);
    one = rms(pOut, cFrames, 0);

    pB = upload(BrMixDevice(pMix), &fmt, pPcm, cb);
    pB->pVtbl->SetVolume(pB, -2000);
    pB->pVtbl->SetCurrentPosition(pB, 0);
    pB->pVtbl->Play(pB, 0, 0, 1);
    pA->pVtbl->SetCurrentPosition(pA, 0);
    BrMixRender(pMix, pOut, cFrames);
    two = rms(pOut, cFrames, 0);

    printf("    one voice rms %.1f, two identical voices rms %.1f\n", one, two);
    check(BrMixPlayingCount(pMix) == 2, "two voices report playing");
    check(fabs(two - 2.0 * one) < 0.05 * two,
          "two identical voices sum to twice the amplitude");
    check(BrMixClipCount(pMix) == 0,
          "...and at a tenth each, nothing clipped");

    /* At unity they do not fit, and the counter says so rather than the
     * result quietly sounding wrong. */
    pA->pVtbl->SetVolume(pA, 0);
    pB->pVtbl->SetVolume(pB, 0);
    pA->pVtbl->SetCurrentPosition(pA, 0);
    pB->pVtbl->SetCurrentPosition(pB, 0);
    BrMixRender(pMix, pOut, cFrames);
    printf("    two at unity: %ld samples clipped\n", BrMixClipCount(pMix));
    check(BrMixClipCount(pMix) > 0,
          "two voices at unity overflow, and the clip counter reports it");

    free(pOut);
    free(pPcm);
    BrMixDestroy(pMix);
}

/* -------------------------------------------------------------- wav I/O */

static void test_wav(void)
{
    BrMixWaveFormat fmt, back;
    void           *pPcm;
    uint32_t        cb, cbBack;
    void           *pBack = NULL;
    const char     *pszTmp = "build/test_br_mix_roundtrip.wav";

    printf("wav round trip\n");

    make_tone(&fmt, &pPcm, &cb, 11025, 440.0, 100);
    check(BrMixWavWrite(pszTmp, &fmt, pPcm, cb) == 0, "a .wav writes");
    check(BrMixWavLoad(pszTmp, &back, &pBack, &cbBack) == 0, "...and reads back");
    check(back.nSamplesPerSec == 11025 && back.nChannels == 1
          && back.wBitsPerSample == 16 && back.wFormatTag == 1,
          "the format survives the round trip");
    check(cbBack == cb && pBack != NULL && memcmp(pBack, pPcm, cb) == 0,
          "and so do the samples, byte for byte");
    free(pBack);
    remove(pszTmp);

    /* The parser must reject what it cannot play rather than mixing noise. */
    {
        static const unsigned char junk[16] = { 'R','I','F','X', 0,0,0,0,
                                                'W','A','V','E', 0,0,0,0 };
        const void *p; uint32_t n;
        check(BrMixWavParse(junk, sizeof(junk), &back, &p, &n) != 0,
              "a non-RIFF file is rejected");
        check(BrMixWavParse(pPcm, 4, &back, &p, &n) != 0,
              "a truncated header is rejected");
    }

    /* An 8-bit unsigned wave -- five of the disc's samples are one, and
     * getting the 128 bias wrong makes them a loud DC thump. */
    {
        BrMix          *pMix = BrMixCreate();
        BrMixWaveFormat f8;
        unsigned char   aPcm[256];
        BrDSBuffer     *pBuf;
        int16_t         aOut[512 * 2];
        int             i;

        for (i = 0; i < 256; ++i)
            aPcm[i] = 128;                    /* 8-bit silence is 0x80 */
        f8.wFormatTag = 1; f8.nChannels = 1; f8.nSamplesPerSec = 22050;
        f8.nBlockAlign = 1; f8.nAvgBytesPerSec = 22050; f8.wBitsPerSample = 8;

        pBuf = upload(BrMixDevice(pMix), &f8, aPcm, sizeof(aPcm));
        pBuf->pVtbl->Play(pBuf, 0, 0, 1);
        BrMixRender(pMix, aOut, 512);
        check(peak(aOut, 512, 0) == 0,
              "8-bit PCM is UNSIGNED: a buffer of 0x80 is silence, not full scale");
        BrMixDestroy(pMix);
    }

    free(pPcm);
}

int main(void)
{
    test_gain_law();
    test_silence();
    test_resample();
    test_pan();
    test_buffer_semantics();
    test_sum();
    test_wav();

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
