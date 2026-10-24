/* br_sndload.c -- audio.
 *
 * Loading one sample file into a fresh voice record: allocate the record,
 * keep the path it came from, read the WAV, create its sound, link it onto the device's voice list and apply the
 * neutral level, the file's own rate and centre pan.
 *
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include "slice1_08.h"
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

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

    pVoice = (BrSndLoadVoice *)calloc(1, sizeof(BrSndLoadVoice));
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
                free(pVoice->pFormat);
            }
            if (pVoice->pData != NULL) {
                free(pVoice->pData);
            }
            free(pVoice);
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
 * 0x10070280 -- the RIFF/WAVE loader.
 * ========================================================================== */
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* the chunk `id` among [pos, end) of the file's bytes: its data offset and
 * size, or 0 (a chunk's data is padded to an even length) */
static int wav_find(const uint8_t *f, uint32_t pos, uint32_t end, uint32_t id,
                    uint32_t *pData, uint32_t *pSize)
{
    while (pos + 8 <= end) {
        uint32_t size = rd32(f + pos + 4);
        if (rd32(f + pos) == id) {
            *pData = pos + 8;
            *pSize = size;
            return 1;
        }
        pos += 8 + ((size + 1) & ~1u);
    }
    return 0;
}

/* WHAT IT DOES: loads a .WAV file for a voice: checks it is a RIFF WAVE,
 * reads its format block (the 16 bytes of a PCM one, and for any other
 * format its cbSize and that many extra bytes) into a block of its own,
 * then the sample data into a block hung off the voice.  Reports 0 with
 * the byte count and the format handed back; on any failure it frees what
 * it had allocated and returns the original's error: 0xE100 no such file,
 * 0xE101 not a RIFF WAVE or a short format chunk, 0xE102 the file ends in
 * the format, 0xE000 no memory, 0xE103 the file ends in the samples,
 * 0x109 a chunk that is not there. */
/* @implements 0x10070280 glide BrWavLoad */
int32_t BrWavLoad(const char *pszPath, uint32_t *pnDataBytes,
                  int32_t *pInfo, uint32_t **ppFormat, BrSndLoadVoice *pVoice)
{
    FILE     *fp;
    uint8_t  *f = NULL;
    long      n;
    uint32_t  size, riffEnd, fmtAt, fmtSize, dataAt, dataSize, extra = 0;
    BrWavFmt *pFmt = NULL;
    int32_t   rc;

    (void)pInfo;
    pVoice->pData = 0;
    *ppFormat     = 0;
    *pnDataBytes  = 0;
    if ((fp = fopen(pszPath, "rb")) == NULL)
        return 0xe100;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    size = n > 0 ? (uint32_t)n : 0;
    f = (uint8_t *)malloc(size ? size : 1);
    if (f == NULL || fread(f, 1, size, fp) != size) {
        fclose(fp);
        free(f);
        return 0xe100;
    }
    fclose(fp);

    rc = 0x109;                                 /* MMIOERR_CHUNKNOTFOUND */
    if (size < 8)
        goto done;
    rc = 0xe101;
    if (size < 12 || rd32(f) != 0x46464952 || rd32(f + 8) != 0x45564157)   /* "RIFF" ... "WAVE" */
        goto done;
    riffEnd = 8 + rd32(f + 4);
    if (riffEnd > size)
        riffEnd = size;
    rc = 0x109;
    if (!wav_find(f, 12, riffEnd, 0x20746d66, &fmtAt, &fmtSize))          /* "fmt " */
        goto done;
    rc = 0xe101;
    if (fmtSize < BR_WAV_FMT_PCM_BYTES)
        goto done;
    rc = 0xe102;
    if (fmtAt + BR_WAV_FMT_PCM_BYTES > size)
        goto done;
    if ((f[fmtAt] | f[fmtAt + 1] << 8) != 1) {                            /* not PCM: cbSize follows */
        if (fmtAt + BR_WAV_FMT_PCM_BYTES + 2 > size)
            goto done;
        extra = (uint32_t)(f[fmtAt + 16] | f[fmtAt + 17] << 8);
    }
    rc = 0xe000;
    if ((pFmt = (BrWavFmt *)malloc(sizeof(BrWavFmt) + extra)) == NULL)
        goto done;
    memcpy(pFmt, f + fmtAt, BR_WAV_FMT_PCM_BYTES);
    pFmt->cbSize = (uint16_t)extra;
    rc = 0xe101;
    if (extra != 0) {
        if (fmtAt + BR_WAV_FMT_PCM_BYTES + 2 + extra > size)
            goto done;
        memcpy(pFmt + 1, f + fmtAt + BR_WAV_FMT_PCM_BYTES + 2, extra);
    }
    rc = 0x109;
    if (!wav_find(f, 12, riffEnd, 0x61746164, &dataAt, &dataSize))        /* "data" */
        goto done;
    rc = 0xe000;
    if ((pVoice->pData = malloc(dataSize)) == NULL)
        goto done;
    rc = 0xe103;
    if (dataAt + dataSize > size)
        goto done;
    memcpy(pVoice->pData, f + dataAt, dataSize);
    rc = 0;
    *ppFormat     = (uint32_t *)pFmt;
    *pnDataBytes  = dataSize;
done:
    if (rc != 0) {
        free(pVoice->pData);
        pVoice->pData = 0;
        free(pFmt);
    }
    free(f);
    return rc;
}
