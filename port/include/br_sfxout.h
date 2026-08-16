/* br_sfxout.h -- wiring: the bank table (br_sfx) + the voice wrapper
 * (slice1_08) + the mixer (br_mix), joined into something that makes sound.
 *
 * This is 0x1006C290 (D3D 0x10073320, the bank loader) and 0x1006C010
 * (0x100730A0, the per-car engine loader) -- the two functions slice1_08.c
 * explicitly left out because they need file I/O and a device.  Both are
 * pure glue over pieces that already exist:
 *
 *      for each group the set names
 *          name  = BrSfxGroupFileName(set, group)         br_sfx.c
 *          pcm   = BrMixWavLoad(name)                     br_mix.c
 *          voice = BrMixVoiceInit(pcm)                    br_mix.c
 *          BrSndVoiceCreate(voice)                        slice1_08.c
 *          BrSndVoices[group*18 + slot] = voice           slice1_08.c
 *
 * and the per-car loader is the same three times with BrSfxCarFileName's
 * three suffixes, on channel 2*iCar.
 *
 * NOTHING HERE DECIDES A PITCH, A PAN OR A VOLUME.  BrSfxOutEngine hands the
 * ported curve (BrSfxEngineHz / BrSfxEngineRatio / BrSfxEngineHighHz) to the
 * ported setters, and the setters call the mixer.  The demo below sweeps the
 * INPUT to that curve; it does not compute an output.
 *
 * No audio device is opened by anything in this file.  Rendering goes to a
 * caller-supplied sink, which is a .wav writer under test and the CoreAudio
 * queue (br_sfxaq.c) when the host wants speakers.
 */
#ifndef BR_SFXOUT_H
#define BR_SFXOUT_H

#include <stdint.h>

#include "br_mix.h"
#include "br_sfx.h"
#include "slice1_08.h"

/* Where the loader looks.  The original's default is "sfx/" out of a mutable
 * 0x40-byte buffer that BossRally.ini's SFXDir= overwrites; the port's
 * extracted copy lands in testdata/sfx/, so that is the default here and the
 * ini's role is taken by the pszDir argument.  Note the trailing separator:
 * br_sfx.c appends none. */
#define BR_SFXOUT_DIR_DEFAULT  "testdata/sfx/"

/* The original builds its names in a 0x400-byte stack buffer. */
#define BR_SFXOUT_PATH_MAX     0x400

typedef struct BrSfxOut {
    BrMix *pMix;

    int    cLoaded;         /* voices successfully created                  */
    int    cMissing;        /* names the bank built that were not on disk   */
    int    set;             /* BR_SFX_SET_RACE / _MENU                      */
    int    iCar;            /* bank car index + 1, 0 == no engine loaded    */

    /* Everything the loader allocated, so teardown is not a guess.  Voices
     * are NOT chained through pNext here -- BrSndVoiceFreeChain walks a
     * chain rooted at a head node the original keeps elsewhere, and this
     * module owns a flat table instead. */
    BrSndVoice *aOwned[BR_SFX_GROUPS * BR_SFX_SLOTS];
    int         cOwned;
} BrSfxOut;

/* ------------------------------------------------------------- lifecycle */

/* Create the mixer and install it where the port expects DirectSound:
 * BrSndPDS and the BrSndG18290FC init guard.  After this every BrSnd* entry
 * point in slice1_08.c is live.  Returns 0 on success. */
int  BrSfxOutOpen(BrSfxOut *pOut);

/* Release every voice, destroy the mixer and clear BrSndVoices, BrSndPDS and
 * the guard.  Safe on a zeroed or already-closed BrSfxOut. */
void BrSfxOutClose(BrSfxOut *pOut);

/* --------------------------------------------------------------- loading */

/* 0x1006C290.  Load every generic group the set names, from pszDir (NULL =
 * BR_SFXOUT_DIR_DEFAULT), into the slot the shipped bank marks for it.
 * Returns the number of voices created; pOut->cMissing counts the names that
 * had no file, which is how a partial asset extraction reports itself
 * instead of looking like success. */
int  BrSfxOutLoadSet(BrSfxOut *pOut, int set, const char *pszDir);

/* 0x1006C010.  Load one car's three engine layers -- "<cc>.wav", "<cc>h.wav",
 * "<cc>r.wav" -- into groups 0, 24 and 25 on channel 2*iCar.  iName is the
 * bank value, i.e. car index + 1.  Returns the number created (0..3). */
int  BrSfxOutLoadCar(BrSfxOut *pOut, int iName, int iCar, const char *pszDir);

/* ------------------------------------------------------------- playback */

/* Play one generic group on the slot the bank marks for it, at the given
 * packed level pair (see BR_SFX_LEVEL_MAX: high half LEFT, low half RIGHT,
 * each 0..32).  This is BrSndPlayEx with the slot looked up rather than
 * assumed, because beep/beep2/water sit on slot 3 and everything else on
 * slot 1.  Returns non-zero when a voice was actually started. */
int  BrSfxOutPlayGroup(BrSfxOut *pOut, int set, int group, uint32_t packed,
                       int loop);

/* Start a car's three engine loops looping, at the given level pair. */
int  BrSfxOutEngineStart(BrSfxOut *pOut, int iCar, uint32_t packed);

/* Retune a car's engine to an RPM and a doppler ratio.  This is the whole
 * point of the seam: the frequencies come out of br_sfx.c's ported curve and
 * go in through slice1_08.c's ported setters.
 *
 *   low  (group 0)   BrSfxHzFromRatio(BrSfxEngineRatio(BrSfxEngineHz(...)))
 *   rev  (group 25)  the same ratio, against the same 11025 base rate
 *   high (group 24)  BrSfxEngineHighHz(doppler)
 *
 * Returns the low layer's applied frequency in hertz, so a caller (or a
 * test) can see the curve without reaching into the mixer. */
uint32_t BrSfxOutEngineSetRpm(BrSfxOut *pOut, int iCar, float rpm,
                              float doppler);

/* ------------------------------------------------------------ rendering */

/* Called with each rendered block of interleaved 16-bit stereo at
 * BR_MIX_RATE.  Return non-zero to keep going, zero to stop early. */
typedef int (*BrSfxSinkFn)(void *pUser, const int16_t *pPcm, int cFrames);

/* Render cFrames frames in blocks, feeding each block to pfnSink.  Between
 * blocks pfnStep (may be NULL) is called with the elapsed time in seconds,
 * which is where the RPM sweep changes the input.  Returns the number of
 * frames actually rendered. */
typedef void (*BrSfxStepFn)(void *pUser, double tSeconds);

int  BrSfxOutRender(BrSfxOut *pOut, int cFrames, int cBlock,
                    BrSfxSinkFn pfnSink, void *pSinkUser,
                    BrSfxStepFn pfnStep, void *pStepUser);

/* A sink that appends to a growing buffer, for callers that want the whole
 * mix in memory (to write, to analyse, or both). */
typedef struct BrSfxCapture {
    int16_t *pPcm;          /* 2 * cFrames int16_t, malloc'd                */
    int      cFrames;
    int      cCap;
    int      failed;
} BrSfxCapture;

int  BrSfxCaptureSink(void *pUser, const int16_t *pPcm, int cFrames);
void BrSfxCaptureFree(BrSfxCapture *pCap);

/* Write a capture out as a 22050/16/stereo .wav.  Returns 0 on success. */
int  BrSfxCaptureWrite(const BrSfxCapture *pCap, const char *pszPath);

/* --------------------------------------------------------------- harness */

/* The `-sfx` / `-sfxrpm` modes.  argv is the tail after the mode word.
 * pfnSink may be NULL, in which case the mix is only written to the file.
 * Returns a process exit code. */
int  BrSfxDemoPlay(const char *pszGroup, const char *pszWavOut,
                   BrSfxSinkFn pfnSink, void *pSinkUser);
int  BrSfxDemoRpmSweep(const char *pszCar, const char *pszWavOut,
                       BrSfxSinkFn pfnSink, void *pSinkUser);

/* Resolve a name typed on the command line to (set, group).  Accepts a bare
 * number, or any name in either set's table with or without ".wav".
 * Returns 0 on success. */
int  BrSfxLookupGroup(const char *pszName, int *pSet, int *pGroup);

/* Resolve a two-letter car code, or a number, to the bank value (car+1).
 * Returns 0 when it is not a car. */
int  BrSfxLookupCar(const char *pszName);

#endif /* BR_SFXOUT_H */
