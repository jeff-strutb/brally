/* br_mix.h -- the software mixer that stands where DirectSound stood.
 *
 * WHERE THIS SITS
 * ===============
 * br_sfx.h establishes that the retail sound engine is DirectSound reached
 * through COM, and slice1_08.c already ports the per-voice wrapper over
 * IDirectSoundBuffer -- BrSndVoiceCreate, BrSndVoiceApplyPan,
 * BrSndVoiceApplyVolume, BrSndVoiceApplyFreq, BrSndVoiceStart and the rest.
 * Every one of those calls through the vtable structs declared in
 * slice1_08.h:
 *
 *     BrSndPDS->pVtbl->CreateSoundBuffer(...)
 *     pBuf->pVtbl->Lock / Unlock / SetVolume / SetPan / SetFrequency / Play
 *
 * That vtable IS the platform seam, and it already existed before this
 * module.  So this module does not introduce a new abstraction: it supplies
 * an OBJECT that satisfies the one the port already calls through.  Install
 * it with
 *
 *     BrSndPDS      = BrMixDevice(pMix);
 *     BrSndG18290FC = pMix;          (any non-NULL -- it is the init guard)
 *
 * and every line of the ported logic above runs unchanged, on macOS, with the
 * numbers it already computes.
 *
 * WHAT THAT BUYS, AND WHAT IT FORBIDS
 * ===================================
 * The division of labour is not negotiable and it is the whole point:
 *
 *   the port decides   which sample, at what frequency (br_sfx.c's 32.32
 *                      arithmetic and engine curve), at what pan and volume
 *                      (BrSndVoiceSetLevels' 400-neutral units)
 *   this module does   resampling, gain, summing, clipping
 *
 * Nothing here computes a pitch, a pan or a volume.  SetFrequency arrives as
 * a frequency in hertz; SetPan and SetVolume arrive as DirectSound
 * hundredths-of-a-decibel.  If a future change finds itself deriving one of
 * those here, the seam has moved to the wrong place.
 *
 * THE DIRECTSOUND SEMANTICS THAT ARE REPRODUCED
 * =============================================
 * Only what the ported code actually depends on, but that part exactly:
 *
 *   SetVolume(cB)      attenuation in 1/100 dB, <= 0.  gain = 10^(cB/2000).
 *                      DSBVOLUME_MIN (-10000) is silence, not 10^-5, so that
 *                      BrSndVoiceApplyVolume's hard-mute path is ACTUALLY
 *                      silent and a test can assert exact zeros.
 *   SetPan(cB)         one channel is attenuated, the other is left alone:
 *                      cB < 0 (DSBPAN_LEFT direction) attenuates the RIGHT.
 *                      This is the convention that makes br_sfx.h's finding
 *                      -- BrSndPan's pGainA is LEFT -- audible rather than
 *                      merely asserted.
 *   SetFrequency(hz)   absolute playback rate.  0 is DSBFREQUENCY_ORIGINAL,
 *                      i.e. the buffer's own nSamplesPerSec.
 *   Play(,,1)          DSBPLAY_LOOPING; 0 plays once and then reports
 *                      not-playing, which is what BrSndVoiceStart's
 *                      GetStatus branch reads.
 *   Stop               keeps the play cursor.  DirectSound does; a rewind is
 *                      SetCurrentPosition(0), which BrSndVoiceStart issues
 *                      separately on its already-playing path.
 *   GetCaps            dwFlags 0 -- a software buffer, so the voice's f24
 *                      (DSBCAPS_LOCHARDWARE) comes back 0.
 *
 * THE MIX FORMAT is the one the original asked the device for at 0x1006C554:
 * 22050 Hz, 16-bit, stereo.  Sample files on the disc are none of those --
 * they are mono 8-bit at 11005/11025 and mono 16-bit at 22050, plus two
 * stereo 44100 -- so conversion is this module's job and the rate ratio is
 * exactly what SetFrequency controls.
 *
 * NO DEVICE IS OPENED HERE.  BrMixRender pulls the mix into a caller's
 * buffer; whether that buffer goes to a speaker (br_sfxaq.c) or to a .wav
 * file (BrMixWavWrite) is the caller's business.  That is what lets the
 * suites assert on samples with no audio hardware present.
 */
#ifndef BR_MIX_H
#define BR_MIX_H

#include <stddef.h>
#include <stdint.h>

#include "slice1_08.h"     /* BrDSound, BrDSBuffer, the vtable layouts */

/* The WAVEFORMATEX the original builds at 0x1006C554 for the primary buffer:
 * wFormatTag=1, nChannels=2, nSamplesPerSec=0x5622, nAvgBytesPerSec=0x15888,
 * nBlockAlign=4, wBitsPerSample=16. */
#define BR_MIX_RATE      22050
#define BR_MIX_CHANNELS  2
#define BR_MIX_BITS      16

/* DirectSound's own limits, in hundredths of a decibel. */
#define BR_MIX_VOLUME_MIN  (-10000)
#define BR_MIX_VOLUME_MAX  0
#define BR_MIX_PAN_LEFT    (-10000)
#define BR_MIX_PAN_RIGHT   10000

/* A model of WAVEFORMATEX.  This is what a BrSndVoice's pFormat points at and
 * what BrMixCreateSoundBuffer reads; both ends are ours, so the fields are
 * declared for clarity rather than laid out to match Windows' packing. */
typedef struct BrMixWaveFormat {
    uint16_t wFormatTag;        /* 1 == WAVE_FORMAT_PCM; nothing else loads */
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;    /* 8 (unsigned) or 16 (signed) */
} BrMixWaveFormat;

typedef struct BrMix BrMix;

/* ------------------------------------------------------------------ device */

/* Create a mixer.  Returns NULL only if out of memory. */
BrMix *BrMixCreate(void);
void   BrMixDestroy(BrMix *pMix);

/* The object to store into BrSndPDS.  Its vtable's CreateSoundBuffer slot is
 * the only one the port calls, and it is at the same offset as
 * IDirectSound's. */
BrDSound *BrMixDevice(BrMix *pMix);

/* Pull cFrames frames of interleaved 16-bit L,R into pDst (2*cFrames
 * int16_t).  Always writes every frame; silence where nothing plays.  This is
 * the only thing that advances any buffer's play cursor, so a test renders as
 * much or as little as it likes and gets the same samples every time. */
void BrMixRender(BrMix *pMix, int16_t *pDst, int cFrames);

/* How many buffers currently report DSBSTATUS_PLAYING.  Diagnostic only. */
int BrMixPlayingCount(const BrMix *pMix);

/* Total buffers alive (created and not yet Released).  Diagnostic only; a
 * leak in the bank loader shows up here. */
int BrMixBufferCount(const BrMix *pMix);

/* Samples the summing stage had to clamp, since the mixer was created.
 *
 * This is not an error counter, it is an honesty counter.  The engine runs
 * three loops at once and BrSndVoiceSetLevels' top level is unity gain, so
 * three samples near full scale sum past it -- DirectSound clipped there too.
 * Reporting the count keeps "the game's own levels overflow" distinguishable
 * from "the mixer has a bug", which sound identical. */
long BrMixClipCount(const BrMix *pMix);

/* DirectSound's attenuation law, exposed so the tests can state the expected
 * gain as the law rather than as a magic number.  cB <= BR_MIX_VOLUME_MIN is
 * exactly 0.0. */
double BrMixGainFromCentibels(int32_t cB);

/* ----------------------------------------------------------------- wav I/O */

/* Parse a RIFF/WAVE image already in memory.  On success *ppPcm points INTO
 * pv (nothing is copied) and *pcbPcm is the size of the data chunk.
 *
 * The original reads its .wav files with WINMM's mmio chunk walker
 * (mmioOpenA / mmioDescend / mmioRead / mmioAscend / mmioClose -- the only
 * audio-shaped imports either DLL has).  This is that walk without the
 * Windows dependency: check "RIFF"/"WAVE", then step chunk by chunk taking
 * "fmt " and "data" and skipping everything else, honouring the odd-length
 * pad byte that RIFF requires.
 *
 * Returns 0 on success, -1 if it is not a PCM WAVE this mixer can play. */
int BrMixWavParse(const void *pv, size_t cb, BrMixWaveFormat *pFmt,
                  const void **ppPcm, uint32_t *pcbPcm);

/* Read a whole .wav off disk.  *ppPcm is malloc'd and belongs to the caller
 * (BrSndVoiceFreeChain's BrSndFreeHook frees it when it is a voice's pData).
 * Returns 0 on success; -1 on any failure, in which case nothing is
 * allocated. */
int BrMixWavLoad(const char *pszPath, BrMixWaveFormat *pFmt,
                 void **ppPcm, uint32_t *pcbPcm);

/* Write a PCM .wav.  Used to make the mix inspectable: a rendered file can be
 * asserted against, played, or looked at long after the run.  Returns 0 on
 * success. */
int BrMixWavWrite(const char *pszPath, const BrMixWaveFormat *pFmt,
                  const void *pvPcm, uint32_t cbPcm);

/* Fill in the 22050/16/stereo format the mixer renders. */
void BrMixOutputFormat(BrMixWaveFormat *pFmt);

/* --------------------------------------------------------------- voices */

/* Populate a BrSndVoice from a loaded sample so that BrSndVoiceCreate can
 * upload it.  Takes ownership of nothing: pPcm must outlive the voice, and
 * pFmt is COPIED into a malloc'd block that becomes pVoice->pFormat (which is
 * what BrSndVoiceFreeChain frees first).  Returns 0 on success.
 *
 * f0C is seeded with the sample's own rate, so a voice that is never retuned
 * plays at its recorded pitch; f10/f14 are seeded to the engine's neutral
 * 400, which is centre and unity. */
int BrMixVoiceInit(BrSndVoice *pVoice, const BrMixWaveFormat *pFmt,
                   void *pPcm, uint32_t cbPcm);

#endif /* BR_MIX_H */
