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

#endif /* BR_MATCHING_BUILD */
