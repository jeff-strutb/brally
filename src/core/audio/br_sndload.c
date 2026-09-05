/* br_sndload.c -- audio.
 *
 * Loading one sample file into a fresh voice record: allocate the record,
 * keep the path it came from, read the WAV through WINMM, create its
 * DirectSound buffer, link it onto the device's voice list and apply the
 * neutral level, the file's own rate and centre pan.
 *
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
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
 * the voice argument's slot holds the file handle once the voice pointer
 * has been copied out, and the format argument's slot receives the byte
 * count the reader hands back. */
/* @implements 0x10070280 glide BrWavLoad */
int32_t BrWavLoad(const char *pszPath, uint32_t *pnDataBytes,
                  int32_t *pInfo, uint32_t **ppFormat, BrSndLoadVoice *pVoice)
{
    BrSndLoadVoice *pv    = pVoice;
    uint32_t      **ppFmt = ppFormat;
    int32_t         rc;
    MMCKINFO        ckData;
    MMCKINFO        ckRiff;

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
    rc = FUN_1006ffc0(pszPath, (int *)&pVoice, (int *)ppFmt, &ckRiff);
    if (rc == 0) {
        rc = BrWaveSeekData((int *)&pVoice, &ckData, &ckRiff);
        if (rc == 0) {
            pv->pData = GlobalAlloc(0, ckData.cksize);
            if (pv->pData == 0) {
                rc = 0xe000;
            } else {
                rc = BrWavReadData((HMMIO)pVoice, ckData.cksize,
                                   (char *)pv->pData, &ckData,
                                   (unsigned int *)&ppFormat);
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
        *pnDataBytes = *(uint32_t *)&ppFormat;
    }
    if ((HMMIO)pVoice != 0) {
        mmioClose((HMMIO)pVoice, 0);
    }
    return rc;
}

#endif /* BR_MATCHING_BUILD */
