/* test_br_sfxout.c -- the whole chain, end to end, with no audio device.
 *
 * br_sfx.c's table and pitch arithmetic and slice1_08.c's voice wrapper each
 * have suites that prove them in isolation.  This one proves they are WIRED:
 * that a real .wav off the disc reaches the mixer, that the frequency the
 * ported curve computed is the frequency the samples come out at, and that
 * the pan the ported level code computed puts the energy on the right side.
 *
 * Every assertion is on rendered int16 samples, and the render is also
 * WRITTEN OUT (build/test_br_sfxout_*.wav) so the result can be inspected or
 * listened to long after the run rather than taken on trust.
 *
 * The disc-backed part SKIPs, loudly, when testdata/sfx is not there -- see
 * README's asset policy.  Nothing is committed.
 */
#include "br_sfxout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------- measures */

static double rms(const int16_t *pPcm, int cFrames, int ch)
{
    double a = 0.0;
    int i;

    if (cFrames <= 0)
        return 0.0;
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

/* Zero crossings per second in one channel, over a window.  An engine loop is
 * not a sine, so this is not "the pitch" -- but it is strictly monotonic in
 * playback rate for a fixed waveform, which is exactly what a pitch sweep has
 * to demonstrate. */
static double zcr(const int16_t *pPcm, int cFrames, int from, int to)
{
    int i, cross = 0, prev = 0;

    if (to > cFrames) to = cFrames;
    if (from < 0) from = 0;
    if (to <= from) return 0.0;
    for (i = from; i < to; ++i) {
        int s = pPcm[i * 2];
        int sign = (s > 300) ? 1 : ((s < -300) ? -1 : 0);
        if (sign != 0) {
            if (prev != 0 && sign != prev) cross++;
            prev = sign;
        }
    }
    return (double)cross * BR_MIX_RATE / (double)(to - from);
}

/* ------------------------------------------------------ name resolution */

/* Needs no disc: the tables are compiled in. */
static void test_lookup(void)
{
    int set = -1, group = -1;

    printf("name lookup\n");

    check(BrSfxLookupGroup("beep", &set, &group) == 0
          && set == BR_SFX_SET_RACE && group == 13,
          "'beep' resolves to race group 13");
    check(BrSfxLookupGroup("beep.wav", &set, &group) == 0 && group == 13,
          "...with or without the extension the table carries");
    check(BrSfxLookupGroup("front-end5", &set, &group) == 0
          && set == BR_SFX_SET_MENU && group == 1,
          "a menu-only name resolves to the MENU set, not race group 1");
    check(BrSfxLookupGroup("hit-another-car1", &set, &group) == 0
          && set == BR_SFX_SET_RACE && group == 1,
          "...and the race name for the same group number still wins for race");
    check(BrSfxLookupGroup("20", &set, &group) == 0 && group == 20,
          "a bare number is a race group");
    check(BrSfxLookupGroup("no-such-sound", &set, &group) != 0,
          "an unknown name is rejected rather than guessed");

    check(BrSfxLookupCar("ce") == 1 && BrSfxLookupCar("mn") == BR_SFX_CARS,
          "car codes resolve to the bank's car+1");
    check(BrSfxLookupCar("zz") == 0 && BrSfxLookupCar("0") == 0,
          "a non-car is 0, which is the bank's 'no car'");
}

/* ------------------------------------------------------------ the device */

/* Needs no disc either: a synthetic voice through the real wiring. */
static void test_wiring(void)
{
    BrSfxOut   out;
    BrSndVoice voice;
    int16_t   *pOut;
    const int  cFrames = 4096;
    uint32_t   status = 0;

    printf("wiring -- the ported setters reach the mixer\n");

    check(BrSfxOutOpen(&out) == 0, "the mixer installs as BrSndPDS");
    check(BrSndPDS != NULL && BrSndG18290FC != NULL,
          "...and satisfies both of the gates every BrSnd* entry point tests");

    {
        BrMixWaveFormat fmt;
        int16_t        *pPcm = (int16_t *)malloc(2205 * sizeof(int16_t));
        int             i;

        for (i = 0; i < 2205; ++i)
            pPcm[i] = (int16_t)(24000.0 * sin(2.0 * 3.14159265358979323846
                                              * 100.0 * i / 11025.0));
        fmt.wFormatTag = 1; fmt.nChannels = 1; fmt.nSamplesPerSec = 11025;
        fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = 22050; fmt.wBitsPerSample = 16;

        check(BrMixVoiceInit(&voice, &fmt, pPcm, 2205 * 2) == 0,
              "a voice is built from a sample");
        check(voice.f10 == 400 && voice.f14 == 400,
              "...seeded at the engine's neutral 400 for pan and volume");
        check(BrSndVoiceCreate(&voice) == 0,
              "BrSndVoiceCreate -- unchanged, ported code -- uploads it");
        check(voice.f24 == 0,
              "and records a software buffer, since GetCaps reports no LOCHARDWARE");
    }

    pOut = (int16_t *)malloc((size_t)cFrames * 2 * sizeof(int16_t));

    /* Drive it entirely through slice1_08.c.  The packed pair is the one
     * 0x10061470 builds: high half LEFT, low half RIGHT, each capped at 32. */
    check(BrSndVoiceSetLevels(&voice, (32u << 16) | 32u) == 1,
          "BrSndVoiceSetLevels accepts a centred pair");
    check(voice.f10 == 400 && voice.f14 == 400,
          "...and derives centre and unity from it");
    BrSndVoiceSetLoopAndStart(&voice, 1);
    voice.pBuf->pVtbl->GetStatus(voice.pBuf, &status);
    check((status & BR_DSBSTATUS_PLAYING) != 0, "BrSndVoiceStart starts it");

    BrMixRender(out.pMix, pOut, cFrames);
    check(rms(pOut, cFrames, 0) > 1000.0 && rms(pOut, cFrames, 1) > 1000.0,
          "a centred voice renders into both channels");

    /* Hard left through the SAME path: only the packed pair changes. */
    BrSndVoiceSetLevels(&voice, (32u << 16) | 0u);
    check(voice.f10 == 0, "a left-heavy pair drives f10 to 0 -- hard left");
    BrMixRender(out.pMix, pOut, cFrames);
    printf("    packed 0x00200000: L rms %.1f  R rms %.1f  ratio %.1f\n",
           rms(pOut, cFrames, 0), rms(pOut, cFrames, 1),
           rms(pOut, cFrames, 1) > 0.0
               ? rms(pOut, cFrames, 0) / rms(pOut, cFrames, 1) : 0.0);
    check(rms(pOut, cFrames, 0) > 90.0 * rms(pOut, cFrames, 1),
          "the HIGH half of the packed pair comes out of the LEFT speaker");

    BrSndVoiceSetLevels(&voice, (0u << 16) | 32u);
    check(voice.f10 == 800, "a right-heavy pair drives f10 to 800");
    BrMixRender(out.pMix, pOut, cFrames);
    check(rms(pOut, cFrames, 1) > 90.0 * rms(pOut, cFrames, 0),
          "...and the LOW half comes out of the RIGHT -- BrSndPan's pGainA is left");

    /* The master volume's hard-mute path, straight through
     * BrSndVoiceApplyVolume. */
    BrSndVoiceSetLevels(&voice, (32u << 16) | 32u);
    BrSndMasterVolume = 0;
    BrSndVoiceApplyVolume(&voice);
    BrMixRender(out.pMix, pOut, cFrames);
    check(peak(pOut, cFrames, 0) == 0 && peak(pOut, cFrames, 1) == 0,
          "master volume 0 is DSBVOLUME_MIN and renders bit-exact silence");
    BrSndMasterVolume = 0xFFu;

    free(pOut);
    BrSndVoiceRelease(&voice);
    free(voice.pFormat);
    free(voice.pData);
    BrSfxOutClose(&out);
    check(BrSndPDS == NULL, "close pulls the device back out from under the port");
}

/* ------------------------------------------------- the disc, when present */

static int have_disc(void)
{
    FILE *f = fopen(BR_SFXOUT_DIR_DEFAULT "beep.wav", "rb");

    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

static void test_bank(void)
{
    BrSfxOut     out;
    BrSfxCapture cap;
    int          n;

    printf("bank load\n");

    check(BrSfxOutOpen(&out) == 0, "device");
    n = BrSfxOutLoadSet(&out, BR_SFX_SET_RACE, NULL);
    printf("    race set: %d voices, %d missing\n", n, out.cMissing);
    check(n == 23 && out.cMissing == 0,
          "the race set loads all 23 rows its loader reaches");
    check(BrMixBufferCount(out.pMix) == 23,
          "each one is a DirectSound buffer -- one per marked slot, not per file");

    /* Groups 8 and 9 are the same file at different base rates.  Two rows,
     * two buffers, one file: that is the shipped arrangement and the reason
     * 26 rows need only 73 files. */
    check(BrSndVoices[BrSfxVoiceIndex(8, 1)] != NULL
       && BrSndVoices[BrSfxVoiceIndex(9, 1)] != NULL
       && BrSndVoices[BrSfxVoiceIndex(8, 1)] != BrSndVoices[BrSfxVoiceIndex(9, 1)],
          "rn_dirt is loaded twice, into two rows, as two independent voices");

    /* beep/beep2/water are the rows that mark slot 3, not slot 1. */
    check(BrSndVoices[BrSfxVoiceIndex(13, 3)] != NULL
       && BrSndVoices[BrSfxVoiceIndex(13, 1)] == NULL,
          "beep lands on slot 3, where the bank marks it -- not the one-shot slot");

    /* The formats really do vary, and the mixer took them all. */
    {
        const BrMixWaveFormat *pA =
            (const BrMixWaveFormat *)BrSndVoices[BrSfxVoiceIndex(13, 3)]->pFormat;
        const BrMixWaveFormat *pB =
            (const BrMixWaveFormat *)BrSndVoices[BrSfxVoiceIndex(20, 1)]->pFormat;
        printf("    beep.wav is %u Hz/%u-bit; taunt1.wav is %u Hz/%u-bit\n",
               pA->nSamplesPerSec, pA->wBitsPerSample,
               pB->nSamplesPerSec, pB->wBitsPerSample);
        check(pA->wBitsPerSample == 8 && pB->wBitsPerSample == 16,
              "the disc mixes 8-bit and 16-bit samples and both loaded");
        check(pA->nSamplesPerSec != BrSfxGroupBaseRate(13),
              "and a file's own rate is NOT its row's base rate -- 11025 in an 11000 row");
    }

    /* Play one and render it.  The evidence is the file. */
    memset(&cap, 0, sizeof(cap));
    check(BrSfxOutPlayGroup(&out, BR_SFX_SET_RACE, 13,
                            ((uint32_t)BR_SFX_LEVEL_MAX << 16)
                            | (uint32_t)BR_SFX_LEVEL_MAX, 0),
          "group 13 (beep) starts");
    BrSfxOutRender(&out, BR_MIX_RATE, BR_MIX_RATE / 50,
                   BrSfxCaptureSink, &cap, NULL, NULL);
    check(cap.cFrames == BR_MIX_RATE, "a second of mix came back");
    check(peak(cap.pPcm, cap.cFrames, 0) > 2000,
          "...and it is not silence: the disc's PCM reached the output");
    check(BrSfxCaptureWrite(&cap, "build/test_br_sfxout_beep.wav") == 0,
          "written to build/test_br_sfxout_beep.wav for inspection");

    /* One-shot, so it must have finished inside the second. */
    check(BrMixPlayingCount(out.pMix) == 0,
          "a one-shot stopped itself rather than looping forever");

    BrSfxCaptureFree(&cap);
    BrSfxOutClose(&out);

    /* The menu set is the other name table over the same rows. */
    check(BrSfxOutOpen(&out) == 0, "device again");
    n = BrSfxOutLoadSet(&out, BR_SFX_SET_MENU, NULL);
    printf("    menu set: %d voices, %d missing\n", n, out.cMissing);
    check(n == 7 && out.cMissing == 0,
          "the menu set loads its 7 rows -- the same numbers, different files");
    BrSfxOutClose(&out);
}

typedef struct SweepRec {
    BrSfxOut *pOut;
    uint32_t  aHz[16];
    int       cHz;
    double    seconds;
} SweepRec;

static void sweep_step(void *pUser, double t)
{
    SweepRec *pS = (SweepRec *)pUser;
    double    u  = t / pS->seconds;
    float     rpm;
    uint32_t  hz;

    if (u > 1.0) u = 1.0;
    rpm = (float)(800.0 + 7200.0 * u);
    hz  = BrSfxOutEngineSetRpm(pS->pOut, 0, rpm, 1.0f);

    /* Record one frequency per sixteenth of the sweep. */
    {
        int slot = (int)(u * 15.999);
        if (slot >= 0 && slot < 16 && pS->aHz[slot] == 0) {
            pS->aHz[slot] = hz;
            if (slot + 1 > pS->cHz) pS->cHz = slot + 1;
        }
    }
}

static void test_engine_sweep(void)
{
    BrSfxOut     out;
    BrSfxCapture cap;
    SweepRec     rec;
    int          i, ok;
    double       zLo, zHi;
    const int    cSec = 4;

    printf("engine sweep -- the pitch curve, audible\n");

    check(BrSfxOutOpen(&out) == 0, "device");
    i = BrSfxOutLoadCar(&out, BrSfxLookupCar("ce"), 0, NULL);
    printf("    car ce: %d of 3 engine layers loaded\n", i);
    check(i == 3, "all three layers of one car load: <cc>.wav, <cc>h.wav, <cc>r.wav");
    check(BrSndVoices[BrSfxVoiceIndex(BR_SFX_GROUP_ENGINE, 0)] != NULL
       && BrSndVoices[BrSfxVoiceIndex(BR_SFX_GROUP_ENGINE_HIGH, 0)] != NULL
       && BrSndVoices[BrSfxVoiceIndex(BR_SFX_GROUP_ENGINE_REV, 0)] != NULL,
          "...on channel 2*iCar == 0, in rows 0, 24 and 25");

    check(BrSfxOutEngineStart(&out, 0, ((uint32_t)BR_SFX_LEVEL_MAX << 16)
                                     | (uint32_t)BR_SFX_LEVEL_MAX) == 3,
          "all three start together -- the engine is three simultaneous loops");
    check(BrMixPlayingCount(out.pMix) == 3, "and the mixer sees three");

    memset(&rec, 0, sizeof(rec));
    memset(&cap, 0, sizeof(cap));
    rec.pOut    = &out;
    rec.seconds = cSec;

    BrSfxOutRender(&out, cSec * BR_MIX_RATE, BR_MIX_RATE / 200,
                   BrSfxCaptureSink, &cap, sweep_step, &rec);

    check(cap.cFrames == cSec * BR_MIX_RATE, "the whole sweep rendered");
    check(BrMixPlayingCount(out.pMix) == 3,
          "the loops are still running -- DSBPLAY_LOOPING outlasts the sample");

    printf("    applied Hz across the sweep:");
    for (i = 0; i < rec.cHz; ++i) printf(" %u", rec.aHz[i]);
    printf("\n");

    ok = (rec.cHz == 16);
    for (i = 1; i < rec.cHz; ++i)
        if (rec.aHz[i] <= rec.aHz[i - 1]) ok = 0;
    check(ok, "the frequency the ported curve produced rises at every step");
    check(rec.aHz[0] == 6299 && rec.aHz[15] > 55000,
          "from 6299 Hz at idle to past 55 kHz near redline");

    check(peak(cap.pPcm, cap.cFrames, 0) > 4000,
          "the sweep is loud enough to hear");
    check(BrSfxCaptureWrite(&cap, "build/test_br_sfxout_sweep.wav") == 0,
          "written to build/test_br_sfxout_sweep.wav for inspection");

    BrSfxCaptureFree(&cap);
    BrSfxOutClose(&out);

    /* ---- and now the same thing MEASURED, one layer at a time ----------
     *
     * The three-layer render above cannot be measured by counting zero
     * crossings, and the reason is itself worth recording: the HIGH layer is
     * pinned at doppler*22050 and does not move with RPM at all, so in a
     * three-layer mix its crossings swamp the two layers that do move.  That
     * is not a defect in the mix -- it is what the original sounds like.
     *
     * So the measurement is made on the low layer alone, which is the one the
     * RPM curve drives, at fixed RPMs.  A fixed waveform played N times
     * faster crosses zero N times as often; if the mixer merely stored the
     * frequency without resampling, this ratio would be 1. */
    printf("  the low layer alone, measured\n");
    {
        static const float aRpm[4] = { 800.0f, 1600.0f, 3200.0f, 6400.0f };
        double  aZ[4];
        uint32_t aHz[4];

        for (i = 0; i < 4; ++i) {
            BrSfxOut o2;
            BrSfxCapture c2;

            BrSfxOutOpen(&o2);
            BrSfxOutLoadCar(&o2, BrSfxLookupCar("ce"), 0, NULL);
            /* Only group 0, so nothing else contributes crossings. */
            BrSndPlayEx(BR_SFX_GROUP_ENGINE, 0,
                        ((uint32_t)BR_SFX_LEVEL_MAX << 16)
                        | (uint32_t)BR_SFX_LEVEL_MAX, 1);
            aHz[i] = BrSfxOutEngineSetRpm(&o2, 0, aRpm[i], 1.0f);

            memset(&c2, 0, sizeof(c2));
            BrSfxOutRender(&o2, BR_MIX_RATE, BR_MIX_RATE / 50,
                           BrSfxCaptureSink, &c2, NULL, NULL);
            aZ[i] = zcr(c2.pPcm, c2.cFrames, 0, c2.cFrames);
            printf("    %5.0f RPM -> %6u Hz -> %6.0f crossings/s\n",
                   (double)aRpm[i], aHz[i], aZ[i]);
            BrSfxCaptureFree(&c2);
            BrSfxOutClose(&o2);
        }

        ok = 1;
        for (i = 1; i < 4; ++i) {
            /* Each step doubles the RPM, so it must roughly double the
             * crossing rate.  A generous band, because an engine loop is not
             * a sine and the top rate resamples above the source's own. */
            double ratio = aZ[i] / aZ[i - 1];
            if (!(ratio > 1.7 && ratio < 2.3)) ok = 0;
        }
        check(ok, "doubling the RPM doubles the measured crossing rate: it RESAMPLED");

        zLo = aZ[0];
        zHi = aZ[3];
        check(zHi > 7.0 * zLo,
              "...over 8x of RPM, an 8x change in the samples themselves");
    }
}

int main(void)
{
    test_lookup();
    test_wiring();

    if (!have_disc()) {
        printf("\nSKIP: needs %s -- run tools/extract_assets.sh\n"
               "      (see README, 'Asset policy'; nothing is committed)\n",
               BR_SFXOUT_DIR_DEFAULT);
        printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
        return g_fail;
    }

    test_bank();
    test_engine_sweep();

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
