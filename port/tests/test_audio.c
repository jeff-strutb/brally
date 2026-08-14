/* test_audio.c -- verify the soundtrack module's policy against the original.
 *
 * No audio device and no files: the module's whole job is policy, so the test
 * supplies a mock BrAudioBackend that records what it was asked to do. That is
 * the point of the vtable -- the behaviour decompiled from BRD3D.dll can be
 * asserted exactly, without a sound card or a FLAC decoder in the loop.
 *
 * The FLAC probe is checked against a hand-built STREAMINFO header, so it needs
 * no retail file either.
 */
#include "br_audio.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------ mock backend */

typedef struct Mock {
    int  nextHandle;
    int  opens, closes, starts, stops, pauses, resumes;
    int  lastVolume, lastPan, lastLoop;
    int  state;
    char lastPath[BR_AUDIO_PATH_MAX];
    int  failOpen;
} Mock;

static int mock_open(void *pUser, const char *pszPath)
{
    Mock *m = (Mock *)pUser;
    m->opens++;
    strcpy(m->lastPath, pszPath);
    if (m->failOpen)
        return BR_AUDIO_HANDLE_NONE;
    return ++m->nextHandle;
}
static void mock_close(void *pUser, int handle)
{ (void)handle; ((Mock *)pUser)->closes++; }
static int mock_start(void *pUser, int handle)
{ (void)handle; ((Mock *)pUser)->starts++;
  ((Mock *)pUser)->state = BR_AUDIO_PLAYING; return 1; }
static void mock_stop(void *pUser, int handle)
{ (void)handle; ((Mock *)pUser)->stops++;
  ((Mock *)pUser)->state = BR_AUDIO_STOPPED; }
static void mock_pause(void *pUser, int handle)
{ (void)handle; ((Mock *)pUser)->pauses++;
  ((Mock *)pUser)->state = BR_AUDIO_PAUSED; }
static void mock_resume(void *pUser, int handle)
{ (void)handle; ((Mock *)pUser)->resumes++;
  ((Mock *)pUser)->state = BR_AUDIO_PLAYING; }
static void mock_setvol(void *pUser, int handle, int v)
{ (void)handle; ((Mock *)pUser)->lastVolume = v; }
static void mock_setpan(void *pUser, int handle, int p)
{ (void)handle; ((Mock *)pUser)->lastPan = p; }
static void mock_setloop(void *pUser, int handle, int l)
{ (void)handle; ((Mock *)pUser)->lastLoop = l; }
static int mock_getstate(void *pUser, int handle)
{ (void)handle; return ((Mock *)pUser)->state; }
static int mock_getpos(void *pUser, int handle, uint32_t *pMs)
{ (void)pUser; (void)handle; *pMs = 1234; return 1; }

static Mock g_mock;
static BrAudioBackend g_backend;

static void backend_reset(void)
{
    memset(&g_mock, 0, sizeof(g_mock));
    memset(&g_backend, 0, sizeof(g_backend));
    g_backend.pUser = &g_mock;
    g_backend.pfnOpen = mock_open;
    g_backend.pfnClose = mock_close;
    g_backend.pfnStart = mock_start;
    g_backend.pfnStop = mock_stop;
    g_backend.pfnPause = mock_pause;
    g_backend.pfnResume = mock_resume;
    g_backend.pfnSetVolume = mock_setvol;
    g_backend.pfnSetPan = mock_setpan;
    g_backend.pfnSetLoop = mock_setloop;
    g_backend.pfnGetState = mock_getstate;
    g_backend.pfnGetPositionMs = mock_getpos;
}

/* Build a module with `n` tracks and a fresh mock. */
static void setup(BrAudio *a, int n)
{
    int i;
    backend_reset();
    BrAudioInit(a, &g_backend);
    for (i = 0; i < n; i++) {
        char path[64], name[32];
        sprintf(path, "music/track%02d.flac", i);
        sprintf(name, "t%d", i);
        BrAudioAddTrack(a, path, name);
    }
}

/* ------------------------------------------------------------------- tests */

static void test_table(void)
{
    BrAudio a;
    printf("track table\n");
    setup(&a, 12);
    check(BrAudioTrackCount(&a) == 12, "twelve tracks added");
    check(strcmp(BrAudioTrackPath(&a, 0), "music/track00.flac") == 0,
          "path stored");
    check(strcmp(BrAudioTrackName(&a, 3), "t3") == 0, "name stored");
    check(BrAudioTrackPath(&a, -1) == NULL, "negative index rejected");
    check(BrAudioTrackPath(&a, 12) == NULL, "out-of-range index rejected");
    check(BrAudioGetCurrentTrack(&a) == -1, "nothing selected before play");

    /* the table is finite and must refuse to overflow */
    {
        BrAudio b;
        int i, added = 0;
        backend_reset();
        BrAudioInit(&b, &g_backend);
        for (i = 0; i < BR_AUDIO_MAX_TRACKS + 8; i++)
            if (BrAudioAddTrack(&b, "x.flac", "x") >= 0) added++;
        check(added == BR_AUDIO_MAX_TRACKS, "table refuses to overflow");
    }
    /* an over-long path must be refused rather than truncated -- a truncated
     * path would silently open the wrong file */
    {
        BrAudio b;
        char big[BR_AUDIO_PATH_MAX + 16];
        memset(big, 'a', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        backend_reset();
        BrAudioInit(&b, &g_backend);
        check(BrAudioAddTrack(&b, big, "x") == -1, "over-long path refused");
    }
    BrAudioShutdown(&a);
}

static void test_play_and_clamp(void)
{
    BrAudio a;
    printf("play and clamping\n");
    setup(&a, 12);

    check(BrAudioPlayTrack(&a, 0) == 0, "play track 0 succeeds");
    check(g_mock.opens == 1 && g_mock.starts == 1, "backend opened and started");
    check(strcmp(g_mock.lastPath, "music/track00.flac") == 0, "opened the right file");
    check(BrAudioGetCurrentTrack(&a) == 0, "current track reported");
    check(BrAudioIsPlaying(&a), "reports playing");

    BrAudioPlayTrack(&a, 99);
    check(BrAudioGetCurrentTrack(&a) == 11, "high index clamps to last track");
    BrAudioPlayTrack(&a, -5);
    check(BrAudioGetCurrentTrack(&a) == 0, "low index clamps to first track");

    /* switching tracks must release the previous one */
    check(g_mock.closes == 2, "previous track closed on each switch");
    BrAudioShutdown(&a);
}

static void test_stepping(void)
{
    BrAudio a;
    printf("prev / next / next-wrap\n");
    setup(&a, 12);

    /* This is the behaviour the original's three helpers differed on:
     * 0x10002970 CdTrackNext CLAMPS at the last track, while 0x100029B0
     * CdTrackNextWrap WRAPS round to the first. */
    BrAudioPlayTrack(&a, 10);
    BrAudioTrackNext(&a);
    check(BrAudioGetCurrentTrack(&a) == 11, "next advances");
    BrAudioTrackNext(&a);
    check(BrAudioGetCurrentTrack(&a) == 11, "next CLAMPS at the last track");
    BrAudioTrackNextWrap(&a);
    check(BrAudioGetCurrentTrack(&a) == 0, "next-wrap WRAPS to the first track");

    BrAudioPlayTrack(&a, 1);
    BrAudioTrackPrev(&a);
    check(BrAudioGetCurrentTrack(&a) == 0, "prev steps back");
    BrAudioTrackPrev(&a);
    check(BrAudioGetCurrentTrack(&a) == 0, "prev clamps at the first track");
    BrAudioShutdown(&a);
}

static void test_repeat_policy(void)
{
    BrAudio a;
    printf("end-of-track policy\n");

    /* CD backend behaviour: MM_MCINOTIFY re-played the SAME track. */
    setup(&a, 12);
    BrAudioSetRepeat(&a, BR_AUDIO_REPEAT_TRACK);
    BrAudioPlayTrack(&a, 5);
    check(g_mock.lastLoop == 1, "repeat-track asks the backend to loop");
    BrAudioOnTrackFinished(&a);
    check(BrAudioGetCurrentTrack(&a) == 5, "repeat-track replays the same track");
    BrAudioShutdown(&a);

    /* EAR backend behaviour: completion advanced with wraparound. */
    setup(&a, 12);
    BrAudioSetRepeat(&a, BR_AUDIO_REPEAT_ADVANCE);
    BrAudioPlayTrack(&a, 11);
    check(g_mock.lastLoop == 0, "repeat-advance does not ask for a backend loop");
    BrAudioOnTrackFinished(&a);
    check(BrAudioGetCurrentTrack(&a) == 0, "repeat-advance wraps past the last");
    BrAudioShutdown(&a);

    setup(&a, 12);
    BrAudioSetRepeat(&a, BR_AUDIO_REPEAT_ONCE);
    BrAudioPlayTrack(&a, 4);
    BrAudioOnTrackFinished(&a);
    check(!BrAudioIsPlaying(&a), "repeat-once stops at the end");

    /* a stale notification after a stop must not resurrect playback */
    BrAudioSetRepeat(&a, BR_AUDIO_REPEAT_ADVANCE);
    BrAudioStop(&a);
    {
        int startsBefore = g_mock.starts;
        BrAudioOnTrackFinished(&a);
        check(g_mock.starts == startsBefore, "stale finish after stop is ignored");
    }
    BrAudioShutdown(&a);
}

static void test_volume(void)
{
    BrAudio a;
    printf("volume\n");

    /* the retail ten-step music table at 0x100ADF68 */
    check(BrAudioVolumeFromSetting(0) == 0x00, "setting 0 is silence");
    check(BrAudioVolumeFromSetting(9) == 0xFF, "setting 9 is full");
    check(BrAudioVolumeFromSetting(5) == 0x8D, "mid setting matches the table");
    check(BrAudioVolumeFromSetting(-3) == 0x00, "negative setting clamps");
    check(BrAudioVolumeFromSetting(99) == 0xFF, "high setting clamps");

    /* 0x10002A20's mask is why 256 is silence, not full volume */
    check(BrAudioVolumeScale(0) == 0, "scale(0) is 0");
    check(BrAudioVolumeScale(128) == 10000, "scale(128) is full scale");
    check(BrAudioVolumeScale(255) == 19921, "scale(255) overshoots, as it did");
    check(BrAudioVolumeScale(256) == 0, "scale(256) is SILENCE -- the 8-bit mask");

    setup(&a, 12);
    BrAudioPlayTrack(&a, 2);
    BrAudioSetVolume(&a, 200);
    check(g_mock.lastVolume == 200, "volume reaches the backend");
    check(BrAudioGetVolume(&a) == 200, "volume read back");

    /* zero means "no music", not "silent music" -- the original's callers
     * gated on it before ever starting playback */
    BrAudioSetVolume(&a, 0);
    check(!BrAudioIsPlaying(&a), "volume 0 stops playback");
    BrAudioSetVolume(&a, 180);
    check(BrAudioIsPlaying(&a), "raising volume from 0 restarts the track");
    check(BrAudioGetCurrentTrack(&a) == 2, "and it is the same track");

    BrAudioSetVolume(&a, 999);
    check(BrAudioGetVolume(&a) == BR_AUDIO_VOLUME_MAX, "volume clamps high");
    BrAudioShutdown(&a);
}

static void test_enable_pause(void)
{
    BrAudio a;
    printf("enable / pause / resume\n");
    setup(&a, 12);

    BrAudioPlayTrack(&a, 3);
    BrAudioPause(&a);
    check(g_mock.pauses == 1 && !BrAudioIsPlaying(&a), "pause takes effect");
    BrAudioResume(&a);
    check(g_mock.resumes == 1 && BrAudioIsPlaying(&a), "resume takes effect");

    /* resume from a state that is not paused must do nothing */
    BrAudioResume(&a);
    check(g_mock.resumes == 1, "resume while playing is a no-op");

    BrAudioSetEnabled(&a, 0);
    check(!BrAudioIsEnabled(&a) && !BrAudioIsPlaying(&a), "disable stops music");
    {
        int startsBefore = g_mock.starts;
        BrAudioPlayTrack(&a, 4);
        check(g_mock.starts == startsBefore, "play while disabled stays silent");
        check(BrAudioGetCurrentTrack(&a) == 4,
              "but the selection is still recorded for the UI");
    }
    BrAudioSetEnabled(&a, 1);
    BrAudioPlayTrack(&a, 4);
    check(BrAudioIsPlaying(&a), "re-enabling allows playback again");
    BrAudioShutdown(&a);
}

static void test_random(void)
{
    BrAudio a;
    int i, pick, inRange = 1, sawDifferent = 0, first;
    printf("random selection\n");
    setup(&a, 12);

    /* The retail game drew race music from CD tracks 3..10 of 2..13, i.e.
     * indices 1..8: never the front-end track and never the last three. */
    BrAudioSetRandomRange(&a, 1, 8);
    BrAudioSeedRandom(&a, 12345);
    first = BrAudioPickRandomTrack(&a);
    for (i = 0; i < 500; i++) {
        pick = BrAudioPickRandomTrack(&a);
        if (pick < 1 || pick > 8) inRange = 0;
        if (pick != first) sawDifferent = 1;
    }
    check(inRange, "random picks stay inside the configured window");
    check(sawDifferent, "random picks actually vary");

    /* determinism: same seed, same sequence */
    {
        BrAudio b;
        int x, y;
        setup(&b, 12);
        BrAudioSetRandomRange(&b, 1, 8);
        BrAudioSeedRandom(&a, 999);
        BrAudioSeedRandom(&b, 999);
        x = BrAudioPickRandomTrack(&a);
        y = BrAudioPickRandomTrack(&b);
        check(x == y, "same seed gives the same pick");
        BrAudioShutdown(&b);
    }

    /* avoids repeating the current track when it has a choice */
    BrAudioPlayTrack(&a, 4);
    for (i = 0; i < 200; i++)
        if (BrAudioPickRandomTrack(&a) == 4) { inRange = 0; break; }
    check(inRange, "random pick avoids the track already playing");

    /* a one-entry window must terminate rather than spin looking for a change */
    BrAudioSetRandomRange(&a, 6, 6);
    BrAudioPlayTrack(&a, 6);
    check(BrAudioPickRandomTrack(&a) == 6, "single-track window returns it");
    BrAudioShutdown(&a);
}

static void test_degenerate(void)
{
    BrAudio a;
    printf("degenerate cases\n");

    /* no backend at all: a pure state machine, must not crash */
    BrAudioInit(&a, NULL);
    BrAudioAddTrack(&a, "x.flac", "x");
    check(BrAudioPlayTrack(&a, 0) == 0, "play with no backend is silent, not an error");
    check(BrAudioGetCurrentTrack(&a) == 0, "selection still tracked");
    BrAudioTrackNextWrap(&a);
    BrAudioPause(&a);
    BrAudioResume(&a);
    BrAudioShutdown(&a);

    /* empty table */
    setup(&a, 0);
    check(BrAudioPlayTrack(&a, 0) == 0, "play with no tracks is a no-op");
    check(BrAudioGetCurrentTrack(&a) == -1, "no current track");
    check(BrAudioPickRandomTrack(&a) == -1, "random pick reports none");
    BrAudioTrackNext(&a);
    BrAudioTrackPrev(&a);
    BrAudioShutdown(&a);

    /* backend that refuses to open */
    setup(&a, 4);
    g_mock.failOpen = 1;
    check(BrAudioPlayTrack(&a, 1) == 1, "failed open is reported");
    check(!BrAudioIsPlaying(&a), "and nothing is playing");
    BrAudioShutdown(&a);

    /* NULL tolerance on the query side */
    check(BrAudioTrackCount(NULL) == 0, "count(NULL)");
    check(BrAudioGetCurrentTrack(NULL) == -1, "current(NULL)");
    check(BrAudioIsEnabled(NULL) == 0, "enabled(NULL)");
}

static void test_position(void)
{
    BrAudio a;
    uint32_t ms = 7;
    printf("position query\n");
    setup(&a, 4);
    check(BrAudioGetPositionMs(&a, &ms) == 0 && ms == 0,
          "position unavailable before playing");
    BrAudioPlayTrack(&a, 0);
    check(BrAudioGetPositionMs(&a, &ms) == 1 && ms == 1234,
          "position comes from the backend");
    BrAudioShutdown(&a);
}

/* ---------------------------------------------------------------- FLAC probe */

static void test_flac_probe(void)
{
    /* A STREAMINFO for 44100 Hz, 2 channels, 16 bit, 9,111,168 frames
     * (3:26.68 -- the length of the first retail music track). The block is
     * bit-packed big-endian, which is exactly what the byte-wise decode is for. */
    unsigned char hdr[8 + 34];
    BrAudioFormat fmt;
    uint64_t frames = 9111168u;

    printf("FLAC STREAMINFO probe\n");
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'f'; hdr[1] = 'L'; hdr[2] = 'a'; hdr[3] = 'C';
    hdr[4] = 0x00;                       /* not last, type 0 = STREAMINFO */
    hdr[5] = 0; hdr[6] = 0; hdr[7] = 34; /* 24-bit length */
    {
        unsigned char *si = hdr + 8;
        uint32_t rate = 44100;
        uint32_t chan = 2 - 1;
        uint32_t bps  = 16 - 1;
        si[10] = (unsigned char)(rate >> 12);
        si[11] = (unsigned char)((rate >> 4) & 0xFF);
        si[12] = (unsigned char)(((rate & 0x0F) << 4)
                                 | ((chan & 0x07) << 1)
                                 | ((bps >> 4) & 0x01));
        si[13] = (unsigned char)(((bps & 0x0F) << 4)
                                 | (unsigned char)((frames >> 32) & 0x0F));
        si[14] = (unsigned char)((frames >> 24) & 0xFF);
        si[15] = (unsigned char)((frames >> 16) & 0xFF);
        si[16] = (unsigned char)((frames >> 8) & 0xFF);
        si[17] = (unsigned char)(frames & 0xFF);
    }

    check(BrAudioProbeFlac(hdr, sizeof(hdr), &fmt) == 0, "probe succeeds");
    check(fmt.sampleRate == 44100, "sample rate decoded");
    check(fmt.channels == 2, "channel count decoded");
    check(fmt.bitsPerSample == 16, "bit depth decoded");
    check(fmt.totalFrames == frames, "frame count decoded across the 36-bit field");
    check(BrAudioFormatDurationMs(&fmt) == 206602, "duration computed");

    /* rejection cases */
    hdr[0] = 'x';
    check(BrAudioProbeFlac(hdr, sizeof(hdr), &fmt) != 0, "bad magic rejected");
    hdr[0] = 'f';
    check(BrAudioProbeFlac(hdr, 12, &fmt) != 0, "short buffer rejected");
    hdr[4] = 0x04;
    check(BrAudioProbeFlac(hdr, sizeof(hdr), &fmt) != 0,
          "non-STREAMINFO first block rejected");
    hdr[4] = 0x00; hdr[7] = 30;
    check(BrAudioProbeFlac(hdr, sizeof(hdr), &fmt) != 0, "wrong block length rejected");
    check(BrAudioProbeFlac(NULL, 64, &fmt) != 0, "NULL data rejected");
}

int main(void)
{
    test_table();
    test_play_and_clamp();
    test_stepping();
    test_repeat_policy();
    test_volume();
    test_enable_pause();
    test_random();
    test_degenerate();
    test_position();
    test_flac_probe();

    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
