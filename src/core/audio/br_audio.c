/* br_audio.c -- soundtrack playback for the port. See br_audio.h.
 *
 * Decompiled from the CD-audio module in BRD3D.dll (0x10002260-0x10002D90) and
 * rewritten to drive local lossless files. Everything here is policy; all actual
 * sound goes through the BrAudioBackend vtable, which the platform layer fills
 * in. That keeps this file free of any OS dependency and lets the whole module
 * be tested with no audio device -- see port/tests/test_audio.c.
 *
 * Deliberate deviations from the original, each noted at the point it applies:
 *   - the two backends (MCI "cdaudio" and EAR) collapse into one code path plus
 *     a repeat policy, because end-of-track handling was their only behavioural
 *     difference;
 *   - the track-stepping helpers compute the clamped index directly instead of
 *     storing the out-of-range value first and overwriting it;
 *   - the FLAC probe decodes byte-wise, so it is endian- and alignment-agnostic.
 */
#include "br_audio.h"

#include <string.h>

/* ------------------------------------------------------------------ helpers */

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Bounded copy that always terminates. Returns 0 if the source did not fit. */
static int copy_str(char *pszDst, size_t cbDst, const char *pszSrc)
{
    size_t n;
    if (cbDst == 0)
        return 0;
    if (pszSrc == NULL) {
        pszDst[0] = '\0';
        return 1;
    }
    n = strlen(pszSrc);
    if (n >= cbDst) {
        pszDst[0] = '\0';
        return 0;
    }
    memcpy(pszDst, pszSrc, n + 1);
    return 1;
}

static int has_backend(const BrAudio *pAudio)
{
    return pAudio != NULL && pAudio->pBackend != NULL;
}

/* Close whatever is open, without touching the selected-track index. */
static void release_handle(BrAudio *pAudio)
{
    if (pAudio->handle != BR_AUDIO_HANDLE_NONE && has_backend(pAudio)) {
        const BrAudioBackend *b = pAudio->pBackend;
        if (b->pfnStop != NULL)
            b->pfnStop(b->pUser, pAudio->handle);
        if (b->pfnClose != NULL)
            b->pfnClose(b->pUser, pAudio->handle);
    }
    pAudio->handle = BR_AUDIO_HANDLE_NONE;
    pAudio->state  = BR_AUDIO_STOPPED;
}

/* ---------------------------------------------------------------- lifecycle */

int BrAudioInit(BrAudio *pAudio, const BrAudioBackend *pBackend)
{
    if (pAudio == NULL)
        return 1;

    memset(pAudio, 0, sizeof(*pAudio));
    pAudio->pBackend    = pBackend;
    pAudio->iCurrent    = -1;
    pAudio->handle      = BR_AUDIO_HANDLE_NONE;
    pAudio->state       = BR_AUDIO_STOPPED;
    pAudio->repeat      = BR_AUDIO_REPEAT_ADVANCE;
    pAudio->volume      = BR_AUDIO_VOLUME_MAX;
    pAudio->pan         = BR_AUDIO_PAN_CENTRE;
    pAudio->enabled     = 1;
    pAudio->iFirstRandom = 0;
    pAudio->iLastRandom  = -1;    /* -1 means "the whole table" */
    pAudio->randState   = 1;      /* the CRT's default rand() seed */
    return 0;
}

void BrAudioShutdown(BrAudio *pAudio)
{
    if (pAudio == NULL)
        return;
    release_handle(pAudio);
    pAudio->pBackend = NULL;
    pAudio->cTracks  = 0;
    pAudio->iCurrent = -1;
}

void BrAudioSetEnabled(BrAudio *pAudio, int enabled)
{
    if (pAudio == NULL)
        return;
    enabled = (enabled != 0);
    if (pAudio->enabled == enabled)
        return;
    pAudio->enabled = enabled;
    /* Disabling silences immediately. Re-enabling deliberately does NOT resume:
     * the original's callers re-issued a play request themselves. */
    if (!enabled)
        release_handle(pAudio);
}

/* @n64 0x8026B930 located */
int BrAudioIsEnabled(const BrAudio *pAudio)
{
    return (pAudio != NULL && pAudio->enabled) ? 1 : 0;
}

/* -------------------------------------------------------------- track table */

int BrAudioAddTrack(BrAudio *pAudio, const char *pszPath, const char *pszName)
{
    BrAudioTrack *pTrack;
    int i;

    if (pAudio == NULL || pszPath == NULL || pszPath[0] == '\0')
        return -1;
    if (pAudio->cTracks >= BR_AUDIO_MAX_TRACKS)
        return -1;

    i = pAudio->cTracks;
    pTrack = &pAudio->aTracks[i];
    if (!copy_str(pTrack->szPath, sizeof(pTrack->szPath), pszPath))
        return -1;
    /* A name that does not fit is truncated away rather than failing the add --
     * the name is cosmetic (the options screen listbox), the path is not. */
    if (!copy_str(pTrack->szName, sizeof(pTrack->szName), pszName))
        pTrack->szName[0] = '\0';

    pAudio->cTracks = i + 1;
    return i;
}

void BrAudioClearTracks(BrAudio *pAudio)
{
    if (pAudio == NULL)
        return;
    release_handle(pAudio);
    pAudio->cTracks = 0;
    pAudio->iCurrent = -1;
    pAudio->iFirstRandom = 0;
    pAudio->iLastRandom = -1;
}

/* @n64 0x80220534 located */
int BrAudioTrackCount(const BrAudio *pAudio)
{
    return (pAudio != NULL) ? pAudio->cTracks : 0;
}

static int track_valid(const BrAudio *pAudio, int iTrack)
{
    return pAudio != NULL && iTrack >= 0 && iTrack < pAudio->cTracks;
}

const char *BrAudioTrackName(const BrAudio *pAudio, int iTrack)
{
    if (!track_valid(pAudio, iTrack))
        return NULL;
    return pAudio->aTracks[iTrack].szName;
}

const char *BrAudioTrackPath(const BrAudio *pAudio, int iTrack)
{
    if (!track_valid(pAudio, iTrack))
        return NULL;
    return pAudio->aTracks[iTrack].szPath;
}

void BrAudioSetRandomRange(BrAudio *pAudio, int iFirst, int iLast)
{
    if (pAudio == NULL)
        return;
    pAudio->iFirstRandom = iFirst;
    pAudio->iLastRandom  = iLast;
}

/* Resolve the configured random window against the current table. */
static void random_window(const BrAudio *pAudio, int *piFirst, int *piLast)
{
    int lo = pAudio->iFirstRandom;
    int hi = (pAudio->iLastRandom < 0) ? pAudio->cTracks - 1 : pAudio->iLastRandom;

    lo = clampi(lo, 0, pAudio->cTracks - 1);
    hi = clampi(hi, 0, pAudio->cTracks - 1);
    if (hi < lo)
        hi = lo;
    *piFirst = lo;
    *piLast  = hi;
}

/* ---------------------------------------------------------------- transport */

/* Open and start the selected track. Silence is not failure; see br_audio.h. */
static int start_current(BrAudio *pAudio)
{
    const BrAudioBackend *b;
    int handle;

    release_handle(pAudio);

    if (!pAudio->enabled || pAudio->volume == 0)
        return 0;
    if (!track_valid(pAudio, pAudio->iCurrent))
        return 0;
    if (!has_backend(pAudio) || pAudio->pBackend->pfnOpen == NULL)
        return 0;

    b = pAudio->pBackend;
    handle = b->pfnOpen(b->pUser, pAudio->aTracks[pAudio->iCurrent].szPath);
    if (handle == BR_AUDIO_HANDLE_NONE)
        return 1;

    pAudio->handle = handle;

    /* Push the current mixer state before starting, so the first samples out are
     * already at the right level rather than jumping a frame later. */
    if (b->pfnSetVolume != NULL)
        b->pfnSetVolume(b->pUser, handle, pAudio->volume);
    if (b->pfnSetPan != NULL)
        b->pfnSetPan(b->pUser, handle, pAudio->pan);
    if (b->pfnSetLoop != NULL)
        b->pfnSetLoop(b->pUser, handle,
                      pAudio->repeat == BR_AUDIO_REPEAT_TRACK ? 1 : 0);

    if (b->pfnStart == NULL || !b->pfnStart(b->pUser, handle)) {
        release_handle(pAudio);
        return 1;
    }
    pAudio->state = BR_AUDIO_PLAYING;
    return 0;
}

int BrAudioPlayTrack(BrAudio *pAudio, int iTrack)
{
    if (pAudio == NULL)
        return 1;
    if (pAudio->cTracks == 0)
        return 0;

    /* Original: 0x100027F0 clamps into [firstTrack, trackCount] before storing.
     * The selected index is recorded even when the module is disabled or muted,
     * because the options screen reads it back to show what "would" be playing. */
    pAudio->iCurrent = clampi(iTrack, 0, pAudio->cTracks - 1);
    pAudio->started  = 1;
    return start_current(pAudio);
}

/* @n64 0x8021C718 located */
void BrAudioStop(BrAudio *pAudio)
{
    if (pAudio == NULL)
        return;
    release_handle(pAudio);
}

void BrAudioPause(BrAudio *pAudio)
{
    const BrAudioBackend *b;
    if (pAudio == NULL || pAudio->state != BR_AUDIO_PLAYING)
        return;
    b = pAudio->pBackend;
    if (has_backend(pAudio) && b->pfnPause != NULL)
        b->pfnPause(b->pUser, pAudio->handle);
    pAudio->state = BR_AUDIO_PAUSED;
}

void BrAudioResume(BrAudio *pAudio)
{
    const BrAudioBackend *b;
    if (pAudio == NULL || pAudio->state != BR_AUDIO_PAUSED)
        return;
    if (!pAudio->enabled || pAudio->volume == 0)
        return;
    b = pAudio->pBackend;
    if (has_backend(pAudio) && b->pfnResume != NULL)
        b->pfnResume(b->pUser, pAudio->handle);
    pAudio->state = BR_AUDIO_PLAYING;
}

/* DEVIATION: the originals stored the unclamped index and then overwrote it.
 * Nothing could observe the intermediate value, so these compute it directly. */
void BrAudioTrackPrev(BrAudio *pAudio)
{
    if (pAudio == NULL || pAudio->cTracks == 0)
        return;
    BrAudioPlayTrack(pAudio, pAudio->iCurrent - 1);   /* clamps at 0 */
}

void BrAudioTrackNext(BrAudio *pAudio)
{
    if (pAudio == NULL || pAudio->cTracks == 0)
        return;
    BrAudioPlayTrack(pAudio, pAudio->iCurrent + 1);   /* clamps at the last */
}

void BrAudioTrackNextWrap(BrAudio *pAudio)
{
    int iNext;
    if (pAudio == NULL || pAudio->cTracks == 0)
        return;
    iNext = pAudio->iCurrent + 1;
    if (iNext >= pAudio->cTracks || iNext < 0)
        iNext = 0;
    BrAudioPlayTrack(pAudio, iNext);
}

void BrAudioSeedRandom(BrAudio *pAudio, uint32_t seed)
{
    if (pAudio != NULL)
        pAudio->randState = seed;
}

/* The Microsoft CRT's rand(), reproduced so the selection sequence matches the
 * original for a given seed. Returns 0..0x7FFF. */
static int next_rand(BrAudio *pAudio)
{
    pAudio->randState = pAudio->randState * 214013u + 2531011u;
    return (int)((pAudio->randState >> 16) & 0x7FFFu);
}

int BrAudioPickRandomTrack(BrAudio *pAudio)
{
    int lo, hi, span, pick, guard;

    if (pAudio == NULL || pAudio->cTracks == 0)
        return -1;

    random_window(pAudio, &lo, &hi);
    span = hi - lo + 1;

    /* Original: 0x100028D0 re-rolled while the pick equalled the current track,
     * but only when there were more than six tracks -- with fewer, a re-roll
     * could spin. The bound here is a fixed guard instead of a track count, so
     * it degrades to "accept a repeat" rather than looping forever. */
    pick = lo;
    for (guard = 0; guard < 16; guard++) {
        pick = lo + (int)(((long)next_rand(pAudio) * span) >> 15);
        pick = clampi(pick, lo, hi);
        if (span <= 1 || pick != pAudio->iCurrent)
            break;
    }
    return pick;
}

void BrAudioSetRepeat(BrAudio *pAudio, int repeat)
{
    const BrAudioBackend *b;
    if (pAudio == NULL)
        return;
    if (repeat < BR_AUDIO_REPEAT_TRACK || repeat > BR_AUDIO_REPEAT_ONCE)
        return;
    pAudio->repeat = repeat;
    b = pAudio->pBackend;
    if (pAudio->handle != BR_AUDIO_HANDLE_NONE && has_backend(pAudio)
            && b->pfnSetLoop != NULL)
        b->pfnSetLoop(b->pUser, pAudio->handle,
                      repeat == BR_AUDIO_REPEAT_TRACK ? 1 : 0);
}

void BrAudioOnTrackFinished(BrAudio *pAudio)
{
    if (pAudio == NULL)
        return;
    /* A finished notification for a track we already stopped is stale; ignore it
     * rather than resurrecting playback. */
    if (pAudio->state == BR_AUDIO_STOPPED)
        return;

    switch (pAudio->repeat) {
    case BR_AUDIO_REPEAT_TRACK:
        /* Original CD backend: MM_MCINOTIFY re-issued MCI_PLAY of the same
         * track (0x10002510). */
        BrAudioPlayTrack(pAudio, pAudio->iCurrent);
        break;
    case BR_AUDIO_REPEAT_ADVANCE:
        /* Original EAR backend: the completion message called CdTrackNextWrap
         * (0x100029B0). */
        BrAudioTrackNextWrap(pAudio);
        break;
    default:
        release_handle(pAudio);
        break;
    }
}

/* ------------------------------------------------------------- volume / pan */

void BrAudioSetVolume(BrAudio *pAudio, int volume)
{
    const BrAudioBackend *b;
    int wasSilent;

    if (pAudio == NULL)
        return;
    volume = clampi(volume, 0, BR_AUDIO_VOLUME_MAX);
    wasSilent = (pAudio->volume == 0);
    pAudio->volume = volume;

    if (volume == 0) {
        /* Original: zero was "no music", and the callers gated on it before
         * starting playback at all. Stopping rather than muting matches that. */
        release_handle(pAudio);
        return;
    }
    /* Coming back from zero, the track has to be reopened -- it was stopped. */
    if (wasSilent) {
        if (pAudio->started && track_valid(pAudio, pAudio->iCurrent))
            start_current(pAudio);
        return;
    }
    b = pAudio->pBackend;
    if (pAudio->handle != BR_AUDIO_HANDLE_NONE && has_backend(pAudio)
            && b->pfnSetVolume != NULL)
        b->pfnSetVolume(b->pUser, pAudio->handle, volume);
}

/* @n64 0x8025C3B0 located */
int BrAudioGetVolume(const BrAudio *pAudio)
{
    return (pAudio != NULL) ? pAudio->volume : 0;
}

int BrAudioVolumeFromSetting(int setting)
{
    /* Original: the ten-entry music table at 0x100ADF68. The parallel SFX table
     * at 0x100ADF90 is a different curve and is not this function's business. */
    static const int aVolume[10] = {
        0x00, 0x1C, 0x38, 0x55, 0x71, 0x8D, 0xAA, 0xC6, 0xE2, 0xFF
    };
    return aVolume[clampi(setting, 0, 9)];
}

int BrAudioVolumeScale(int volume)
{
    /* Original: 0x10002A20. The mask is faithful and is why 256 is silence --
     * see the note in br_audio.h. The multiply was written as four lea-by-five
     * steps and a shift (x5 x5 x5 x5 x16 = x10000) followed by a reciprocal
     * multiply by 0x80808081 and an arithmetic shift, i.e. a divide by 128. */
    return ((volume & 0xFF) * 10000) / 128;
}

void BrAudioSetPan(BrAudio *pAudio, int pan)
{
    const BrAudioBackend *b;
    if (pAudio == NULL)
        return;
    pAudio->pan = clampi(pan, BR_AUDIO_PAN_LEFT, BR_AUDIO_PAN_RIGHT);
    b = pAudio->pBackend;
    if (pAudio->handle != BR_AUDIO_HANDLE_NONE && has_backend(pAudio)
            && b->pfnSetPan != NULL)
        b->pfnSetPan(b->pUser, pAudio->handle, pAudio->pan);
}

/* @n64 0x80234FDC located */
int BrAudioGetPan(const BrAudio *pAudio)
{
    return (pAudio != NULL) ? pAudio->pan : BR_AUDIO_PAN_CENTRE;
}

/* ----------------------------------------------------------------- querying */

int BrAudioGetCurrentTrack(const BrAudio *pAudio)
{
    if (pAudio == NULL)
        return -1;
    return track_valid(pAudio, pAudio->iCurrent) ? pAudio->iCurrent : -1;
}

int BrAudioGetState(const BrAudio *pAudio)
{
    const BrAudioBackend *b;
    if (pAudio == NULL)
        return BR_AUDIO_STOPPED;
    /* Prefer the backend's own view when it has one: a stream can end on its own
     * without anyone having called us. */
    b = pAudio->pBackend;
    if (pAudio->handle != BR_AUDIO_HANDLE_NONE && has_backend(pAudio)
            && b->pfnGetState != NULL)
        return b->pfnGetState(b->pUser, pAudio->handle);
    return pAudio->state;
}

int BrAudioIsPlaying(const BrAudio *pAudio)
{
    return BrAudioGetState(pAudio) == BR_AUDIO_PLAYING;
}

int BrAudioGetPositionMs(const BrAudio *pAudio, uint32_t *pMs)
{
    const BrAudioBackend *b;
    if (pMs == NULL)
        return 0;
    *pMs = 0;
    if (pAudio == NULL || pAudio->handle == BR_AUDIO_HANDLE_NONE)
        return 0;
    b = pAudio->pBackend;
    if (!has_backend(pAudio) || b->pfnGetPositionMs == NULL)
        return 0;
    return b->pfnGetPositionMs(b->pUser, pAudio->handle, pMs) ? 1 : 0;
}

/* ------------------------------------------------------------ FLAC probing */

int BrAudioProbeFlac(const void *pvData, size_t cb, BrAudioFormat *pFmt)
{
    const unsigned char *p = (const unsigned char *)pvData;
    const unsigned char *si;
    uint32_t blockLen;

    if (pFmt == NULL)
        return 1;
    memset(pFmt, 0, sizeof(*pFmt));
    if (p == NULL || cb < 4 + 4 + 34)
        return 1;

    if (p[0] != 'f' || p[1] != 'L' || p[2] != 'a' || p[3] != 'C')
        return 1;

    /* Metadata block header: bit 7 of byte 0 is "last block", bits 0..6 are the
     * type; STREAMINFO is type 0 and must be first. Then a 24-bit big-endian
     * length, which for STREAMINFO is always 34. */
    if ((p[4] & 0x7F) != 0)
        return 1;
    blockLen = ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 8) | (uint32_t)p[7];
    if (blockLen != 34)
        return 1;

    si = p + 8;
    /* STREAMINFO is bit-packed big-endian. Bytes 0..9 are the block and frame
     * size limits, which we do not need. From byte 10:
     *     20 bits sample rate
     *      3 bits channels - 1
     *      5 bits bits per sample - 1
     *     36 bits total frames (0 = unknown)
     * That is exactly 64 bits, bytes 10..17, unpacked here by hand so that no
     * part of this depends on host endianness or alignment. */
    pFmt->sampleRate = ((uint32_t)si[10] << 12)
                     | ((uint32_t)si[11] << 4)
                     | ((uint32_t)si[12] >> 4);
    pFmt->channels      = (int)((si[12] >> 1) & 0x07) + 1;
    pFmt->bitsPerSample = (int)((((uint32_t)si[12] & 0x01) << 4)
                                | ((uint32_t)si[13] >> 4)) + 1;
    pFmt->totalFrames = ((uint64_t)(si[13] & 0x0F) << 32)
                      | ((uint64_t)si[14] << 24)
                      | ((uint64_t)si[15] << 16)
                      | ((uint64_t)si[16] << 8)
                      | (uint64_t)si[17];

    if (pFmt->sampleRate == 0)
        return 1;
    return 0;
}

uint32_t BrAudioFormatDurationMs(const BrAudioFormat *pFmt)
{
    if (pFmt == NULL || pFmt->sampleRate == 0 || pFmt->totalFrames == 0)
        return 0;
    return (uint32_t)((pFmt->totalFrames * 1000u) / pFmt->sampleRate);
}
