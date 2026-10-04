/* audio.c: the sound the platform plays -- one output, two sources.
 *
 *   the mixer  the host's output, started on first use; each block is the
 *              DirectSound buffers (dsound.c) plus the CD
 *   the CD     the disc's audio tracks, decoded by the host from the files
 *              host_music_dir() names (track02.flac ...), played the way a
 *              drive plays: from a track, on through to a last track, then
 *              stopped. Both of the game's CD backends drive it -- MCI
 *              (win_mm.c) and the EAR engine's CD channel (ear.c) -- and it
 *              tells whichever started the play when the play ends.
 *
 * A feeder thread decodes ahead into a ring so the output never waits on a
 * file. With no output device, no decoder or no track files (headless), the
 * CD plays silently and never ends, as the wasm lane's does; BR_MUSIC=0
 * and BR_VCLOCK keep it silent too. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "host.h"

#define OUT_RATE 44100

void plat_dsound_mix(float *lr, int n);          /* dsound.c: adds into lr */
int  plat_dsound_live(void);

/* ---- the mixer ------------------------------------------------------------------- */
static int s_out;                                /* -1 failed, 0 not tried, 1 running */
static void cd_mix(float *lr, int n);

static void mix(float *lr, int n, void *user)
{
    (void)user;
    memset(lr, 0, (size_t)n * 2 * sizeof *lr);
    plat_dsound_mix(lr, n);
    cd_mix(lr, n);
    {   /* BR_LOG: once a second, what is sounding and how loud */
        static int acc;
        int i;
        if (g_plat_log && (acc += n) >= OUT_RATE) {
            float pk = 0;
            for (i = 0; i < 2 * n; i++)
                pk = fabsf(lr[i]) > pk ? fabsf(lr[i]) : pk;
            PLOG("audio: %d buffers, cd track %d, peak %.3f\n", plat_dsound_live(), plat_cd_current(), pk);
            acc = 0;
        }
    }
}

/* the output: 1 when a device is mixing (it then moves the play cursors) */
int plat_audio_start(void)
{
    if (s_out == 0) {
        const char *e = getenv("BR_SFX");
        s_out = -1;
        if (!(e && e[0] == '0') && !plat_vclock() && host_audio_open(OUT_RATE, mix, NULL))
            s_out = 1;
    }
    return s_out == 1;
}

/* ---- the CD --------------------------------------------------------------------------- */
#define RING (OUT_RATE * 2)                      /* frames decoded ahead: two seconds */

static host_mutex  *s_cdl;
static host_cond   *s_cdc;
static float        s_ring[RING * 2];
static int          s_rd, s_wr;                  /* frames, mod RING */
static int          s_track, s_to;               /* playing, last of the play; 0 stopped */
static int          s_paused, s_feeding_done, s_ended, s_gen;
static float        s_vol = 1.0f;
static host_stream *s_stream;
static int          s_first, s_last;             /* the disc's track numbers, 0 unknown */
static void       (*s_end_fn)(void *user);
static void        *s_end_user;
static int          s_thread;
static long         s_fed, s_cap;           /* frames of this track fed; BR_CDLEN's cap */

static void cdlock(void)
{
    if (!s_cdl) {
        s_cdl = host_mutex_new();
        s_cdc = host_cond_new();
    }
    host_mutex_lock(s_cdl);
}
static void cdunlock(void) { host_mutex_unlock(s_cdl); }

static int music_on(void)
{
    const char *e = getenv("BR_MUSIC");
    return !(e && e[0] == '0') && host_music_dir() != NULL && plat_audio_start();
}

static int track_path(int t, char *out, size_t n)
{
    static const char *ext[] = { "flac", "wav", "aiff", "mp3", "m4a", "ogg" };
    const char *dir = host_music_dir();
    size_t i;
    if (!dir)
        return 0;
    for (i = 0; i < sizeof ext / sizeof ext[0]; i++) {
        FILE *f;
        snprintf(out, n, "%s/track%02d.%s", dir, t, ext[i]);
        if ((f = fopen(out, "rb")) != NULL) {
            fclose(f);
            return 1;
        }
    }
    return 0;
}

/* the disc's first and last audio track (the data track is 1) */
void plat_cd_tracks(int *first, int *last)
{
    if (!s_first) {
        char p[1200];
        int t;
        s_first = 2;
        s_last = 2;
        for (t = 2; t < 100 && track_path(t, p, sizeof p); t++)
            s_last = t;
        if (!track_path(2, p, sizeof p))
            s_last = 13;                         /* no files: the shipped disc's layout */
    }
    *first = s_first;
    *last = s_last;
}

/* decode ahead; called with s_cdl held, drops it while decoding */
static void *feeder(void *arg)
{
    float buf[4096 * 2];
    (void)arg;
    cdlock();
    for (;;) {
        int room = RING - 1 - (s_wr - s_rd + RING) % RING;
        int gen = s_gen, got = 0, k;
        if (!s_track || s_paused || s_feeding_done || room < 4096 || !s_stream) {
            host_cond_wait(s_cdc, s_cdl, 50);
            continue;
        }
        {
            host_stream *st = s_stream;
            cdunlock();
            got = host_stream_read(st, buf, 4096);
            cdlock();
        }
        if (s_cap && s_fed >= s_cap)
            got = 0;                             /* BR_CDLEN: the track ends here */
        s_fed += got > 0 ? got : 0;
        if (gen != s_gen)
            continue;                            /* a new play began meanwhile */
        if (got <= 0) {
            /* the track is over: the next one of the play, or the end */
            char p[1200];
            host_stream_close(s_stream);
            s_stream = NULL;
            if (s_track < s_to && track_path(s_track + 1, p, sizeof p) &&
                (s_stream = host_stream_open(p, OUT_RATE)) != NULL) {
                s_track++;
                s_fed = 0;
            }
            else
                s_feeding_done = 1;
            continue;
        }
        for (k = 0; k < got; k++) {
            s_ring[2 * s_wr] = buf[2 * k];
            s_ring[2 * s_wr + 1] = buf[2 * k + 1];
            s_wr = (s_wr + 1) % RING;
        }
    }
    return NULL;
}

static void cd_mix(float *lr, int n)
{
    int i;
    if (!s_cdl)
        return;
    cdlock();
    if (s_track && !s_paused) {
        for (i = 0; i < n && s_rd != s_wr; i++) {
            lr[2 * i] += s_ring[2 * s_rd] * s_vol;
            lr[2 * i + 1] += s_ring[2 * s_rd + 1] * s_vol;
            s_rd = (s_rd + 1) % RING;
        }
        if (s_rd == s_wr && s_feeding_done) {
            s_track = 0;
            s_ended = 1;                         /* plat_cd_poll tells the backend */
        }
        host_cond_broadcast(s_cdc);
    }
    cdunlock();
}

/* play tracks from..to (to 0: just from); end_fn is called, from the main
 * thread's message pump, when the play has finished. 0 when nothing can be
 * heard (the state still says playing, as a silent drive would) */
int plat_cd_play(int from, int to, void (*end_fn)(void *), void *user)
{
    char p[1200];
    int audible;
    audible = music_on() && track_path(from, p, sizeof p);
    cdlock();
    if (s_stream) {
        host_stream_close(s_stream);
        s_stream = NULL;
    }
    s_gen++;
    s_rd = s_wr = 0;
    s_track = from;
    s_to = to > from ? to : from;
    s_paused = 0;
    s_ended = 0;
    s_feeding_done = 0;
    s_end_fn = end_fn;
    s_end_user = user;
    s_fed = 0;
    {   /* BR_CDLEN=seconds: every track that long (checks the hand-on to the next) */
        const char *e = getenv("BR_CDLEN");
        s_cap = e ? (long)(atof(e) * OUT_RATE) : 0;
    }
    if (audible && (s_stream = host_stream_open(p, OUT_RATE)) != NULL) {
        if (!s_thread) {
            s_thread = 1;
            host_thread_start(feeder, NULL);
        }
    } else {
        s_feeding_done = 0;                      /* silent: plays on, never ends */
        audible = 0;
    }
    host_cond_broadcast(s_cdc);
    cdunlock();
    PLOG("cd: play track %d..%d%s\n", from, to > from ? to : from, audible ? "" : " (silent)");
    return audible;
}

void plat_cd_stop(void)
{
    cdlock();
    s_gen++;
    if (s_stream) {
        host_stream_close(s_stream);
        s_stream = NULL;
    }
    s_track = 0;
    s_ended = 0;
    s_rd = s_wr = 0;
    cdunlock();
    PLOG("cd: stop\n");
}

void plat_cd_pause(int on)
{
    cdlock();
    s_paused = on;
    host_cond_broadcast(s_cdc);
    cdunlock();
    PLOG("cd: %s\n", on ? "pause" : "resume");
}

/* 0..1 */
void plat_cd_volume(float v)
{
    cdlock();
    s_vol = v < 0 ? 0 : v > 1 ? 1 : v;
    cdunlock();
}

int plat_cd_current(void) { return s_track; }
int plat_cd_playing(void) { return s_track != 0 && !s_paused; }

/* from the message pump: a play that has ended tells its backend */
void plat_cd_poll(void)
{
    void (*fn)(void *) = NULL;
    void *user = NULL;
    if (!s_cdl)
        return;
    cdlock();
    if (s_ended) {
        s_ended = 0;
        fn = s_end_fn;
        user = s_end_user;
    }
    cdunlock();
    if (fn)
        fn(user);
}
