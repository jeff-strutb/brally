/* br_sfxlive.h -- CONTINUOUS effect output, as against br_sfxaq.c's
 * play-one-finished-block-and-block.
 *
 * WHY A SECOND AUDIOQUEUE FILE
 * ============================
 * br_sfxaq.c is the right shape for the `-sfx` demo and the wrong shape for a
 * game: it takes a finished buffer, plays it, and does not return until it has
 * been heard.  Nothing in a menu or a race can call that.  What a running game
 * needs is the opposite -- a queue that is already running, pulling from
 * BrMixRender whenever the device wants more, so that a BrSndPlayEx issued on
 * the game thread becomes audible on its own.
 *
 * So this file is br_sfxaq.c inverted, and it stays just as dumb: it owns no
 * voices, decides no pitch, pan or volume, and its callback does exactly one
 * thing -- BrMixRender into a system buffer.
 *
 * THREADING, STATED RATHER THAN ASSUMED
 * =====================================
 * BrMixRender runs on CoreAudio's thread; BrSndPlayEx and friends run on the
 * game thread.  br_mix.c's render loop reads `playing`, `pos`, `freq`, `vol`
 * and `pan` per voice and writes `playing` and `pos`, so a concurrent start is
 * a torn read of at most one voice for at most one block.
 *
 * BrSfxLiveLock / BrSfxLiveUnlock close that properly for any caller willing
 * to bracket its play calls, and br_wireaudio.c's helpers do.  The menu's own
 * activate sound goes through slice4_50.c's BrSub10072AF0 and is NOT bracketed
 * -- that path is inside ported code this pass does not own.  The failure mode
 * is one glitched 1024-frame block, not corruption, and it is recorded here
 * rather than papered over.
 *
 * THE TAP
 * =======
 * BrSfxLiveTapBegin makes the queue keep a copy of every sample it hands to
 * the device.  That copy, written out by BrSfxLiveTapWrite, is the evidence
 * this port's convention asks for -- and it is stronger than rendering a
 * separate .wav offline, because it is literally what went to the speakers on
 * that run rather than a second rendering of the same inputs.
 */
#ifndef BR_SFXLIVE_H
#define BR_SFXLIVE_H

#include <stdint.h>

#include "br_mix.h"

/* Frames per system buffer, and how many.  1024 frames at BR_MIX_RATE is
 * 46.4 ms, so three buffers is about 139 ms of latency -- comfortably inside
 * what a start-light beep needs and far enough from underrun that a 30 Hz
 * game loop cannot starve it. */
#define BR_SFXLIVE_FRAMES   1024
#define BR_SFXLIVE_BUFFERS  3

/* Start pulling pMix.  Returns 1 on success, 0 if no device could be opened
 * -- which is not fatal: everything upstream still runs and the tap still
 * records, so a machine with no output device still produces the .wav. */
int  BrSfxLiveStart(BrMix *pMix);

/* Stop and dispose.  Safe on a queue that never started. */
void BrSfxLiveStop(void);

/* Non-zero while a queue is running. */
int  BrSfxLiveRunning(void);

/* Frames actually rendered into system buffers since the queue started.
 * Zero after a run means the device never asked for anything, which is a
 * different failure from "it asked and got silence". */
uint64_t BrSfxLiveFramesRendered(void);

/* Bracket a burst of voice mutations against the render callback. */
void BrSfxLiveLock(void);
void BrSfxLiveUnlock(void);

/* ----------------------------------------------------------------- the tap */

/* Record up to cFrames frames of everything the queue renders.  Returns 1 on
 * success.  A second call replaces the buffer. */
int  BrSfxLiveTapBegin(int cFrames);

/* Frames captured so far. */
int  BrSfxLiveTapFrames(void);

/* Peak absolute sample in the tap, 0..32768.  A tap that is all zeros is a
 * run in which the mixer produced silence -- which is exactly the outcome
 * this module exists to be able to distinguish from "no device". */
int  BrSfxLiveTapPeak(void);

/* Write the tap as a BR_MIX_RATE / 16-bit / stereo .wav.  Returns 0 on
 * success. */
int  BrSfxLiveTapWrite(const char *pszPath);

void BrSfxLiveTapEnd(void);

/* ------------------------------------------------------------- offline mode */

/* When no device is wanted (a test, a headless run), the caller drives the
 * mixer itself: this renders cFrames straight into the tap, exactly as the
 * callback would, so the same evidence file comes out. */
int  BrSfxLivePumpOffline(int cFrames);

/* Point the offline path at a mixer without starting a queue. */
void BrSfxLiveSetMix(BrMix *pMix);

#endif /* BR_SFXLIVE_H */
