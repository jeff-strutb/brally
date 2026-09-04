/* br_sndvoice.c -- audio.
 *
 * The DirectSound voice and buffer layer: starting, stopping, releasing and
 * retuning the individual buffers a sound plays through, the channel-to-voice
 * binding on top of them, the bank-wide mute and teardown, and the mixing
 * thread's stop handshake.
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
extern void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                                int a0,  int b0,  int c0,  int d0,
                                int Aa0, int Ab0, int Ac0, int Ad0,
                                int a1,  int b1,  int c1,  int d1,
                                int Aa1, int Ab1, int Ac1, int Ad1);

/* slice5_61.h -- 0x10042AF0 and 0x10060E90. */
extern void    BrGfx42AF0_1(void *p0);
extern int32_t BrTimeNow(void);

/* slice2_15.h / slice5_62.h -- 0x10069490, an adapter over br_pool.c. */
struct BrMat4;
extern struct BrMat4 *BrSub_10069490(void);

/* slice3_41.h -- 0x10069530. */
extern void *BrPool32Alloc(void);

/* slice5_63.h -- 0x1003E310. */
extern void BrSub1003E310(void);

/* slice4_53.h -- 0x1006A4A0. */
extern void BrSub1006A4A0(void *pThis, void *pArg);

/* slice1_10.h -- 0x10079550; slice3_45.h owns the one instance. */
struct BrFfb;
extern void BrFfbShutdown(struct BrFfb *pFfb);
extern struct BrFfb g_brFfb;

/* slice2_25.h -- 0x100443E0 and 0x10044280.  Both return int; both callers
 * (slice2_26.c) declare void and ignore it. */
struct BrGameObj;
extern int BrOptOpen2950A(struct BrGameObj *pUnused);
extern int BrOptOpen2950B(struct BrGameObj *pUnused);

/* slice4_50.h -- 0x10043BF0. */
extern void BrSub10043BF0(struct BrGameObj *p);

/* slice1_08.h -- 0x10072550, and the three "is sound usable" gates. */
struct BrSndVoice;
extern int32_t BrSndVoiceStop(struct BrSndVoice *pVoice);
extern int32_t   BrSndG0B5DE8;    /* 0x100B5DE8 */
struct BrDSound;
extern struct BrDSound *BrSndPDS; /* 0x118290F8 */
extern void     *BrSndG18290FC;   /* 0x118290FC */

/* slice1_08.h / slice3_40.h -- 0x100BBAE0, a BYTE master volume. */
extern uint8_t BrSndMasterVolume;

/* slice3_40.h -- 0x100BBAD8, and the two ten-entry level tables. */
extern uint8_t BrG_0BBAD8;
extern const int32_t BrOptLevelATable[10];   /* 0x100ADF68 */
extern const int32_t BrOptLevelBTable[10];   /* 0x100ADF90 */

/* slice2_25.h / slice3_40.h -- the two slider positions. */
extern int32_t g_brB4E708;   /* 0x10B4E708 */
extern int32_t g_brB4E70C;   /* 0x10B4E70C */

/* slice2_18.h -- 0x106C65E4, the hi-res flag: non-zero doubles every rect. */
extern int32_t BrG_6C65E4;

/* slice2_20.h -- 0x100B8C90.  br_data.c defines it as 1. */
extern int g_i0B8C90;

/* slice4_50.h:250 -- 0x10094294, the local slot / palette index.  slice4_50.c
 * OWNS the storage; this packet only reads it.  See the note in section 1. */
extern int32_t g_br094294;



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
    if (BrSndG0B5DE8 != 0) {
        if (BrSndPDS != NULL) {
            if (BrSndG18290FC != NULL) {
                /* No bounds check on a0 in the original.  Preserved. */
                pVoice = (struct BrSndVoice *)g_aBrSndBankVoice[a0];
                if (pVoice != NULL)
                    return BrSndVoiceStop(pVoice) == 0;
            }
        }
    }
    return 1;
}


/* ── Ghidra-matched functions ─────────────────────── */
#ifdef BR_MATCHING_BUILD

/* 0x1184C1E8 -- each channel's base rate; br_sfxsrc.h owns the model. */
extern double g_aBrSfxChanRate[];

/* 0x1184C080 stride 24 -- br_sfxsrc.h's "applied" channel array; only its
 * +0x08 ratio field is touched here, so it is indexed as int64 elements,
 * three per channel.  0x10077C00 is the ratio-to-hertz scale constant. */
extern int64_t DAT_1184c088[];
extern double  DAT_10077c00;

int BrSndBufSetVolume(int param_1, int param_2);

/* WHAT IT DOES: push a channel's 32.32 pitch ratio at its voice.  The ratio
 * is scaled by the channel's base rate and the fixed-point constant to give
 * a frequency in hertz, which goes to the DirectSound buffer; only if that
 * succeeds is the ratio recorded as the one actually applied, so the record
 * never claims a pitch the device refused.  Sound down is a silent 1. */
/* @implements 0x1006B5F0 glide BrSndChanSetRatio */

int BrSndChanSetRatio(int iSlot, int64_t ratio)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (BrSndBufSetVolume((int)g_aBrSndBankVoice[iSlot],
                          (unsigned int)((double)ratio * g_aBrSfxChanRate[iSlot]
                                         * DAT_10077c00)) != 0) {
      DAT_1184c088[iSlot * 3] = ratio;
      return 1;
    }
    return 0;
  }
  return 1;
}

#include <windows.h>
extern int DAT_11849e60;
extern int DAT_1184c078;
extern int DAT_1184c07c;


/* WHAT IT DOES: signal the sound-mixing thread to exit, wait for it, and close its handles. */
/* @implements 0x1006B1E0 glide BrSndThreadStop */

int BrSndThreadStop(void)

{
  if (DAT_1184c078 != 0) {
    SetEvent(DAT_11849e60);
    WaitForSingleObject(DAT_1184c07c,0xffffffff);
    CloseHandle(DAT_1184c07c);
    DAT_1184c07c = (HANDLE)0x0;
    CloseHandle(DAT_11849e60);
    DAT_11849e60 = (HANDLE)0x0;
    DAT_1184c078 = 0;
  }
  return;
}


typedef void (__stdcall *dsbuf_fn2)(int, int);

typedef int (__stdcall *dsbuf_fn1)(int);


/* WHAT IT DOES: release the voice's DirectSound buffer (vtable +8 = Release)
 * and clear the pointer; always returns 0. */
/* @implements 0x1006B490 glide BrSndVoiceBufRelease */

int BrSndVoiceBufRelease(int param_1)

{
  int *piVar1;

  piVar1 = *(int **)(param_1 + 0x9c);
  if (piVar1 != (int *)0x0) {
    (*(dsbuf_fn1 *)(*piVar1 + 8))((int)piVar1);
    *(int *)(param_1 + 0x9c) = 0;
  }
  return 0;
}

/* WHAT IT DOES: if the voice is playing, call IDirectSoundBuffer::Stop
 * (vtable +0x48) and clear the playing flag on S_OK; returns the HRESULT. */
/* @implements 0x1006B4C0 glide BrSndVoiceBufStop */

int BrSndVoiceBufStop(int param_1)

{
  int iVar1;

  if (*(int *)(param_1 + 0x1c) == 0) {
    return 0;
  }
  iVar1 = (*(dsbuf_fn1 *)(**(int **)(param_1 + 0x9c) + 0x48))(*(int *)(param_1 + 0x9c));
  if (iVar1 == 0) {
    *(int *)(param_1 + 0x1c) = iVar1;
  }
  return iVar1;
}

/* WHAT IT DOES: call IDirectSoundBuffer::SetPan with a computed pan value. */
/* @implements 0x1006B400 glide BrSndVoiceApplyPan */

void BrSndVoiceApplyPan(int param_1)

{
  dsbuf_fn2 fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x40);
  fn(*(int *)(param_1 + 0x9c), (*(int *)(param_1 + 0x10) + -400) * 10);
  return;
}

/* WHAT IT DOES: call IDirectSoundBuffer::SetFrequency from the voice struct. */
/* @implements 0x1006B420 glide BrSndVoiceApplyFreq */

void BrSndVoiceApplyFreq(int param_1)

{
  dsbuf_fn2 fn = *(dsbuf_fn2 *)(**(int **)(param_1 + 0x9c) + 0x44);
  fn(*(int *)(param_1 + 0x9c), *(int *)(param_1 + 0xc));
  return;
}

/* br_musiccmd.c -- 0x1006BB60 and 0x1006BB90, the two list walkers. */
extern int BrSndBufStopAll(int param_1);
extern int BrSndBufFreeAll(int param_1);

/* 0x1184C2A8, the DirectSound object the two walkers hang their list off;
 * 0x1184C260, the live group count 0x1006C290 stores; 0x100B55F8, the voice
 * table, 0x12 dwords per group row (see br_sfx.h). */
extern int DAT_1184c2a8;
extern int DAT_1184c260;
extern int DAT_100b55f8[];

void *memset(void *, int, size_t);

/* WHAT IT DOES: tear the sound bank down -- stop every buffer on the device's
 * list, free the memory behind them, then zero the per-group voice rows (the
 * first 15 dwords of each 0x12-dword row, for as many groups as are loaded)
 * and the 15-slot bank voice array.  The device itself is left open, so a
 * reload can refill the same tables.  Sound disabled is a silent success. */
/* @implements 0x1006C460 glide BrSndBankFree */

int BrSndBankFree(void)

{
  int *pRow;
  int  cGroups;

  if (BrSndG0B5DE8 == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  BrSndBufStopAll((int)&DAT_1184c2a8);
  BrSndBufFreeAll((int)&DAT_1184c2a8);
  cGroups = DAT_1184c260;
  if (0 < cGroups) {
    pRow = DAT_100b55f8;
    do {
      memset(pRow, 0, 60);
      pRow = pRow + 0x12;
    } while (--cGroups != 0);
  }
  memset(g_aBrSndBankVoice, 0, sizeof(g_aBrSndBankVoice));
  return 1;
}

/* 0x1184C1E8 -- each channel's base rate; br_sfxsrc.h owns the model. */
extern double g_aBrSfxChanRate[];

/* WHAT IT DOES: bind one of a group's voices to a playback channel.  Copies
 * the group row's 8-byte base rate into the channel's rate slot, silences
 * whatever the channel was already holding, then stores the new voice.
 * Returns whether the channel ended up holding a voice -- and, as everywhere
 * else on this path, a silent 1 when sound is not up. */
/* @implements 0x1006B530 glide BrSndChanBind */

int BrSndChanBind(int iGroup, int iSlot)

{
  int pVoice;

  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    g_aBrSfxChanRate[iSlot] = ((double *)DAT_100b55f8)[iGroup * 9 + 8];
    if (g_aBrSndBankVoice[iSlot] != 0) {
      BrX10072580(iSlot);
    }
    pVoice = DAT_100b55f8[iGroup * 0x12 + iSlot];
    g_aBrSndBankVoice[iSlot] = (void *)pVoice;
    return pVoice != 0;
  }
  return 1;
}

typedef int (__stdcall *dsbuf_fn2i)(int, int);
typedef int (__stdcall *dsbuf_fn4i)(int, int, int, int);

/* WHAT IT DOES: start a voice's buffer.  If the buffer is already playing
 * (GetStatus, vtable +0x24, reports DSBSTATUS_PLAYING) it is rewound instead
 * -- SetCurrentPosition(0), vtable +0x34 -- so retriggering a live sound
 * restarts it rather than layering a second Play on it.  Otherwise Play
 * (vtable +0x30) runs, looping iff the voice's +0x18 flag is set, and the
 * voice's "playing" flag at +0x1c is raised only when Play returns S_OK. */
/* @implements 0x1006B970 glide BrSndVoiceBufStart */

void BrSndVoiceBufStart(int param_1)

{
  unsigned int status;
  int          bLoop;

  status = 0;
  bLoop  = 0;
  if (*(int *)(param_1 + 0x18) != 0) {
    bLoop = 1;
  }
  if (((*(dsbuf_fn2i *)(**(int **)(param_1 + 0x9c) + 0x24))
         (*(int *)(param_1 + 0x9c), (int)&status) == 0) && ((status & 1) == 1)) {
    (*(dsbuf_fn2i *)(**(int **)(param_1 + 0x9c) + 0x34))
      (*(int *)(param_1 + 0x9c), 0);
    return;
  }
  if ((*(dsbuf_fn4i *)(**(int **)(param_1 + 0x9c) + 0x30))
        (*(int *)(param_1 + 0x9c), 0, 0, bLoop) == 0) {
    *(int *)(param_1 + 0x1c) = 1;
  }
  return;
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
  int    pVoice;

  if (BrSndG0B5DE8 == 0) {
    return 1;
  }
  if (BrSndPDS == 0) {
    return 1;
  }
  if (BrSndG18290FC == 0) {
    return 1;
  }
  ppVoice = g_aBrSndBankVoice;
  do {
    pVoice = (int)*ppVoice;
    if (pVoice != 0) {
      (*(dsbuf_fn2 *)(**(int **)(pVoice + 0x9c) + 0x3c))
        (*(int *)(pVoice + 0x9c), -10000);
      (*(dsbuf_fn2 *)(**(int **)(pVoice + 0x9c) + 0x40))
        (*(int *)(pVoice + 0x9c), 0);
    }
    ppVoice = ppVoice + 1;
  } while ((int)ppVoice < (int)&g_aBrSndBankVoice[BR_SND_BANK_VOICES]);
  return 1;
}

/* WHAT IT DOES: set the volume on a DirectSound buffer and commit the change. */
/* @implements 0x1006B670 glide BrSndBufSetVolume */

int BrSndBufSetVolume(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (param_1 != 0) {
      *(int *)(param_1 + 0xc) = param_2;
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

int BrSndListAppend(int param_1,int param_2)
{
  *(int *)(param_2 + 0x1a8) = 0;
  *(int *)(param_2 + 0x1c) = 0;
  while (*(int *)(param_1 + 0x1a8) != 0) {
    param_1 = *(int *)(param_1 + 0x1a8);
  }
  *(int *)(param_1 + 0x1a8) = param_2;
  return 0;
}


#endif /* BR_MATCHING_BUILD */
