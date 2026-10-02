/* br_sfxsrc.c -- the sound-source layer.  See br_sfxsrc.h for the chain, the
 * stack traces and the evidence for what each argument is.
 *
 * Transcribed from orig/BRGlide.dll.  Every branch carries the address of the
 * instruction it is.
 */
#include "slice2_24.h"   /* br_globals: its objects */
#include <string.h>

/* Header prototype takes int ch; the original's first argument is a SHORT
 * (movsx esi,[esp+0xC]) and it returns nothing. */
#define BrSfxSrcStart BrSfxSrcStart_cdecl
#include "br_sfxsrc.h"
#undef BrSfxSrcStart
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

/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x118EEF40, stride 24 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x1184C080, stride 24 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

int32_t     g_brSfxSrcLast;

/* ==========================================================================
 * 0x10061362 -- the table initialiser
 * ========================================================================== */

/* (port-only BrSfxSrcTableInit removed) */


/* ==========================================================================
 * The channel layer
 * ==========================================================================
 *
 * The three gates.  Every function in this layer opens with the same
 * `0x100B55F0 / 0x1184C458 / 0x1184C45C` test and returns its SUCCESS code
 * without touching anything when any is zero.  slice1_08.h already names all
 * three; see its "is sound usable" note for why 1 is success here.
 */
/* (port-only sfx_off removed) */


/* (port-only chan_ok removed) */


/* BrSfxChanBind: the original function is BrSndChanBind */

/* BrSfxChanSetLoop: the placed body is br_sndvoice.c */

/* BrSfxChanSetLevels: the original function is BrSndVoiceSetFreq */

/* WHAT IT DOES: actually starts a sound playing on one of the mixer's
 * channels. It finds the sound the channel is meant to be playing, sets it
 * going, works out how fast to step through the samples so the sound comes out
 * at its recorded pitch, and remembers that speed in two places -- the
 * channel's own record and the separate note of what the hardware has really
 * been told -- so the per-frame retune can tell whether anything has changed.
 * With sound switched off it reports success without doing anything. */
/* @implements 0x1006B880 glide BrSfxChanStart */
/* Hand-transcribed from the asm.  No channel range check and no voice-index
 * helper: the voice is BrSndVoices[group*18 + ch] straight.  The success path
 * is the nested block -- it falls through with the failure `return 0` after
 * it and the sound-off `return 1` last, as the original lays them out. */
int BrSfxChanStart(int group, int ch, int32_t loop)
{
    BrSndVoice *pVoice;

    if ((*(int *)&DAT_100b51e4[1036]) != 0 && BrSndPDS != NULL && BrSndG18290FC != NULL) {
        pVoice = g_aBrSndRow[group].aSlot[ch];
        if (pVoice != NULL && BrSndBufSetPan(pVoice, loop) == 0) {
            /* f0C zero-extended to 64 bits, * 2^32, / the channel's rate */
            g_aBrSfxChan[ch].ratio =
                (int64_t)((double)(int64_t)(uint32_t)pVoice->f0C
                          * 4294967296.0 / g_aBrSfxChanRate[ch]);
            (*(BrSndVoice * (*)[15])&g_apBrSfxChanVoice)[ch] = pVoice;
            g_aBrSfxChanApplied[ch].ratio = g_aBrSfxChan[ch].ratio;
            return 1;
        }
        return 0;
    }
    return 1;
}

/* (port-only BrSfxSrcChannelsReset removed) */


/* ==========================================================================
 * The source layer
 * ========================================================================== */

/* WHAT IT DOES: bind and start a sound on a channel: record the channel's
 * group/loop-flag/packed levels, bind the voice, then pan, frequency, start.
 * The record stores happen before any gate; a2 is pushed by callers and
 * never read. */
/* @implements 0x1006E4C0 glide BrSfxSrcStart */
void BrSfxSrcStart(short ch, int group, int32_t f0C, int32_t loop,
                   uint32_t packed)
{
    int c = ch;

    g_aBrSfxChan[c].group  = group;
    g_aBrSfxChan[c].f10    = 0;
    g_aBrSfxChan[c].packed = packed;
    if (BrSndChanBind(group, c) != 0) {
        BrSfxChanSetLoop(c, loop);
        BrSndVoiceSetFreq(c, packed);
        BrSfxChanStart(group, c, loop);
    }
}

/* WHAT IT DOES: start a sound at full volume on a given channel -- a hit,
 * a menu beep, an engine, whatever the group's bank holds. */
/* @implements 0x1006E530 glide BrSfxSrcPlay */
int BrSfxSrcPlay(int ch, int group, int32_t f0C, int32_t loop)
{
    /* 0x1006E53C pushes 0x00200020 as the FIFTH argument. */
    BrSfxSrcStart(ch, group, f0C, loop, BR_SFXSRC_PACKED);
    return 0;   /* the original returns whatever the callee left in eax */
}

/* WHAT IT DOES: start the same sound silent.  Engine loops use this so
 * the per-frame driver can raise the volume from how close the car is. */
/* @implements 0x1006E560 glide BrSfxSrcPlaySilent */
int BrSfxSrcPlaySilent(int ch, int group, int32_t f0C, int32_t loop)
{
    /* 0x1006E56C pushes 0.  The engine loops start inaudible and 0x10061470
     * raises them from the car's distance every frame. */
    BrSfxSrcStart(ch, group, f0C, loop, 0u);
    return 0;   /* the original returns whatever the callee left in eax */
}

/* @n64 0x8022B370 located */
/* BrSfxSrcTrigger: the original function is BrSfxBankPlay */

/* Both are eleven bytes -- `push <n>; call 0x10060DB0; add esp,4; ret` -- and
 * the ONLY thing that tells them apart is that immediate.  The manifest line
 * below used to read 0x10060DF0 for Beep2, which is the address of Beep. */
/* WHAT IT DOES: plays the game's ordinary beep -- the one the countdown uses
 * for three, two and one. */
/* @implements 0x10067D80 d3d BrSfxSrcBeep */
/* @n64 0x8022B3C4 exact */
void BrSfxSrcBeep(void)  { BrSfxBankPlay(BR_SFXSRC_BEEP);  }   /* push 0x0D */
/* WHAT IT DOES: plays the second of the game's two beeps -- the one used for
 * the "go" at the end of the race countdown, where the first three steps use
 * the ordinary beep. */
/* @implements 0x10060E00 glide BrSfxSrcBeep2 */
/* @n64 0x8022B3E4 exact */
void BrSfxSrcBeep2(void) { BrSfxBankPlay(BR_SFXSRC_BEEP2); }   /* push 0x0E */

/* @n64 0x80264E0C located */
/* (port-only BrSfxSrcRaceCountdown removed) */


/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_1006b790: prototype in br_funcs.h */
/* FUN_1006e590: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: play an SFX bank entry by index (channel 3, bank-relative params). */
/* @implements 0x10060DB0 glide BrSfxBankPlay */

int BrSfxBankPlay(int param_1)

{
  BrSfxSrcPlay(3,(*(int *)&g_brStages[27 + param_1].f10[0]),g_brStages[28 + param_1].f04,
               g_brStages[28 + param_1].f08);
  g_BrSndAA3470 = param_1;
  return;
}

/* WHAT IT DOES: set the playback frequency on a sound voice by bank index. */
/* @implements 0x1006B730 glide BrSndVoiceSetFreq */

int BrSndVoiceSetFreq(int param_1,int param_2)

{
  int iVar1;
  
  if ((((*(int *)&DAT_100b51e4[1036]) != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    iVar1 = BrSndVoiceSetLR((&((*(BrSndVoice * (*)[15])&g_apBrSfxChanVoice)[0]))[param_1],param_2);
    if (iVar1 != 0) {
      (*(int *)&g_aBrSfxChanApplied[param_1].packed) = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: thunk: forwards to the shared no-op at 0x1006E590. */
/* @implements 0x1006E580 glide BrThunk6E580 */

int BrThunk6E580(void)

{
  BrNop6E590();
  return;
}


/* WHAT IT DOES: no-op: the shared target of multiple thunks. */
/* @implements 0x1006E590 glide BrNop6E590 */

int BrNop6E590(void)

{
  return;
}

