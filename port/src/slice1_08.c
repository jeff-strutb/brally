/* slice1_08.c -- Boss Rally (BRD3D.dll) slice-1 a later pass.
 *
 * See slice1_08.h for the address map and for the DirectSound identification.
 *
 * Not ported from this range (each with the reason in the report):
 *   0x1006F0C0, 0x1006F720, 0x10070E60, 0x100730A0, 0x10073320
 */

#include <stdlib.h>
#include <string.h>

#include "slice1_08.h"

/* ------------------------------------------------------------------ */
/* 0x1006C9A0                                                          */
/* ------------------------------------------------------------------ */

float BrPlaneEval(const BrVec3 *pN, float d, const BrVec3 *pP)
{
    /* x87 order, traced through the fxch chain at 0x1006C9B3..0x1006C9C4:
     *   st0 = Py*Ny, st1 = Pz*Nz, st2 = Nx*Px
     *   faddp -> (Py*Ny + Pz*Nz), then + Nx*Px, then + d.
     *
     * DEVIATION: the original keeps every intermediate in an 80-bit x87
     * register and never rounds to float until the caller stores it (the
     * caller at 0x1006F184 compares st0 directly). A portable build rounds
     * each step to `float`, so results can differ in the last bits. */
    return ((pP->y * pN->y + pP->z * pN->z) + pN->x * pP->x) + d;
}

/* ------------------------------------------------------------------ */
/* module globals                                                      */
/* ------------------------------------------------------------------ */

/* 0x100BBAE0 -- the DLL image has 0xFF here. */
uint8_t BrSndMasterVolume = 0xFFu;

/* 0x100B5DE8 -- the DLL image has 1 here. */
int32_t   BrSndG0B5DE8  = 1;
BrDSound *BrSndPDS      = NULL;
void     *BrSndG18290FC = NULL;

BrSndVoice *BrSndVoices[BR_SND_GROUPS * BR_SND_SLOTS_PER_GROUP];

static void BrSndFreeDefault(void *p)
{
    free(p);
}

BrSndFreeFn BrSndFreeHook = BrSndFreeDefault;

/* ------------------------------------------------------------------ */
/* 0x100722D0  create the buffer and upload the sample                 */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceCreate(BrSndVoice *pVoice)
{
    BrDSBufferDesc desc;
    BrDSBCaps      caps;
    void          *pLock1 = NULL;
    void          *pLock2 = NULL;
    uint32_t       nLock1 = 0;
    uint32_t       nLock2 = 0;      /* the original aims this at its own arg1
                                     * stack slot; a local is equivalent. */
    BrDSBuffer    *pBuf;
    int32_t        hr;

    desc.dwSize        = 0x14u;     /* sizeof(DSBUFFERDESC) */
    desc.dwFlags       = (pVoice->f28 == 0) ? BR_SND_DESC_FLAGS
                                            : BR_SND_DESC_FLAGS_ALT;
    desc.dwBufferBytes = pVoice->nDataBytes;
    desc.dwReserved    = 0;
    desc.lpwfxFormat   = pVoice->pFormat;

    /* Note: pVoice->pBuf is NOT pre-cleared by the original -- it relies on
     * CreateSoundBuffer to write it. */
    hr = BrSndPDS->pVtbl->CreateSoundBuffer(BrSndPDS, &desc, &pVoice->pBuf,
                                            NULL);
    if (hr == 0) {
        pBuf = pVoice->pBuf;
        hr = pBuf->pVtbl->Lock(pBuf, 0, pVoice->nDataBytes,
                               &pLock1, &nLock1, &pLock2, &nLock2, 0);
        if (hr == 0) {
            /* DEVIATION: the original is a `rep movsd` + `rep movsb` pair,
             * i.e. a plain forward byte copy of nDataBytes bytes. It does not
             * consult nLock1, so an undersized lock would overrun; memcpy is
             * the same copy without that being any safer, but at least the
             * intent is explicit. */
            memcpy(pLock1, pVoice->pData, pVoice->nDataBytes);

            /* GOTCHA: the byte count passed to Unlock is nDataBytes, not the
             * nLock1 that Lock reported. */
            hr = pBuf->pVtbl->Unlock(pBuf, pLock1, pVoice->nDataBytes,
                                     NULL, 0);
            if (hr == 0) {
                pLock1 = NULL;
                hr = pBuf->pVtbl->SetVolume(pBuf, 0);
                if (hr == 0) {
                    hr = pBuf->pVtbl->SetPan(pBuf, 0);
                    if (hr == 0) {
                        caps.dwSize = 0x14u;   /* sizeof(DSBCAPS) */
                        hr = pBuf->pVtbl->GetCaps(pBuf, &caps);
                        if (hr == 0) {
                            pVoice->f24 =
                                ((caps.dwFlags & BR_DSBCAPS_LOCHARDWARE) != 0)
                                ? 1 : 0;
                            return hr;     /* success; buffer kept */
                        }
                    }
                }
            }
        }
    }

    if (pLock1 != NULL) {
        pBuf = pVoice->pBuf;
        /* GOTCHA (faithful): this assignment clobbers the failure code that
         * brought us here, so a create that failed at Unlock reports whatever
         * the retry Unlock returns -- possibly 0. */
        hr = pBuf->pVtbl->Unlock(pBuf, pLock1, pVoice->nDataBytes, NULL, 0);
        pLock1 = NULL;
    }
    if (pVoice->pBuf != NULL) {
        pVoice->pBuf->pVtbl->Release(pVoice->pBuf);
        pVoice->pBuf = NULL;
    }
    (void)pLock1;
    return hr;
}

/* ------------------------------------------------------------------ */
/* 0x10072450  append to the tail of a chain                           */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceAppend(BrSndVoice *pHead, BrSndVoice *pNode)
{
    BrSndVoice *pCur;
    BrSndVoice *pNext;

    pNode->pNext = NULL;
    pNode->f1C   = 0;

    pCur  = pHead;
    pNext = pCur->pNext;
    while (pNext != NULL) {
        pCur  = pNext;
        pNext = pCur->pNext;
    }
    pCur->pNext = pNode;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 0x10072490 / 0x100724B0 / 0x100724D0  parameter application         */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceApplyPan(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf = pVoice->pBuf;
    int32_t     pan  = (pVoice->f10 - 400) * 10;

    return pBuf->pVtbl->SetPan(pBuf, pan);
}

int32_t BrSndVoiceApplyFreq(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf = pVoice->pBuf;

    return pBuf->pVtbl->SetFrequency(pBuf, pVoice->f0C);
}

int32_t BrSndVoiceApplyVolume(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf;
    uint32_t    scaled;
    int32_t     vol;

    if (BrSndMasterVolume == 0) {
        pBuf = pVoice->pBuf;
        return pBuf->pVtbl->SetVolume(pBuf, BR_DSBVOLUME_MIN);
    }

    /* The original does `imul` then an UNSIGNED reciprocal divide by 255
     * (magic 0x80808081, shift 39). DEVIATION: the multiply is done in
     * uint32_t here so that a large f14 cannot be signed-overflow UB -- the
     * bit pattern, and therefore the result, is identical. */
    scaled = ((uint32_t)pVoice->f14 * (uint32_t)BrSndMasterVolume) / 255u;
    vol    = ((int32_t)scaled - 400) * 10;

    pBuf = pVoice->pBuf;
    return pBuf->pVtbl->SetVolume(pBuf, vol);
}

/* ------------------------------------------------------------------ */
/* 0x10072520 / 0x10072550  teardown                                   */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceRelease(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf = pVoice->pBuf;

    if (pBuf != NULL) {
        pBuf->pVtbl->Release(pBuf);
        pVoice->pBuf = NULL;
    }
    return 0;
}

int32_t BrSndVoiceStop(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf;
    int32_t     hr;

    if (pVoice->f1C == 0) {
        return 0;
    }
    pBuf = pVoice->pBuf;
    hr   = pBuf->pVtbl->Stop(pBuf);
    if (hr == 0) {
        pVoice->f1C = 0;
    }
    return hr;
}

/* ------------------------------------------------------------------ */
/* 0x10072820  packed pair -> pan + volume                             */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceSetLevels(BrSndVoice *pVoice, uint32_t packed)
{
    int32_t hi;
    int32_t lo;

    if (BrSndG0B5DE8 == 0)     return 1;
    if (BrSndPDS == NULL)      return 1;
    if (BrSndG18290FC == NULL) return 1;
    if (pVoice == NULL)        return 0;

    hi = (int32_t)(packed >> 16);
    lo = (int32_t)(packed & 0xFFFFu);
    if (hi > 0x20) hi = 0x20;
    if (lo > 0x20) lo = 0x20;

    /* Both arms of the original's branch are the same formula with the
     * larger of the two as the scale; kept split so the (dead in one arm)
     * zero guards stay visible. */
    if (hi > lo) {
        pVoice->f10 = 400;
        pVoice->f14 = (hi * 400) / 32;
        if (hi != 0) {                       /* unreachable: hi > lo >= 0 */
            pVoice->f10 = ((lo - hi) * 400) / hi + 400;
        }
    } else {
        pVoice->f10 = 400;
        pVoice->f14 = (lo * 400) / 32;
        if (lo != 0) {
            pVoice->f10 = ((lo - hi) * 400) / lo + 400;
        }
    }

    if (BrSndVoiceApplyVolume(pVoice) != 0) return 0;
    if (BrSndVoiceApplyPan(pVoice) != 0)    return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10072A00 / 0x100729E0  start                                      */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceStart(BrSndVoice *pVoice)
{
    BrDSBuffer *pBuf;
    uint32_t    status = 0;
    uint32_t    play;
    int32_t     hr;

    play = (pVoice->f18 != 0) ? 1u : 0u;   /* DSBPLAY_LOOPING */

    pBuf = pVoice->pBuf;
    hr   = pBuf->pVtbl->GetStatus(pBuf, &status);
    if (hr == 0 && (status & BR_DSBSTATUS_PLAYING) == BR_DSBSTATUS_PLAYING) {
        /* Already playing: rewind only. f1C is deliberately left alone. */
        pBuf = pVoice->pBuf;
        return pBuf->pVtbl->SetCurrentPosition(pBuf, 0);
    }

    pBuf = pVoice->pBuf;
    hr   = pBuf->pVtbl->Play(pBuf, 0, 0, play);
    if (hr == 0) {
        pVoice->f1C = 1;
    }
    return hr;
}

int32_t BrSndVoiceSetLoopAndStart(BrSndVoice *pVoice, int32_t loop)
{
    pVoice->f18 = loop;
    return BrSndVoiceStart(pVoice);
}

/* ------------------------------------------------------------------ */
/* 0x10072A90 / 0x10072A70 / 0x10072AF0  table-driven play             */
/* ------------------------------------------------------------------ */

int32_t BrSndPlayEx(int32_t group, int32_t slot, uint32_t packed, int32_t loop)
{
    BrSndVoice *pVoice;
    int32_t     idx;

    if (BrSndG0B5DE8 == 0)     return 1;
    if (BrSndPDS == NULL)      return 1;
    if (BrSndG18290FC == NULL) return 1;

    idx = slot + group * BR_SND_SLOTS_PER_GROUP;

    /* DEVIATION (memory safety): the original indexes 0x100B5DF0 with no
     * bounds check at all. Out-of-range indices are treated as "sound not
     * available", matching the disabled-sound result. */
    if (idx < 0 || idx >= (int32_t)(BR_SND_GROUPS * BR_SND_SLOTS_PER_GROUP)) {
        return 1;
    }
    pVoice = BrSndVoices[idx];

    if (BrSndVoiceSetLevels(pVoice, packed) == 0) {
        return 0;                     /* 0x10072820 returns 0 on failure */
    }
    if (BrSndVoiceSetLoopAndStart(pVoice, loop) == 0) {
        return 1;                     /* HRESULT 0 == started */
    }
    return 0;
}

int32_t BrSndPlayGroup(int32_t group, uint32_t packed, int32_t loop)
{
    return BrSndPlayEx(group, 1, packed, loop);
}

int32_t BrSndPlaySimple(int32_t group, uint32_t packed)
{
    return BrSndPlayGroup(group, packed, 0);
}

/* ------------------------------------------------------------------ */
/* 0x10072BF0 / 0x10072C20  chain operations                           */
/* ------------------------------------------------------------------ */

int32_t BrSndVoiceStopChain(BrSndVoice *pHead)
{
    BrSndVoice *pCur = pHead->pNext;

    while (pCur != NULL) {
        BrSndVoiceStop(pCur);
        /* The original re-reads the link out of the node it just handled,
         * so a Stop that rewrote pNext would be observed here. */
        pCur = pCur->pNext;
    }
    return 0;
}

int32_t BrSndVoiceFreeChain(BrSndVoice *pHead)
{
    BrSndVoice *pCur = pHead->pNext;

    pHead->pNext = NULL;
    while (pCur != NULL) {
        BrSndVoice *pNext;

        BrSndVoiceRelease(pCur);
        BrSndFreeHook(pCur->pFormat);   /* +0x08 first, then +0x00 */
        BrSndFreeHook(pCur->pData);
        pNext = pCur->pNext;            /* read before the node is freed */
        BrSndFreeHook(pCur);
        pCur = pNext;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 0x10073060                                                          */
/* ------------------------------------------------------------------ */

void BrSndBankReset(BrSndBank *pBank)
{
    int i;

    /* The original is one do-while walking a cursor from 0x100B6C00 to
     * 0x100B6C3C in steps of 4 and writing [c-0x6C0], [c], [c+0x48] --
     * exactly 15 iterations over three parallel tables. */
    for (i = 0; i < BR_SND_BANK_SLOTS; ++i) {
        pBank->aName0[i] = 0;
        pBank->aName1[i] = 0;
        pBank->aName2[i] = 0;
    }
}
