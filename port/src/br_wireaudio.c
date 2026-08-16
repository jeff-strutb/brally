/* br_wireaudio.c -- the wiring.  See br_wireaudio.h.
 *
 * Every function here is glue.  The only judgement calls are WHICH bank is
 * loaded at boot (the menu set, because that is what the original loads for
 * the front end) and WHERE the files are looked for, and both are stated in
 * the header and overridable by environment variable.
 */
#include "br_wireaudio.h"

#include "br_musicaq.h"
#include "br_racestep.h"
#include "br_sfxlive.h"
#include "br_sfxsrc.h"
#include "slice1_08.h"
#include "slice6_76.h"
#include "slice8_86.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BrSfxOut s_sfx;
static BrAudio  s_music;
static int      s_wired;
/* SILENT BY DEFAULT. This was 1, which meant every invocation of the host and
 * EVERY TEST IN THE SUITE opened the real output device and played through the
 * user's speakers. Running ./tools/regress.sh made the machine emit a stream of
 * beeps from nowhere, with no indication which process was responsible -- the
 * user heard it before anyone noticed it was us.
 *
 * A decompilation harness is run headless, in batch, and from test suites far
 * more often than it is played. Audible output is the special case and has to
 * be asked for: BR_AUDIO_LIVE=1 in the environment, or BrWireAudioSetLive(1)
 * from an interactive mode that genuinely wants sound. Everything else still
 * mixes exactly as before -- BrSfxLivePumpOffline renders the same samples and
 * the .wav evidence files are unchanged -- it simply does not reach a speaker.
 *
 * The mixing is what the tests assert on. Playing it aloud was never part of
 * the evidence. */
static int      s_live      = 0;
static int      s_device;
static int      s_setLoaded = -1;

/* ==========================================================================
 * The music volume seam, 0x100029F0
 * ==========================================================================
 *
 * slice6_76.c's 0x10060D90 hands this the value it has just pushed through
 * BrOptLevelATable (0x100ADF68) -- which is the SAME ten-entry table
 * br_audio.h documents as the music volume curve, so the byte arriving here
 * is already 0..255 and BrAudioVolumeFromSetting must NOT be applied a second
 * time.
 *
 * The mask reproduces 0x10060D90's own GOTCHA: it pushes
 * (index & 0xFFFFFF00) | tableByte, so only the low byte is the volume. */
static void wire_music_volume(int32_t volume)
{
    BrAudioSetVolume(&s_music, (int)((uint32_t)volume & 0xFFu));
}

/* ==========================================================================
 * The race countdown seam
 * ==========================================================================
 *
 * br_racestep.c calls this four times per race, from inside the per-driver
 * loop of the state-2 arm, with the POST-increment beep counter.  The lock is
 * taken because this runs on the game thread while BrMixRender runs on the
 * queue's; see br_sfxlive.h. */
static void wire_race_sound(int iStep)
{
    BrSfxLiveLock();
    BrSfxSrcRaceCountdown(iStep);
    BrSfxLiveUnlock();
}

/* ==========================================================================
 * The soundtrack directory
 * ==========================================================================
 *
 * tools/extract_cdaudio.py writes "track%02d.flac" for CD tracks 2..13 and
 * tools/extract_xm.py writes "xm_%06X.flac"; both are sorted correctly by
 * plain strcmp, so the directory order IS the track order and no manifest
 * parsing is needed.  .wav is accepted too, because ExtAudioFile decodes it
 * through the same path and it is what a fixture is.
 */
static int name_has_ext(const char *pszName, const char *pszExt)
{
    size_t n = strlen(pszName), e = strlen(pszExt);

    return (n > e && strcmp(pszName + n - e, pszExt) == 0);
}

static int wire_load_music_dir(const char *pszDir)
{
    /* BR_AUDIO_MAX_TRACKS is 32; the retail disc has 12 and the ROM 6. */
    char  *apName[BR_AUDIO_MAX_TRACKS];
    int    n = 0, i, j;
    DIR   *d;
    struct dirent *ent;

    /* PROVENANCE GATE: game audio comes from the builder's own disc, or the
     * game has no music. Nothing is ever stood in for.
     *
     * tools/extract_cdaudio.py writes cdaudio.manifest.json (carrying a
     * source_fingerprint of the disc image) beside the tracks it extracts, and
     * tools/extract_xm.py does the same for the ROM. Loose .flac files with no
     * manifest were not produced from a disc, so they are NOT loaded.
     *
     * This is not hypothetical. A development pass dropped two synthesised
     * sine-tone FLACs in this directory to prove the backend worked. They did
     * prove it -- and they also made every log line read "music: 2 track(s)",
     * which is indistinguishable from real game music in every report the
     * project prints. Nobody looking at the output could have told. A
     * fingerprinted manifest is the difference between "there is audio here"
     * and "there is audio here that came from the game". */
    {
        char szManifest[1024];
        FILE *fm;
        int   fOk = 0;
        static const char *const apszManifest[] = {
            "cdaudio.manifest.json", "xm.manifest.json"
        };
        size_t k;
        for (k = 0; k < sizeof apszManifest / sizeof apszManifest[0]; k++) {
            snprintf(szManifest, sizeof szManifest, "%s/%s", pszDir, apszManifest[k]);
            fm = fopen(szManifest, "rb");
            if (fm != NULL) { fclose(fm); fOk = 1; break; }
        }
        if (!fOk)
            return 0;      /* audio with no provenance is not game audio */
    }

    d = opendir(pszDir);
    if (d == NULL)
        return 0;

    while ((ent = readdir(d)) != NULL && n < BR_AUDIO_MAX_TRACKS) {
        if (ent->d_name[0] == '.')
            continue;
        if (!name_has_ext(ent->d_name, ".flac") && !name_has_ext(ent->d_name, ".wav"))
            continue;
        apName[n] = strdup(ent->d_name);
        if (apName[n] == NULL)
            break;
        n++;
    }
    closedir(d);

    /* Insertion sort: n is at most 32 and this keeps the file dependency-free. */
    for (i = 1; i < n; ++i) {
        char *p = apName[i];
        for (j = i; j > 0 && strcmp(apName[j - 1], p) > 0; --j)
            apName[j] = apName[j - 1];
        apName[j] = p;
    }

    for (i = 0; i < n; ++i) {
        char szPath[BR_AUDIO_PATH_MAX];
        int  cb = snprintf(szPath, sizeof szPath, "%s/%s", pszDir, apName[i]);

        if (cb > 0 && (size_t)cb < sizeof szPath)
            (void)BrAudioAddTrack(&s_music, szPath, apName[i]);
        free(apName[i]);
    }
    return BrAudioTrackCount(&s_music);
}

/* ========================================================================== */

void BrWireAudioSetLive(int fLive)
{
    s_live = (fLive != 0);
}

int BrWireAudioLoadSet(int set)
{
    const char *pszDir = getenv("BR_SFX_DIR");
    int         n;

    if (!s_wired)
        return 0;
    if (pszDir == NULL || pszDir[0] == '\0')
        pszDir = BR_WIREAUDIO_SFX_DIR;

    /* Voices already loaded for the other set stay where they are: the two
     * sets overlap in group number and the original reloads over the top,
     * which is what BrSfxOutLoadSet does. */
    BrSfxLiveLock();
    /* 0x1006C4BE: a channel must not keep a pointer into the outgoing bank. */
    BrSfxSrcChannelsReset();
    n = BrSfxOutLoadSet(&s_sfx, set, pszDir);
    BrSfxLiveUnlock();
    s_setLoaded = set;
    return n;
}

void BrHostWireAudio(void)
{
    const char *pszMusic;

    if (s_wired)
        return;

    /* 1. The mixer.  This is what makes BrSndPDS and the init guard non-NULL,
     *    and therefore what stops every BrSnd* entry point from taking its
     *    "sound is disabled" early exit. */
    if (BrSfxOutOpen(&s_sfx) != 0)
        return;
    s_wired = 1;
    BrSfxLiveSetMix(s_sfx.pMix);

    /* PlaySFX= out of BossRally.ini; the shipped default is 1 and slice1_08's
     * gate reads it. */
    if (BrSndG0B5DE8 == 0)
        BrSndG0B5DE8 = 1;

    /* 2. The bank the FRONT END wants.  0x1006C290 is called with 0 there and
     *    with 1 from the race sound init (Glide 0x10061310). */
    (void)BrWireAudioLoadSet(BR_SFX_SET_MENU);

    /* 3. The source table.  Without this every source's group is 0 -- the
     *    engine -- and the countdown would play the wrong sample if it played
     *    at all.  See BrSfxSrcTableInit. */
    BrSfxSrcTableInit();

    /* 4. The race step's countdown hole, which is the whole reason this pass
     *    exists.  br_racestep.c counts the hole either way, so the counter
     *    stays honest about how often it fired. */
    g_brRaceStepHooks.pfnSound = wire_race_sound;

    /* 5. Music: the backend br_audio.c has always expected, plus the global
     *    slice8_86.c's two CD entry points read. */
    (void)BrAudioInit(&s_music, BrMusicAqBackend());
    g_pBrAudio86 = &s_music;

    pszMusic = getenv("BR_MUSIC_DIR");
    if (pszMusic == NULL || pszMusic[0] == '\0')
        pszMusic = BR_WIREAUDIO_MUSIC_DIR;
    (void)wire_load_music_dir(pszMusic);

    /* The original's front end plays index 0 once at start-up and a race
     * draws at random from indices 1..8 -- br_audio.h's window.  Both are
     * policy already in br_audio.c; all that is set here is the window and
     * the end-of-track behaviour, which is EAR's (advance and wrap), because
     * that is the value baked into .data. */
    if (BrAudioTrackCount(&s_music) > 2)
        BrAudioSetRandomRange(&s_music, 1, BrAudioTrackCount(&s_music) - 4);
    BrAudioSetRepeat(&s_music, BR_AUDIO_REPEAT_ADVANCE);

    /* 6. The 0x100029F0 seam. */
    g_pfnBrMusicVolume0029F0 = wire_music_volume;

    /* 7. The device.  Last, so that a failure here leaves everything above
     *    wired and a headless caller can still pump the mixer offline. */
    if (s_live || (getenv("BR_AUDIO_LIVE") != NULL))
        s_device = BrSfxLiveStart(s_sfx.pMix);
}

void BrHostWireAudioShutdown(void)
{
    if (!s_wired)
        return;
    BrSfxLiveStop();
    BrMusicAqShutdown();
    BrAudioShutdown(&s_music);
    g_pBrAudio86             = NULL;
    g_pfnBrMusicVolume0029F0 = NULL;
    g_brRaceStepHooks.pfnSound = NULL;
    BrSfxSrcChannelsReset();
    BrSfxOutClose(&s_sfx);
    BrSfxLiveSetMix(NULL);
    s_wired  = 0;
    s_device = 0;
}

void BrHostWireAudioFrame(void)
{
    if (s_wired)
        BrMusicAqPump(&s_music);
}

void BrHostWireAudioReport(void)
{
    static const char *const apszSet[2] = { "menu", "race" };

    printf("audio: %s\n", s_wired ? "wired" : "NOT wired (no mixer)");
    if (!s_wired)
        return;
    printf("  sfx bank  : set %s, %d voices loaded, %d names missing\n",
           (s_setLoaded == 0 || s_setLoaded == 1) ? apszSet[s_setLoaded] : "?",
           s_sfx.cLoaded, s_sfx.cMissing);
    printf("  output    : %s (%llu frames rendered)\n",
           s_device ? "AudioQueue open" : "no device",
           (unsigned long long)BrSfxLiveFramesRendered());
    printf("  music     : %d track(s), backend %s\n",
           BrAudioTrackCount(&s_music),
           BrMusicAqAvailable() ? "available" : "unavailable");
    if (BrAudioTrackCount(&s_music) == 0)
        printf("              (none found -- run tools/extract_cdaudio.py "
               "into %s)\n", BR_WIREAUDIO_MUSIC_DIR);
    printf("  countdown : hook %s\n",
           g_brRaceStepHooks.pfnSound ? "installed" : "MISSING");
    printf("  music vol : hook %s\n",
           g_pfnBrMusicVolume0029F0 ? "installed" : "MISSING");
}

BrSfxOut *BrWireAudioSfx(void)   { return s_wired ? &s_sfx   : NULL; }
BrAudio  *BrWireAudioMusic(void) { return s_wired ? &s_music : NULL; }

int BrWireAudioVoicesLoaded(void)  { return s_wired ? s_sfx.cLoaded  : 0; }
int BrWireAudioNamesMissing(void)  { return s_wired ? s_sfx.cMissing : 0; }
int BrWireAudioMusicTracks(void)   { return s_wired ? BrAudioTrackCount(&s_music) : 0; }
int BrWireAudioDeviceOpen(void)    { return s_device; }
