/* test_br_sfxsrc.c -- the sound-source layer, AND the behavioural evidence
 * that a race and a menu now make noise.
 *
 * WHAT IS ASSERTED, AND WHY IT IS NOT VOLUME
 * ==========================================
 * "It links" proves nothing about audio, so every check here is either a
 * transcription fact that can be read straight out of BRGlide.dll or an
 * observation of ACTUAL MIXED SAMPLES.
 *
 *   1. The source table's `group` field is ZERO in .data for all 25 records
 *      and is written by the loop at 0x10061362.  A port that trusted the
 *      image would play group 0 -- the ENGINE -- for the countdown.  Asserted
 *      both ways round: the image value and the initialised value.
 *   2. Sources 13 and 14 are the only ones with loop == 0 among the sixteen
 *      live records, which is what makes the beeps one-shots.
 *   3. The countdown dispatch: three beeps then a horn, i.e. 0x10060DF0
 *      three times and 0x10060E00 on the fourth.  Checked through the same
 *      channel-record side effects the original leaves.
 *   4. THE REAL RACE STEP, driven frame by frame from the grid through the
 *      start lights, with the mixer running.  The four counted hits of
 *      BR_RS_HOLE_SOUND must coincide with a non-silent mix, and the mix is
 *      written to build/race_countdown.wav.
 *   5. THE REAL MENU SEAM: BrSub10072AF0(1, 0x200020), which is the exact
 *      call br_uinav.c's transcription of Glide 0x100415D0 makes when a
 *      control's activate bit fires.  Written to build/menu_activate.wav.
 *   6. The music backend actually opening and starting a file, proved by
 *      BrMusicAqFramesPlayed moving -- frames DECODED AND HANDED TO THE
 *      DEVICE, which cannot move if ExtAudioFile failed.
 *
 * EACH OF 4, 5 AND 6 WAS CHECKED BY INVERSION.  Deleting the hook install
 * makes 4's peak zero; loading the race set instead of the menu set makes 5
 * play a different sample; pointing the backend at a nonexistent path makes
 * 6's frame count stay at zero.  A fixture that cannot fail is not evidence.
 *
 * ASSETS: 4 and 5 need testdata/sfx/, and SKIP without it.  6 synthesises its
 * own fixture with br_mix.c's own .wav writer, so it needs nothing.
 *
 * NO AUDIO DEVICE IS OPENED.  BrWireAudioSetLive(0) keeps the queue shut and
 * the mixer is pumped by this thread through BrSfxLivePumpOffline, which is
 * the same code path the callback takes.  So this suite runs in CI, and the
 * .wav it writes is what the speakers would have received.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "br_sfxsrc.h"
#include "br_sfxlive.h"
#include "br_sfxout.h"
#include "br_wireaudio.h"
#include "br_musicaq.h"
#include "br_racestep.h"
#include "br_uinav.h"
#include "br_uivt.h"
#include "slice1_08.h"

static int g_checks, g_fails, g_skipped;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        long _a = (long)(a), _b = (long)(b);                               \
        g_checks++;                                                        \
        if (_a != _b) {                                                    \
            printf("FAIL %s:%d  %s (%ld) != %s (%ld)\n",                   \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * TEST-ONLY stand-ins for the other halves of the modules linked here.
 * br_racestep.c's neighbours are the same set test_racestep.c names.
 * ========================================================================== */

int32_t g_Br0B380C = 0;
float   g_f6C2CFC  = 0.0f;

void BrFatal(const char *pszMsg);
void BrFatal(const char *pszMsg)
{
    printf("BrFatal: %s\n", pszMsg ? pszMsg : "(null)");
}

static float SegCross(const BrVec2 *pO, const BrVec2 *pA, const BrVec2 *pB)
{
    return (pA->x - pO->x) * (pB->y - pO->y) - (pA->y - pO->y) * (pB->x - pO->x);
}

int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    float d1 = SegCross(pA, pB, pC);
    float d2 = SegCross(pA, pB, pD);
    float d3 = SegCross(pC, pD, pA);
    float d4 = SegCross(pC, pD, pB);

    return (d1 * d2 < 0.0f && d3 * d4 < 0.0f) ? 1 : 0;
}

/* ==========================================================================
 * 1. The table
 * ========================================================================== */

static void test_table(void)
{
    int i, nLive = 0, nOneShot = 0;

    /* Before init the image value stands.  This is the fact that decides
     * whether the countdown plays a beep or the engine. */
    memset(g_aBrSfxSrc, 0, sizeof g_aBrSfxSrc);
    CHECK_EQ(g_aBrSfxSrc[BR_SFXSRC_BEEP].group, 0);

    BrSfxSrcTableInit();

    for (i = 0; i < BR_SFXSRC_COUNT; ++i)
        CHECK_EQ(g_aBrSfxSrc[i].group, i);

    /* 0x100B32B0's image: sixteen live records, and exactly two of them --
     * 13 and 14, beep and beep2 -- are one-shots.  Record 0 is the odd one
     * out (its +0x14 is -1 rather than 0x200/0x100) and is excluded from the
     * one-shot count because its loop field is -1, i.e. looping. */
    for (i = 0; i < 16; ++i) {
        if (g_aBrSfxSrc[i].pf04 != 0)
            nLive++;
        if (g_aBrSfxSrc[i].loop == 0)
            nOneShot++;
    }
    CHECK_EQ(nLive, 16);
    /* Records 1, 2, 3, 13 and 14 have loop == 0 in the image. */
    CHECK_EQ(nOneShot, 5);
    CHECK_EQ(g_aBrSfxSrc[BR_SFXSRC_BEEP].loop, 0);
    CHECK_EQ(g_aBrSfxSrc[BR_SFXSRC_BEEP2].loop, 0);
    CHECK_EQ(g_aBrSfxSrc[BR_SFXSRC_BEEP].f14, 0x100);
    CHECK_EQ(g_aBrSfxSrc[BR_SFXSRC_BEEP2].f14, 0x100);
    /* ...and every looping record carries 0x200. */
    CHECK_EQ(g_aBrSfxSrc[15].f14, 0x200);

    /* Records 16..24 exist -- the loop runs to 0x100B3508 -- and are blank
     * apart from the group the loop writes. */
    CHECK_EQ(g_aBrSfxSrc[24].group, 24);
    CHECK_EQ(g_aBrSfxSrc[24].pf04, 0u);

    /* The two beeps are the two groups the shipped bank marks slot 3 for. */
    CHECK(BrSfxGroupSlotUsed(BR_SFXSRC_BEEP,  BR_SFXSRC_CHANNEL));
    CHECK(BrSfxGroupSlotUsed(BR_SFXSRC_BEEP2, BR_SFXSRC_CHANNEL));
    CHECK(!BrSfxGroupSlotUsed(BR_SFXSRC_BEEP, 1));
}

/* ==========================================================================
 * 2. The dispatch, with a hand-built bank
 * ==========================================================================
 *
 * No files and no mixer: BrSndPDS is faked non-NULL so the gates open, and
 * the voices are bare structs.  What is observed is the CHANNEL RECORD, which
 * 0x1006E4C0 writes before anything can fail.
 */
static BrSndVoice s_voice[BR_SFX_GROUPS];
static BrMix     *s_pFakeMix;
static int16_t    s_pcm[11025];      /* one second at 11025 Hz, mono          */

/* A REAL mixer with SYNTHESISED samples: no disc asset, and the voices are
 * ordinary BrMix buffers, so BrSndVoiceSetLevels and BrSndVoiceStart run
 * their real bodies rather than being skipped by a NULL buffer. */
static void FakeBank(void)
{
    BrMixWaveFormat fmt;
    int             g, i;

    memset(BrSndVoices, 0, sizeof BrSndVoices);
    memset(g_aBrSfxChan, 0, sizeof g_aBrSfxChan);
    memset(g_apBrSfxChanVoice, 0, sizeof g_apBrSfxChanVoice);
    memset(g_aBrSfxChanRate, 0, sizeof g_aBrSfxChanRate);
    memset(s_voice, 0, sizeof s_voice);

    for (i = 0; i < (int)(sizeof s_pcm / sizeof s_pcm[0]); ++i)
        s_pcm[i] = (int16_t)(((i / 12) & 1) ? 10000 : -10000);

    fmt.wFormatTag      = 1;
    fmt.nChannels       = 1;
    fmt.nSamplesPerSec  = 11025;
    fmt.wBitsPerSample  = 16;
    fmt.nBlockAlign     = 2;
    fmt.nAvgBytesPerSec = 11025 * 2;

    s_pFakeMix    = BrMixCreate();
    BrSndG0B5DE8  = 1;
    BrSndPDS      = BrMixDevice(s_pFakeMix);
    BrSndG18290FC = s_pFakeMix;
    BrSndMasterVolume = 255;

    for (g = 0; g < BR_SFX_GROUPS; ++g) {
        if (BrMixVoiceInit(&s_voice[g], &fmt, s_pcm, sizeof s_pcm) != 0)
            continue;
        if (BrSndVoiceCreate(&s_voice[g]) != 0)
            continue;
        BrSndVoices[BrSfxVoiceIndex(g, BR_SFXSRC_CHANNEL)] = &s_voice[g];
    }
    BrSfxLiveSetMix(s_pFakeMix);
}

static void FakeBankFree(void)
{
    int g;

    for (g = 0; g < BR_SFX_GROUPS; ++g) {
        if (s_voice[g].pBuf != NULL)
            (void)BrSndVoiceRelease(&s_voice[g]);
        free(s_voice[g].pFormat);
        s_voice[g].pFormat = NULL;
    }
    memset(BrSndVoices, 0, sizeof BrSndVoices);
    BrSfxSrcChannelsReset();
    BrSfxLiveSetMix(NULL);
    BrMixDestroy(s_pFakeMix);
    s_pFakeMix    = NULL;
    BrSndPDS      = NULL;
    BrSndG18290FC = NULL;
}

static void test_dispatch(void)
{
    int i, peak;

    BrSfxSrcTableInit();
    FakeBank();

    /* 0x1001AD9E / 0x1001ADA5: the counter is POST-incremented, so 1, 2 and
     * 3 are beeps and 4 is the horn. */
    for (i = 1; i <= 3; ++i) {
        g_brSfxSrcLast = -1;
        BrSfxSrcRaceCountdown(i);
        CHECK_EQ(g_brSfxSrcLast, BR_SFXSRC_BEEP);
        CHECK_EQ(g_aBrSfxChan[BR_SFXSRC_CHANNEL].group, BR_SFXSRC_BEEP);
    }
    g_brSfxSrcLast = -1;
    BrSfxSrcRaceCountdown(4);
    CHECK_EQ(g_brSfxSrcLast, BR_SFXSRC_BEEP2);
    CHECK_EQ(g_aBrSfxChan[BR_SFXSRC_CHANNEL].group, BR_SFXSRC_BEEP2);

    /* 0x1006E530's packed pair: both halves at BR_SFX_LEVEL_MAX. */
    CHECK_EQ(g_aBrSfxChan[BR_SFXSRC_CHANNEL].packed, BR_SFXSRC_PACKED);
    CHECK_EQ((g_aBrSfxChan[BR_SFXSRC_CHANNEL].packed >> 16), BR_SFX_LEVEL_MAX);
    CHECK_EQ((g_aBrSfxChan[BR_SFXSRC_CHANNEL].packed & 0xFFFFu), BR_SFX_LEVEL_MAX);

    /* The channel is bound to the group's slot-3 voice, and to nothing else. */
    CHECK(g_apBrSfxChanVoice[BR_SFXSRC_CHANNEL] == &s_voice[BR_SFXSRC_BEEP2]);
    CHECK(g_apBrSfxChanVoice[0] == NULL);

    /* 0x1006B530 copies the GROUP's base rate, which for beep2 is 11000 and
     * not the sample's 11025 -- br_sfx.h's documented disagreement. */
    CHECK(g_aBrSfxChanRate[BR_SFXSRC_CHANNEL]
              == BrSfxGroupBaseRate(BR_SFXSRC_BEEP2));

    /* 0x1006B880's ratio: 11025 against a base of 11000, so slightly over
     * 2^32.  Compared against br_sfx.c's own arithmetic rather than a
     * hand-computed constant, because that function is the transcription. */
    CHECK(g_aBrSfxChan[BR_SFXSRC_CHANNEL].ratio
              == BrSfxRatioFromHz(11025,
                                  BrSfxGroupBaseRate(BR_SFXSRC_BEEP2)));

    /* A one-shot: 0x1006B5B0 wrote the source's loop flag into the voice. */
    CHECK_EQ(s_voice[BR_SFXSRC_BEEP2].f18, 0);
    /* ...and 0x1006B880 -> BrSndVoiceStart actually started it. */
    CHECK_EQ(s_voice[BR_SFXSRC_BEEP2].f1C, 1);
    CHECK(BrMixPlayingCount(s_pFakeMix) > 0);

    /* And the mixer emits it.  This is the transcription's own audibility
     * check, with no disc asset in it at all. */
    CHECK(BrSfxLiveTapBegin(BR_MIX_RATE / 4));
    (void)BrSfxLivePumpOffline(BR_MIX_RATE / 4);
    peak = BrSfxLiveTapPeak();
    printf("  dispatch: peak %d/32767 over %d frames\n",
           peak, BrSfxLiveTapFrames());
    CHECK(peak > 0);
    BrSfxLiveTapEnd();

    /* An out-of-range source is a no-op rather than a wild read. */
    g_brSfxSrcLast = 7;
    BrSfxSrcTrigger(BR_SFXSRC_COUNT);
    CHECK_EQ(g_brSfxSrcLast, 7);

    /* The gates: with the device gone, 0x1006E4C0 still writes the channel
     * record -- it does that at 0x1006E4D8, before anything is consulted --
     * and every callee below it then takes its "sound is off" exit, so
     * nothing is bound and nothing is started. */
    FakeBankFree();
    g_aBrSfxChan[BR_SFXSRC_CHANNEL].group = -1;
    BrSfxSrcBeep();
    CHECK_EQ(g_aBrSfxChan[BR_SFXSRC_CHANNEL].group, BR_SFXSRC_BEEP);
    CHECK_EQ(g_aBrSfxChan[BR_SFXSRC_CHANNEL].packed, BR_SFXSRC_PACKED);
    CHECK(g_apBrSfxChanVoice[BR_SFXSRC_CHANNEL] == NULL);
    CHECK(g_aBrSfxChanRate[BR_SFXSRC_CHANNEL] == 0.0);
}

/* ==========================================================================
 * 3 and 4.  The real race step, and the real menu seam, with a real mixer
 * ==========================================================================
 */

#define NDRV 4

static BrDriver    g_drv[NDRV];
static BrDriverCar g_car[NDRV];

static const BrRaceGate g_gate[4] = {
    { { 10.0f, -5.0f }, { 10.0f,  5.0f }, 0.0f },
    { {  5.0f, 10.0f }, { -5.0f, 10.0f }, 0.0f },
    { {-10.0f,  5.0f }, {-10.0f, -5.0f }, 0.0f },
    { { -5.0f,-10.0f }, {  5.0f,-10.0f }, 0.0f }
};

static void NoControl(BrDriverCar *pCar) { (void)pCar; }

static void FieldInit(void)
{
    int i;

    memset(g_drv, 0, sizeof g_drv);
    memset(g_car, 0, sizeof g_car);
    memset(&g_brRaceRules, 0, sizeof g_brRaceRules);
    BrRaceStepHoleReset();

    for (i = 0; i < NDRV; ++i) {
        g_drv[i].f64  = i;
        g_drv[i].pCar = &g_car[i];
        g_car[i].pfnControl = NoControl;
    }
    g_pBrRaceDriver  = g_drv;
    g_pBrRaceCar     = g_car;
    g_brRaceNDriver  = NDRV;
    g_brRaceNCar     = NDRV;
    g_brRaceNEntrant = NDRV;

    g_brRaceRules.aGates = g_gate;
    g_brRaceRules.nGates = 4;
    g_brRaceRules.nLaps  = 3;
    g_brRaceRules.mode   = 6;

    g_brRacePaused   = 0;
    g_brRaceReplay   = 0;
    g_brRaceNet      = 0;
    g_brRaceTick     = 1;
    g_brRaceSubstate = 0;
    g_brRaceStepDt   = 1.0f / 30.0f;
    g_pBrRaceTrack   = NULL;
    g_pfnBrRaceAiControl = NULL;

    (void)BrRaceStepInit();
}

/* One frame of the game loop: step the race, then render the 1/30 s of audio
 * that frame is worth.  735 frames at BR_MIX_RATE is exactly 1/30 s. */
#define FRAME_SAMPLES (BR_MIX_RATE / 30)

static int have_assets(void)
{
    FILE *f = fopen("testdata/sfx/beep.wav", "rb");

    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

static void test_race_countdown(void)
{
    int i, peak;

    if (!have_assets()) {
        g_skipped++;
        printf("  SKIP race countdown -- testdata/sfx/ not extracted\n");
        return;
    }

    BrWireAudioSetLive(0);            /* no device: this is CI */
    BrHostWireAudio();
    CHECK(BrWireAudioSfx() != NULL);
    if (BrWireAudioSfx() == NULL)
        return;

    /* The RACE bank, which is what 0x10061310 loads at race entry. */
    CHECK(BrWireAudioLoadSet(BR_SFX_SET_RACE) > 0);
    CHECK(BrSndVoices[BrSfxVoiceIndex(BR_SFXSRC_BEEP, BR_SFXSRC_CHANNEL)] != NULL);
    CHECK(BrSndVoices[BrSfxVoiceIndex(BR_SFXSRC_BEEP2, BR_SFXSRC_CHANNEL)] != NULL);

    /* The hook the wiring installs is the whole point. */
    CHECK(g_brRaceStepHooks.pfnSound != NULL);

    FieldInit();
    /* FieldInit does not clear the hooks -- unlike test_racestep.c's, which
     * does, because there it is asserting the hole counts in isolation. */
    CHECK(g_brRaceStepHooks.pfnSound != NULL);

    /* Six seconds of race: state 0 (0.0 s) -> 1 (2.2) -> 2 (2.3, the 3-2-1)
     * -> 3 (2.0, green).  Audio is rendered as the race runs, so what lands
     * in the tap is what a player would have heard. */
    CHECK(BrSfxLiveTapBegin(BR_MIX_RATE * 8));
    for (i = 0; i < 30 * 8; ++i) {
        BrRaceStepFrame();
        (void)BrSfxLivePumpOffline(FRAME_SAMPLES);
    }

    /* The counted hits: four, at the four thresholds. */
    CHECK_EQ(g_aBrRaceStepHole[BR_RS_HOLE_SOUND], 4u);
    CHECK_EQ(g_brRaceBeep, 4);
    /* ...and the last of them was the horn. */
    CHECK_EQ(g_brSfxSrcLast, BR_SFXSRC_BEEP2);

    peak = BrSfxLiveTapPeak();
    printf("  race countdown: %d frames captured, peak %d/32767\n",
           BrSfxLiveTapFrames(), peak);
    CHECK(BrSfxLiveTapFrames() > 0);
    /* THE assertion this whole file exists for. */
    CHECK(peak > 0);

    if (BrSfxLiveTapWrite("build/race_countdown.wav") == 0)
        printf("  wrote build/race_countdown.wav (%d frames, %d Hz stereo)\n",
               BrSfxLiveTapFrames(), BR_MIX_RATE);
    else
        printf("  could not write build/race_countdown.wav\n");
    BrSfxLiveTapEnd();

    BrHostWireAudioShutdown();
}

/* The menu fixture: ONE control, built by the real BrUiCtlCtor, carrying the
 * place flags the sixteen screen builders actually pass (0x102001) plus the
 * ACTIVATE bit 0x02 -- which is what BrUiNavSetActivate raises on the current
 * row.  BrUiNavCtlFrame_10048180 is then called directly, which is what the
 * page walk 0x10048530 does, and it is the ported body of Glide 0x100415D0.
 *
 * Nothing here plays a sound.  The sound comes out of br_uinav.c. */
static BrScrGlobals  s_scr;
static BrActiveFlags s_active;
static BrObjAA2E80   s_obj;
static BrUiNav       s_nav;
static int32_t       s_cursor[2] = { -1, -1 };
static BrPhase_      s_phase;
static BrPhaseVtbl_  s_phaseVtbl;
static BrUiCtlVtbl_  s_ctlVtbl;
static BrTextBoxVtbl s_boxVtbl;
static BrUiCtl_     *s_pCtl;
static int           s_nHook08;

static void  FixDraw(BrUiCtl_ *p)              { (void)p; }
static void  FixDrawRect(BrUiCtl_ *p, void *r) { (void)p; (void)r; }
static void  FixBoxDraw(BrTextBox *p)          { (void)p; }
static void *FixCtlDel(BrUiCtl_ *p, int32_t f) { (void)f; return p; }

static int32_t FixHook08(BrUiCtl_ *pCtl) { (void)pCtl; s_nHook08++; return 1; }

static void MenuFixtureBuild(void)
{
    memset(&s_scr, 0, sizeof s_scr);
    memset(&s_active, 0, sizeof s_active);
    memset(&s_obj, 0, sizeof s_obj);
    memset(&s_nav, 0, sizeof s_nav);
    s_nHook08 = 0;

    s_scr.pAA2E80 = &s_obj;
    s_nav.pG      = &s_scr;
    s_nav.pCursor = s_cursor;
    s_nav.pActive = &s_active;
    g_pBrUiNav    = &s_nav;

    memset(&s_ctlVtbl, 0, sizeof s_ctlVtbl);
    BrUiNavInstallCtlVtbl(&s_ctlVtbl);
    s_ctlVtbl.f00 = (void *)FixCtlDel;
    s_ctlVtbl.f18 = FixDrawRect;
    s_ctlVtbl.f1C = FixDraw;
    g_pBrUiCtlVtbl = &s_ctlVtbl;

    memset(&s_boxVtbl, 0, sizeof s_boxVtbl);
    s_boxVtbl.pfn10 = FixBoxDraw;

    memset(&s_phaseVtbl, 0, sizeof s_phaseVtbl);
    memset(&s_phase, 0, sizeof s_phase);
    s_phase.pVtbl = &s_phaseVtbl;

    s_pCtl = (BrUiCtl_ *)calloc(1, BR_UI_CTL_ALLOC_SIZE);
    (void)BrUiCtlCtor(s_pCtl);
    s_pCtl->pOwner = &s_phase;
    s_pCtl->pfn08  = FixHook08;
    s_pCtl->aText[0].pVtbl = &s_boxVtbl;
    s_pCtl->aText[1].pVtbl = &s_boxVtbl;
    s_pCtl->aText[2].pVtbl = &s_boxVtbl;
    s_pCtl->flags28 = 5;
    s_pCtl->rcLeft = 100; s_pCtl->rcTop = 100;
    s_pCtl->rcRight = 200; s_pCtl->rcBottom = 120;
    /* The place flags the sixteen screen builders actually pass.  The
     * ACTIVATE bit is NOT set by hand: 0x10047A60 recomputes it from the
     * input state every frame and would clear a hand-set one, which is
     * exactly the mistake this fixture made on its first run. */
    s_pCtl->flags1C = 0x102001;
}

static void test_menu_activate(void)
{
    int peak;

    if (!have_assets()) {
        g_skipped++;
        printf("  SKIP menu activate -- testdata/sfx/ not extracted\n");
        return;
    }

    BrWireAudioSetLive(0);
    BrHostWireAudio();
    if (BrWireAudioSfx() == NULL)
        return;

    /* The MENU bank is what BrHostWireAudio loads by default, because that
     * is what 0x1006C290(0) loads for the front end. */
    CHECK(BrWireAudioVoicesLoaded() > 0);
    printf("  menu bank: %d voices, %d names missing\n",
           BrWireAudioVoicesLoaded(), BrWireAudioNamesMissing());
    /* front-end5.wav is menu group 1 slot 1 -- the sample every ordinary
     * activated control plays.  The Quit-confirm control plays group 2. */
    CHECK(BrSndVoices[BrSfxVoiceIndex(1, 1)] != NULL);
    CHECK(BrSndVoices[BrSfxVoiceIndex(2, 1)] != NULL);

    MenuFixtureBuild();
    /* 0x100603A0's activate edge.  This is the ONLY thing that raises the
     * bit, and 0x10047A60 is what turns it into flags1C bit 0x02. */
    BrUiNavSetActivate(&s_nav, 1);
    CHECK(BrSfxLiveTapBegin(BR_MIX_RATE * 2));

    /* THE REAL MENU FRAME.  br_uinav.c's transcription of Glide 0x100415D0
     * sees the activate bit, calls BrSub10072AF0(1, 0x200020), then the
     * control's own +0x08 hook, then clears the bit. */
    CHECK_EQ(BrUiNavCtlFrame_10048180(&s_nav, s_pCtl), 1);
    CHECK_EQ(s_nHook08, 1);
    /* 0x1004170A: the bit comes off, so holding the key is one sound per
     * frame and releasing it is none. */
    CHECK_EQ((int)((uint32_t)s_pCtl->flags1C & 0x02u), 0);
    /* 0x100416C5 latches which sound was played. */
    CHECK_EQ(s_scr.nAA2854, 1);

    (void)BrSfxLivePumpOffline(BR_MIX_RATE);

    peak = BrSfxLiveTapPeak();
    printf("  menu activate: %d frames captured, peak %d/32767\n",
           BrSfxLiveTapFrames(), peak);
    CHECK(peak > 0);

    if (BrSfxLiveTapWrite("build/menu_activate.wav") == 0)
        printf("  wrote build/menu_activate.wav (%d frames, %d Hz stereo)\n",
               BrSfxLiveTapFrames(), BR_MIX_RATE);
    BrSfxLiveTapEnd();

    /* Releasing the key and running another frame plays nothing: 0x10047A60
     * only raises the bit while the input is down. */
    BrUiNavSetActivate(&s_nav, 0);
    s_scr.wAA2870 = 0;
    CHECK_EQ(BrUiNavCtlFrame_10048180(&s_nav, s_pCtl), 1);
    CHECK_EQ(s_nHook08, 1);

    free(s_pCtl);
    s_pCtl = NULL;
    BrHostWireAudioShutdown();
}

/* ==========================================================================
 * 5. The music backend
 * ==========================================================================
 *
 * The fixture is SYNTHESISED with br_mix.c's own .wav writer, so this needs
 * no extracted asset and no ffmpeg: what is being tested is that the backend
 * opens a file through ExtAudioFile, decodes it, and hands frames to a queue.
 * A .flac from tools/extract_cdaudio.py takes the identical path -- the only
 * difference is which decoder ExtAudioFile picks -- and BR_MUSIC_DIR points
 * this at real tracks when a builder has them.
 */
static int write_tone_wav(const char *pszPath, int cFrames, int hz)
{
    BrMixWaveFormat fmt;
    int16_t        *pPcm;
    int             i, rc;

    pPcm = (int16_t *)calloc((size_t)cFrames * 2, sizeof(int16_t));
    if (pPcm == NULL)
        return -1;
    for (i = 0; i < cFrames; ++i) {
        /* A square wave: no libm dependency and unmistakably non-silent. */
        int16_t v = ((i / (22050 / (2 * hz))) & 1) ? 12000 : -12000;
        pPcm[i * 2]     = v;
        pPcm[i * 2 + 1] = v;
    }
    BrMixOutputFormat(&fmt);
    rc = BrMixWavWrite(pszPath, &fmt, pPcm,
                       (uint32_t)cFrames * 2 * (uint32_t)sizeof(int16_t));
    free(pPcm);
    return rc;
}

static void test_music_backend(void)
{
    static BrAudio      audio;
    const BrAudioBackend *pB = BrMusicAqBackend();
    uint64_t before, after;
    int      handle, spins;

    CHECK(pB != NULL);
    CHECK(pB->pfnOpen != NULL && pB->pfnStart != NULL
          && pB->pfnStop != NULL && pB->pfnClose != NULL);

    /* The negative control FIRST: a path that does not exist must fail to
     * open, so that a success below is about the file and not about the
     * function returning a handle unconditionally. */
    CHECK_EQ(pB->pfnOpen(pB->pUser, "build/no-such-track.flac"),
             BR_AUDIO_HANDLE_NONE);
    CHECK(BrMusicAqLastError() != 0);       /* it failed for a REASON */
    BrMusicAqClearError();

    if (write_tone_wav("build/music_fixture.wav", 22050, 440) != 0) {
        g_skipped++;
        printf("  SKIP music backend -- could not write the fixture\n");
        return;
    }

    CHECK_EQ(BrAudioInit(&audio, pB), 0);
    CHECK(BrAudioAddTrack(&audio, "build/music_fixture.wav", "fixture") >= 0);
    CHECK_EQ(BrAudioTrackCount(&audio), 1);
    BrAudioSetRepeat(&audio, BR_AUDIO_REPEAT_ONCE);
    BrAudioSetVolume(&audio, BrAudioVolumeFromSetting(9));

    /* Open by hand once, to separate "the file decodes" from "the queue
     * starts": a machine with no output device fails the second and not the
     * first, and the two are different findings. */
    handle = pB->pfnOpen(pB->pUser, "build/music_fixture.wav");
    CHECK(handle != BR_AUDIO_HANDLE_NONE);
    if (handle != BR_AUDIO_HANDLE_NONE)
        pB->pfnClose(pB->pUser, handle);

    before = BrMusicAqFramesPlayed();
    if (!BrMusicAqAvailable()) {
        g_skipped++;
        printf("  SKIP music playback -- no audio output device\n");
        BrAudioShutdown(&audio);
        return;
    }

    CHECK_EQ(BrAudioPlayTrack(&audio, 0), 0);
    CHECK(BrAudioIsPlaying(&audio));

    /* Let the queue drain the one-second fixture.  BrMusicAqPump is what
     * turns the queue thread's end-of-stream into br_audio.c's policy. */
    for (spins = 0; spins < 400 && BrAudioIsPlaying(&audio); ++spins) {
        struct timespec ts;
        ts.tv_sec  = 0;
        ts.tv_nsec = 10 * 1000 * 1000;
        nanosleep(&ts, NULL);
        BrMusicAqPump(&audio);
    }

    after = BrMusicAqFramesPlayed();
    printf("  music backend: %llu frames decoded and queued (%s)\n",
           (unsigned long long)(after - before),
           BrMusicAqLastError() ? BrMusicAqLastErrorWhere() : "no errors");
    /* Frames only move when ExtAudioFileRead succeeded AND a queue buffer
     * was filled, so this is the end-to-end assertion. */
    CHECK(after > before);
    CHECK_EQ(BrAudioGetState(&audio), BR_AUDIO_STOPPED);

    BrAudioShutdown(&audio);
    BrMusicAqShutdown();
}

/* ==========================================================================
 * Stand-ins for the cross-module callees, exactly as test_uinav.c names them.
 *
 * BrSub10072AF0 is the ONE that matters, and it is slice4_50.c's body
 * verbatim -- one line, `BrSndPlaySimple(a, b)`.  slice4_50.o itself is not
 * linked here because it drags in the whole netplay block; the HOST links the
 * real object and the body is identical, so the sound this suite records is
 * the sound the host produces.
 * ========================================================================== */

int32_t BrSub10075020(void) { return 0; }

void BrSub10072AF0(int a, int b)
{
    (void)BrSndPlaySimple((int32_t)a, (uint32_t)b);
}

/* slice6_76.c owns this in the host; it is not linked here (0x10060D90 pulls
 * in both option tables and the FFB block), so the seam gets storage of its
 * own and br_wireaudio.c installs into it exactly as it would there. */
void (*g_pfnBrMusicVolume0029F0)(int32_t volume);

/* slice8_86.c owns this in the host, for the same reason. */
BrAudio *g_pBrAudio86;

void    BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }
char    g_aBr39B720[0x104];

void BrSub1003E310(void)                    { }
void BrSub1006A4A0(void *pThis, void *pArg) { (void)pThis; (void)pArg; }

BrPhase_ *BrOptObjCtor(BrPhase_ *pThis) { return pThis; }
void      BrExt_1004F700(BrPhase_ *pSelf) { (void)pSelf; }

/* ========================================================================== */

int main(void)
{
    printf("test_br_sfxsrc\n");
    test_table();
    test_dispatch();
    test_race_countdown();
    test_menu_activate();
    test_music_backend();
    printf("%d checks, %d failures, %d skipped\n",
           g_checks, g_fails, g_skipped);
    return g_fails ? 1 : 0;
}
