/* br_sfxout.c -- the bank loader, the engine driver and the harness modes.
 * See br_sfxout.h for what each piece corresponds to in the original.
 *
 * The rule this file exists to obey: every number that reaches the mixer was
 * computed by ported code.  Frequencies come from br_sfx.c's engine curve and
 * 32.32 arithmetic; pan and volume come from slice1_08.c's BrSndVoiceSetLevels
 * out of a packed level pair.  This file chooses WHEN, never WHAT.
 */
#include "br_sfxout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * lifecycle
 * ---------------------------------------------------------------------- */

int BrSfxOutOpen(BrSfxOut *pOut)
{
    if (pOut == NULL)
        return -1;
    memset(pOut, 0, sizeof(*pOut));

    pOut->pMix = BrMixCreate();
    if (pOut->pMix == NULL)
        return -1;

    /* The three gates every BrSnd* entry point tests.  BrSndG0B5DE8 is
     * PlaySFX= out of the ini and defaults to 1; the other two are the device
     * and the one-shot init guard. */
    BrSndPDS      = BrMixDevice(pOut->pMix);
    BrSndG18290FC = pOut->pMix;
    pOut->iCar    = 0;
    return 0;
}

void BrSfxOutClose(BrSfxOut *pOut)
{
    int i;

    if (pOut == NULL)
        return;

    for (i = 0; i < pOut->cOwned; ++i) {
        BrSndVoice *pVoice = pOut->aOwned[i];
        if (pVoice == NULL)
            continue;
        BrSndVoiceStop(pVoice);
        BrSndVoiceRelease(pVoice);
        BrSndFreeHook(pVoice->pFormat);
        BrSndFreeHook(pVoice->pData);
        BrSndFreeHook(pVoice);
    }
    pOut->cOwned = 0;

    memset(BrSndVoices, 0, sizeof(BrSndVoices));

    if (pOut->pMix != NULL) {
        BrMixDestroy(pOut->pMix);
        pOut->pMix = NULL;
    }
    BrSndPDS      = NULL;
    BrSndG18290FC = NULL;
}

/* ------------------------------------------------------------------------
 * loading
 * ---------------------------------------------------------------------- */

/* The slot the shipped bank marks for a generic group -- 1 for most,
 * 3 for beep / beep2 / water.  -1 when the row marks none, which is the three
 * per-car engine rows. */
static int marked_slot(int group)
{
    int s;

    for (s = 0; s < BR_SFX_SLOTS; ++s)
        if (BrSfxGroupSlotUsed(group, s))
            return s;
    return -1;
}

/* Load one file into BrSndVoices[group*18 + slot].  Returns 1 on success,
 * 0 when the file is not there or is not a PCM wave this mixer plays. */
static int load_one(BrSfxOut *pOut, const char *pszPath, int group, int slot)
{
    BrMixWaveFormat fmt;
    void           *pPcm = NULL;
    uint32_t        cbPcm = 0;
    BrSndVoice     *pVoice;
    int             idx = BrSfxVoiceIndex(group, slot);

    if (idx < 0 || pOut->cOwned >= (int)(sizeof(pOut->aOwned) / sizeof(pOut->aOwned[0])))
        return 0;
    if (BrMixWavLoad(pszPath, &fmt, &pPcm, &cbPcm) != 0)
        return 0;

    pVoice = (BrSndVoice *)calloc(1, sizeof(*pVoice));
    if (pVoice == NULL) { free(pPcm); return 0; }

    if (BrMixVoiceInit(pVoice, &fmt, pPcm, cbPcm) != 0) {
        free(pVoice); free(pPcm); return 0;
    }
    /* 0x100722D0.  On failure it has already released and NULLed pBuf. */
    if (BrSndVoiceCreate(pVoice) != 0) {
        BrSndFreeHook(pVoice->pFormat);
        free(pPcm);
        free(pVoice);
        return 0;
    }

    /* A group's base rate is a property of the GROUP, not of the file, and
     * the two do not always agree (beep.wav is 11025 in a row whose rate is
     * 11000).  The voice is seeded with the file's own rate by
     * BrMixVoiceInit; the retuning code overwrites f0C from the bank's rate
     * whenever it touches the channel, exactly as 0x1006B530 does. */
    BrSndVoices[idx]           = pVoice;
    pOut->aOwned[pOut->cOwned++] = pVoice;
    return 1;
}

int BrSfxOutLoadSet(BrSfxOut *pOut, int set, const char *pszDir)
{
    char buf[BR_SFXOUT_PATH_MAX];
    int  group, n = 0;

    if (pOut == NULL || pOut->pMix == NULL)
        return 0;
    if (pszDir == NULL)
        pszDir = BR_SFXOUT_DIR_DEFAULT;
    pOut->set = set;

    /* `for (row = 1; row < count - 1; row++)` -- 0x1006C290's own bound, which
     * is why the race set stops at 23 and never reaches the per-car rows. */
    for (group = 1; group < BrSfxGroupCount(set) - 1; ++group) {
        int slot = marked_slot(group);

        if (slot < 0)
            continue;                    /* 0x1006C290 skips a zero bank entry */
        if (BrSfxGroupFileName(set, group, pszDir, buf, sizeof(buf)) < 0)
            continue;
        if (load_one(pOut, buf, group, slot))
            n++;
        else
            pOut->cMissing++;
    }
    pOut->cLoaded += n;
    return n;
}

int BrSfxOutLoadCar(BrSfxOut *pOut, int iName, int iCar, const char *pszDir)
{
    static const int aGroup[3] = {
        BR_SFX_GROUP_ENGINE, BR_SFX_GROUP_ENGINE_HIGH, BR_SFX_GROUP_ENGINE_REV
    };
    char buf[BR_SFXOUT_PATH_MAX];
    int  i, n = 0;
    int  ch = BrSfxCarChannel(iCar);

    if (pOut == NULL || pOut->pMix == NULL || ch < 0)
        return 0;
    if (pszDir == NULL)
        pszDir = BR_SFXOUT_DIR_DEFAULT;

    for (i = 0; i < 3; ++i) {
        if (BrSfxCarFileName(aGroup[i], iName, pszDir, buf, sizeof(buf)) < 0)
            continue;
        if (load_one(pOut, buf, aGroup[i], ch))
            n++;
        else
            pOut->cMissing++;
    }
    if (n > 0)
        pOut->iCar = iName;
    pOut->cLoaded += n;
    return n;
}

/* ------------------------------------------------------------------------
 * playback
 * ---------------------------------------------------------------------- */

int BrSfxOutPlayGroup(BrSfxOut *pOut, int set, int group, uint32_t packed,
                      int loop)
{
    int slot;

    (void)set;
    if (pOut == NULL)
        return 0;
    slot = marked_slot(group);
    if (slot < 0)
        return 0;

    /* 0x10072A90: sets the levels from the packed pair, then starts.  It
     * returns 1 both on success and when sound is disabled, so the voice is
     * checked directly rather than trusting that code. */
    if (BrSndVoices[BrSfxVoiceIndex(group, slot)] == NULL)
        return 0;
    return BrSndPlayEx(group, slot, packed, loop) != 0;
}

int BrSfxOutEngineStart(BrSfxOut *pOut, int iCar, uint32_t packed)
{
    static const int aGroup[3] = {
        BR_SFX_GROUP_ENGINE, BR_SFX_GROUP_ENGINE_HIGH, BR_SFX_GROUP_ENGINE_REV
    };
    int i, n = 0;
    int ch = BrSfxCarChannel(iCar);

    if (pOut == NULL || ch < 0)
        return 0;

    /* All three at once: that is what the original does, and it is why one
     * car needs three .wav files rather than one.  0x1006BDD0 picks between
     * the low and rev layers per frame; both are running either way. */
    for (i = 0; i < 3; ++i) {
        int idx = BrSfxVoiceIndex(aGroup[i], ch);
        if (idx < 0 || BrSndVoices[idx] == NULL)
            continue;
        if (BrSndPlayEx(aGroup[i], ch, packed, 1) != 0)
            n++;
    }
    return n;
}

/* Apply an absolute frequency to one engine layer, through the ported
 * setter.  Returns the frequency actually applied. */
static uint32_t apply_hz(int group, int ch, uint32_t hz)
{
    int         idx = BrSfxVoiceIndex(group, ch);
    BrSndVoice *pVoice;

    if (idx < 0)
        return 0;
    pVoice = BrSndVoices[idx];
    if (pVoice == NULL)
        return 0;

    pVoice->f0C = hz;
    BrSndVoiceApplyFreq(pVoice);     /* 0x100724B0 -> SetFrequency(f0C) */
    return hz;
}

uint32_t BrSfxOutEngineSetRpm(BrSfxOut *pOut, int iCar, float rpm,
                              float doppler)
{
    double   hz;
    int64_t  ratio;
    uint32_t fLow, fHigh;
    int      ch = BrSfxCarChannel(iCar);

    if (pOut == NULL || ch < 0)
        return 0;

    /* The curve, whole, from br_sfx.c.  Nothing below recomputes any of it. */
    hz    = BrSfxEngineHz(rpm, doppler);
    ratio = BrSfxEngineRatio(hz);

    /* ...and back out through the channel's own base rate, which is 11025 for
     * both the low and the rev row.  This round trip is not the identity and
     * that is deliberate: it is where the shipped 11025/11000 mistuning and
     * the documented one-hertz truncation loss actually happen. */
    fLow  = BrSfxHzFromRatio(ratio, BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE));
    fHigh = BrSfxEngineHighHz(doppler);

    apply_hz(BR_SFX_GROUP_ENGINE, ch, fLow);
    apply_hz(BR_SFX_GROUP_ENGINE_REV, ch,
             BrSfxHzFromRatio(ratio, BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE_REV)));
    apply_hz(BR_SFX_GROUP_ENGINE_HIGH, ch, fHigh);

    return fLow;
}

/* ------------------------------------------------------------------------
 * rendering
 * ---------------------------------------------------------------------- */

int BrSfxOutRender(BrSfxOut *pOut, int cFrames, int cBlock,
                   BrSfxSinkFn pfnSink, void *pSinkUser,
                   BrSfxStepFn pfnStep, void *pStepUser)
{
    int16_t *pBlk;
    int      done = 0;

    if (pOut == NULL || pOut->pMix == NULL || cFrames <= 0)
        return 0;
    if (cBlock <= 0)
        cBlock = BR_MIX_RATE / 100;      /* 10 ms */

    pBlk = (int16_t *)malloc((size_t)cBlock * BR_MIX_CHANNELS * sizeof(int16_t));
    if (pBlk == NULL)
        return 0;

    while (done < cFrames) {
        int n = cFrames - done;
        if (n > cBlock) n = cBlock;

        if (pfnStep != NULL)
            pfnStep(pStepUser, (double)done / (double)BR_MIX_RATE);

        BrMixRender(pOut->pMix, pBlk, n);
        done += n;

        if (pfnSink != NULL && pfnSink(pSinkUser, pBlk, n) == 0)
            break;
    }

    free(pBlk);
    return done;
}

int BrSfxCaptureSink(void *pUser, const int16_t *pPcm, int cFrames)
{
    BrSfxCapture *pCap = (BrSfxCapture *)pUser;

    if (pCap == NULL || pCap->failed)
        return 0;
    if (pCap->cFrames + cFrames > pCap->cCap) {
        int      cNew = (pCap->cCap ? pCap->cCap * 2 : BR_MIX_RATE);
        int16_t *p;
        while (cNew < pCap->cFrames + cFrames)
            cNew *= 2;
        p = (int16_t *)realloc(pCap->pPcm,
                               (size_t)cNew * BR_MIX_CHANNELS * sizeof(int16_t));
        if (p == NULL) { pCap->failed = 1; return 0; }
        pCap->pPcm = p;
        pCap->cCap = cNew;
    }
    memcpy(pCap->pPcm + (size_t)pCap->cFrames * BR_MIX_CHANNELS, pPcm,
           (size_t)cFrames * BR_MIX_CHANNELS * sizeof(int16_t));
    pCap->cFrames += cFrames;
    return 1;
}

void BrSfxCaptureFree(BrSfxCapture *pCap)
{
    if (pCap == NULL)
        return;
    free(pCap->pPcm);
    pCap->pPcm    = NULL;
    pCap->cFrames = 0;
    pCap->cCap    = 0;
}

int BrSfxCaptureWrite(const BrSfxCapture *pCap, const char *pszPath)
{
    BrMixWaveFormat fmt;

    if (pCap == NULL || pCap->pPcm == NULL || pCap->cFrames <= 0)
        return -1;
    BrMixOutputFormat(&fmt);
    return BrMixWavWrite(pszPath, &fmt, pCap->pPcm,
                         (uint32_t)pCap->cFrames * BR_MIX_CHANNELS
                             * (uint32_t)sizeof(int16_t));
}

/* ------------------------------------------------------------------------
 * name lookup
 * ---------------------------------------------------------------------- */

static int name_eq(const char *pszA, const char *pszB)
{
    size_t a, b;

    if (pszA == NULL || pszB == NULL)
        return 0;
    if (strcmp(pszA, pszB) == 0)
        return 1;
    /* ...or the same with the .wav the table carries and a user would not
     * type. */
    a = strlen(pszA);
    b = strlen(pszB);
    if (b > 4 && strcmp(pszB + b - 4, ".wav") == 0 && b - 4 == a)
        return memcmp(pszA, pszB, a) == 0;
    return 0;
}

int BrSfxLookupGroup(const char *pszName, int *pSet, int *pGroup)
{
    static const int aSet[2] = { BR_SFX_SET_RACE, BR_SFX_SET_MENU };
    int i, g;

    if (pszName == NULL || pSet == NULL || pGroup == NULL)
        return -1;

    if (pszName[0] >= '0' && pszName[0] <= '9') {
        g = atoi(pszName);
        if (g < 0 || g >= BR_SFX_GROUPS)
            return -1;
        *pSet   = BR_SFX_SET_RACE;
        *pGroup = g;
        return 0;
    }

    /* The race set is searched first because the two sets OVERLAP in group
     * number -- group 1 is hit-another-car1 in one and front-end5 in the
     * other -- so a bare name has to name the set as well as the row. */
    for (i = 0; i < 2; ++i)
        for (g = 1; g < BrSfxGroupCount(aSet[i]) - 1; ++g)
            if (name_eq(pszName, BrSfxGroupName(aSet[i], g))) {
                *pSet   = aSet[i];
                *pGroup = g;
                return 0;
            }
    return -1;
}

int BrSfxLookupCar(const char *pszName)
{
    int i;

    if (pszName == NULL)
        return 0;
    if (pszName[0] >= '0' && pszName[0] <= '9') {
        i = atoi(pszName);
        return (i >= 1 && i <= BR_SFX_CARS) ? i : 0;
    }
    for (i = 1; i <= BR_SFX_CARS; ++i)
        if (strcmp(pszName, BrSfxCarCode[i]) == 0)
            return i;
    return 0;
}

/* ------------------------------------------------------------------------
 * the harness modes
 * ---------------------------------------------------------------------- */

/* Long enough for any sample on the disc (the longest is under three
 * seconds) plus a tail, so a one-shot is never cut off. */
#define BR_SFX_DEMO_SECONDS   4
#define BR_SFX_SWEEP_SECONDS  8

int BrSfxDemoPlay(const char *pszGroup, const char *pszWavOut,
                  BrSfxSinkFn pfnSink, void *pSinkUser)
{
    BrSfxOut     out;
    BrSfxCapture cap;
    int          set = 0, group = 0;
    uint32_t     packed;
    int          rc = 0;

    if (BrSfxLookupGroup(pszGroup, &set, &group) != 0) {
        fprintf(stderr, "sfx: no such group '%s'\n", pszGroup);
        return 2;
    }
    if (BrSfxOutOpen(&out) != 0) {
        fprintf(stderr, "sfx: mixer would not start\n");
        return 1;
    }
    memset(&cap, 0, sizeof(cap));

    BrSfxOutLoadSet(&out, set, NULL);
    if (out.cLoaded == 0) {
        fprintf(stderr,
                "sfx: SKIP -- no samples under %s.  Run tools/extract_assets.sh\n"
                "     (see README, 'Asset policy'); nothing is committed.\n",
                BR_SFXOUT_DIR_DEFAULT);
        BrSfxOutClose(&out);
        return 3;
    }

    printf("sfx: set %s, group %d = %s, base rate %.0f Hz\n",
           (set == BR_SFX_SET_RACE) ? "race" : "menu", group,
           BrSfxGroupName(set, group), BrSfxGroupBaseRate(group));
    printf("     %d voices loaded, %d names missing\n", out.cLoaded, out.cMissing);

    /* Dead centre: both halves of the pair at the BR_SFX_LEVEL_MAX ceiling,
     * which BrSndVoiceSetLevels turns into f10 = 400 (centre) and f14 = 400
     * (unity). */
    packed = ((uint32_t)BR_SFX_LEVEL_MAX << 16) | (uint32_t)BR_SFX_LEVEL_MAX;

    if (!BrSfxOutPlayGroup(&out, set, group, packed, 0)) {
        fprintf(stderr, "sfx: group %d has no voice (file missing?)\n", group);
        rc = 3;
    } else {
        /* Render exactly as long as the sample lasts at the rate it is being
         * played, plus a short tail -- a fixed four seconds would make most
         * of the demo silence and, worse, make the speaker path sit there for
         * four seconds on a 200 ms beep. */
        int slot   = -1;
        int cFrames = BR_SFX_DEMO_SECONDS * BR_MIX_RATE;
        int s;

        for (s = 0; s < BR_SFX_SLOTS; ++s)
            if (BrSfxGroupSlotUsed(group, s)) { slot = s; break; }
        if (slot >= 0) {
            BrSndVoice *pV = BrSndVoices[BrSfxVoiceIndex(group, slot)];
            const BrMixWaveFormat *pF =
                (pV != NULL) ? (const BrMixWaveFormat *)pV->pFormat : NULL;
            if (pF != NULL && pF->nBlockAlign != 0 && pV->f0C != 0) {
                double sec = (double)(pV->nDataBytes / pF->nBlockAlign)
                           / (double)pV->f0C;
                int n = (int)(sec * BR_MIX_RATE) + BR_MIX_RATE / 10;
                if (n > 0 && n < cFrames) cFrames = n;
                printf("     %.2f s of sample at %u Hz\n", sec, pV->f0C);
            }
        }

        BrSfxOutRender(&out, cFrames,
                       BR_MIX_RATE / 50, BrSfxCaptureSink, &cap, NULL, NULL);
        if (pfnSink != NULL && cap.pPcm != NULL)
            pfnSink(pSinkUser, cap.pPcm, cap.cFrames);
        if (pszWavOut != NULL && BrSfxCaptureWrite(&cap, pszWavOut) == 0)
            printf("     wrote %s (%d frames, %d Hz stereo)\n",
                   pszWavOut, cap.cFrames, BR_MIX_RATE);
    }

    BrSfxCaptureFree(&cap);
    BrSfxOutClose(&out);
    return rc;
}

typedef struct BrSfxSweep {
    BrSfxOut *pOut;
    int       iCar;
    double    seconds;
    uint32_t  hzFirst, hzLast, hzPeak;
    int       nSteps;
} BrSfxSweep;

/* The sweep drives the INPUT to the ported curve and reads back what the
 * curve produced.  It computes no frequency of its own. */
static void sweep_step(void *pUser, double t)
{
    BrSfxSweep *pS = (BrSfxSweep *)pUser;
    double      u  = t / pS->seconds;
    float       rpm;
    uint32_t    hz;

    if (u > 1.0) u = 1.0;
    /* 800 RPM idle up to the 8000 the gearbox writes as redline, and back --
     * the two literals at 0x1006278F and 0x10062AFE. */
    rpm = (float)(800.0 + 7200.0 * (u < 0.5 ? u * 2.0 : (1.0 - u) * 2.0));

    hz = BrSfxOutEngineSetRpm(pS->pOut, pS->iCar, rpm, 1.0f);
    if (pS->nSteps == 0) pS->hzFirst = hz;
    if (hz > pS->hzPeak) pS->hzPeak = hz;
    pS->hzLast = hz;
    pS->nSteps++;
}

int BrSfxDemoRpmSweep(const char *pszCar, const char *pszWavOut,
                      BrSfxSinkFn pfnSink, void *pSinkUser)
{
    BrSfxOut     out;
    BrSfxCapture cap;
    BrSfxSweep   sweep;
    int          iName;
    uint32_t     packed;

    iName = BrSfxLookupCar(pszCar != NULL ? pszCar : "ce");
    if (iName == 0) {
        fprintf(stderr, "sfx: no such car '%s'\n", pszCar);
        return 2;
    }
    if (BrSfxOutOpen(&out) != 0) {
        fprintf(stderr, "sfx: mixer would not start\n");
        return 1;
    }
    memset(&cap, 0, sizeof(cap));

    if (BrSfxOutLoadCar(&out, iName, 0, NULL) == 0) {
        fprintf(stderr,
                "sfx: SKIP -- no engine samples for car '%s' under %s.\n"
                "     Run tools/extract_assets.sh (see README, 'Asset policy').\n",
                BrSfxCarCode[iName], BR_SFXOUT_DIR_DEFAULT);
        BrSfxOutClose(&out);
        return 3;
    }

    printf("sfx: car %s -- %s.wav + %sh.wav + %sr.wav, %d of 3 loaded\n",
           BrSfxCarCode[iName], BrSfxCarCode[iName], BrSfxCarCode[iName],
           BrSfxCarCode[iName], out.cLoaded);

    packed = ((uint32_t)BR_SFX_LEVEL_MAX << 16) | (uint32_t)BR_SFX_LEVEL_MAX;
    BrSfxOutEngineStart(&out, 0, packed);

    memset(&sweep, 0, sizeof(sweep));
    sweep.pOut    = &out;
    sweep.iCar    = 0;
    sweep.seconds = BR_SFX_SWEEP_SECONDS;

    BrSfxOutRender(&out, BR_SFX_SWEEP_SECONDS * BR_MIX_RATE,
                   BR_MIX_RATE / 100, BrSfxCaptureSink, &cap,
                   sweep_step, &sweep);

    printf("     RPM 800 -> 8000 -> 800 over %ds in %d steps\n",
           BR_SFX_SWEEP_SECONDS, sweep.nSteps);
    printf("     low layer %u Hz at idle, %u Hz at redline "
           "(base rate %.0f, so x%.2f)\n",
           sweep.hzFirst, sweep.hzPeak,
           BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE),
           (double)sweep.hzPeak / BrSfxGroupBaseRate(BR_SFX_GROUP_ENGINE));
    printf("     high layer fixed at %u Hz -- it is doppler only, not RPM\n",
           BrSfxEngineHighHz(1.0f));
    if (BrMixClipCount(out.pMix) > 0)
        printf("     %ld samples clipped: three loops at the level pair's "
               "unity ceiling sum past full scale, as they did on DirectSound\n",
               BrMixClipCount(out.pMix));

    if (pfnSink != NULL && cap.pPcm != NULL)
        pfnSink(pSinkUser, cap.pPcm, cap.cFrames);
    if (pszWavOut != NULL && BrSfxCaptureWrite(&cap, pszWavOut) == 0)
        printf("     wrote %s (%d frames, %d Hz stereo)\n",
               pszWavOut, cap.cFrames, BR_MIX_RATE);

    BrSfxCaptureFree(&cap);
    BrSfxOutClose(&out);
    return 0;
}
