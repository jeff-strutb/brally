/* br_sndbuf.c -- audio: creating and filling one DirectSound buffer.
 *
 * Filed out of the address batch slice1_08.c.  The DirectSound object,
 * the voice record and the BR_STDCALL vtable layouts all come from
 * slice1_08.h, exactly as the batch compiled them.
 */

#include <stdlib.h>
#include <string.h>

#include "slice1_08.h"

/* WHAT IT DOES: creates the DirectSound buffer for one loaded sample and
 * pours the sample bytes into it -- lock, copy, unlock -- then centres its
 * volume and pan and records whether the driver put it in hardware. Any
 * failure unwinds the lock and the buffer and reports the error. */
/* @t4-pass 0x1006B240 1 2026-09-12 probes 10 bytes 370 insns 152 regions 2 rows 4 census no  (hand: memset literal/sizeof, truthiness, !=0 flags, hr decl position, negated caps arms (15, worse), desc field order (16, worse), memcpy casts, comment strip, (int32_t)1 -- all inert or worse) */
/* @t4-pass 0x1006B240 2 2026-09-12 probes 10 bytes 370 insns 152 regions 2 rows 4 census yes  (hand: cast return, SetVolume/Create/Unlock zero spellings, 0-vs-NULL x2, unsigned zeros, early-return arm, f28 truthy, sizeof(desc/caps) -- all inert; census push 33 jcc 10 call 8 rep 2 ret 3 IDENTICAL both streams) */
/* @t3 0x1006B240 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 370/370 insns 152/152 rows 2+2 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is ONE fold at the two caps-arm returns: the original returns the
 * hr web (`mov eax,esi`); ours proves hr==0 through the GetCaps branch and
 * rematerialises (`xor eax,eax`) -- the tritest/netopen constant-return
 * class, cancelled as the mov/xor singleton quad. Size equal at 370/370 in
 * the placed image instructions equal,
 * census identical. Everything else -- the memset'd desc, the arg-slot
 * nLock2, the label-inside-the-failure-arm cleanup, the per-call pBuf
 * re-reads -- is byte-exact. Do not reopen before the end-grind. */
/* @implements 0x1006B240 glide BrSndVoiceCreate */
int32_t BrSndVoiceCreate(BrSndVoice *pVoice)
{
    void          *pLock1 = NULL;   /* frame+0 */
    void          *pLock2 = NULL;   /* frame+4 */
    uint32_t       nLock1;          /* frame+8, the callee writes it */
    BrDSBufferDesc desc;            /* frame+0xC..0x1F, zeroed whole */
    BrDSBCaps      caps;            /* frame+0x20..0x33 */
    uint32_t       nLock2;          /* colored into the dead arg slot */
    int32_t        hr;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize        = 0x14u;     /* sizeof(DSBUFFERDESC) */
    desc.dwFlags       = BR_SND_DESC_FLAGS;
    if (pVoice->f28 != 0) {
        desc.dwFlags   = BR_SND_DESC_FLAGS_ALT;
    }
    desc.dwBufferBytes = pVoice->nDataBytes;
    desc.lpwfxFormat   = pVoice->pFormat;

    /* Note: pVoice->pBuf is NOT pre-cleared by the original -- it relies on
     * CreateSoundBuffer to write it.  Every later call re-reads it through
     * &pVoice->pBuf (the original keeps that ADDRESS in ebx and loads the
     * value fresh each time). */
    hr = BrSndPDS->pVtbl->CreateSoundBuffer(BrSndPDS, &desc, &pVoice->pBuf,
                                            NULL);
    if (hr != 0)
        goto fail;
    hr = pVoice->pBuf->pVtbl->Lock(pVoice->pBuf, 0, pVoice->nDataBytes,
                                   &pLock1, &nLock1, &pLock2, &nLock2, 0);
    if (hr != 0)
        goto fail;
    /* The `rep movsd` + `rep movsb` pair: a plain forward copy of
     * nDataBytes bytes; nLock1 is never consulted. */
    memcpy(pLock1, pVoice->pData, pVoice->nDataBytes);

    /* GOTCHA: the byte count passed to Unlock is nDataBytes, not the
     * nLock1 that Lock reported. */
    hr = pVoice->pBuf->pVtbl->Unlock(pVoice->pBuf, pLock1,
                                     pVoice->nDataBytes, NULL, 0);
    if (hr != 0)
        goto fail;
    pLock1 = NULL;
    hr = pVoice->pBuf->pVtbl->SetVolume(pVoice->pBuf, 0);
    if (hr != 0)
        goto fail;
    hr = pVoice->pBuf->pVtbl->SetPan(pVoice->pBuf, 0);
    if (hr != 0)
        goto fail;
    caps.dwSize = 0x14u;   /* sizeof(DSBCAPS) */
    hr = pVoice->pBuf->pVtbl->GetCaps(pVoice->pBuf, &caps);
    /* The label sits INSIDE the failure arm -- same layout idiom as
     * BrSndVoiceLoad: the five earlier failures goto into it, and the
     * success arm (the caps test) is laid after it. */
    if (hr != 0) {
fail:
        if (pLock1 != NULL) {
            /* GOTCHA (faithful): this assignment clobbers the failure code
             * that brought us here, so a create that failed at Unlock
             * reports whatever the retry Unlock returns -- possibly 0. */
            hr = pVoice->pBuf->pVtbl->Unlock(pVoice->pBuf, pLock1,
                                             pVoice->nDataBytes, NULL, 0);
            pLock1 = NULL;
        }
        if (pVoice->pBuf != NULL) {
            pVoice->pBuf->pVtbl->Release(pVoice->pBuf);
            pVoice->pBuf = NULL;
        }
        return hr;
    }
    if (caps.dwFlags & BR_DSBCAPS_LOCHARDWARE) {
        pVoice->f24 = 1;
    } else {
        pVoice->f24 = 0;
    }
    return hr;
}
