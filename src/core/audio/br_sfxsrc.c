/* br_sfxsrc.c -- the sound-source layer.  See br_sfxsrc.h for the chain, the
 * stack traces and the evidence for what each argument is.
 *
 * Transcribed from orig/BRGlide.dll.  Every branch carries the address of the
 * instruction it is.
 */
#include <string.h>

#include "br_sfxsrc.h"
#include "slice1_08.h"

/* ==========================================================================
 * The data, read out of BRGlide.dll's .data rather than assumed
 * ==========================================================================
 *
 * 0x100B32B0, 25 records of 24 bytes.  Records 16..24 are entirely zero in
 * the image and are reproduced as such; the loop at 0x10061362 still writes
 * their `group`, so they are 25 real records and not 16 followed by padding.
 */
static const BrSfxSrcDef s_aBrSfxSrcImage[BR_SFXSRC_COUNT] = {
    /*  0 */ { 0, 0x100B52B0u, 0x100B52B4u, 0, -1, -1 },
    /*  1 */ { 0, 0x100B52B8u, 0x100B52BCu, 0,  0, 0x200 },
    /*  2 */ { 0, 0x100B52C0u, 0x100B52C4u, 0,  0, 0x200 },
    /*  3 */ { 0, 0x100B52C8u, 0x100B52CCu, 0,  0, 0x200 },
    /*  4 */ { 0, 0x100B52D0u, 0x100B52D4u, 0, -1, 0x200 },
    /*  5 */ { 0, 0x100B52D8u, 0x100B52DCu, 0, -1, 0x200 },
    /*  6 */ { 0, 0x100B52E0u, 0x100B52E4u, 0, -1, 0x200 },
    /*  7 */ { 0, 0x100B52E8u, 0x100B52ECu, 0, -1, 0x200 },
    /*  8 */ { 0, 0x100B52F0u, 0x100B52F4u, 0, -1, 0x200 },
    /*  9 */ { 0, 0x100B52F8u, 0x100B52FCu, 0, -1, 0x200 },
    /* 10 */ { 0, 0x100B5300u, 0x100B5304u, 0, -1, 0x200 },
    /* 11 */ { 0, 0x100B5308u, 0x100B530Cu, 0, -1, 0x200 },
    /* 12 */ { 0, 0x100B5310u, 0x100B5314u, 0, -1, 0x200 },
    /* 13 */ { 0, 0x100B5318u, 0x100B531Cu, 0,  0, 0x100 },   /* beep   */
    /* 14 */ { 0, 0x100B5320u, 0x100B5324u, 0,  0, 0x100 },   /* beep2  */
    /* 15 */ { 0, 0x100B5328u, 0x100B532Cu, 0, -1, 0x200 },   /* water  */
    /* 16 */ { 0, 0, 0, 0, 0, 0 },
    /* 17 */ { 0, 0, 0, 0, 0, 0 },
    /* 18 */ { 0, 0, 0, 0, 0, 0 },
    /* 19 */ { 0, 0, 0, 0, 0, 0 },
    /* 20 */ { 0, 0, 0, 0, 0, 0 },
    /* 21 */ { 0, 0, 0, 0, 0, 0 },
    /* 22 */ { 0, 0, 0, 0, 0, 0 },
    /* 23 */ { 0, 0, 0, 0, 0, 0 },
    /* 24 */ { 0, 0, 0, 0, 0, 0 }
};

BrSfxSrcDef g_aBrSfxSrc[BR_SFXSRC_COUNT];

BrSfxChan   g_aBrSfxChan[BR_SFX_CHANNELS];        /* 0x118EEF40, stride 24 */
BrSfxChan   g_aBrSfxChanApplied[BR_SFX_CHANNELS]; /* 0x1184C080, stride 24 */
BrSndVoice *g_apBrSfxChanVoice[BR_SFX_CHANNELS];
double      g_aBrSfxChanRate[BR_SFX_CHANNELS];

int32_t     g_brSfxSrcLast;

/* ==========================================================================
 * 0x10061362 -- the table initialiser
 * ========================================================================== */

void BrSfxSrcTableInit(void)
{
    int i;

    memcpy(g_aBrSfxSrc, s_aBrSfxSrcImage, sizeof(g_aBrSfxSrc));

    /* 0x10061369: `mov [eax], ecx` with ecx counting from 0 and eax stepping
     * 24 until it reaches 0x100B3508.  Source i's group is i. */
    for (i = 0; i < BR_SFXSRC_COUNT; ++i)
        g_aBrSfxSrc[i].group = (int32_t)i;
}

/* ==========================================================================
 * The channel layer
 * ==========================================================================
 *
 * The three gates.  Every function in this layer opens with the same
 * `0x100B55F0 / 0x1184C458 / 0x1184C45C` test and returns its SUCCESS code
 * without touching anything when any is zero.  slice1_08.h already names all
 * three; see its "is sound usable" note for why 1 is success here.
 */
static int sfx_off(void)
{
    return (BrSndG0B5DE8 == 0 || BrSndPDS == NULL || BrSndG18290FC == NULL);
}

static int chan_ok(int ch)
{
    return (ch >= 0 && ch < BR_SFX_CHANNELS);
}

int BrSfxChanBind(int group, int ch)
{
    int         idx;
    BrSndVoice *pVoice;

    if (sfx_off())                       /* 0x1006B539 / 42 / 4B */
        return 1;
    /* DEVIATION: the original bounds-checks neither argument.  br_sfx.h's
     * BrSfxVoiceIndex makes the same deviation and for the same reason. */
    if (!chan_ok(ch))
        return 0;
    idx = BrSfxVoiceIndex(group, ch);
    if (idx < 0)
        return 0;

    /* 0x1006B558: the row's dword 16 is its base rate, copied into the
     * channel's own 8-byte slot as two dwords.  br_sfx.h owns the table. */
    g_aBrSfxChanRate[ch] = BrSfxGroupBaseRate(group);

    /* 0x1006B56D: if the channel already holds a voice, 0x1006B4F0 stops it
     * (0x1006B4C0 is BrSndVoiceStop).  Note it does NOT clear the pointer --
     * the store below is what replaces it. */
    if (g_apBrSfxChanVoice[ch] != NULL)
        (void)BrSndVoiceStop(g_apBrSfxChanVoice[ch]);

    /* 0x1006B590 */
    pVoice = BrSndVoices[idx];
    g_apBrSfxChanVoice[ch] = pVoice;

    /* 0x1006B599 `setne dl` -- the return is the POINTER's truth, not a
     * status code. */
    return (pVoice != NULL);
}

int BrSfxChanSetLoop(int ch, int32_t loop)
{
    BrSndVoice *pVoice;

    if (sfx_off())                       /* 0x1006B5B7 / C0 / C9 */
        return 1;
    if (!chan_ok(ch))
        return 0;

    pVoice = g_apBrSfxChanVoice[ch];     /* 0x1006B5CF */
    if (pVoice == NULL)                  /* 0x1006B5D8 */
        return 0;
    pVoice->f18 = loop;                  /* 0x1006B5DE */
    return 1;
}

int BrSfxChanSetLevels(int ch, uint32_t packed)
{
    BrSndVoice *pVoice;

    if (sfx_off())                       /* 0x1006B739 / 42 / 4B */
        return 1;
    if (!chan_ok(ch))
        return 0;

    pVoice = g_apBrSfxChanVoice[ch];     /* 0x1006B756 */

    /* 0x1006B75E -> 0x1006B790, slice1_08.c's BrSndVoiceSetLevels.  Its
     * return convention is inverted relative to the rest of that module:
     * 1 is success.  A NULL voice yields 0 and the store is skipped. */
    if (BrSndVoiceSetLevels(pVoice, packed) == 0)   /* 0x1006B766 */
        return 0;

    g_aBrSfxChan[ch].packed = packed;    /* 0x1006B772 */
    return 1;
}

/* WHAT IT DOES: actually starts a sound playing on one of the mixer's
 * channels. It finds the sound the channel is meant to be playing, sets it
 * going, works out how fast to step through the samples so the sound comes out
 * at its recorded pitch, and remembers that speed in two places -- the
 * channel's own record and the separate note of what the hardware has really
 * been told -- so the per-frame retune can tell whether anything has changed.
 * With sound switched off it reports success without doing anything. */
/* @implements 0x1006B880 glide BrSfxChanStart */
int BrSfxChanStart(int group, int ch, int32_t loop)
{
    int         idx;
    BrSndVoice *pVoice;

    if (sfx_off())                       /* 0x1006B88D / 9A / A7 */
        return 1;
    if (!chan_ok(ch))
        return 0;
    idx = BrSfxVoiceIndex(group, ch);
    if (idx < 0)
        return 0;

    /* 0x1006B8BB: the voice is looked up by (group, ch) again, NOT taken
     * from the channel's bound pointer.  With 0x1006B530 having just run
     * they are the same object; the original still does the lookup. */
    pVoice = BrSndVoices[idx];
    if (pVoice == NULL)                  /* 0x1006B8C4 */
        return 0;

    /* 0x1006B8CC -> 0x1006B950 == slice1_08.c's BrSndVoiceSetLoopAndStart.
     * It returns the DirectSound HRESULT, so non-zero is failure. */
    if (BrSndVoiceSetLoopAndStart(pVoice, loop) != 0)   /* 0x1006B8D4 */
        return 0;

    /* 0x1006B8D8..0x1006B8FE: fild the voice's f0C zero-extended to 64 bits,
     * multiply by 2^32, divide by the CHANNEL's base rate, truncate.  That
     * is br_sfx.c's BrSfxRatioFromHz exactly -- including the unguarded
     * divide, which is why a channel bound to a rate of 0 yields the x87
     * indefinite here rather than a diagnostic. */
    /* 0x1006B903/09, esi = ch*24 */
    g_aBrSfxChan[ch].ratio = BrSfxRatioFromHz(pVoice->f0C,
                                              g_aBrSfxChanRate[ch]);

    /* 0x1006B915: only NOW does the channel's bound voice become this one on
     * the path where 0x1006B530 was never called. */
    g_apBrSfxChanVoice[ch] = pVoice;

    /* THE SECOND STORE, which this port used not to make at all.  The ratio
     * goes to TWO stride-24 arrays:
     *
     *   1006B903  mov [esi+0x118EEF48], eax    ; chan[ch].ratio, low
     *   1006B909  mov [esi+0x118EEF4C], edx    ;                 high
     *   1006B90F  mov ecx, [esi+0x118EEF48]    ; READ BACK the low dword
     *   1006B915  mov [edi*4+0x1184C268], ebx  ; the bound voice, above
     *   1006B91C  mov [esi+0x1184C088], ecx    ; applied[ch].ratio, low
     *   1006B922  mov [esi+0x1184C08C], edx    ;                    high
     *
     * The two source registers differ -- eax then ecx -- and that is worth
     * being explicit about, because it looks like two different values and is
     * not one: 0x1006B90F reloads ecx from the slot 0x1006B903 has just
     * written, with only the high-dword store in between, so ecx == eax and
     * edx is still the same high dword.  The two arrays get the same 64-bit
     * ratio.  Writing chan -> applied rather than the ftol result twice is the
     * literal instruction order. */
    g_aBrSfxChanApplied[ch].ratio = g_aBrSfxChan[ch].ratio;
    return 1;
}

void BrSfxSrcChannelsReset(void)
{
    memset(g_apBrSfxChanVoice,  0, sizeof g_apBrSfxChanVoice);
    memset(g_aBrSfxChanRate,    0, sizeof g_aBrSfxChanRate);
    memset(g_aBrSfxChan,        0, sizeof g_aBrSfxChan);
    memset(g_aBrSfxChanApplied, 0, sizeof g_aBrSfxChanApplied);
}

/* ==========================================================================
 * The source layer
 * ========================================================================== */

int BrSfxSrcStart(int ch, int group, int32_t f0C, int32_t loop,
                  uint32_t packed)
{
    /* Argument mapping, traced instruction by instruction against esp:
     *
     *   0x1006E4C2  movsx esi,[esp+0xC]   esp = E-8   -> E+4  = a0 = ch
     *   0x1006E4C7  mov  ebp,[esp+0x1C]   esp = E-8   -> E+14 = a4 = packed
     *   0x1006E4CC  mov  edi,[esp+0x14]   esp = E-12  -> E+8  = a1 = group
     *   0x1006E4FB  mov  ebx,[esp+0x20]   esp = E-16  -> E+10 = a3 = loop
     *
     * a2 (f0C) is pushed by both callers and never read.  Kept in the
     * signature because the table field it comes from is real. */
    (void)f0C;

    if (!chan_ok(ch))
        return 0;

    /* 0x1006E4D8 / DE / E8 -- the channel record is written FIRST and
     * unconditionally, before any gate is consulted. */
    g_aBrSfxChan[ch].group  = group;
    g_aBrSfxChan[ch].f10    = 0;
    g_aBrSfxChan[ch].packed = packed;

    /* 0x1006E4EE.  A zero return skips the whole tail -- and the gates make
     * that return 1, so a disabled sound system takes the LONG path and each
     * callee no-ops individually. */
    if (BrSfxChanBind(group, ch) == 0)   /* 0x1006E4F8 */
        return 0;

    (void)BrSfxChanSetLoop(ch, loop);        /* 0x1006E501 */
    (void)BrSfxChanSetLevels(ch, packed);    /* 0x1006E50B */
    return BrSfxChanStart(group, ch, loop);  /* 0x1006E516 */
}

/* WHAT IT DOES: start a sound at full volume on a given channel -- a hit,
 * a menu beep, an engine, whatever the group's bank holds. */
/* @implements 0x1006E530 glide BrSfxSrcPlay */
int BrSfxSrcPlay(int ch, int group, int32_t f0C, int32_t loop)
{
    /* 0x1006E53C pushes 0x00200020 as the FIFTH argument. */
    return BrSfxSrcStart(ch, group, f0C, loop, BR_SFXSRC_PACKED);
}

/* WHAT IT DOES: start the same sound silent.  Engine loops use this so
 * the per-frame driver can raise the volume from how close the car is. */
/* @implements 0x1006E560 glide BrSfxSrcPlaySilent */
int BrSfxSrcPlaySilent(int ch, int group, int32_t f0C, int32_t loop)
{
    /* 0x1006E56C pushes 0.  The engine loops start inaudible and 0x10061470
     * raises them from the car's distance every frame. */
    return BrSfxSrcStart(ch, group, f0C, loop, 0u);
}

void BrSfxSrcTrigger(int iSrc)
{
    const BrSfxSrcDef *pS;

    /* DEVIATION: the original indexes the table with no bound check at all
     * (0x10060DB5 `lea eax,[esi+esi*2]; shl eax,3`). */
    if (iSrc < 0 || iSrc >= BR_SFXSRC_COUNT)
        return;
    pS = &g_aBrSfxSrc[iSrc];

    /* 0x10060DBB/C1/C7 read +0x10, +0x0C and +0x00 in that order and push
     * them as (3, group, f0C, loop). */
    (void)BrSfxSrcPlay(BR_SFXSRC_CHANNEL, pS->group, pS->f0C, pS->loop);

    /* 0x10060DDA -- latched AFTER the play, so a failed play still records
     * the attempt. */
    g_brSfxSrcLast = (int32_t)iSrc;
}

/* Both are eleven bytes -- `push <n>; call 0x10060DB0; add esp,4; ret` -- and
 * the ONLY thing that tells them apart is that immediate.  The manifest line
 * below used to read 0x10060DF0 for Beep2, which is the address of Beep. */
/* WHAT IT DOES: plays the game's ordinary beep -- the one the countdown uses
 * for three, two and one. */
/* @implements 0x10067D80 d3d BrSfxSrcBeep */
void BrSfxSrcBeep(void)  { BrSfxSrcTrigger(BR_SFXSRC_BEEP);  }   /* push 0x0D */
/* WHAT IT DOES: plays the second of the game's two beeps -- the one used for
 * the "go" at the end of the race countdown, where the first three steps use
 * the ordinary beep. */
/* @implements 0x10060E00 glide BrSfxSrcBeep2 */
void BrSfxSrcBeep2(void) { BrSfxSrcTrigger(BR_SFXSRC_BEEP2); }   /* push 0x0E */

void BrSfxSrcRaceCountdown(int iStep)
{
    /* 0x1001AD93 `cmp ecx,4` / 0x1001AD9C `jne`.  The counter has already
     * been incremented, so 4 is the fourth and last beep -- the GO. */
    if (iStep == 4)
        BrSfxSrcBeep2();
    else
        BrSfxSrcBeep();
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_100b32b0;
extern int DAT_100b32bc;
extern int DAT_100b32c0;
extern int DAT_1184c094;
int FUN_1006b790();
int FUN_1006e590();
extern int g_BrSndAA3470;
extern int g_aBrSndBankVoice;

/* WHAT IT DOES: play an SFX bank entry by index (channel 3, bank-relative params). */
/* @implements 0x10060DB0 glide BrSfxBankPlay */

int BrSfxBankPlay(int param_1)

{
  BrSfxSrcPlay(3,(&DAT_100b32b0)[param_1 * 6],(&DAT_100b32bc)[param_1 * 6],
               (&DAT_100b32c0)[param_1 * 6]);
  g_BrSndAA3470 = param_1;
  return;
}

/* WHAT IT DOES: set the playback frequency on a sound voice by bank index. */
/* @implements 0x1006B730 glide BrSndVoiceSetFreq */

int BrSndVoiceSetFreq(int param_1,int param_2)

{
  int iVar1;
  
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    iVar1 = FUN_1006b790((&g_aBrSndBankVoice)[param_1],param_2);
    if (iVar1 != 0) {
      (&DAT_1184c094)[param_1 * 6] = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: thunk — forwards to the shared no-op at 0x1006E590. */
/* @implements 0x1006E580 glide BrThunk6E580 */

int BrThunk6E580(void)

{
  FUN_1006e590();
  return;
}

#endif /* BR_MATCHING_BUILD */
