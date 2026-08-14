/* br_audio.h -- soundtrack playback for the port (portable C99).
 *
 * Decompiled from the CD-audio module in BRD3D.dll (0x10002260-0x10002D90, ~40
 * module globals at 0x10220C38-0x10220CDC) and rewritten to drive local lossless
 * files instead of a CD or a proprietary DLL.
 *
 * WHAT THE ORIGINALS DID
 * ----------------------
 * The retail PC game had two music backends, chosen by `PlayMusic=` in
 * BossRally.ini (parsed at 0x10007BD0, stored to the selector at 0x10008061):
 *
 *   0  no music at all. Every entry point tests the selector first.
 *   1  Redbook CD audio, through WINMM mciSendCommandA on the "cdaudio" device.
 *   2  EAR ("EAR Interactive Around-Sound", earpds.dll / earias.dll), reached
 *      through a function-pointer table at 0x10575450-0x105754A0.
 *
 * 2 is the value baked into .data, so an install with no ini file used EAR.
 *
 * The two backends do NOT behave the same at end-of-track, and the difference is
 * the single most important thing this header has to settle:
 *
 *   CD   MM_MCINOTIFY (0x3B9) -> 0x10002510 -> re-issues MCI_PLAY of the SAME
 *        track. A track repeats forever until something else changes it.
 *   EAR  the EAR completion message -> 0x100029B0 CdTrackNextWrap -> advances to
 *        the next track, wrapping past the last back to the first.
 *
 * Track numbering was CD track numbers: the data track is 1, so music is 2..13,
 * twelve tracks, named in a table at 0x100B89C8. This header uses 0-based track
 * INDICES and leaves the +2 to the caller, because the N64 soundtrack has six
 * tracks and no data track. `BrAudioTrackName` keeps the names.
 *
 * Selection rules, all from 0x1002C500 (the race driver) and 0x10038EC0 (init):
 *   - front end plays index 0 (CD track 2), once, at startup.
 *   - a race normally plays a RANDOM track. 0x100028D0 computes
 *         t = rand() * (nTracks - 5) / 32768 + 3          [CD numbering]
 *     clamped to [3, nTracks], re-rolled while it equals the current track (only
 *     when nTracks > 6). With the retail disc's 13 tracks that is 3..10 -- the
 *     first and last three tracks are never picked at random.
 *   - one game mode instead forces CD track 12 or 13.
 *
 * Volume was never a bool despite one backend treating it as one. It comes from a
 * ten-entry table at 0x100ADF68 indexed by the user's 0..9 setting:
 *     { 0x00, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF }
 * The CD backend only tested it against zero; EAR scaled it (see
 * BrAudioVolumeScale). Callers gate on `volume != 0` BEFORE starting playback, so
 * zero means "no music", not "music at silence".
 *
 * WHAT THIS DOES INSTEAD
 * ----------------------
 * The port plays locally produced lossless files -- FLAC, generated on the
 * builder's machine by tools/extract_cdaudio.py and tools/extract_xm.py from
 * their own disc image and ROM. Neither Redbook CD audio nor a Windows-only
 * middleware DLL is acceptable, so the backend indirection is kept but is now a
 * plain C vtable the platform layer fills in (see BrAudioBackend). Everything in
 * this module is policy -- track table, clamping, wrap, volume curve, repeat
 * behaviour -- and none of it touches the operating system, so it is testable
 * with no audio hardware and no files.
 *
 * DEVIATION: the original's two backends are collapsed into one code path plus a
 * BrAudioRepeat setting, because the only behavioural difference that mattered
 * was end-of-track handling. BR_AUDIO_REPEAT_TRACK reproduces the CD backend and
 * BR_AUDIO_REPEAT_ADVANCE reproduces EAR.
 *
 * DEVIATION: the original clamping helpers (0x10002930 CdTrackPrev, 0x10002970
 * CdTrackNext, 0x100029B0 CdTrackNextWrap) each stored the out-of-range value to
 * the current-track global and only then overwrote it with the clamped one. That
 * intermediate store is unobservable -- nothing runs in between -- so the port
 * computes the final value directly.
 *
 * RELATIONSHIP TO THE DECOMPILATION
 * ---------------------------------
 * The decompilation slices already declare the ORIGINAL entry points under
 * `BrCd*` names, against the same addresses:
 *
 *     BrCdTrackGet   0x10002910      g_brCdEnabled     g_brCdTrackCur
 *     BrCdTrackPlay  0x100027C0      g_brCdPlaying     g_brCdTrackFirst
 *     (slice5_60.h)  0x10002930 / 0x10002970           g_brCdTrackLast
 *
 * Those stay as they are -- they describe what the binary does. This module is
 * the port's REPLACEMENT for that subsystem, not a second transcription of it,
 * which is why it is named `BrAudio*` and takes 0-based indices. Do not coin a
 * third set of names for these addresses; extend one of the two that exist.
 */
#ifndef BR_AUDIO_H
#define BR_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define BR_AUDIO_MAX_TRACKS   32
#define BR_AUDIO_PATH_MAX     260
#define BR_AUDIO_NAME_MAX     32

/* Volume is 0..255, matching the original's table. */
#define BR_AUDIO_VOLUME_MAX   255

/* Pan is 0 (hard left) .. 255 (hard right); 128 is centre. */
#define BR_AUDIO_PAN_LEFT     0
#define BR_AUDIO_PAN_CENTRE   128
#define BR_AUDIO_PAN_RIGHT    255

/* Returned by BrAudioBackend::pfnOpen for "could not open". */
#define BR_AUDIO_HANDLE_NONE  0

typedef enum BrAudioState {
    BR_AUDIO_STOPPED = 0,
    BR_AUDIO_PLAYING = 1,
    BR_AUDIO_PAUSED  = 2
} BrAudioState;

/* End-of-track policy. See the header comment: these are the two behaviours the
 * two original backends actually had. */
typedef enum BrAudioRepeat {
    BR_AUDIO_REPEAT_TRACK   = 0,  /* replay the same track (the CD backend) */
    BR_AUDIO_REPEAT_ADVANCE = 1,  /* next track, wrapping (the EAR backend) */
    BR_AUDIO_REPEAT_ONCE    = 2   /* stop at the end (neither original did this) */
} BrAudioRepeat;

/* One decoded-or-streaming sound owned by the platform layer.
 *
 * Handles are opaque non-zero ints rather than pointers so that a backend can use
 * a table index, and so that BR_AUDIO_HANDLE_NONE is a valid "none" without
 * inventing a null pointer. This mirrors the original's handle discipline, where
 * 0 was reserved as null (see BrHandleLookup in br_bits.h).
 *
 * Every entry may be NULL except pfnOpen/pfnClose/pfnStart/pfnStop; the module
 * checks before calling, so a minimal backend is only four functions.
 */
typedef struct BrAudioBackend {
    void *pUser;

    /* Open pszPath. Returns a handle, or BR_AUDIO_HANDLE_NONE on failure. */
    int  (*pfnOpen)(void *pUser, const char *pszPath);
    void (*pfnClose)(void *pUser, int handle);

    /* Start from the beginning. Returns non-zero on success. */
    int  (*pfnStart)(void *pUser, int handle);
    void (*pfnStop)(void *pUser, int handle);
    void (*pfnPause)(void *pUser, int handle);
    void (*pfnResume)(void *pUser, int handle);

    void (*pfnSetVolume)(void *pUser, int handle, int volume);  /* 0..255 */
    void (*pfnSetPan)(void *pUser, int handle, int pan);        /* 0..255 */
    void (*pfnSetLoop)(void *pUser, int handle, int loop);      /* 0 or 1 */

    /* Query. pfnGetState returns a BrAudioState; pfnGetPositionMs writes the
     * playback position and returns non-zero if it could. */
    int  (*pfnGetState)(void *pUser, int handle);
    int  (*pfnGetPositionMs)(void *pUser, int handle, uint32_t *pMs);
} BrAudioBackend;

typedef struct BrAudioTrack {
    char szPath[BR_AUDIO_PATH_MAX];
    char szName[BR_AUDIO_NAME_MAX];
} BrAudioTrack;

typedef struct BrAudio {
    const BrAudioBackend *pBackend;

    BrAudioTrack aTracks[BR_AUDIO_MAX_TRACKS];
    int          cTracks;

    /* Selection window. The original hardcoded a floor of CD track 2 (index 0)
     * and drew random tracks from a narrower band still; see BrAudioSetRandomRange. */
    int          iFirstRandom, iLastRandom;

    int          iCurrent;      /* index of the requested track, -1 if none */
    int          handle;        /* backend handle of the open track */
    int          state;         /* BrAudioState */
    int          repeat;        /* BrAudioRepeat */

    int          volume;        /* 0..255 */
    int          pan;           /* 0..255 */
    int          enabled;       /* 0 = module disabled entirely */
    int          started;       /* has anything ever been asked to play */

    uint32_t     randState;
} BrAudio;

/* ---------------------------------------------------------------- lifecycle */

/* pBackend may be NULL, in which case the module runs as a pure state machine --
 * useful for headless tests and for a build with no audio device. Returns 0 on
 * success. */
int  BrAudioInit(BrAudio *pAudio, const BrAudioBackend *pBackend);
void BrAudioShutdown(BrAudio *pAudio);

/* Original: the `PlayMusic=0` case, which every entry point tested for.
 * Disabling stops anything playing; re-enabling does not resume by itself. */
void BrAudioSetEnabled(BrAudio *pAudio, int enabled);
int  BrAudioIsEnabled(const BrAudio *pAudio);

/* ------------------------------------------------------------- track table */

/* Append a track. pszName may be NULL. Returns the new index, or -1 if the table
 * is full or the path does not fit. */
int         BrAudioAddTrack(BrAudio *pAudio, const char *pszPath,
                            const char *pszName);
void        BrAudioClearTracks(BrAudio *pAudio);
int         BrAudioTrackCount(const BrAudio *pAudio);
const char *BrAudioTrackName(const BrAudio *pAudio, int iTrack);
const char *BrAudioTrackPath(const BrAudio *pAudio, int iTrack);

/* The band BrAudioPickRandomTrack draws from, inclusive. Defaults to the whole
 * table. The retail game's equivalent window was CD tracks 3..10 of 2..13, i.e.
 * indices 1..8 -- it excluded the front-end track and the last three. */
void BrAudioSetRandomRange(BrAudio *pAudio, int iFirst, int iLast);

/* -------------------------------------------------------------- transport */

/* Original: 0x100027C0 CdPlayTrack. iTrack is clamped into the table. Returns 0
 * on success. A disabled module, a zero volume or an empty table is not an
 * error -- it is simply silent, matching the original's callers, which gate on
 * volume != 0 before ever calling this. */
int  BrAudioPlayTrack(BrAudio *pAudio, int iTrack);

/* Original: 0x10002C30 CdStop, 0x10002B70 CdPause, 0x10002BD0 CdResume. */
void BrAudioStop(BrAudio *pAudio);
void BrAudioPause(BrAudio *pAudio);
void BrAudioResume(BrAudio *pAudio);

/* Original: 0x10002930 / 0x10002970 / 0x100029B0.
 * Prev and Next CLAMP at the ends; NextWrap WRAPS. All three then play. */
void BrAudioTrackPrev(BrAudio *pAudio);
void BrAudioTrackNext(BrAudio *pAudio);
void BrAudioTrackNextWrap(BrAudio *pAudio);

/* Original: 0x100028D0. Draws from the random range, avoiding the current track
 * when the range holds more than one candidate. Deterministic given the seed. */
int  BrAudioPickRandomTrack(BrAudio *pAudio);
void BrAudioSeedRandom(BrAudio *pAudio, uint32_t seed);

/* Call when the backend reports a track has finished. Replaces the original's
 * MM_MCINOTIFY (0x3B9) and EAR-completion window messages, which were handled in
 * the WndProc at 0x10079CA0. Applies the BrAudioRepeat policy. */
void BrAudioOnTrackFinished(BrAudio *pAudio);

void BrAudioSetRepeat(BrAudio *pAudio, int repeat);

/* ----------------------------------------------------------- volume / pan */

/* Original: 0x100029F0 / 0x10002A80. volume is 0..255; 0 stops playback, because
 * the original's callers treated zero as "no music" rather than "silent music". */
void BrAudioSetVolume(BrAudio *pAudio, int volume);
int  BrAudioGetVolume(const BrAudio *pAudio);

/* The retail ten-step music setting -> 0..255, from the table at 0x100ADF68.
 * Out-of-range settings clamp. */
int  BrAudioVolumeFromSetting(int setting);

/* Original: 0x10002A20 BrCdVolumeScale, the EAR attenuation conversion.
 *
 *     level = (volume & 0xFF) * 10000 / 128
 *
 * DEVIATION (documented, not fixed): the mask is faithful, and it means 256 is
 * SILENCE rather than full volume -- 256 & 0xFF == 0. The scale factor also
 * implies the author expected a 0..128 input, so the volume table's 0xFF maximum
 * overshoots a presumed 10000 full scale by roughly 2x. Both behaviours are
 * preserved here so the curve matches; clamp at the call site if you want sanity
 * rather than fidelity. */
int  BrAudioVolumeScale(int volume);

void BrAudioSetPan(BrAudio *pAudio, int pan);
int  BrAudioGetPan(const BrAudio *pAudio);

/* -------------------------------------------------------------- querying */

/* Original: 0x10002910 CdGetCurrentTrack, polled every 120 frames on the CD
 * backend and every frame on EAR purely to keep the options-screen listbox in
 * sync (0x100488C0). Returns -1 when nothing is selected. */
int  BrAudioGetCurrentTrack(const BrAudio *pAudio);
int  BrAudioGetState(const BrAudio *pAudio);
int  BrAudioIsPlaying(const BrAudio *pAudio);

/* Position of the playing track in milliseconds. Returns 0 if unavailable. */
int  BrAudioGetPositionMs(const BrAudio *pAudio, uint32_t *pMs);

/* ------------------------------------------------------- format probing */

typedef struct BrAudioFormat {
    uint32_t sampleRate;
    int      channels;
    int      bitsPerSample;
    uint64_t totalFrames;    /* 0 means "unknown", which FLAC does encode */
} BrAudioFormat;

/* Read a FLAC STREAMINFO block out of the first bytes of a file, so the port can
 * report a track's duration without decoding it.
 *
 * STREAMINFO is bit-packed big-endian, so this decodes byte-wise and shifts by
 * hand -- no struct overlay, no endian assumption, no alignment requirement.
 * cb need only cover the 4-byte magic, the 4-byte block header and 34 bytes of
 * STREAMINFO (42 bytes total). Returns 0 on success. */
int  BrAudioProbeFlac(const void *pvData, size_t cb, BrAudioFormat *pFmt);

/* Duration in milliseconds from a probed format, or 0 if unknown. */
uint32_t BrAudioFormatDurationMs(const BrAudioFormat *pFmt);

#endif /* BR_AUDIO_H */
