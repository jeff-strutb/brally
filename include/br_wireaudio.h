/* br_wireaudio.h -- the ONE call that makes the game audible.
 *
 * WHERE THIS LIVES AND WHY IT IS NOT IN port/host/
 * ================================================
 * The brief for this pass asked for `port/host/br_wireaudio.c`.  build.sh
 * DISCOVERS port/src/ and port/tests/test_ sources, but names the six
 * port/host/ .c files one by one -- so a seventh file there would be compiled
 * by nothing and linked into nothing, and the pass was also told not to edit
 * build.sh.  port/src/ is the only directory whose new files are picked up
 * automatically and linked into build/brally, so the wiring lives here.  It is
 * still host wiring and nothing in port/src/ calls it.
 *
 * WHAT IT WIRES, AND WHAT WAS ALREADY THERE
 * =========================================
 * Almost everything was already there.  What was missing was an OPEN DEVICE
 * and a BANK, so every ported play call ran to completion against a NULL
 * mixer and returned success:
 *
 *   MENU        br_uinav.c's BrUiNavCtlFrame_10048180 already transcribes
 *               Glide 0x100415D0's activate block -- BrSub10072AF0(1 or 2,
 *               0x200020) -> slice4_50.c -> slice1_08.c's BrSndPlaySimple.
 *               Wholly ported, and silent because BrSndVoices was empty.
 *               (Checked in the binary: 0x100415D0 is the ONLY menu function
 *               that plays anything.  The page walk 0x10041980 and the hit
 *               test 0x10040EB0 both move the cursor at 0x10AC5BC4 and
 *               neither calls the sound engine, so the PC build has no
 *               selection-MOVE sound at all -- only activate.)
 *   COUNTDOWN   br_racestep.c's BR_RS_HOLE_SOUND, four hits per race, hooked
 *               here to br_sfxsrc.c's transcription of 0x10060DF0/0x10060E00.
 *   MUSIC       slice8_85 -> slice5_63 -> slice8_86 -> br_audio.c was
 *               complete except that BrAudio::pBackend was never filled and
 *               g_pBrAudio86 was never set.  Both happen here, against
 *               br_musicaq.c.
 *   VOLUME      slice6_76.c's g_pfnBrMusicVolume0029F0, the 0x100029F0 seam,
 *               installed here.
 *
 * WHAT IT DOES NOT DO
 * ===================
 * It never decides a group, a pitch, a pan or a volume.  Every number that
 * reaches the mixer was computed by ported code.
 */
#ifndef BR_WIREAUDIO_H
#define BR_WIREAUDIO_H

#include "br_audio.h"
#include "br_sfxout.h"

/* Where the effect bank is read from when BR_SFX_DIR is not set. */
#define BR_WIREAUDIO_SFX_DIR   "testdata/sfx/"

/* Where the soundtrack is read from when BR_MUSIC_DIR is not set.  This is
 * tools/extract_cdaudio.py's own documented output directory; nothing is
 * committed there and a machine without it simply has no music, which is
 * reported rather than hidden. */
#define BR_WIREAUDIO_MUSIC_DIR "build/audio/music"

/* THE ONE CALL.  Add it to port/host/brally.c immediately after WireNav().
 *
 * Opens the mixer, loads the MENU effect bank (set 0: front-end5, DontQuit,
 * Quit, taunt1..4), installs the race countdown hook and the music volume
 * hook, attaches the music backend, and starts the output queue.  Everything
 * is best-effort: a machine with no device, no sfx/ and no music still boots
 * and still runs, and BrHostWireAudioReport says exactly which of the three
 * was missing. */
void BrHostWireAudio(void);

/* Release everything.  Safe before BrHostWireAudio and safe twice. */
void BrHostWireAudioShutdown(void);

/* Print what was and was not wired: voices loaded, names missing, whether a
 * device opened, how many music tracks were found. */
void BrHostWireAudioReport(void);

/* Call once per frame if music is wanted -- it is what turns an end-of-track
 * observed on the queue thread into br_audio.c's repeat/advance policy.
 * Costs a mutex and nothing else when no music is playing. */
void BrHostWireAudioFrame(void);

/* --------------------------------------------------------------- controls */

/* Set BEFORE BrHostWireAudio: 0 means "do not open an output device", which is
 * what a test wants so it can drive BrSfxLivePumpOffline instead and still get
 * the same tap.  Default 1. */
void BrWireAudioSetLive(int fLive);

/* Swap the loaded effect bank between BR_SFX_SET_MENU and BR_SFX_SET_RACE.
 * The original does exactly this: 0x1006C290 is called with 0 for the front
 * end and with 1 from the race sound init at Glide 0x10061310.  Returns the
 * number of voices loaded. */
int  BrWireAudioLoadSet(int set);

/* The BrSfxOut the wiring owns, or NULL before BrHostWireAudio. */
BrSfxOut *BrWireAudioSfx(void);

/* The BrAudio the wiring owns, or NULL. */
BrAudio  *BrWireAudioMusic(void);

/* Counters, so a caller can prove sound happened rather than assert it. */
int BrWireAudioVoicesLoaded(void);
int BrWireAudioNamesMissing(void);
int BrWireAudioMusicTracks(void);
int BrWireAudioDeviceOpen(void);

#endif /* BR_WIREAUDIO_H */
