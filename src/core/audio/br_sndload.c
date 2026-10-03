/* br_sndload.c -- audio.
 *
 * Loading one sample file into a fresh voice record: allocate the record,
 * keep the path it came from, read the WAV through WINMM, create its
 * DirectSound buffer, link it onto the device's voice list and apply the
 * neutral level, the file's own rate and centre pan.
 *
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include <string.h>

#include <windows.h>

/* The voice record, 0x1AC bytes, GlobalAlloc'd.  slice1_08.h's BrSndVoice
 * names the same fields with the same offsets; only what this file touches
 * is laid out here, so the two headers' partial models never meet. */
typedef struct BrSndLoadVoice {
    void         *pData;        /* +0x000  sample bytes (GlobalAlloc'd) */
    uint32_t      nDataBytes;   /* +0x004 */
    uint32_t     *pFormat;      /* +0x008  WAVEFORMATEX* (GlobalAlloc'd) */
    uint32_t      f0C;          /* +0x00C  playback rate -> SetFrequency */
    int32_t       f10;          /* +0x010  pan, 400 = centre */
    int32_t       f14;          /* +0x014  volume, 400 = unity */
    int32_t       f18, f1C, f20, f24;
    int32_t       f28;          /* +0x028  selects the alternate desc flags */
    unsigned char pad2C[0x9C - 0x2C];
    void         *pBuf;         /* +0x09C  the IDirectSoundBuffer */
    int32_t       nameOff;      /* +0x0A0  index just past the last '\' */
    char          name[0x104];  /* +0x0A4  the path it was loaded from */
    struct BrSndLoadVoice *pNext; /* +0x1A8  singly-linked chain */
} BrSndLoadVoice;
typedef char br_assert_loadvoice_size[(sizeof(BrSndLoadVoice) == 0x1AC) ? 1 : -1];

/* The head of the WAVEFORMATEX the reader fills: the rate sits at +4. */
typedef struct BrWavFmtHead {
    uint16_t wFormatTag, nChannels;
    uint32_t nSamplesPerSec;
} BrWavFmtHead;

/* 0x10070280 (D3D 0x10076FA0) -- the WINMM RIFF/WAVE reader slice1_09.c
 * describes.  Fills the byte count and format block, hangs the sample data
 * off the voice, and returns 0 on success. */
extern int32_t BrWavLoad(const char *pszPath, uint32_t *pnDataBytes,
                         int32_t *pInfo, uint32_t **ppFormat,
                         BrSndLoadVoice *pVoice);

/* 0x1006B240 -- create the DirectSound buffer and upload the sample. */
extern int32_t BrSndVoiceCreate(BrSndLoadVoice *pVoice);
/* br_sndvoice.c -- 0x1006B3C0, 0x1006B490, 0x1006B420, 0x1006B400. */
extern int32_t BrSndListAppend(void *pHead, BrSndLoadVoice *pNode);
extern int32_t BrSndVoiceBufRelease(BrSndLoadVoice *pVoice);
extern void    BrSndVoiceApplyFreq(BrSndLoadVoice *pVoice);
extern void    BrSndVoiceApplyPan(BrSndLoadVoice *pVoice);
/* slice6_76.c -- 0x1006B440. */
extern void    BrSndVoiceApplyVolume(BrSndLoadVoice *pVoice);

/* 0x1184C2A8 -- the DirectSound object every loaded voice is chained off. */
extern int DAT_1184c2a8;

/* WHAT IT DOES: loads one sample file into a brand-new voice: allocates the
 * record, keeps the path (and where its file name starts), reads the WAV,
 * creates the DirectSound buffer, links the voice onto the device's list
 * and applies unity volume, the file's own rate and centre pan.  On any
 * failure it unwinds everything it allocated and hands back NULL. */
/* @implements 0x1006BC10 glide BrSndVoiceLoad */
BrSndLoadVoice *BrSndVoiceLoad(const char *pszPath)
{
    BrSndLoadVoice *pVoice;
    int32_t         info;
    char           *pSlash;

    pVoice = (BrSndLoadVoice *)GlobalLock(
        GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, sizeof(BrSndLoadVoice)));
    if (pVoice == NULL)
        goto fail;
    pVoice->pData   = NULL;
    pVoice->pFormat = NULL;
    pVoice->pBuf    = NULL;
    pVoice->f28     = 0;
    strcpy(pVoice->name, pszPath);
    if (BrWavLoad(pszPath, &pVoice->nDataBytes, &info, &pVoice->pFormat,
                  pVoice) != 0)
        goto fail;
    if (BrSndVoiceCreate(pVoice) != 0)
        goto fail;
    /* The label sits INSIDE the failure arm.  Every other shape was tried
     * and lays the success arm first: an || or && chain (either polarity,
     * either arm returning or not), goto-to-a-label-after-the-if, and two
     * source copies of the cleanup behind an inlined helper (VC5 merged
     * them but threaded the NULL test past the guard).  Only a lone
     * `if (x != 0) F else S` gives `je S` with F falling through, and only
     * a goto lets the three earlier failures land on that same F. */
    if (BrSndListAppend(&DAT_1184c2a8, pVoice) != 0) {
fail:
        if (pVoice != NULL) {
            BrSndVoiceBufRelease(pVoice);
            if (pVoice->pFormat != NULL) {
                GlobalUnlock(GlobalHandle(pVoice->pFormat));
                GlobalFree(GlobalHandle(pVoice->pFormat));
            }
            if (pVoice->pData != NULL) {
                GlobalUnlock(GlobalHandle(pVoice->pData));
                GlobalFree(GlobalHandle(pVoice->pData));
            }
            GlobalUnlock(GlobalHandle(pVoice));
            GlobalFree(GlobalHandle(pVoice));
            pVoice = NULL;
        }
    } else {
        pSlash = strrchr(pVoice->name, '\\');
        if (pSlash != NULL)
            pVoice->nameOff = strrchr(pVoice->name, '\\') - pVoice->name + 1;
        else
            pVoice->nameOff = 0;
        pVoice->f0C = ((BrWavFmtHead *)pVoice->pFormat)->nSamplesPerSec;
        pVoice->f10 = 400;
        pVoice->f14 = 400;
        BrSndVoiceApplyVolume(pVoice);
        BrSndVoiceApplyFreq(pVoice);
        BrSndVoiceApplyPan(pVoice);
    }
    return pVoice;
}

/* ==========================================================================
 * 0x100701B0 -- the WINMM byte reader BrWavLoad uses for the sample data.
 * ========================================================================== */
#include <mmsystem.h>

/* WHAT IT DOES: reads up to n bytes of the current RIFF chunk into the
 * caller's buffer straight out of WINMM's I/O buffer, refilling it with
 * mmioAdvance as it empties, and takes what it read off the chunk's
 * remaining size.  Reports how many bytes it stored; a 0xE103 (end of
 * file) with nothing stored if the file ran out, or the WINMM error. */
/* @t4-pass 0x100701B0 1 2026-09-07 probes 122 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100701B0 2 2026-09-07 probes 92 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100701B0 3 2026-09-07 probes 93 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100701B0 4 2026-09-09 probes 10 bytes 213 insns 86 regions 5 rows 4 census no  (hand, fn.py variants: copy/loop/guard/decl spellings, all inert or worse) */
/* @t4-pass 0x100701B0 5 2026-09-09 probes 10 bytes 213 insns 86 regions 5 rows 4 census yes  (hand, fn.py variants: operand orders, casts, index forms, all inert; corpus query at +0xb0) */
/* @implements 0x100701B0 glide BrWavReadData */
unsigned int BrWavReadData(HMMIO hmmio, unsigned int n, char *pDst,
                           MMCKINFO *pCk, unsigned int *pnRead)
{
    MMIOINFO     info;
    int          rc;
    unsigned int i, nIn;

    /* The DirectX SDK sample's WaveReadFile (wave.c), as the game linked it,
     * including its precedence slip: `rc = mmioGetInfo(...) != 0` stores
     * the comparison, not the result.  0xE103 is the sample's
     * ER_CORRUPTWAVEFILE.  The ERROR_CANNOT_READ / FINISHED_READING label
     * pair is what puts the end-of-file exit last and returns
     * mmioSetInfo's own zero on success. */

    if (rc = mmioGetInfo(hmmio, &info, 0) != 0) {
        goto fail;
    }

    nIn = n;
    if (nIn > pCk->cksize)
        nIn = pCk->cksize;

    pCk->cksize -= nIn;

    for (i = 0; i < nIn; i++) {
        if (info.pchNext == info.pchEndRead) {
            if ((rc = mmioAdvance(hmmio, &info, 0)) != 0) {
                goto fail;
            }
            if (info.pchNext == info.pchEndRead) {
                rc = 0xe103;
                goto fail;
            }
        }
        pDst[i] = *info.pchNext++;
    }

    if ((rc = mmioSetInfo(hmmio, &info, 0)) != 0) {
        goto fail;
    }

    *pnRead = nIn;
    goto done;

fail:
    *pnRead = 0;

done:
    return rc;
}

/* ==========================================================================
 * 0x10070280 -- the RIFF/WAVE loader itself.
 * ========================================================================== */
extern unsigned int FUN_1006ffc0(const char *, int *, int *, MMCKINFO *); /* 0x1006FFC0 open + fmt */
extern int BrWaveSeekData(int *, MMCKINFO *, MMCKINFO *);               /* 0x10070170 */

/* WHAT IT DOES: loads a .WAV file for a voice: opens it and reads its
 * format block, seeks to the sample data, allocates a block for it and
 * reads it in, then closes the file.  Reports 0 with the byte count and
 * the format handed back; on any failure it frees whatever it had
 * allocated (the data block and the format block) and returns the error,
 * 0xE000 when the allocation itself failed.
 *
 * Two argument SLOTS are reused as locals, exactly as the original does:
 * the voice argument's slot holds the file handle and the format
 * argument's slot receives the byte count the reader hands back -- VC5's
 * own packing once both parameters live in registers. */
/* @t4-pass 0x10070280 1 2026-09-07 probes 89 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10070280 2 2026-09-07 probes 100 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10070280 3 2026-09-07 probes 84 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10070280 4 2026-09-09 probes 10 bytes 229 insns 86 regions 4 rows 0 census no  (hand, fn.py variants: copy-init order/statement forms, decl orders, literal spellings, all inert or worse) */
/* @t4-pass 0x10070280 5 2026-09-09 probes 10 bytes 229 insns 86 regions 4 rows 0 census yes  (hand, fn.py variants: TU position sweep -- both other slots inert -- plus name-swap allocation-hint and cast respellings, all inert) */
/* @implements 0x10070280 glide BrWavLoad */
int32_t BrWavLoad(const char *pszPath, uint32_t *pnDataBytes,
                  int32_t *pInfo, uint32_t **ppFormat, BrSndLoadVoice *pVoice)
{
    HMMIO    hmmio;
    MMCKINFO ckRiff;
    MMCKINFO ckData;
    int32_t  rc;
    uint32_t nRead;

    /* The DirectX SDK sample's WaveLoadFile (wave.c), as the game linked it:
     * the voice's pData is the sample's ppbData, and the cleanup is the
     * sample's ERROR_LOADING / DONE_LOADING label pair.  VC5 enregisters the
     * parameters and parks hmmio and nRead in their dead argument slots. */

    pVoice->pData = NULL;
    *ppFormat = NULL;
    *pnDataBytes = 0;

    if ((rc = FUN_1006ffc0(pszPath, (int *)&hmmio, (int *)ppFormat, &ckRiff)) != 0) {
        goto fail;
    }
    if ((rc = BrWaveSeekData((int *)&hmmio, &ckData, &ckRiff)) != 0) {
        goto fail;
    }
    if ((pVoice->pData = GlobalAlloc(0, ckData.cksize)) == NULL) {
        rc = 0xe000;
        goto fail;
    }
    if ((rc = BrWavReadData(hmmio, ckData.cksize, (char *)pVoice->pData, &ckData,
                            (unsigned int *)&nRead)) != 0) {
        goto fail;
    }
    *pnDataBytes = nRead;
    goto done;

fail:
    if (pVoice->pData != NULL) {
        GlobalFree(pVoice->pData);
        pVoice->pData = NULL;
    }
    if (*ppFormat != NULL) {
        GlobalFree(*ppFormat);
        *ppFormat = NULL;
    }

done:
    if (hmmio != NULL) {
        mmioClose(hmmio, 0);
        hmmio = NULL;
    }
    return rc;
}




/* WaveOpenFile (DX5 wave.c, CSE'd PCM/extra alloc). Ghidra shredded
 * PCMWAVEFORMAT into 4 ints so mmioRead's HPSTR was only known to touch
 * the first dword: /O2 frame 0x18 vs orig 0x24. cbExtraBytes lives in
 * the dead pszFileName slot. Success returns the mmioAscend result, not
 * a fresh 0; cleanup nulls hmmio then stores it. */
/* WHAT IT DOES: open a .WAV file and get it ready to read -- walks the RIFF
 * chunks, checks it really is PCM audio, hands back the format description
 * and leaves the file positioned at the start of the samples. */
/* @implements 0x1006FFC0 glide FUN_1006ffc0 */
MMRESULT FUN_1006ffc0(const char *param_1,int *param_2,int *param_3,LPMMCKINFO param_4)

{
  int *piVar1;
  LPMMCKINFO pmmckiParent;
  HMMIO hmmio;
  LONG LVar2;
  int *puVar3;
  unsigned int uVar4;
  MMRESULT MVar5;
  PCMWAVEFORMAT pcmWaveFormat;
  MMCKINFO local_14;
  
  *param_3 = 0;
  hmmio = mmioOpenA((LPSTR)param_1,(LPMMIOINFO)0x0,0x10000);
  pmmckiParent = param_4;
  if (hmmio == 0) {
    MVar5 = 0xe100;
    goto LAB_10070133;
  }

    MVar5 = mmioDescend(hmmio,param_4,(MMCKINFO *)0x0,0);
    if (MVar5 == 0) {
      if ((pmmckiParent->ckid == 0x46464952) && (pmmckiParent->fccType == 0x45564157)) {
        local_14.ckid = 0x20746d66;
        MVar5 = mmioDescend(hmmio,&local_14,pmmckiParent,0x10);
        if (MVar5 != 0) goto LAB_10070133;
        if (local_14.cksize >= sizeof(PCMWAVEFORMAT)) {
          LVar2 = mmioRead(hmmio,(HPSTR)&pcmWaveFormat,sizeof(PCMWAVEFORMAT));
          if (LVar2 != (LONG)sizeof(PCMWAVEFORMAT)) {
            MVar5 = 0xe102;
            goto LAB_10070133;
          }
          if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM) {
            param_1 = (LPSTR)0x0;
          }
          else {
            LVar2 = mmioRead(hmmio,(HPSTR)&param_1,2);
            if (LVar2 != 2) {
              MVar5 = 0xe102;
              goto LAB_10070133;
            }
          }
          puVar3 = GlobalAlloc(0,((unsigned int)param_1 & 0xffff) + sizeof(WAVEFORMATEX));
          *param_3 = (int)puVar3;
          if (puVar3 == (int *)0x0) {
            MVar5 = 0xe000;
            goto LAB_10070133;
          }
          *(PCMWAVEFORMAT *)puVar3 = pcmWaveFormat;
          *(short *)(*param_3 + 0x10) = (short)param_1;
          if (((short)param_1 == 0) ||
             (uVar4 = mmioRead(hmmio,(HPSTR)(*param_3 + 0x12),(unsigned int)param_1 & 0xffff),
             uVar4 == ((unsigned int)param_1 & 0xffff))) {
            MVar5 = mmioAscend(hmmio,&local_14,0);
            if (MVar5 != 0) goto LAB_10070133;
            goto TEMPCLEANUP;

          }
        }
      }
      MVar5 = 0xe101;
    }
  
LAB_10070133: ;
  piVar1 = param_3;
  if ((HGLOBAL)*param_3 != (HGLOBAL)0x0) {
    GlobalFree((HGLOBAL)*param_3);
    *piVar1 = 0;
  }
  if (hmmio != (HMMIO)0x0) {
    mmioClose(hmmio,0);
    hmmio = (HMMIO)0x0;
  }
TEMPCLEANUP:
  *param_2 = (int)hmmio;
  return MVar5;
}
