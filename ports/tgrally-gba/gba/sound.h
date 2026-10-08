#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>

/* a sample: 8-bit, len bytes and a tail after them (the loop again, or
   silence); a voice past len steps back loopLen, or stops if it does not loop */
typedef struct {
    const uint8_t *data;          /* offset by 128 */
    uint32_t len, loopLen;
    uint8_t vol;                  /* 0..64 (module samples) */
    uint8_t loops;
    int8_t relNote;
    uint8_t pad;
} SndSample;

/* an effect voice in a retrace: its sample (0xFF: silent), its levels, whether it
   (re)starts here and from where, its step (Q12) */
typedef struct {
    uint8_t smp, left, right, start;
    uint16_t rate, off;
} SfxVoice;

/* the player as the game had it at the replay's start: BrModState's counts, the
   channels (sound.s's layout), the voices (their instrument, Q12 place and step, level) */
typedef struct {
    uint8_t note, smp, vol, fx, param, inst, arp[3], arpIdx, arpOn, pad;
    int16_t porta, slide;
    uint32_t target;
} ModChan;
typedef struct {
    uint32_t inst, pos, rate;
    int32_t base;
} ModVoice;
typedef struct {
    uint32_t speed, ticks, rows, order, rowOff;
    ModChan chan[6];
    ModVoice voice[6];
} ModInit;
/* a module (tools/sound.py module_c): its samples (by instrument - 1), patterns, order
   list, length, restart, speed, channels (up to 16), the game's music level times its
   fade, and the player's state to start from (0: BrModReset's) */
typedef struct {
    const SndSample *smp;
    const uint8_t *const *pat;
    const uint8_t *order;
    int32_t len, restart, speed, chans, level;
    const ModInit *init;
} Song;
extern const ModInit g_mod_init;
extern const Song g_race_song;
extern const uint32_t g_note_rate[120], g_porta_k;
extern const SndSample g_sfx_smp[];
extern const int g_sfx_frames;
extern const SfxVoice g_sfx_trace[][6];
void snd_play(const Song *song);          /* sound.s: the music from its start (or its init) */
void snd_sfx(const SndSample *s, int rate, int left, int right);   /* an effect on a free voice */
extern volatile int32_t s_sfx_trace;         /* the race's effects (the trace) play: 1 */
#endif
