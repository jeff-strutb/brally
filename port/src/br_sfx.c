/* br_sfx.c -- the sound-effect bank table, filename rule and pitch
 * arithmetic.  See br_sfx.h for the evidence behind every number here.
 *
 * Reference: BRGlide.dll.  D3D addresses in parentheses.
 *
 *   0x1006C010 (0x100730A0)  per-car engine loader -- the three suffixes
 *   0x1006C290 (0x10073320)  bank loader -- the two name tables
 *   0x1006BFD0 (0x10073060)  bank reset  (already ported: BrSndBankReset)
 *   0x1006BFF0 (0x10073080)  bank set-car
 *   0x1006B530 (0x100725C0)  bind group's slot to a channel, copy base rate
 *   0x1006B5F0 (0x10072680)  32.32 ratio -> SetFrequency
 *   0x1006B880 (0x10072910)  start + Hz -> 32.32 ratio
 *   0x1006B6C0 (0x10072750)  float Hz -> SetFrequency
 *
 * Nothing in this file opens a file, touches a device or calls into
 * slice1_08's DirectSound wrapper.  That is deliberate: the platform layer is
 * a separate decision and this pass does not make it.
 */
#include "br_sfx.h"

#include <string.h>

/* ------------------------------------------------------------------------
 * The shipped table, transcribed from .data.
 *
 * Glide 0x100B55F8 and 0x100B5D48; D3D 0x100B5DF0 and 0x100B6540.  All four
 * blocks are byte-identical, which is the second, independent reading that
 * CONVENTIONS.md asks for.
 *
 * Only 115 of the 1872 bytes of each block are non-zero, and they are the 26
 * doubles plus one flag byte per generic row.
 * ---------------------------------------------------------------------- */

#define S1 { 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* marks slot 1 */
#define S3 { 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* marks slot 3 */
#define S0 { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }   /* nothing marked */

const BrSfxGroupDef BrSfxGroups[BR_SFX_GROUPS] = {
    { S0, 11025.0 },   /*  0  engine  "<cc>.wav"                    */
    { S1, 22050.0 },   /*  1  hit-another-car1 | menu front-end5    */
    { S1, 22050.0 },   /*  2  big-impact1      | menu DontQuit      */
    { S1, 22050.0 },   /*  3  bottom-out       | menu Quit          */
    { S1, 11000.0 },   /*  4  s_dirtz          | menu taunt1        */
    { S1, 11000.0 },   /*  5  s_dirt           | menu taunt2        */
    { S1, 11000.0 },   /*  6  s_snow           | menu taunt3        */
    { S1, 11000.0 },   /*  7  s_tarmc          | menu taunt4        */
    { S1, 11000.0 },   /*  8  rn_dirt                               */
    { S1, 30000.0 },   /*  9  rn_dirt   (the table repeats itself)  */
    { S1, 30000.0 },   /* 10  rn_tarm                               */
    { S1, 11000.0 },   /* 11  rn_snow                               */
    { S1, 11000.0 },   /* 12  rn_watr                               */
    { S3, 11000.0 },   /* 13  beep                                  */
    { S3, 11000.0 },   /* 14  beep2                                 */
    { S3, 11000.0 },   /* 15  water                                 */
    { S1, 22050.0 },   /* 16  hit-another-car2                      */
    { S1, 22050.0 },   /* 17  hit-another-car3                      */
    { S1, 22050.0 },   /* 18  big-impact2                           */
    { S1, 22050.0 },   /* 19  big-impact3                           */
    { S1, 22050.0 },   /* 20  taunt1                                */
    { S1, 22050.0 },   /* 21  taunt2                                */
    { S1, 22050.0 },   /* 22  taunt3                                */
    { S1, 22050.0 },   /* 23  taunt4                                */
    { S0, 22050.0 },   /* 24  engine  "<cc>h.wav"                   */
    { S0, 11025.0 }    /* 25  engine  "<cc>r.wav"                   */
};

#undef S1
#undef S3
#undef S0

/* 0x100B7CFC.  Seventeen pointers, the first NULL.  The disc carries three
 * .wav files for each of the sixteen codes. */
const char *const BrSfxCarCode[BR_SFX_CARS + 1] = {
    NULL, "ce", "es", "ns", "rs", "sp", "ps", "m3", "ip",
    "ld", "hm", "mt", "cu", "bb", "pj", "tr", "mn"
};

/* 0x100B8140, 26 entries -- the array is exactly BR_SFX_GROUPS long and the
 * menu table starts in the very next dword.  Rows 24..25 are NULL because the
 * engine groups are named per car.
 *
 * Entry 9 really is a second "rn_dirt.wav"; there is no rn_grass or
 * equivalent on the disc, so the duplicate is the shipped behaviour and not a
 * transcription slip. */
static const char *const s_aRaceName[BR_SFX_GROUPS] = {
    NULL,
    "hit-another-car1.wav", "big-impact1.wav", "bottom-out.wav",
    "s_dirtz.wav", "s_dirt.wav", "s_snow.wav", "s_tarmc.wav",
    "rn_dirt.wav", "rn_dirt.wav", "rn_tarm.wav", "rn_snow.wav",
    "rn_watr.wav", "beep.wav", "beep2.wav", "water.wav",
    "hit-another-car2.wav", "hit-another-car3.wav",
    "big-impact2.wav", "big-impact3.wav",
    "taunt1.wav", "taunt2.wav", "taunt3.wav", "taunt4.wav",
    NULL, NULL
};

/* 0x100B81A8, 9 entries, immediately after the race table.  0x100B81D0 --
 * the very next dword -- is the CD track-name table br_audio.h describes, so
 * this one's extent is pinned on both sides. */
#define BR_SFX_MENU_GROUPS 9
static const char *const s_aMenuName[BR_SFX_MENU_GROUPS] = {
    NULL,
    "front-end5.wav", "DontQuit.wav", "Quit.wav",
    "taunt1.wav", "taunt2.wav", "taunt3.wav", "taunt4.wav",
    NULL
};

/* group -> filename suffix, for the three per-car groups. */
static const char *engine_suffix(int group)
{
    switch (group) {
    case BR_SFX_GROUP_ENGINE:      return ".wav";    /* 0x100B64A8 */
    case BR_SFX_GROUP_ENGINE_HIGH: return "h.wav";   /* 0x100B64A0 */
    case BR_SFX_GROUP_ENGINE_REV:  return "r.wav";   /* 0x100B6498 */
    default:                       return NULL;
    }
}

/* ------------------------------------------------------------------ table */

int BrSfxGroupCount(int set)
{
    /* 0x1006C290 stores 0x19 for the race set and 9 for the menu set into
     * 0x1184C260, then loops `for (row = 1; row < count - 1; row++)`. */
    if (set == BR_SFX_SET_RACE)
        return 25;
    if (set == BR_SFX_SET_MENU)
        return BR_SFX_MENU_GROUPS;
    return 0;
}

const char *BrSfxGroupName(int set, int group)
{
    if (group < 0)
        return NULL;
    if (set == BR_SFX_SET_RACE)
        return (group < BR_SFX_GROUPS) ? s_aRaceName[group] : NULL;
    if (set == BR_SFX_SET_MENU)
        return (group < BR_SFX_MENU_GROUPS) ? s_aMenuName[group] : NULL;
    return NULL;
}

double BrSfxGroupBaseRate(int group)
{
    if (group < 0 || group >= BR_SFX_GROUPS)
        return 0.0;
    return BrSfxGroups[group].baseRate;
}

int BrSfxGroupSlotUsed(int group, int slot)
{
    if (group < 0 || group >= BR_SFX_GROUPS)
        return 0;
    if (slot < 0 || slot >= BR_SFX_SLOTS)
        return 0;
    return BrSfxGroups[group].aSlot[slot] != 0;
}

/* ------------------------------------------------------------- addressing */

int BrSfxVoiceIndex(int group, int slot)
{
    /* The original is `lea eax,[grp+grp*8]; lea idx,[slot+eax*2]` -- 18*group
     * + slot, with no bounds check anywhere.  DEVIATION: bounds-checked here,
     * matching what slice1_08.c already does at its own entry points. */
    if (group < 0 || group >= BR_SFX_GROUPS)
        return -1;
    if (slot < 0 || slot >= BR_SFX_SLOTS)
        return -1;
    return group * BR_SFX_ROW_DWORDS + slot;
}

int BrSfxCarChannel(int iCar)
{
    int ch;
    if (iCar < 0)
        return -1;
    ch = iCar * 2;
    if (ch >= BR_SFX_CHANNELS)
        return -1;
    return ch;
}

/* ------------------------------------------------------------- filenames */

/* strcpy/strcat with a hard bound.  Returns the total length, or -1 if any
 * piece did not fit.  The original builds these in a 0x400-byte stack buffer
 * with no bound at all; the buffer is large enough for every shipped name, so
 * the check is a DEVIATION that cannot change behaviour on real data. */
static int join3(char *pszDst, size_t cbDst,
                 const char *a, const char *b, const char *c)
{
    size_t la, lb, lc;

    if (pszDst == NULL || cbDst == 0)
        return -1;
    pszDst[0] = '\0';

    la = strlen(a);
    lb = strlen(b);
    lc = strlen(c);
    if (la + lb + lc + 1 > cbDst)
        return -1;

    memcpy(pszDst, a, la);
    memcpy(pszDst + la, b, lb);
    memcpy(pszDst + la + lb, c, lc + 1);
    return (int)(la + lb + lc);
}

int BrSfxCarFileName(int group, int iName, const char *pszPrefix,
                     char *pszDst, size_t cbDst)
{
    const char *pszSuffix = engine_suffix(group);

    if (pszDst != NULL && cbDst != 0)
        pszDst[0] = '\0';
    if (pszSuffix == NULL)
        return -1;
    /* 0x1006C010 tests the bank entry against zero and bails to "no sound"
     * before it builds anything; index 0 of the code table is NULL, so a zero
     * would dereference it. */
    if (iName <= 0 || iName > BR_SFX_CARS)
        return -1;
    if (pszPrefix == NULL)
        pszPrefix = BR_SFX_DIR_DEFAULT;

    return join3(pszDst, cbDst, pszPrefix, BrSfxCarCode[iName], pszSuffix);
}

int BrSfxGroupFileName(int set, int group, const char *pszPrefix,
                       char *pszDst, size_t cbDst)
{
    const char *pszName = BrSfxGroupName(set, group);

    if (pszDst != NULL && cbDst != 0)
        pszDst[0] = '\0';
    if (pszName == NULL)
        return -1;
    if (pszPrefix == NULL)
        pszPrefix = BR_SFX_DIR_DEFAULT;

    /* No extension is appended: 0x1006C290 concatenates the prefix and the
     * table entry and stops, and every table entry already ends in ".wav". */
    return join3(pszDst, cbDst, pszPrefix, pszName, "");
}

/* ----------------------------------------------------------------- pitch */

/* MSVC's _ftol, which 0x10074560 is a jump thunk to.  It sets the x87 round
 * control to chop and does a 64-bit fistp, so it truncates toward zero and
 * stores the "integer indefinite" 0x8000000000000000 when the value does not
 * fit -- which includes NaN.
 *
 * The range test is written NEGATED on purpose: an x87 unordered compare sets
 * C0/C3 exactly as "less than" does, so NaN must take the indefinite side.
 * See CONVENTIONS.md. */
static int64_t br_ftol64(double v)
{
    /* -2^63 is representable exactly and is in range; +2^63 is not. */
    if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0))
        return (int64_t)((uint64_t)1u << 63);
    return (int64_t)v;
}

/* The 32-bit flavour: the same fistp, of which only the low dword is kept.
 * The low dword of the indefinite value is zero, which is why an overflow
 * here reads as 0 rather than as 0x80000000. */
static uint32_t br_ftol32(double v)
{
    return (uint32_t)((uint64_t)br_ftol64(v) & 0xFFFFFFFFu);
}

int64_t BrSfxRatioFromHz(uint32_t hz, double baseRate)
{
    /* 0x1006B880: the frequency is stored into a 64-bit stack slot with the
     * high dword zeroed and loaded with `fild qword`, so it is
     * zero-extended, then multiplied by 0x10077C08 (2^32) and divided by the
     * channel's base rate.  No guard on baseRate. */
    return br_ftol64((double)(uint64_t)hz * 4294967296.0 / baseRate);
}

uint32_t BrSfxHzFromRatio(int64_t ratio, double baseRate)
{
    /* 0x1006B5F0: `fild qword` of the signed 64-bit ratio, `fmul` the base
     * rate, then `fmul` 0x10077C00 (2^-32).  The order is preserved because
     * floating-point multiplication is not associative. */
    return br_ftol32((double)ratio * baseRate * 2.3283064365386963e-10);
}

uint32_t BrSfxHzFromFloat(float hz)
{
    /* 0x1006B6C0: `fld dword` then straight into _ftol. */
    return br_ftol32((double)hz);
}
