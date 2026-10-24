/* br_sndbuf.c -- audio: making one voice's sound.
 *
 * Filed out of the address batch slice1_08.c; the voice record comes from
 * slice1_08.h.
 */

#include <stdlib.h>
#include <string.h>

#include "slice1_08.h"

/* WHAT IT DOES: makes the sound for one loaded sample from its format and
 * bytes, at full volume and centred (a new sound starts so), and records
 * that it is not in a sound card's own memory. Reports 0, or nonzero when
 * the sample is not a format the mixer plays. */
/* @implements 0x1006B240 glide BrSndVoiceCreate */
int32_t BrSndVoiceCreate(BrSndVoice *pVoice)
{
    pVoice->pBuf = br_snd_new((const BrWavFmt *)pVoice->pFormat, pVoice->pData, pVoice->nDataBytes);
    if (pVoice->pBuf == NULL)
        return -1;
    pVoice->f24 = 0;
    return 0;
}
