/* br_sfx.h -- the sound-effect BANK: which sample belongs to which voice slot,
 * what its file is called, and the fixed-point pitch conversion the mixer uses.
 *
 * Reference is BRGlide.dll.  Every function modelled here is classified
 * `shared` in config/shared.csv, so the D3D addresses are given alongside and
 * either binary answers the same question.
 *
 * WHAT THE SOUND ENGINE ACTUALLY IS
 * =================================
 * Not DirectSound-by-assumption -- DirectSound by evidence, and reached in a
 * way that hides it from the import table:
 *
 *   - Neither DLL imports dsound.dll.  The audio-shaped imports are WINMM
 *     (mmioOpenA/Descend/Read/Ascend/Close -- the RIFF chunk reader --
 *     plus mciSendCommandA for Redbook and timeGetTime) and MSACM32
 *     (acmMetrics only).
 *   - The device is created through COM: 0x1006C4D0 (D3D 0x10073560) calls
 *     ole32!CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_INPROC_SERVER,
 *     IID_IDirectSound, &g_pDS), then IDirectSound::Initialize(NULL),
 *     SetCooperativeLevel(hwnd, DSSCL_PRIORITY) and CreateSoundBuffer of a
 *     DSBCAPS_PRIMARYBUFFER, which it leaves Play()ing with DSBPLAY_LOOPING.
 *     The two GUIDs sit at 0x10078A18 / 0x10078A38 in BRGlide.
 *   - The mixer format it asks for is 22050 Hz, 16-bit, stereo
 *     (WAVEFORMATEX built at 0x1006C554: wFormatTag=1, nChannels=2,
 *     nSamplesPerSec=0x5622, nAvgBytesPerSec=0x15888, nBlockAlign=4,
 *     wBitsPerSample=16).
 *   - EAR ("EAR Interactive Around-Sound", earpds.dll / earias.dll) is
 *     LoadLibrary'd at 0x1001794E and is the MUSIC backend only (see
 *     br_audio.h).  No SFX path touches it.
 *
 * The per-voice wrapper over IDirectSoundBuffer is ALREADY PORTED, in
 * port/src/slice1_08.c under BrSnd* names (0x100722D0..0x10073060 in D3D =
 * 0x1006B240..0x1006BFD0 in Glide).  Do not coin a third set of names for
 * those addresses.  This header is the layer ABOVE it: the table that says
 * which sample goes in which slot, and the pitch arithmetic.
 *
 * THE TWO PARALLEL 26 x 18 TABLES
 * ===============================
 * Glide 0x100B55F8 (voices) and 0x100B5D48 (bank), D3D 0x100B5DF0 and
 * 0x100B6540.  Each is 1872 bytes = 26 rows of 72, and the two abut exactly:
 * 0x100B55F8 + 1872 == 0x100B5D48, and 0x100B5D48 + 1872 == 0x100B6498, which
 * is the string "r.wav".  A row is
 *
 *     struct { void *aSlot[16]; double baseRate; }     72 bytes
 *
 * -- 16 dwords then an 8-byte double, which is why the code indexes voices as
 * `row*18 + slot` (18 dwords) but only ever touches slots 0..14, and why
 * 0x1006B530 reads the row's rate from `0x100B5638 + 72*row`, i.e. dword 16.
 *
 * The two tables are BYTE-IDENTICAL in .data in both builds, and the
 * initialiser is a per-slot presence flag plus the rate: exactly one slot per
 * generic group is marked (slot 1 for most, slot 3 for beep/beep2/water) and
 * the three engine groups (0, 24, 25) are marked by the game at runtime
 * instead.  0x1006C290's loader skips any slot whose bank entry is zero, so
 * the flag is what decides how many DirectSound buffers a sample gets -- and
 * it is why the disc needs only 73 .wav files.
 *
 *   group 0   "sfx/<cc>.wav"      per-car engine, slot 2*car
 *   group 24  "sfx/<cc>h.wav"     per-car engine, high layer
 *   group 25  "sfx/<cc>r.wav"     per-car engine, rev layer
 *   groups 1..23   one shared sample each, named by BrSfxGroupName()
 *
 * `<cc>` is a two-letter car code out of the table at 0x100B7CFC, whose slot 0
 * is NULL -- so it holds 16 CODES in 17 slots. An earlier note here read that
 * as 17 codes and raised "the disc has 17 .rca but only 16 sound codes; which
 * car is silent?" as an open question. There is no such question: the disc
 * carries exactly 16 .rca files, counted disc-wide, and every one has a code.
 * The NULL slot 0 is the off-by-one, not a silent car.
 * whose slot 0 is NULL, so 0x1006BFF0 stores `car + 1` and 0 means "no car".
 * 16 codes x 3 suffixes = 48 files; the 25 distinct generic names bring the
 * total to exactly the 73 .wav files in SFX/ on the disc.
 *
 * SLOT == CHANNEL
 * ===============
 * 0x1006B530(group, ch) installs `voices[group].aSlot[ch]` as channel `ch`'s
 * current voice and copies that group's baseRate into the per-channel rate
 * array, so the slot index and the channel index are one number.  There are
 * 15 channels (0x1006C460 and 0x1006BFD0 both clear exactly 15; 0x1006BD70
 * walks 0x1184C268..0x1184C2A4, which is 15 dwords).  Car i owns channel
 * 2*i -- every per-car entry point doubles its argument (0x1006BA80,
 * 0x1006BAF0, 0x1006BB10, 0x1006B6C0, 0x1006C010) -- and the one-shot entry
 * point 0x1006BA60 hardcodes channel 1.
 *
 * POSITIONING
 * ===========
 * There is no DirectSound3D.  IID_IDirectSound3DBuffer and
 * IID_IDirectSound3DListener are present in .rdata and referenced by nothing,
 * and the buffer-creation flags (0x000100E2 / 0x000140E2, 0x1006B240) do not
 * include DSBCAPS_CTRL3D.  Panning is computed in software by BrSndPan
 * (0x10060C30 Glide / 0x10067BC0 D3D, already ported in slice3_41.c), which
 * yields two gains and an integer distance volume; the caller packs them as
 *
 *     packed = ((int)(gainA * vol) << 16) | (int)(gainB * vol)
 *
 * and BrSndVoiceSetLevels (slice1_08.c) turns that pair into DirectSound pan
 * and volume.  slice3_41.h records that it could not establish which of
 * BrSndPan's two gains is physically left; this pass settles it -- see
 * BR_SFX_LEVEL_MAX below.
 *
 * WHAT THIS MODULE DOES NOT DO
 * ============================
 * No playback, no device, no file I/O.  It is the table, the filename rule
 * and the pitch arithmetic, all of which are exact and testable with no audio
 * hardware and no disc.
 */
#ifndef BR_SFX_H
#define BR_SFX_H

#include <stddef.h>
#include <stdint.h>

/* Geometry, all pinned by the arithmetic above. */
#define BR_SFX_GROUPS      26   /* rows in both tables                       */
#define BR_SFX_SLOTS       16   /* voice pointers per row                    */
#define BR_SFX_ROW_DWORDS  18   /* row stride in dwords (16 + a double)      */
#define BR_SFX_CHANNELS    15   /* slots the engine ever touches             */
#define BR_SFX_CARS        16   /* two-letter car codes                      */

/* The three per-car engine groups and their filename suffixes. */
#define BR_SFX_GROUP_ENGINE      0    /* "<cc>.wav"   */
#define BR_SFX_GROUP_ENGINE_HIGH 24   /* "<cc>h.wav"  */
#define BR_SFX_GROUP_ENGINE_REV  25   /* "<cc>r.wav"  */

/* The channel a one-shot is played on: 0x1006BA60 (D3D 0x10072AF0) passes a
 * literal 1 for the slot, which is why the bank marks slot 1 for almost every
 * generic group. */
#define BR_SFX_CHANNEL_ONESHOT   1

/* 0x1006B790 (D3D 0x10072820) clamps each half of the packed level pair to
 * this before deriving pan and volume.  The HIGH half is LEFT: when it is the
 * larger of the two, the derived f10 falls below the 400 centre and
 * IDirectSoundBuffer::SetPan is called with a negative value, which is
 * DSBPAN_LEFT.  0x10061470 (D3D 0x10068400) builds the pair as
 * `(gainA*vol) << 16 | (gainB*vol)`, so BrSndPan's pGainA is LEFT and pGainB
 * is RIGHT -- the question slice3_41.h left open. */
#define BR_SFX_LEVEL_MAX   32

/* Which name table 0x1006C290 (D3D 0x10073320) reads.  The values are the
 * argument the original passes. */
#define BR_SFX_SET_MENU    0    /* 0x100B81A8, 9 groups, rows 1..7  */
#define BR_SFX_SET_RACE    1    /* 0x100B8140, 25 groups, rows 1..23 */

/* The default directory prefix.  It is a 0x40-byte mutable buffer at
 * 0x100B7D40 that BossRally.ini's `SFXDir=` overwrites (0x10008286), so it is
 * not a constant in the original -- but the shipped default is this, and note
 * that the value carries its own trailing separator because nothing appends
 * one. */
#define BR_SFX_DIR_DEFAULT "sfx/"

/* One row of either table.  This is a MODEL of the original 72-byte row, not
 * a copy of it: `aSlot` here is the presence flag from .data rather than the
 * runtime voice pointer, because a pointer widens on this host and the row
 * would stop being 72 bytes.  Index by member, never by byte. */
typedef struct BrSfxGroupDef {
    uint8_t aSlot[BR_SFX_SLOTS];  /* non-zero -> this slot gets a voice */
    double  baseRate;             /* dword 16..17 of the row, in Hz      */
} BrSfxGroupDef;

/* The shipped initialiser of both tables, read out of .data.  Identical in
 * BRGlide.dll and BRD3D.dll. */
extern const BrSfxGroupDef BrSfxGroups[BR_SFX_GROUPS];

/* 0x100B7CFC.  BrSfxCarCode[0] is NULL on purpose: the bank stores car+1 so
 * that zero can mean "this slot has no car". */
extern const char *const BrSfxCarCode[BR_SFX_CARS + 1];

/* ------------------------------------------------------------------ table */

/* Number of groups the loader walks for a set: 25 for the race set, 9 for the
 * menu set (0x1006C290 stores these into 0x1184C260).  Returns 0 for an
 * unknown set.  NOTE this is not BR_SFX_GROUPS: the loop is
 * `for (row = 1; row < count - 1; row++)`, so the race set covers rows 1..23
 * and the three engine rows are loaded separately by 0x1006C010. */
int BrSfxGroupCount(int set);

/* The sample file name for a generic group, or NULL when the set does not
 * name that group.  Groups 0, 24 and 25 always return NULL -- they are
 * per-car and go through BrSfxCarFileName instead.
 *
 * The two sets OVERLAP in group number: the menu set puts front-end5.wav in
 * group 1, where the race set puts hit-another-car1.wav. */
const char *BrSfxGroupName(int set, int group);

/* The base playback rate of a group, in Hz, or 0.0 for an out-of-range group.
 * This is the rate at which a pitch ratio of 1.0 plays; it is a property of
 * the GROUP, not of the file, and the two do not always agree. */
double BrSfxGroupBaseRate(int group);

/* Non-zero if the shipped bank marks (group, slot) for instantiation. */
int BrSfxGroupSlotUsed(int group, int slot);

/* ------------------------------------------------------------- addressing */

/* The flat voice-table index the original computes: group*18 + slot.
 * Returns -1 when either is out of range (the original bounds-checks
 * neither -- this is a DEVIATION, and the same one slice1_08.c already
 * makes). */
int BrSfxVoiceIndex(int group, int slot);

/* The channel a car's engine voices live on: 2*iCar.  Returns -1 when the
 * result would fall outside the 15 channels. */
int BrSfxCarChannel(int iCar);

/* ------------------------------------------------------------- filenames */

/* Build the file name for a per-car engine sample.
 *
 *   group    BR_SFX_GROUP_ENGINE / _HIGH / _REV
 *   iName    the bank value, which is car index + 1; 0 means "no car"
 *
 * Writes "<prefix><cc><suffix>", where prefix defaults to BR_SFX_DIR_DEFAULT
 * when pszPrefix is NULL.  Returns the length written, or -1 if anything is
 * out of range or the buffer is too small (in which case *pszDst is left
 * empty).  This is 0x1006C010's three strcpy/strcat chains. */
int BrSfxCarFileName(int group, int iName, const char *pszPrefix,
                     char *pszDst, size_t cbDst);

/* Build the file name for a generic group: "<prefix><name>".  0x1006C290
 * appends no extension -- the names in the table already carry ".wav".
 * Same return convention as BrSfxCarFileName. */
int BrSfxGroupFileName(int set, int group, const char *pszPrefix,
                       char *pszDst, size_t cbDst);

/* ----------------------------------------------------------------- pitch */

/* The mixer carries pitch as a 32.32 fixed-point ratio against the channel's
 * base rate: 0x100000000 is "play at baseRate".  The two constants are read
 * out of the image at 0x10077C08 (4294967296.0 = 2^32) and 0x10077C00
 * (2.3283064365386963e-10 = 2^-32), both exact.
 *
 * BrSfxRatioFromHz is 0x1006B880's tail:
 *      ratio = (int64)( (double)hz * 2^32 / baseRate )
 * BrSfxHzFromRatio is 0x1006B5F0's head, and the multiply order is preserved
 * because it is not associative in floating point:
 *      hz    = (uint32)( (double)ratio * baseRate * 2^-32 )
 *
 * Both truncate through MSVC's _ftol (0x10074560 is a jump thunk to
 * MSVCRT!_ftol), i.e. toward zero, storing the x87 "integer indefinite"
 * 0x8000000000000000 when the value will not fit -- of which BrSfxHzFromRatio
 * keeps only the low dword, so an overflow there reads as 0.  See
 * CONVENTIONS.md's note on 0x1007C8A0.
 *
 * The pair is NOT an exact inverse: both directions truncate toward zero, so
 * a frequency written as a ratio and read back is hz or hz-1, never hz+1.
 * 0x1006BDD0 re-derives every channel's frequency from its stored ratio each
 * frame, but it compares the ratio -- not the frequency -- before reapplying
 * it, so the loss happens once and does not accumulate.
 *
 * GOTCHA, preserved: neither guards baseRate against zero.  A channel that
 * has never been bound has a rate of 0, and the divide then produces an
 * infinity that _ftol turns into the indefinite value. */
int64_t  BrSfxRatioFromHz(uint32_t hz, double baseRate);
uint32_t BrSfxHzFromRatio(int64_t ratio, double baseRate);

/* 0x1006B6C0 (D3D 0x10072750) sets an absolute frequency from a float: the
 * engine's high layer is driven this way rather than through the ratio.  The
 * float is truncated by the same _ftol, so out-of-range yields 0. */
uint32_t BrSfxHzFromFloat(float hz);

/* ------------------------------------------------------- the engine curve */

/* The three per-car loops are retuned every frame from the car's RPM, in
 * 0x10061470 (D3D 0x10068400) -- the same function that packs BrSndPan's
 * output into the level pair BR_SFX_LEVEL_MAX describes.  The two layers are
 * driven DIFFERENTLY, and that asymmetry is the interesting part:
 *
 *   low / rev  (groups 0 and 25)   an absolute frequency in hertz, turned
 *                                  into a 32.32 ratio and re-applied each
 *                                  frame by 0x1006BDD0
 *   high       (group 24)          the doppler ratio times 22050, straight
 *                                  through BrSfxHzFromFloat -- no RPM at all
 *
 * The RPM input is the float at car+0xE24.  It is RPM and not speed: the
 * gearbox code writes 800.0f, 900.0f and 8000.0f into that field (0x1006278F,
 * 0x10062AE1, 0x10062AFE), which are idle, fast idle and redline.
 *
 * Addresses: 0x1006156A..0x100615C0 is BrSfxEngineHz, 0x10061601..0x1006162E
 * is BrSfxEngineRatio, 0x10061995..0x100619AE is BrSfxEngineHighHz. */

/* rpm -> hertz for the low and rev layers.
 *
 *      v = (rpm > 0 ? rpm*0.5f : rpm*-0.5f) * 15.714285850524902f * doppler
 *      if (v > 100000 || !(v >= 0))  v = 0
 *
 * 15.714285850524902f is the float nearest 110/7, so with the 0.5 the scale
 * is 55/7 Hz per RPM and unity pitch (11000 Hz, see BrSfxEngineRatio) lands
 * at 1400 RPM.  The magnitude fold means a negative RPM sounds like its
 * positive twin rather than silencing the engine.
 *
 * NaN takes the zero exit at BOTH tests, because an x87 unordered compare
 * sets C0 exactly as "less than" does -- the same trap br_ftol64 documents.
 *
 * DEVIATION: the original keeps this whole chain in one 80-bit x87 register
 * and never rounds to float, so the intermediates are computed in double
 * here rather than float.  Rounding each step to float would be the larger
 * deviation, not the smaller one. */
double BrSfxEngineHz(float rpm, float doppler);

/* hertz -> the 32.32 ratio the low/rev channel stores.
 *
 * GOTCHA, and it is a real one: this does NOT go through BrSfxRatioFromHz.
 * The original multiplies by the FLOAT reciprocal of 11000 and then by 2^32
 * (0x100779E4 and 0x100779E8) rather than dividing by the channel's base
 * rate -- and the channel's base rate is 11025, because that is what row 0
 * and row 25 of the bank carry.  0x1006B5F0 then converts the ratio back
 * using 11025.  The engine therefore plays 11025/11000 sharp, about a
 * quarter of a percent, and that is shipped behaviour rather than a
 * transcription slip: the two constants are in the image and they disagree.
 *
 * Truncation is br_ftol64's, so an out-of-range value yields the x87 integer
 * indefinite exactly as BrSfxRatioFromHz does. */
int64_t BrSfxEngineRatio(double hz);

/* The high layer's absolute frequency: doppler * 22050 (0x10077A00), through
 * BrSfxHzFromFloat.  Group 24's base rate is also 22050, so a doppler of 1
 * is unity pitch -- the high layer is pure doppler and ignores RPM. */
uint32_t BrSfxEngineHighHz(float doppler);

/* The base rate the low/rev ratio is DERIVED against, as against the 11025
 * the bank rows carry and BrSfxHzFromRatio reads back with. */
#define BR_SFX_ENGINE_RATIO_RATE  11000.0

#endif /* BR_SFX_H */
