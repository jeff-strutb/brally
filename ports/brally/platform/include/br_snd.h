/* br_snd.h: the game's sound effects, mixed for the host (platform/common/br_snd.c).
 *
 * A sample is PCM loaded once from a WAV file and then played, one-shot or
 * looping, with a volume, a pan and a playback rate, the laws the game was
 * written for:
 *
 *   volume  hundredths of a dB, 0 = as recorded, never amplified;
 *           -10000 is silence
 *   pan     hundredths of a dB taken off the far side (-10000..10000)
 *   rate    samples a second it is played at, 0 = its own
 *
 * Force-included into the core. */
#ifndef BR_SND_H
#define BR_SND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a WAV file's format block (its 'fmt ' chunk), followed by cbSize extra bytes */
#pragma pack(push, 1)
typedef struct BrWavFmt {
    uint16_t wFormatTag;        /* 1: PCM */
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
} BrWavFmt;
#pragma pack(pop)
#define BR_WAV_FMT_PCM_BYTES 16 /* the block without cbSize, all a PCM file has */

typedef struct BrSnd BrSnd;

/* the output, started on first use (the game holds it while its sound is on) */
void  *br_snd_open(void);
void   br_snd_close(void);
/* a sample of the format's 8- or 16-bit PCM (any other format plays silence);
 * NULL for no bytes */
BrSnd *br_snd_new(const BrWavFmt *fmt, const void *pcm, uint32_t bytes);
void   br_snd_free(BrSnd *s);
void   br_snd_play(BrSnd *s, int loop);     /* from where it stands */
void   br_snd_stop(BrSnd *s);               /* where it stands is kept */
void   br_snd_seek(BrSnd *s, uint32_t byte);
int    br_snd_playing(BrSnd *s);
void   br_snd_volume(BrSnd *s, int32_t cb);
void   br_snd_pan(BrSnd *s, int32_t cb);
void   br_snd_rate(BrSnd *s, uint32_t hz);

/* the CD drive's music (cdaudio.c): 0 or an error, as the game's MCI calls
 * answered; play ends with the window `notify` posted 0x3B9 */
int br_cd_open(uint32_t *pDevice, uint32_t *pLast);
int br_cd_close(void);
int br_cd_track(void);
int br_cd_play(void *notify, int from, int to);
int br_cd_stop(void);
int br_cd_pause(void);

#ifdef __cplusplus
}
#endif
#endif
