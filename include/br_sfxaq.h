/* br_sfxaq.h -- the one file in the port that talks to a real audio device.
 *
 * WHY AUDIOTOOLBOX AND NOT COREAUDIO PROPER
 * =========================================
 * AudioQueue is plain C, takes a C function pointer as its callback and needs
 * no Objective-C runtime, no AVFoundation object graph and no HAL property
 * dance.  AUHAL/AudioUnit would work too but wants component descriptions,
 * property setting and a render callback with AudioBufferList unpacking --
 * three times the code for the same PCM.  The build compiles exactly one
 * Objective-C file, by name, so a .m here would silently not be built at all;
 * this is C and is picked up by the ordinary port/src sweep.
 *
 * The framework is linked WITHOUT touching build.sh, via the Mach-O
 * `.linker_option` directive in the .c -- the same mechanism clang's module
 * autolinking uses.  Only a binary that links this object pulls the framework
 * in, so no test acquires a dependency on it.
 *
 * WHAT IT DOES NOT DO
 * ===================
 * It does not mix, resample, pan or attenuate.  It receives finished
 * interleaved 16-bit stereo at BR_MIX_RATE from BrMixRender and hands it to
 * the system.  Everything audible was decided upstream.
 */
#ifndef BR_SFXAQ_H
#define BR_SFXAQ_H

#include <stdint.h>

/* Play a finished block and BLOCK until it has been heard, then tear the
 * queue down.  Signature is BrSfxSinkFn, so it drops straight into
 * BrSfxDemoPlay / BrSfxDemoRpmSweep.
 *
 * pUser is ignored.  Returns 1 on success and 0 if no device could be opened
 * -- which is not an error the caller should treat as fatal: a machine with
 * no output device still renders the .wav, and the .wav is the evidence. */
int BrSfxAqPlay(void *pUser, const int16_t *pPcm, int cFrames);

/* Non-zero if an output queue could be created at all.  Lets the harness say
 * "no audio device" once rather than failing silently. */
int BrSfxAqAvailable(void);

#endif /* BR_SFXAQ_H */
