/* mixcheck.c: the music's routing in mux.c, checked against computed values
 * (ports/remastered/tools/mixcheck.sh builds and runs it).
 *
 * The real multiplexer is linked with a host that only takes the output's
 * mixer, and two games whose mixers give constants: Top Gear Rally TGR (its
 * music and effects, as one), Boss Rally BR.  Each case sets the game in
 * front and the music chosen, mixes, and compares the output with what the
 * gains in mux.c say it must be: N64_GAIN on Top Gear Rally (times Boss
 * Rally's music volume under a race), Boss Rally heard in front or, behind
 * the menus, with the PC's music only, a change crossfaded over FADE_S, the
 * limiter over the sum.  Exit 0 when every case holds. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "mux.h"
#include "options.h"

#define RATE 48000
static const float N64_GAIN = 5.70f, FADE_S = 0.5f;   /* mux.c's */

/* ---- the host: only the output's mixer ---------------------------------------- */
static host_audio_fn s_mix;
int host_audio_open(int rate, host_audio_fn fn, void *user) { (void)rate; (void)user; s_mix = fn; return 1; }
void host_audio_close(void) {}
void host_init(int argc, char **argv) { (void)argc; (void)argv; }
void host_shutdown(void) {}
const char *host_save_dir(void) { return "."; }
const char *host_game_dir(void) { return "."; }
const char *host_cd_dir(void) { return "."; }
const char *host_resource_dir(void) { return "."; }
const char *host_music_dir(void) { return "."; }
int host_mkdir(const char *p) { (void)p; return 1; }
int host_window_open(int w, int h, const char *t) { (void)w; (void)h; (void)t; return 1; }
void host_window_close(void) {}
int host_window_visible(void) { return 1; }
void host_window_lock_aspect(int lock) { (void)lock; }
int host_poll_event(host_event *ev, uint32_t wait_ms) { (void)ev; (void)wait_ms; return 0; }
void host_message_box(const char *a, const char *b) { (void)a; (void)b; }
void *host_window_handle(void) { return NULL; }
void host_window_pixels(int *w, int *h) { *w = *h = 0; }
void host_present(const uint32_t *p, int w, int h) { (void)p; (void)w; (void)h; }
host_stream *host_stream_open(const char *p, int r) { (void)p; (void)r; return NULL; }
int host_stream_read(host_stream *s, float *lr, int n) { (void)s; (void)lr; (void)n; return 0; }
void host_stream_close(host_stream *s) { (void)s; }
int host_pad_read(host_pad *p) { memset(p, 0, sizeof *p); return 0; }
struct host_mutex { int x; };
struct host_cond { int x; };
host_mutex *host_mutex_new(void) { static host_mutex m; return &m; }
void host_mutex_lock(host_mutex *m) { (void)m; }
void host_mutex_unlock(host_mutex *m) { (void)m; }
host_cond *host_cond_new(void) { static host_cond c; return &c; }
int host_cond_wait(host_cond *c, host_mutex *m, uint32_t t) { (void)c; (void)m; (void)t; return 0; }
void host_cond_broadcast(host_cond *c) { (void)c; }

/* ---- the games ------------------------------------------------------------------ */
static float s_tgr_level = 0.1f, s_br_level = 0.05f, s_br_vol = 1.0f;
static int s_muted, s_quiet;
static float s_cd_gain = -1;
static void tgr_mix(float *lr, int n, void *u) { int i; (void)u; for (i = 0; i < 2 * n; i++) lr[i] = s_tgr_level; }
static void br_mix(float *lr, int n, void *u) { int i; (void)u; for (i = 0; i < 2 * n; i++) lr[i] = s_br_level; }
void  tgr_music_mute(int on) { s_muted = on; }
void  br_music_gain(float g) { s_cd_gain = g; }
void  br_effects_quiet(int on) { s_quiet = on; }
float br_music_volume(void) { return s_br_vol; }
void  br_race_click(float x, float y) { (void)x; (void)y; }
void  rm_br_start(void) {}
void  rm_options_load(void) {}
int   tgr_flag(int flag) { (void)flag; return 1; }
int   mux_tgr_host_audio_open(int rate, host_audio_fn fn, void *user);
int   mux_br_host_audio_open(int rate, host_audio_fn fn, void *user);

/* ---- the cases -------------------------------------------------------------------- */
static int s_fail, s_n;

static float out(int frames)
{
    static float buf[2 * 48000];
    s_mix(buf, frames, NULL);
    return buf[2 * frames - 2];         /* the last frame's left */
}

static void expect(const char *what, double got, double want)
{
    int ok = fabs(got - want) <= 1e-4 + 1e-4 * fabs(want);
    s_n++;
    if (!ok)
        s_fail++;
    printf("%s %-58s %.5f (want %.5f)\n", ok ? "ok  " : "FAIL", what, got, want);
}

static float limited(float x)            /* mux.c's limiter, independently */
{
    float a = fabsf(x);
    if (a <= 0.9f)
        return x;
    a = 0.9f + 0.1f * tanhf((a - 0.9f) / 0.1f);
    return x < 0 ? -a : a;
}

int main(void)
{
    mux_tgr_host_audio_open(RATE, tgr_mix, NULL);
    mux_br_host_audio_open(RATE, br_mix, NULL);

    /* the menus, the N64's music */
    mux_front(MUX_TGR);
    mux_music(RM_MUSIC_N64);
    out(RATE);
    expect("menus, N64: Top Gear Rally at the gain, Boss Rally silent", out(256), s_tgr_level * N64_GAIN);
    expect("menus, N64: Top Gear Rally's music on", s_muted, 0);
    expect("menus, N64: Boss Rally's CD gain 0", s_cd_gain, 0);

    /* the menus, the PC's: Top Gear Rally's music off, Boss Rally's CD alone heard */
    mux_music(RM_MUSIC_PC);
    out(RATE);
    expect("menus, PC: Top Gear Rally (effects) + Boss Rally's CD", out(256), s_tgr_level * N64_GAIN + s_br_level);
    expect("menus, PC: Top Gear Rally's music muted", s_muted, 1);
    expect("menus, PC: Boss Rally's effects quiet", s_quiet, 1);
    expect("menus, PC: Boss Rally's CD gain 1", s_cd_gain, 1);

    /* a race, the PC's: Top Gear Rally at once silent, Boss Rally all heard */
    mux_front(MUX_BR);
    expect("race, PC: Top Gear Rally silent from the first frame", out(1), s_br_level);
    expect("race, PC: Boss Rally's effects on", s_quiet, 0);
    expect("race, PC: Top Gear Rally's music unmuted (gain decides)", s_muted, 0);

    /* the N64's chosen in the race: a crossfade, its piece at the gain times
       Boss Rally's music volume */
    s_br_vol = 0.8f;
    mux_music(RM_MUSIC_N64);
    {
        float want = N64_GAIN * s_br_vol, quarter = (FADE_S / 2) * RATE;
        expect("race, PC -> N64: at a quarter of FADE_S, half way",
               out((int)quarter), s_br_level + s_tgr_level * want / 2);
        out(RATE);
        expect("race, PC -> N64: after FADE_S, all of it", out(256), limited(s_br_level + s_tgr_level * want));
        expect("race, N64: Boss Rally's CD gain 0", s_cd_gain, 0);
    }

    /* the limiter: a sum over 0.9 */
    s_tgr_level = 0.2f;
    out(RATE);
    expect("race, N64, loud: the limiter holds the sum under 1",
           out(256), limited(s_br_level + 0.2f * N64_GAIN * s_br_vol));
    expect("race, N64, loud: under 1", out(256) < 1.0f, 1);

    /* back to the menus: the N64's gain at once */
    s_tgr_level = 0.1f;
    mux_front(MUX_TGR);
    expect("menus again, N64: Top Gear Rally at the gain at once", out(1), s_tgr_level * N64_GAIN);

    printf("mixcheck: %d of %d cases hold\n", s_n - s_fail, s_n);
    return s_fail != 0;
}
