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
#include "slice1_08.h"
#include <string.h>

#include <windows.h>

/* The voice record, 0x1AC bytes, GlobalAlloc'd.  slice1_08.h's BrSndVoice
 * names the same fields with the same offsets; only what this file touches
 * is laid out here, so the two headers' partial models never meet. */
typedef struct BrSndVoice BrSndLoadVoice;   /* the same object as BrSndVoice */


/* The head of the WAVEFORMATEX the reader fills: the rate sits at +4. */
typedef struct BrWavFmtHead {
    uint16_t wFormatTag, nChannels;
    uint32_t nSamplesPerSec;
} BrWavFmtHead;

/* 0x10070280 (D3D 0x10076FA0) -- the WINMM RIFF/WAVE reader slice1_09.c
 * describes.  Fills the byte count and format block, hangs the sample data
 * off the voice, and returns 0 on success. */
/* BrWavLoad: prototype in br_funcs.h */

/* 0x1006B240 -- create the DirectSound buffer and upload the sample. */
/* BrSndVoiceCreate: prototype in br_funcs.h */
/* br_sndvoice.c -- 0x1006B3C0, 0x1006B490, 0x1006B420, 0x1006B400. */
/* BrSndListAppend: prototype in br_funcs.h */
/* BrSndVoiceBufRelease: prototype in br_funcs.h */
/* BrSndVoiceApplyFreq: prototype in br_funcs.h */
/* BrSndVoiceApplyPan: prototype in br_funcs.h */
/* slice6_76.c -- 0x1006B440. */
/* BrSndVoiceApplyVolume: prototype in br_funcs.h */

/* 0x1184C2A8 -- the DirectSound object every loaded voice is chained off. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

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
    strcpy((*(char (*)[260])&pVoice->name[0]), pszPath);
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
    if (BrSndListAppend(&g_BrSndPrimary, (BrSndVoice *)pVoice) != 0) {
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
        pSlash = strrchr((*(char (*)[260])&pVoice->name[0]), '\\');
        if (pSlash != NULL)
            pVoice->nameOff = strrchr((*(char (*)[260])&pVoice->name[0]), '\\') - (*(char (*)[260])&pVoice->name[0]) + 1;
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
/* @t4-pass 0x100701B0 1 2026-09-07 probes 122 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100701B0 2 2026-09-07 probes 92 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100701B0 3 2026-09-07 probes 93 bytes 207 insns 85 regions 3 rows 1 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100701B0 4 2026-09-09 probes 10 bytes 213 insns 86 regions 5 rows 4 census no  (hand, fn.py variants: copy/loop/guard/decl spellings, all inert or worse) */
/* @t4-pass 0x100701B0 5 2026-09-09 probes 10 bytes 213 insns 86 regions 5 rows 4 census yes  (hand, fn.py variants: operand orders, casts, index forms, all inert; corpus query at +0xb0) */
/* @t3 0x100701B0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 213/205 insns 86/84 rows 1+3 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 5 zero-movement 4 5
 * residue is the EOF-block layout fork: the original lays the end-of-file
 * exit last and closes the loop with a bottom jb into mmioSetInfo; VC5
 * places it after the loop, turning the exit into jae/jmp (cancelled as
 * the either-or layout triple) and rematerialising the success zero
 * (xor singleton).  Dead list in the RESIDUE block below.
 * Do not reopen before the end-grind. */
/* @implements 0x100701B0 glide BrWavReadData */
unsigned int BrWavReadData(HMMIO hmmio, unsigned int n, char *pDst,
                           MMCKINFO *pCk, unsigned int *pnRead)
{
    MMIOINFO     info;
    unsigned int rc;
    unsigned int left;
    unsigned int i;

    /* RESIDUE (T2, 8 B long, RAW 9+7, REGNORM 3+1): the end-of-file exit.
     * The original lays that block LAST (0x100702B6) and closes the loop
     * with a bottom `jb` falling into mmioSetInfo; VC5 puts it right after
     * the loop, so the loop exit becomes `jae`/`jmp`, and it materialises
     * the success return with `xor eax,eax` where the original returns
     * mmioSetInfo's eax untouched.  PROBED AND DEAD, do not re-run: the EOF
     * return inline in the loop, as a `goto` to a trailing label, the
     * success `return rc` vs `return 0`, the fail arm as then-arm of the
     * last test vs a trailing label, `if (rc == 0) {success}` first.
     * What IS settled: the two unsigned tests are `n > left` and `n > 0`
     * (`left < n` / `n != 0` encode `jae`/`jne` for the original's
     * `jbe`/`jbe`); the fail arm is the THEN arm of the mmioSetInfo test
     * with the two earlier failures jumping into it. */
    rc = (mmioGetInfo(hmmio, &info, 0) != 0);
    if (rc != 0) {
        goto fail;
    }
    left = pCk->cksize;
    if (n > left) {
        n = left;
    }
    i = 0;
    pCk->cksize = left - n;
    if (n > 0) {
        do {
            if (info.pchNext == info.pchEndRead) {
                rc = mmioAdvance(hmmio, &info, 0);
                if (rc != 0) {
                    goto fail;
                }
                if (info.pchNext == info.pchEndRead) {
                    /* inline: VC5 hoists a `return` inside a loop to the
                     * END of the function, which is where the original has
                     * it; spelled as a `goto` to a trailing label the block
                     * lands right after the loop instead */
                    *pnRead = 0;
                    return 0xe103;
                }
            }
            pDst[i] = *info.pchNext++;
            i++;
        } while (i < n);
    }
    rc = mmioSetInfo(hmmio, &info, 0);
    if (rc != 0) {
fail:
        *pnRead = 0;
        return rc;
    }
    *pnRead = n;
    return rc;          /* the zero mmioSetInfo left in eax, not a fresh 0 */
}

/* ==========================================================================
 * 0x10070280 -- the RIFF/WAVE loader itself.
 * ========================================================================== */
/* FUN_1006ffc0: prototype in br_funcs.h */
/* BrWaveSeekData: prototype in br_funcs.h */

/* WHAT IT DOES: loads a .WAV file for a voice: opens it and reads its
 * format block, seeks to the sample data, allocates a block for it and
 * reads it in, then closes the file.  Reports 0 with the byte count and
 * the format handed back; on any failure it frees whatever it had
 * allocated (the data block and the format block) and returns the error,
 * 0xE000 when the allocation itself failed.
 *
 * Two argument SLOTS are reused as locals, exactly as the original does:
 * the voice argument's slot holds the file handle once the voice pointer
 * has been copied out, and the format argument's slot receives the byte
 * count the reader hands back. */
/* @t4-pass 0x10070280 1 2026-09-07 probes 89 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10070280 2 2026-09-07 probes 100 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10070280 3 2026-09-07 probes 84 bytes 227 insns 86 regions 4 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10070280 4 2026-09-09 probes 10 bytes 229 insns 86 regions 4 rows 0 census no  (hand, fn.py variants: copy-init order/statement forms, decl orders, literal spellings, all inert or worse) */
/* @t4-pass 0x10070280 5 2026-09-09 probes 10 bytes 229 insns 86 regions 4 rows 0 census yes  (hand, fn.py variants: TU position sweep -- both other slots inert -- plus name-swap allocation-hint and cast respellings, all inert) */
/* @t3 0x10070280 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 229/229 insns 86/86 rows 0+0 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 5 zero-movement 4 5
 * residue is register colouring only: the whole-body ebx/esi transposition
 * of the two argument copies (RAW 14+14, REGNORM 0+0, size-exact); the
 * dead list is in the RESIDUE block below plus the position/name-swap
 * sweep in pass 5.  Three crank census passes at the pre-fix numbers and
 * two hand passes at the current ones.
 * Do not reopen before the end-grind. */
/* @implements 0x10070280 glide BrWavLoad */
int32_t BrWavLoad(const char *pszPath, uint32_t *pnDataBytes,
                  int32_t *pInfo, uint32_t **ppFormat, BrSndLoadVoice *pVoice)
{
    BrSndLoadVoice *pv    = pVoice;
    uint32_t      **ppFmt = ppFormat;
    int32_t         rc;
    MMCKINFO        ckData;
    MMCKINFO        ckRiff;
    /* The original keeps the open file in pVoice's argument slot and the
     * byte count in ppFormat's: a pointer-sized slot that a 32-bit handle
     * or count only half fills here, so each gets a local of its own. */
    HMMIO           hmmio = 0;
    unsigned int    nRead = 0;

    (void)pInfo;
    /* RESIDUE (T2, 43 masked B, RAW 14+14, REGNORM 0+0, size-exact): the
     * two register copies come out swapped -- the original holds the
     * format pointer in ebx and the voice in esi, VC5 the reverse.  Every
     * instruction is otherwise identical.  PROBED AND DEAD, do not re-run:
     * declaration order of the two copies, the order of the two zeroing
     * stores (+2 B), an allocation temp in place of re-reading pv->pData.
     * What IS settled: every value read goes through the copies (taking
     * `&pVoice`/`&ppFormat` pins the arguments to their slots, a parameter
     * read is a reload); the cleanup is `if (rc != 0) {free} else {store
     * count}` after ONE joined test, not a goto past it (that moves the
     * count store to the end and the read arm out of line); the alloc
     * failure threads straight to the cleanup. */
    pv->pData    = 0;
    *ppFmt       = 0;
    *pnDataBytes = 0;
    rc = FUN_1006ffc0(pszPath, &hmmio, (void **)ppFmt, &ckRiff);
    if (rc == 0) {
        rc = BrWaveSeekData(&hmmio, &ckData, &ckRiff);
        if (rc == 0) {
            pv->pData = GlobalAlloc(0, ckData.cksize);
            if (pv->pData == 0) {
                rc = 0xe000;
            } else {
                rc = BrWavReadData(hmmio, ckData.cksize,
                                   (char *)pv->pData, &ckData, &nRead);
            }
        }
    }
    if (rc != 0) {
        if (pv->pData != 0) {
            GlobalFree(pv->pData);
            pv->pData = 0;
        }
        if (*ppFmt != 0) {
            GlobalFree(*ppFmt);
            *ppFmt = 0;
        }
    } else {
        *pnDataBytes = nRead;
    }
    if (hmmio != 0) {
        mmioClose(hmmio, 0);
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
MMRESULT FUN_1006ffc0(const char *param_1,HMMIO *param_2,void **param_3,LPMMCKINFO param_4)

{
  void **piVar1;
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
          *param_3 = puVar3;
          if (puVar3 == (int *)0x0) {
            MVar5 = 0xe000;
            goto LAB_10070133;
          }
          *(PCMWAVEFORMAT *)puVar3 = pcmWaveFormat;
          *(short *)((char *)*param_3 + 0x10) = (short)param_1;
          if (((short)param_1 == 0) ||
             (uVar4 = mmioRead(hmmio,(HPSTR)((char *)*param_3 + 0x12),(unsigned int)param_1 & 0xffff),
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
  *param_2 = hmmio;
  return MVar5;
}
