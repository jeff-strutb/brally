/* br_mix.c -- software mixer standing in for DirectSound.  See br_mix.h for
 * why the seam is the vtable slice1_08.c already calls through, and for the
 * DirectSound semantics reproduced.
 *
 * Nothing in this file computes a pitch, a pan or a volume.  It receives a
 * frequency in hertz and two attenuations in hundredths of a decibel and
 * turns them into samples.
 */
#include "br_mix.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * objects
 * ---------------------------------------------------------------------- */

typedef struct BrMixBuf {
    /* MUST be first: the port holds this as a BrDSBuffer* and calls through
     * pVtbl at offset 0, exactly as it did the COM object. */
    const BrDSBufferVtbl *pVtbl;

    BrMix           *pMix;
    struct BrMixBuf *pNext;        /* the mixer's list of live buffers */

    uint8_t         *pPcm;         /* dwBufferBytes, filled by Lock/Unlock */
    uint32_t         cbPcm;
    uint32_t         cFrames;

    BrMixWaveFormat  fmt;

    uint32_t         freq;         /* SetFrequency; 0 == the buffer's own    */
    int32_t          pan;          /* SetPan, 1/100 dB                       */
    int32_t          vol;          /* SetVolume, 1/100 dB                    */
    int              playing;
    int              looping;
    double           pos;          /* play cursor, in source frames          */
} BrMixBuf;

struct BrMix {
    BrDSound   dev;                /* first: BrSndPDS points here            */
    BrMixBuf  *pHead;
    int        cBufs;
    long       cClipped;
};

/* ------------------------------------------------------------------------
 * DirectSound's gain law
 * ---------------------------------------------------------------------- */

double BrMixGainFromCentibels(int32_t cB)
{
    if (cB <= BR_MIX_VOLUME_MIN)
        return 0.0;                /* DSBVOLUME_MIN is silence, exactly     */
    if (cB >= 0)
        return 1.0;                /* DirectSound never amplifies           */
    return pow(10.0, (double)cB / 2000.0);
}

/* cB < 0 leans LEFT, so it is the RIGHT channel that is attenuated.  This is
 * the convention that makes BrSndVoiceApplyPan's (f10-400)*10 -- negative for
 * f10 below the 400 centre -- come out of the left speaker. */
static void pan_gains(int32_t pan, double *pL, double *pR)
{
    if (pan < BR_MIX_PAN_LEFT)  pan = BR_MIX_PAN_LEFT;
    if (pan > BR_MIX_PAN_RIGHT) pan = BR_MIX_PAN_RIGHT;

    *pL = (pan > 0) ? BrMixGainFromCentibels(-pan) : 1.0;
    *pR = (pan < 0) ? BrMixGainFromCentibels( pan) : 1.0;
}

/* ------------------------------------------------------------------------
 * sample access
 * ---------------------------------------------------------------------- */

/* One source sample in [-1, 1].  8-bit PCM in a .wav is UNSIGNED with 128 as
 * silence; 16-bit is signed.  Both spellings are on the disc (beep.wav and
 * the rn_* set are 8-bit; the engine loops are 16-bit). */
static double src_sample(const BrMixBuf *b, uint32_t frame, int ch)
{
    uint32_t i;

    if (frame >= b->cFrames)
        return 0.0;
    if (ch >= b->fmt.nChannels)
        ch = 0;                    /* mono source feeds both outputs        */

    if (b->fmt.wBitsPerSample == 8) {
        i = frame * b->fmt.nChannels + (uint32_t)ch;
        return ((double)b->pPcm[i] - 128.0) / 128.0;
    } else {
        const int16_t *p = (const int16_t *)(const void *)b->pPcm;
        i = frame * b->fmt.nChannels + (uint32_t)ch;
        return (double)p[i] / 32768.0;
    }
}

/* Linearly interpolated fetch at a fractional cursor.  The original mixer was
 * DirectSound's, whose resampler on a software buffer was point-sampling on
 * some drivers and linear on others; linear is chosen here for the same
 * reason tools/xm_render.c chose it -- point-sampling would bake aliasing
 * permanently into any rendered file. */
static double src_lerp(const BrMixBuf *b, double pos, int ch)
{
    double   f  = floor(pos);
    double   t  = pos - f;
    uint32_t i0 = (uint32_t)f;
    uint32_t i1 = i0 + 1;

    if (i1 >= b->cFrames)
        i1 = b->looping ? 0 : i0;

    return src_sample(b, i0, ch) * (1.0 - t) + src_sample(b, i1, ch) * t;
}

/* ------------------------------------------------------------------------
 * the mix
 * ---------------------------------------------------------------------- */

void BrMixRender(BrMix *pMix, int16_t *pDst, int cFrames)
{
    BrMixBuf *b;
    int       i;

    if (pDst == NULL || cFrames <= 0)
        return;
    memset(pDst, 0, (size_t)cFrames * BR_MIX_CHANNELS * sizeof(int16_t));
    if (pMix == NULL)
        return;

    for (b = pMix->pHead; b != NULL; b = b->pNext) {
        double rate, step, gVol, gL, gR;

        if (!b->playing || b->cFrames == 0 || b->pPcm == NULL)
            continue;

        /* SetFrequency(0) is DSBFREQUENCY_ORIGINAL. */
        rate = (b->freq != 0) ? (double)b->freq : (double)b->fmt.nSamplesPerSec;
        step = rate / (double)BR_MIX_RATE;
        if (!(step > 0.0))
            continue;

        gVol = BrMixGainFromCentibels(b->vol);
        pan_gains(b->pan, &gL, &gR);

        for (i = 0; i < cFrames; ++i) {
            long acc;
            double l, r;

            if (b->pos >= (double)b->cFrames) {
                if (b->looping) {
                    b->pos = fmod(b->pos, (double)b->cFrames);
                } else {
                    /* A non-looping DirectSound buffer that reaches its end
                     * stops and leaves the cursor at the start -- which is
                     * what lets 0x1006BA60's one-shots fire more than once,
                     * since BrSndVoiceStart does NOT rewind on the
                     * not-currently-playing path. */
                    b->playing = 0;
                    b->pos     = 0.0;
                    break;
                }
            }

            l = src_lerp(b, b->pos, 0) * gVol * gL;
            r = src_lerp(b, b->pos, 1) * gVol * gR;
            b->pos += step;

            acc = (long)pDst[i * 2] + (long)(l * 32767.0);
            if (acc >  32767) { acc =  32767; pMix->cClipped++; }
            if (acc < -32768) { acc = -32768; pMix->cClipped++; }
            pDst[i * 2] = (int16_t)acc;

            acc = (long)pDst[i * 2 + 1] + (long)(r * 32767.0);
            if (acc >  32767) { acc =  32767; pMix->cClipped++; }
            if (acc < -32768) { acc = -32768; pMix->cClipped++; }
            pDst[i * 2 + 1] = (int16_t)acc;
        }
    }
}

/* ------------------------------------------------------------------------
 * IDirectSoundBuffer
 * ---------------------------------------------------------------------- */

static void buf_unlink(BrMixBuf *b)
{
    BrMixBuf **pp;

    if (b->pMix == NULL)
        return;
    for (pp = &b->pMix->pHead; *pp != NULL; pp = &(*pp)->pNext) {
        if (*pp == b) {
            *pp = b->pNext;
            b->pMix->cBufs--;
            break;
        }
    }
    b->pMix = NULL;
}

static int32_t mix_Release(BrDSBuffer *pThis)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    buf_unlink(b);
    free(b->pPcm);
    free(b);
    return 0;
}

static int32_t mix_GetCaps(BrDSBuffer *pThis, BrDSBCaps *pCaps)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    if (pCaps == NULL)
        return -1;
    /* dwFlags without DSBCAPS_LOCHARDWARE, so BrSndVoiceCreate records f24=0:
     * this is a software buffer and saying otherwise would be a lie the port
     * could act on. */
    pCaps->dwFlags              = 0;
    pCaps->dwBufferBytes        = b->cbPcm;
    pCaps->dwUnlockTransferRate = 0;
    pCaps->dwPlayCpuOverhead    = 0;
    return 0;
}

static int32_t mix_GetStatus(BrDSBuffer *pThis, uint32_t *pStatus)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    if (pStatus == NULL)
        return -1;
    *pStatus = b->playing ? BR_DSBSTATUS_PLAYING : 0u;
    return 0;
}

static int32_t mix_Lock(BrDSBuffer *pThis, uint32_t off, uint32_t cb,
                        void **pp1, uint32_t *pn1,
                        void **pp2, uint32_t *pn2, uint32_t flags)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    (void)flags;
    if (pp1 == NULL || pn1 == NULL)
        return -1;
    if (off > b->cbPcm || cb > b->cbPcm - off)
        return -1;

    /* One contiguous region: the second pointer is what a circular buffer
     * needs and this is not one.  BrSndVoiceCreate ignores nLock1 and copies
     * nDataBytes regardless (its documented GOTCHA), so the bound above is
     * the only thing standing between a malformed sample and an overrun. */
    *pp1 = b->pPcm + off;
    *pn1 = cb;
    if (pp2 != NULL) *pp2 = NULL;
    if (pn2 != NULL) *pn2 = 0;
    return 0;
}

static int32_t mix_Unlock(BrDSBuffer *pThis, void *p1, uint32_t n1,
                          void *p2, uint32_t n2)
{
    (void)pThis; (void)p1; (void)n1; (void)p2; (void)n2;
    return 0;
}

static int32_t mix_Play(BrDSBuffer *pThis, uint32_t r1, uint32_t r2,
                        uint32_t flags)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    (void)r1; (void)r2;
    b->looping = (flags & 1u) ? 1 : 0;     /* DSBPLAY_LOOPING */
    b->playing = 1;
    return 0;
}

static int32_t mix_Stop(BrDSBuffer *pThis)
{
    BrMixBuf *b = (BrMixBuf *)pThis;

    /* DirectSound keeps the play cursor across a Stop; only
     * SetCurrentPosition rewinds. */
    b->playing = 0;
    return 0;
}

static int32_t mix_SetCurrentPosition(BrDSBuffer *pThis, uint32_t off)
{
    BrMixBuf *b = (BrMixBuf *)pThis;
    uint32_t  align = b->fmt.nBlockAlign ? b->fmt.nBlockAlign : 1u;

    if (off > b->cbPcm)
        return -1;
    b->pos = (double)(off / align);
    return 0;
}

static int32_t mix_SetVolume(BrDSBuffer *pThis, int32_t cB)
{
    ((BrMixBuf *)pThis)->vol = cB;
    return 0;
}

static int32_t mix_SetPan(BrDSBuffer *pThis, int32_t cB)
{
    ((BrMixBuf *)pThis)->pan = cB;
    return 0;
}

static int32_t mix_SetFrequency(BrDSBuffer *pThis, uint32_t hz)
{
    ((BrMixBuf *)pThis)->freq = hz;
    return 0;
}

static const BrDSBufferVtbl s_bufVtbl = {
    NULL,                     /* QueryInterface      */
    NULL,                     /* AddRef              */
    mix_Release,
    mix_GetCaps,
    NULL,                     /* GetCurrentPosition  */
    NULL,                     /* GetFormat           */
    NULL,                     /* GetVolume           */
    NULL,                     /* GetPan              */
    NULL,                     /* GetFrequency        */
    mix_GetStatus,
    NULL,                     /* Initialize          */
    mix_Lock,
    mix_Play,
    mix_SetCurrentPosition,
    NULL,                     /* SetFormat           */
    mix_SetVolume,
    mix_SetPan,
    mix_SetFrequency,
    mix_Stop,
    mix_Unlock,
    NULL                      /* Restore             */
};

/* ------------------------------------------------------------------------
 * IDirectSound
 * ---------------------------------------------------------------------- */

static int32_t mix_CreateSoundBuffer(BrDSound *pThis, const BrDSBufferDesc *pDesc,
                                     BrDSBuffer **ppBuf, void *pUnk)
{
    BrMix                 *pMix = (BrMix *)pThis;
    const BrMixWaveFormat *pFmt;
    BrMixBuf              *b;

    (void)pUnk;
    if (ppBuf == NULL)
        return -1;
    *ppBuf = NULL;
    if (pDesc == NULL || pDesc->dwBufferBytes == 0)
        return -1;

    pFmt = (const BrMixWaveFormat *)pDesc->lpwfxFormat;
    if (pFmt == NULL || pFmt->wFormatTag != 1 || pFmt->nChannels == 0
        || pFmt->nSamplesPerSec == 0
        || (pFmt->wBitsPerSample != 8 && pFmt->wBitsPerSample != 16))
        return -1;

    b = (BrMixBuf *)calloc(1, sizeof(*b));
    if (b == NULL)
        return -1;
    b->pPcm = (uint8_t *)calloc(1, pDesc->dwBufferBytes);
    if (b->pPcm == NULL) {
        free(b);
        return -1;
    }

    b->pVtbl   = &s_bufVtbl;
    b->cbPcm   = pDesc->dwBufferBytes;
    b->fmt     = *pFmt;
    if (b->fmt.nBlockAlign == 0)
        b->fmt.nBlockAlign = (uint16_t)(b->fmt.nChannels * (b->fmt.wBitsPerSample / 8));
    b->cFrames = b->cbPcm / b->fmt.nBlockAlign;
    b->freq    = 0;                 /* DSBFREQUENCY_ORIGINAL until retuned  */

    b->pMix    = pMix;
    b->pNext   = pMix->pHead;
    pMix->pHead = b;
    pMix->cBufs++;

    *ppBuf = (BrDSBuffer *)b;
    return 0;
}

static const BrDSoundVtbl s_devVtbl = {
    NULL,                     /* QueryInterface */
    NULL,                     /* AddRef         */
    NULL,                     /* Release        */
    mix_CreateSoundBuffer
};

/* ------------------------------------------------------------------------
 * lifecycle
 * ---------------------------------------------------------------------- */

/* @n64 0x80256228 located */
BrMix *BrMixCreate(void)
{
    BrMix *pMix = (BrMix *)calloc(1, sizeof(*pMix));

    if (pMix == NULL)
        return NULL;
    pMix->dev.pVtbl = &s_devVtbl;
    return pMix;
}

void BrMixDestroy(BrMix *pMix)
{
    if (pMix == NULL)
        return;
    while (pMix->pHead != NULL)
        mix_Release((BrDSBuffer *)pMix->pHead);
    free(pMix);
}

BrDSound *BrMixDevice(BrMix *pMix)
{
    return (pMix != NULL) ? &pMix->dev : NULL;
}

int BrMixPlayingCount(const BrMix *pMix)
{
    const BrMixBuf *b;
    int n = 0;

    if (pMix == NULL)
        return 0;
    for (b = pMix->pHead; b != NULL; b = b->pNext)
        if (b->playing) n++;
    return n;
}

int BrMixBufferCount(const BrMix *pMix)
{
    return (pMix != NULL) ? pMix->cBufs : 0;
}

long BrMixClipCount(const BrMix *pMix)
{
    return (pMix != NULL) ? pMix->cClipped : 0L;
}

/* ------------------------------------------------------------------------
 * RIFF
 * ---------------------------------------------------------------------- */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

int BrMixWavParse(const void *pv, size_t cb, BrMixWaveFormat *pFmt,
                  const void **ppPcm, uint32_t *pcbPcm)
{
    const uint8_t *p = (const uint8_t *)pv;
    size_t         off;
    int            haveFmt = 0, haveData = 0;

    if (p == NULL || pFmt == NULL || ppPcm == NULL || pcbPcm == NULL)
        return -1;
    if (cb < 12 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
        return -1;

    /* Chunk walk, mmioDescend/mmioAscend style.  RIFF pads odd-sized chunks
     * to an even boundary and the pad byte is NOT counted in the size, which
     * is the one detail a hand-rolled parser usually gets wrong. */
    off = 12;
    while (off + 8 <= cb) {
        uint32_t n = rd32(p + off + 4);
        size_t   body = off + 8;

        if (n > cb - body)
            n = (uint32_t)(cb - body);       /* truncated file: take what is there */

        if (memcmp(p + off, "fmt ", 4) == 0 && n >= 16) {
            pFmt->wFormatTag      = rd16(p + body + 0);
            pFmt->nChannels       = rd16(p + body + 2);
            pFmt->nSamplesPerSec  = rd32(p + body + 4);
            pFmt->nAvgBytesPerSec = rd32(p + body + 8);
            pFmt->nBlockAlign     = rd16(p + body + 12);
            pFmt->wBitsPerSample  = rd16(p + body + 14);
            haveFmt = 1;
        } else if (memcmp(p + off, "data", 4) == 0) {
            *ppPcm  = p + body;
            *pcbPcm = n;
            haveData = 1;
        }

        off = body + n + (n & 1u);
    }

    if (!haveFmt || !haveData)
        return -1;
    if (pFmt->wFormatTag != 1 || pFmt->nChannels == 0
        || pFmt->nSamplesPerSec == 0
        || (pFmt->wBitsPerSample != 8 && pFmt->wBitsPerSample != 16))
        return -1;
    if (pFmt->nBlockAlign == 0)
        pFmt->nBlockAlign = (uint16_t)(pFmt->nChannels * (pFmt->wBitsPerSample / 8));
    return 0;
}

int BrMixWavLoad(const char *pszPath, BrMixWaveFormat *pFmt,
                 void **ppPcm, uint32_t *pcbPcm)
{
    FILE       *f;
    long        n;
    uint8_t    *pFile;
    const void *pPcm;
    uint32_t    cbPcm;
    void       *pOut;

    if (pszPath == NULL || pFmt == NULL || ppPcm == NULL || pcbPcm == NULL)
        return -1;
    *ppPcm = NULL;
    *pcbPcm = 0;

    f = fopen(pszPath, "rb");
    if (f == NULL)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    n = ftell(f);
    if (n <= 12 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }

    pFile = (uint8_t *)malloc((size_t)n);
    if (pFile == NULL) { fclose(f); return -1; }
    if (fread(pFile, 1, (size_t)n, f) != (size_t)n) {
        free(pFile); fclose(f); return -1;
    }
    fclose(f);

    if (BrMixWavParse(pFile, (size_t)n, pFmt, &pPcm, &cbPcm) != 0 || cbPcm == 0) {
        free(pFile);
        return -1;
    }

    /* Hand back just the samples: the voice owns its pData and the RIFF
     * wrapper is not part of it. */
    pOut = malloc(cbPcm);
    if (pOut == NULL) { free(pFile); return -1; }
    memcpy(pOut, pPcm, cbPcm);
    free(pFile);

    *ppPcm  = pOut;
    *pcbPcm = cbPcm;
    return 0;
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

int BrMixWavWrite(const char *pszPath, const BrMixWaveFormat *pFmt,
                  const void *pvPcm, uint32_t cbPcm)
{
    uint8_t hdr[44];
    FILE   *f;
    uint16_t align;

    if (pszPath == NULL || pFmt == NULL || pvPcm == NULL)
        return -1;

    align = pFmt->nBlockAlign;
    if (align == 0)
        align = (uint16_t)(pFmt->nChannels * (pFmt->wBitsPerSample / 8));

    memcpy(hdr + 0, "RIFF", 4);
    wr32(hdr + 4, 36u + cbPcm);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    wr32(hdr + 16, 16u);
    wr16(hdr + 20, pFmt->wFormatTag);
    wr16(hdr + 22, pFmt->nChannels);
    wr32(hdr + 24, pFmt->nSamplesPerSec);
    wr32(hdr + 28, pFmt->nSamplesPerSec * align);
    wr16(hdr + 32, align);
    wr16(hdr + 34, pFmt->wBitsPerSample);
    memcpy(hdr + 36, "data", 4);
    wr32(hdr + 40, cbPcm);

    f = fopen(pszPath, "wb");
    if (f == NULL)
        return -1;
    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)
        || (cbPcm != 0 && fwrite(pvPcm, 1, cbPcm, f) != cbPcm)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

void BrMixOutputFormat(BrMixWaveFormat *pFmt)
{
    if (pFmt == NULL)
        return;
    pFmt->wFormatTag      = 1;
    pFmt->nChannels       = BR_MIX_CHANNELS;
    pFmt->nSamplesPerSec  = BR_MIX_RATE;
    pFmt->nBlockAlign     = BR_MIX_CHANNELS * (BR_MIX_BITS / 8);
    pFmt->nAvgBytesPerSec = BR_MIX_RATE * pFmt->nBlockAlign;
    pFmt->wBitsPerSample  = BR_MIX_BITS;
}

/* --------------------------------------------------------------- voices */

int BrMixVoiceInit(BrSndVoice *pVoice, const BrMixWaveFormat *pFmt,
                   void *pPcm, uint32_t cbPcm)
{
    BrMixWaveFormat *pCopy;

    if (pVoice == NULL || pFmt == NULL || pPcm == NULL || cbPcm == 0)
        return -1;

    pCopy = (BrMixWaveFormat *)malloc(sizeof(*pCopy));
    if (pCopy == NULL)
        return -1;
    *pCopy = *pFmt;

    memset(pVoice, 0, sizeof(*pVoice));
    pVoice->pData      = pPcm;
    pVoice->nDataBytes = cbPcm;
    pVoice->pFormat    = pCopy;
    pVoice->f0C        = pFmt->nSamplesPerSec;   /* its own recorded pitch  */
    pVoice->f10        = 400;                    /* centre                   */
    pVoice->f14        = 400;                    /* unity                    */
    return 0;
}
