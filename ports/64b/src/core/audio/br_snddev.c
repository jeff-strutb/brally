/* br_snddev.c -- bringing the DirectSound device up and taking it down.  The
 * bank tables they clear are br_sfx.c / br_sndvoice.c.
 *
 * Reference: BRGlide.dll.  The D3D build shares the body at 0x10073560.
 *
 *   0x1006C6A0               close it again: the reference-counted teardown.
 *   0x1006C4D0 (0x10073560)  open the device: reference count, ACM format
 *                            size, the 22 kHz stereo 16-bit WAVEFORMATEX,
 *                            CoCreateInstance(CLSID_DirectSound), Initialize,
 *                            SetCooperativeLevel(PRIORITY), primary buffer,
 *                            Play(LOOPING).
 */
/* The original is /MD: CRT and Win32 calls go through the import table
 * (FF 15).  acmMetrics is the exception: it is reached through the linker's
 * jmp[IAT] thunk at 0x10074558 (E8), so it is declared WITHOUT dllimport --
 * which is how MSACM.H declares it. */
#define _CRTIMP __declspec(dllimport)
#include "slice1_08.h"   /* br_globals: its objects */
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <dsound.h>

#include <string.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x100B55F0  sound enabled      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* 0x1184C45C  device user count  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x1184C268  bank voice table   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* the locked primary format      */
/* 64-bit core: declared once, in br_globals.h or its struct's header *//* the primary buffer             */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x1184C458  the device         */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x105BC72C  the game window    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* CLSID_DirectSound              */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* IID_IDirectSound               */

/* BrSndBankClear: prototype in br_funcs.h */

/* WHAT IT DOES: open the sound device for one more user, and do the real
 * work only for the first: clear the voice table and the bank, ask the audio
 * compression manager how big a wave format can be and allocate one that
 * size (22 kHz, stereo, 16-bit), create the DirectSound object, initialise
 * it, take priority cooperative level on the game window, create the primary
 * buffer and start it looping.  Any failure after the object exists releases
 * what was made.  Returns whether the device is usable; sound switched off
 * or an already-open device is a silent success. */
/* @implements 0x1006C4D0 glide BrSndDevOpen */
int BrSndDevOpen(void)
{
    DWORD        cbFormat;
    DSBUFFERDESC dsbd;
    HRESULT      hr;

    if (BrSndG0B5DE8 == 0)
        return 1;
    BrSndG18290FC++;
    if (BrSndG18290FC != 1)
        return 1;

    memset(g_aBrSndBankVoice, 0, sizeof(g_aBrSndBankVoice));
    BrSndBankClear();
    if (acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, &cbFormat) != 0)
        return 0;

    DAT_1184c2b0 = (LPWAVEFORMATEX)GlobalLock(
        GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, cbFormat));
    if (DAT_1184c2b0 == NULL)
        return 0;
    DAT_1184c2b0->wFormatTag      = WAVE_FORMAT_PCM;
    DAT_1184c2b0->nChannels       = 2;
    DAT_1184c2b0->nSamplesPerSec  = 22050;
    DAT_1184c2b0->nAvgBytesPerSec = 88200;
    DAT_1184c2b0->nBlockAlign     = 4;
    DAT_1184c2b0->wBitsPerSample  = 16;
    DAT_1184c2b0->cbSize          = 0;

    hr = CoCreateInstance(&DAT_10078a18, NULL, CLSCTX_INPROC_SERVER,
                          &DAT_10078a38, (void **)&BrSndPDS);
    if (hr >= 0 && BrSndPDS != NULL) {
        hr = IDirectSound_Initialize(BrSndPDS, NULL);
        if (hr >= 0) {
            hr = IDirectSound_SetCooperativeLevel(BrSndPDS, g_brOwner5BC72C,
                                                  DSSCL_PRIORITY);
            if (hr >= 0) {
                memset(&dsbd, 0, sizeof(dsbd));
                dsbd.dwSize  = sizeof(dsbd);
                dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
                hr = IDirectSound_CreateSoundBuffer(BrSndPDS, &dsbd,
                                                    &DAT_1184c344, NULL);
                if (hr >= 0) {
                    hr = IDirectSoundBuffer_Play(DAT_1184c344, 0, 0,
                                                 DSBPLAY_LOOPING);
                    if (hr < 0) {
                        IDirectSoundBuffer_Release(DAT_1184c344);
                        DAT_1184c344 = NULL;
                    }
                }
            }
        }
        if (hr < 0) {
            IDirectSound_Release(BrSndPDS);
            BrSndPDS = NULL;
        }
    }
    return hr >= 0;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* the ACM scratch format        */
/* BrSndBankFree: prototype in br_funcs.h */

/* WHAT IT DOES: drop one user of the sound system and, when the last one
 * goes, actually shut DirectSound down -- frees the banks, stops and
 * releases the primary buffer, releases the device, and frees the two
 * format blocks.  Reference counted so one subsystem releasing sound does
 * not silence another that still wants it.  Always reports success. */
/* @implements 0x1006C6A0 glide FUN_1006c6a0 */
int FUN_1006c6a0(void)
{
  BrSndG18290FC = BrSndG18290FC + -1;
  if (BrSndG18290FC != 0) {
    return 1;
  }
  BrSndBankFree();
  if (DAT_1184c344 != NULL) {
    IDirectSoundBuffer_Stop(DAT_1184c344);
    IDirectSoundBuffer_Release(DAT_1184c344);
    DAT_1184c344 = NULL;
  }
  if (BrSndPDS != NULL) {
    IDirectSound_Release(BrSndPDS);
    BrSndPDS = NULL;
  }
  if (DAT_1184c2b0 != NULL) {
    GlobalUnlock(GlobalHandle(DAT_1184c2b0));
    GlobalFree(GlobalHandle(DAT_1184c2b0));
    DAT_1184c2b0 = NULL;
  }
  if (DAT_1184c2a8 != NULL) {
    GlobalUnlock(GlobalHandle(DAT_1184c2a8));
    GlobalFree(GlobalHandle(DAT_1184c2a8));
    DAT_1184c2a8 = NULL;
  }
  return 1;
}
