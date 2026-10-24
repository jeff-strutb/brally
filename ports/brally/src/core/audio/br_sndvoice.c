/* br_sndvoice.c -- audio.
 *
 * The DirectSound voice and buffer layer: starting, stopping, releasing and
 * retuning the individual buffers a sound plays through, the channel-to-voice
 * binding on top of them, the per-frame apply of pending pitch/freq/start,
 * the bank-wide mute and teardown, and the mixing thread's stop handshake.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 *
 * Cross-module declarations are copied VERBATIM from the owning header, as
 * slice6_76.c does and for the same reason: those owners' headers carry
 * conflicting partial models of the same objects and cannot coexist in one
 * translation unit.
 */

#include "br_sfxsrc.h"   /* br_globals: its objects */
#include "slice1_08.h"   /* br_globals: its objects */
#include <stddef.h>
#include <stdint.h>

#include "slice6_76.h"

/* ==========================================================================
 * 0. Cross-module declarations (see the banner)
 * ========================================================================== */

/* slice1_05.h -- 0x1002F900.  slice2_15.h calls the same address with its own
 * name for the command pair; both structs are {uint32_t w0, w1;}. */
struct BrGfxWords;
struct BrGfxCmd;
/* BrRdpSetCombineLERP: prototype in br_funcs.h */

/* slice5_61.h -- 0x10042AF0 and 0x10060E90. */
extern void    BrGfx42AF0_1(void *p0);
extern int32_t BrTimeNow(void);

/* slice2_15.h / slice5_62.h -- 0x10069490, an adapter over br_pool.c. */
struct BrMat4;
/* BrSub_10069490: prototype in br_funcs.h */

/* slice3_41.h -- 0x10069530. */
/* BrPool32Alloc: prototype in br_funcs.h */

/* slice5_63.h -- 0x1003E310. */
extern void BrSub1003E310(void);

/* slice4_53.h -- 0x1006A4A0. */
extern void BrSub1006A4A0(void *pThis, void *pArg);

/* slice1_10.h -- 0x10079550; slice3_45.h owns the one instance. */
struct BrFfb;
extern void BrFfbShutdown(struct BrFfb *pFfb);
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* slice2_25.h -- 0x100443E0 and 0x10044280.  Both return int; both callers
 * (slice2_26.c) declare void and ignore it. */
struct BrGameObj;
/* BrOptOpen2950A: prototype in br_funcs.h */
/* BrOptOpen2950B: prototype in br_funcs.h */

/* slice4_50.h -- 0x10043BF0. */
/* BrSub10043BF0: prototype in br_funcs.h */

/* slice1_08.h -- 0x10072550, and the three "is sound usable" gates. */
struct BrSndVoice;
/* BrSndVoiceStop: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B5DE8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x118290F8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x118290FC */

/* slice1_08.h / slice3_40.h -- 0x100BBAE0, a BYTE master volume. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* slice3_40.h -- 0x100BBAD8, and the two ten-entry level tables. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100ADF68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100ADF90 */

/* slice2_25.h / slice3_40.h -- the two slider positions. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10B4E708 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10B4E70C */

/* slice2_18.h -- 0x106C65E4, the hi-res flag: non-zero doubles every rect. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* slice2_20.h -- 0x100B8C90.  br_data.c defines it as 1. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* slice4_50.h:250 -- 0x10094294, the local slot / palette index.  slice4_50.c
 * OWNS the storage; this packet only reads it.  See the note in section 1. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */



/* ==========================================================================
 * 5. 0x10072580 -- stop one bank voice
 * ==========================================================================
 *
 * Three call sites.  Four guards, then a Stop; the original returns 1 from
 * every guard and (hr == 0) from the tail, expressed as `neg/sbb/inc`.
 *
 * slice2_17.c:95 declares this void and discards the result.  The original
 * returns int -- 1 from every guard, (hr == 0) from the tail -- and matching
 * needs that, so the definition follows the image rather than the host
 * prototype.  Callers still ignore eax. */
/* WHAT IT DOES: silences one of the game's sound-effect slots. If sound was
 * never brought up, or that slot is not holding a sound, it quietly does
 * nothing. */
/* @implements 0x10072580 d3d BrX10072580 */
int BrX10072580(int a0)
{
    struct BrSndVoice *pVoice;

    /* Nested so /O2 shares one `mov eax, 1 / ret` epilogue (`je` to it). */
    if ((*(int32_t *)&DAT_100b51e4[1036]) != 0) {
        if (BrSndPDS != NULL) {
            if (BrSndG18290FC != NULL) {
                /* No bounds check on a0 in the original.  Preserved. */
                pVoice = (struct BrSndVoice *)g_apBrSfxChanVoice[a0];
                if (pVoice != NULL)
                    return BrSndVoiceBufStop(pVoice) == 0;
            }
        }
    }
    return 1;
}


/* -- Ghidra-matched functions ----------------------- */

/* 0x1184C1E8 -- each channel's base rate; br_sfxsrc.h owns the model. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x1184C080 stride 24 -- br_sfxsrc.h's "applied" channel array; only its
 * +0x08 ratio field is touched here, so it is indexed as int64 elements,
 * three per channel.  0x10077C00 is the ratio-to-hertz scale constant. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, by its definition's header */

/* WHAT IT DOES: push a channel's 32.32 pitch ratio at its voice.  The ratio
 * is scaled by the channel's base rate and the fixed-point constant to give
 * a frequency in hertz, which goes to the DirectSound buffer; only if that
 * succeeds is the ratio recorded as the one actually applied, so the record
 * never claims a pitch the device refused.  Sound down is a silent 1. */
/* @implements 0x1006B5F0 glide BrSndChanSetRatio */

int BrSndChanSetRatio(int iSlot, int64_t ratio)

{
  if ((((*(int32_t *)&DAT_100b51e4[1036]) != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (BrSndBufSetVolume((struct BrSndVoice *)g_apBrSfxChanVoice[iSlot],
                          (unsigned int)((double)ratio * g_aBrSfxChanRate[iSlot]
                                         * DAT_10077c00)) != 0) {
      g_aBrSfxChanApplied[iSlot].ratio = ratio;
      return 1;
    }
    return 0;
  }
  return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */


/* WHAT IT DOES: signal the sound-mixing thread to exit, wait for it, and close its handles. */
/* @implements 0x1006B1E0 glide BrSndThreadStop */

int BrSndThreadStop(void)

{
  if ((*(int *)&g_fBrSndThread86) != 0) {
    br_os_set(g_hBrSndWake86);
    br_os_wait(g_hBrSndThread86, 0xffffffff);
    br_os_close(g_hBrSndThread86);
    g_hBrSndThread86 = (void *)0x0;
    br_os_close(g_hBrSndWake86);
    g_hBrSndWake86 = (void *)0x0;
    (*(int *)&g_fBrSndThread86) = 0;
  }
  return;
}




/* WHAT IT DOES: free the voice's sound and clear the pointer; always
 * returns 0. */
/* @implements 0x1006B490 glide BrSndVoiceBufRelease */

int BrSndVoiceBufRelease(BrSndVoice *param_1)

{
  if (param_1->pBuf != 0) {
    br_snd_free(param_1->pBuf);
    param_1->pBuf = 0;
  }
  return 0;
}

/* WHAT IT DOES: if the voice is playing, stop its sound and clear the
 * playing flag; returns 0. */
/* @implements 0x1006B4C0 glide BrSndVoiceBufStop */

int BrSndVoiceBufStop(BrSndVoice * param_1)

{
  if (param_1->f1C == 0) {
    return 0;
  }
  br_snd_stop(param_1->pBuf);
  param_1->f1C = 0;
  return 0;
}

/* WHAT IT DOES: set the voice's pan from its 0..800 position (400 centre). */
/* @implements 0x1006B400 glide BrSndVoiceApplyPan */

int BrSndVoiceApplyPan(BrSndVoice * param_1)

{
  br_snd_pan(param_1->pBuf, (param_1->f10 + -400) * 10);
  return 0;
}

/* WHAT IT DOES: set the voice's playback rate from the voice struct. */
/* @implements 0x1006B420 glide BrSndVoiceApplyFreq */

int BrSndVoiceApplyFreq(BrSndVoice *param_1)

{
  br_snd_rate(param_1->pBuf, param_1->f0C);
  return 0;
}

/* br_musiccmd.c -- 0x1006BB60 and 0x1006BB90, the two list walkers. */
/* BrSndBufStopAll: prototype in br_funcs.h */
/* BrSndBufFreeAll: prototype in br_funcs.h */

/* 0x1184C2A8, the DirectSound object the two walkers hang their list off;
 * 0x1184C260, the live group count 0x1006C290 stores; 0x100B55F8, the voice
 * table, 0x12 dwords per group row (see br_sfx.h). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

void *memset(void *, int, size_t);

/* WHAT IT DOES: tear the sound bank down -- stop every buffer on the device's
 * list, free the memory behind them, then zero the per-group voice rows (the
 * first 15 dwords of each 0x12-dword row, for as many groups as are loaded)
 * and the 15-slot bank voice array.  The device itself is left open, so a
 * reload can refill the same tables.  Sound disabled is a silent success. */
/* @implements 0x1006C460 glide BrSndBankFree */

int BrSndBankFree(void)

{
  BrSndRow *pRow;
  int  cGroups;

  if ((*(int32_t *)&DAT_100b51e4[1036]) == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  BrSndBufStopAll(&g_BrSndPrimary);
  BrSndBufFreeAll(&g_BrSndPrimary);
  cGroups = DAT_1184c260;
  if (0 < cGroups) {
    pRow = g_aBrSndRow;
    do {
      memset(pRow->aSlot, 0, 15 * sizeof pRow->aSlot[0]);   /* slots 0..14 */
      pRow = pRow + 1;
    } while (--cGroups != 0);
  }
  memset(g_apBrSfxChanVoice, 0, sizeof(g_apBrSfxChanVoice));
  return 1;
}

/* 0x1184C1E8 -- each channel's base rate; br_sfxsrc.h owns the model. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: bind one of a group's voices to a playback channel.  Copies
 * the group row's 8-byte base rate into the channel's rate slot, silences
 * whatever the channel was already holding, then stores the new voice.
 * Returns whether the channel ended up holding a voice -- and, as everywhere
 * else on this path, a silent 1 when sound is not up. */
/* @implements 0x1006B530 glide BrSndChanBind */

int BrSndChanBind(int iGroup, int iSlot)

{
  BrSndVoice *pVoice;

  if ((((*(int32_t *)&DAT_100b51e4[1036]) != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    g_aBrSfxChanRate[iSlot] = g_aBrSndRow[iGroup].baseRate;
    if (g_apBrSfxChanVoice[iSlot] != 0) {
      BrX10072580(iSlot);
    }
    pVoice = g_aBrSndRow[iGroup].aSlot[iSlot];
    g_apBrSfxChanVoice[iSlot] = (void *)pVoice;
    return pVoice != 0;
  }
  return 1;
}

/* WHAT IT DOES: set or clear the loop flag (+0x18) on the voice bound to one
 * playback channel -- the flag BrSndVoiceBufStart reads to decide whether
 * Play loops.  Returns 1 on success, 0 when the channel holds no voice, and a
 * silent 1 when sound is not up.  Starts nothing. */
/* @implements 0x1006B5B0 glide BrSfxChanSetLoop */

int BrSfxChanSetLoop(int iSlot, int loop)

{
  if ((((*(int32_t *)&DAT_100b51e4[1036]) != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (g_apBrSfxChanVoice[iSlot] != 0) {
      ((BrSndVoice *)g_apBrSfxChanVoice[iSlot])->f18 = loop;
      return 1;
    }
    return 0;
  }
  return 1;
}


/* WHAT IT DOES: start a voice's buffer.  If the buffer is already playing
 * (GetStatus, vtable +0x24, reports DSBSTATUS_PLAYING) it is rewound instead
 * -- SetCurrentPosition(0), vtable +0x34 -- so retriggering a live sound
 * restarts it rather than layering a second Play on it.  Otherwise Play
 * (vtable +0x30) runs, looping iff the voice's +0x18 flag is set, and the
 * voice's "playing" flag at +0x1c is raised only when Play returns S_OK. */
/* @implements 0x1006B970 glide BrSndVoiceBufStart */

int BrSndVoiceBufStart(BrSndVoice *param_1)

{
  if (br_snd_playing(param_1->pBuf)) {
    br_snd_seek(param_1->pBuf, 0);      /* already playing: from the top */
    return 0;
  }
  br_snd_play(param_1->pBuf, param_1->f18 != 0);
  param_1->f1C = 1;
  return 0;
}

/* WHAT IT DOES: silence the whole sound bank -- for every occupied voice slot
 * drive its DirectSound buffer to DSBVOLUME_MIN (vtable +0x3c) and recentre
 * the pan (vtable +0x40).  The buffers keep playing; only their output is
 * killed, so a later volume/pan restore resumes them mid-sound.  Empty slots
 * are skipped and the sound-disabled case is a silent success. */
/* @implements 0x1006BD70 glide BrSndBankMute */

int BrSndBankMute(void)

{
  void **ppVoice;
  BrSndVoice *pVoice;

  if ((*(int32_t *)&DAT_100b51e4[1036]) == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  ppVoice = g_apBrSfxChanVoice;
  do {
    pVoice = (BrSndVoice *)*ppVoice;
    if (pVoice != 0) {
      br_snd_volume(pVoice->pBuf, -10000);
      br_snd_pan(pVoice->pBuf, 0);
    }
    ppVoice = ppVoice + 1;
  } while ((uintptr_t)ppVoice < (uintptr_t)&g_apBrSfxChanVoice[BR_SND_BANK_VOICES]);
  return 1;
}

/* BrSfxChanSetLoop: prototype in br_funcs.h */
/* BrSndVoiceSetFreq: prototype in br_funcs.h */
/* BrSfxChanStart: prototype in br_funcs.h */
/* FUN_1006bf50: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: one tick of the sound-channel table. For each of 15 slots,
 * if the bound voice changed, re-bind it (bank 0x19 or bank 0 depending on
 * a mode flag), centre the pan, push the pending pitch and frequency, and
 * start it. Then copy across any still-pending volume/ratio/freq, and if a
 * pitch is live but the voice reports it has stopped, clear that pitch. */
/* @implements 0x1006BDD0 glide FUN_1006bdd0 */
int FUN_1006bdd0(void)
{
  int z;
  int i;
  BrSndVoice **p;
  int off;
  BrSndVoice *v;
  int a;
  int c;

  z = 0;
  i = 0;
  p = &g_aBrSndRow[25].aSlot[0];
  off = 0;
  do {
    if (g_184C454 != z) {
      v = g_aBrSndRow[0].aSlot[i];
      if (v != z && v == g_apBrSfxChanVoice[i]) {
        BrSndChanBind(0x19, i);
        BrSfxChanSetLoop(i, 1);
        BrSndChanSetRatio(i, *(__int64 *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off));
        BrSndVoiceSetFreq(i, *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x14)) + off));
        BrSfxChanStart(0x19, i, 1);
      }
    } else {
      v = *p;
      if (v != z && v == g_apBrSfxChanVoice[i]) {
        BrSndChanBind(z, i);
        BrSfxChanSetLoop(i, 1);
        BrSndChanSetRatio(i, *(__int64 *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off));
        BrSndVoiceSetFreq(i, *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x14)) + off));
        BrSfxChanStart(z, i, 1);
      }
    }
    a = *(int *)((char *)&(*(int *)&g_aBrSfxChan) + off);
    if (*(int *)((char *)&(*(int *)&g_aBrSfxChanApplied) + off) != a) {
      *(int *)((char *)&(*(int *)&g_aBrSfxChanApplied) + off) = a;
    }
    if (*(__int64 *)((char *)&(*(int64_t (*)[])((char *)&g_aBrSfxChanApplied + 0x8)) + off)
        != *(__int64 *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off)) {
      BrSndChanSetRatio(i, *(__int64 *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off));
    }
    a = *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x14)) + off);
    if (*(int *)((char *)&(*(int *)((char *)&g_aBrSfxChanApplied + 0x14)) + off) != a) {
      BrSndVoiceSetFreq(i, a);
    }
    a = *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off);
    c = *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0xC)) + off);
    if ((a | c) != 0) {
      if (BrSndVoiceIsPlaying(i) == 0) {
        *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + off) = z;
        *(int *)((char *)&(*(int64_t (*)[])((char *)&g_aBrSfxChanApplied + 0x8)) + off) = z;
        *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0xC)) + off) = z;
        *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChanApplied + 0xC)) + off) = z;
      }
    }
    p = p + 1;
    i = i + 1;
    off = off + 0x18;
  } while (p < &g_aBrSndRow[25].aSlot[15]);
  return 1;
}

/* WHAT IT DOES: set the volume on a DirectSound buffer and commit the change. */
/* @implements 0x1006B670 glide BrSndBufSetVolume */

int BrSndBufSetVolume(BrSndVoice *param_1,int param_2)

{
  if ((((*(int32_t *)&DAT_100b51e4[1036]) != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (param_1 != 0) {
      *(int *)&param_1->f0C = param_2;
      BrSndVoiceApplyFreq(param_1);
      return 1;
    }
    return 0;
  }
  return 1;
}

/* WHAT IT DOES: append node `param_2` to the singly linked list (next pointer at +0x1A8)
 * headed at `param_1`, clearing the new node's next and its +0x1C word. Returns 0. */
/* @implements 0x1006B3C0 glide BrSndListAppend */

int BrSndListAppend(BrSndVoice *param_1,BrSndVoice *param_2)
{
  param_2->pNext = 0;
  param_2->f1C = 0;
  while (param_1->pNext != 0) {
    param_1 = param_1->pNext;
  }
  param_1->pNext = param_2;
  return 0;
}


