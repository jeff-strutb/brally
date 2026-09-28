/* br_musicaq.h -- the MUSIC backend br_audio.c has been waiting for.
 *
 * WHAT WAS MISSING, MEASURED
 * ==========================
 * The whole music chain is already ported and it dead-ends one step short of
 * a device:
 *
 *   slice8_85.c  BrUiHook85_1004E810   the car-list row -> CD track *pRow + 2
 *   slice5_63.c  BrCdTrackPlay         0x100027C0, the backend selector
 *   slice8_86.c  BrSub10002870 / BrSub100027F0
 *                                      -> BrAudioPlayTrack(g_pBrAudio86, ...)
 *   br_audio.c   BrAudioPlayTrack      pBackend->pfnOpen / pfnStart / ...
 *
 * and `BrAudio::pBackend` is a plain C vtable that NOTHING in the tree ever
 * filled in.  Grep for `BrAudioBackend` before this file existed and every
 * hit is either the declaration or a test's own stub.  So: the hook is
 * installed, the policy is ported, and no byte ever reached an audio device.
 * That is the answer to "does music have a backend" -- it did not.
 *
 * WHY AUDIOFILE + AUDIOQUEUE
 * ==========================
 * br_sfxaq.c's reasoning applies unchanged (plain C, C function-pointer
 * callback, no Objective-C runtime, framework linked through a Mach-O
 * `.linker_option` so build.sh never learns about it).  What is added here is
 * ExtAudioFile, which is the decoding half of the same framework: it opens a
 * file, is told the CLIENT format it should hand back, and does whatever
 * decoding that requires.  That is what lets the port play the FLAC the
 * builder produced with tools/extract_cdaudio.py without this file carrying a
 * FLAC decoder -- and it plays the .wav fixtures the tests use through
 * exactly the same code path, which is what makes the tests evidence about
 * the shipping path rather than about a second one.
 *
 * NO FORMAT IS HARDCODED.  The client format is 16-bit stereo at the FILE's
 * own sample rate, so a 44100 Hz CD rip and a 22050 Hz effect both play at
 * the right pitch.  This is the one place in the port that is allowed to
 * differ from BR_MIX_RATE, because music does not go through BrMix at all --
 * the original's music was a separate device too (Redbook, then EAR).
 *
 * WHAT IT DOES NOT DO
 * ===================
 * It does not choose tracks, clamp indices, wrap, apply the volume curve or
 * decide what happens at end-of-track.  All of that is br_audio.c, which is
 * the ported policy.  This file is open / start / stop / close and four
 * parameter setters.
 *
 * END OF TRACK
 * ============
 * The queue's callback cannot call into br_audio.c: it runs on CoreAudio's
 * thread and BrAudio is not reentrant.  Instead the callback raises a flag
 * and BrMusicAqPump -- called from the game loop -- turns it into exactly one
 * BrAudioOnTrackFinished, which is where BR_AUDIO_REPEAT_TRACK /
 * _ADVANCE / _ONCE get applied.  Looping WITHIN a track is done in the
 * callback by seeking to frame 0, because that never needs to reach policy.
 */
#ifndef BR_MUSICAQ_H
#define BR_MUSICAQ_H

#include "br_audio.h"

/* How many tracks may be open at once.  br_audio.c holds exactly one handle,
 * so this is headroom for a crossfade rather than a requirement. */
#define BR_MUSICAQ_HANDLES 4

/* The backend vtable, ready to hand to BrAudioInit.  Its pUser is this
 * module's own state; there is one instance and it is process-wide, matching
 * the original's single "cdaudio" device. */
const BrAudioBackend *BrMusicAqBackend(void);

/* Non-zero if an output queue can be created at all -- the same question
 * BrSfxAqAvailable answers for effects.  Lets a caller say "no audio device"
 * once instead of failing silently per track. */
int BrMusicAqAvailable(void);

/* Call once per frame with the BrAudio the backend is attached to.  Converts
 * any end-of-track the queue thread observed into exactly one
 * BrAudioOnTrackFinished, on the caller's thread.  Safe with NULL. */
void BrMusicAqPump(BrAudio *pAudio);

/* Stop and close everything.  Safe to call twice. */
void BrMusicAqShutdown(void);

/* ------------------------------------------------------------ diagnostics */

/* Frames actually handed to the device since the process started, summed over
 * every handle.  This is the number that says "music was audible": it moves
 * only when the queue thread has consumed decoded PCM. */
uint64_t BrMusicAqFramesPlayed(void);

/* The last CoreAudio OSStatus that made an operation fail, and the operation
 * that produced it.  Zero / NULL when nothing has failed. */
int         BrMusicAqLastError(void);
const char *BrMusicAqLastErrorWhere(void);

/* Forget the last error.  A caller that deliberately opens a missing file --
 * the negative control every audio test needs -- would otherwise leave that
 * failure latched and read it back as a real one. */
void        BrMusicAqClearError(void);

#endif /* BR_MUSICAQ_H */
