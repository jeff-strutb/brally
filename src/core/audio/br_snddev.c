/* br_snddev.c -- bringing the DirectSound device up.  The teardown twin is
 * 0x1006C6A0 (src/core/generated/0x1006C6A0.c); the bank tables it clears
 * are br_sfx.c / br_sndvoice.c.
 *
 * Reference: BRGlide.dll.  The D3D build shares the body at 0x10073560.
 *
 *   0x1006C4D0 (0x10073560)  open the device: reference count, ACM format
 *                            size, the 22 kHz stereo 16-bit WAVEFORMATEX,
 *                            CoCreateInstance(CLSID_DirectSound), Initialize,
 *                            SetCooperativeLevel(PRIORITY), primary buffer,
 *                            Play(LOOPING).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT and Win32 calls go through the import table
 * (FF 15).  acmMetrics is the exception: it is reached through the linker's
 * jmp[IAT] thunk at 0x10074558 (E8), so it is declared WITHOUT dllimport --
 * which is how MSACM.H declares it. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <dsound.h>

#include <string.h>

extern int   BrSndG0B5DE8;              /* 0x100B55F0  sound enabled      */
extern int   BrSndG18290FC;             /* 0x1184C45C  device user count  */
extern void *g_aBrSndBankVoice[15];     /* 0x1184C268  bank voice table   */
extern LPWAVEFORMATEX     DAT_1184c2b0; /* the locked primary format      */
extern LPDIRECTSOUNDBUFFER DAT_1184c344;/* the primary buffer             */
extern LPDIRECTSOUND      BrSndPDS;     /* 0x1184C458  the device         */
extern HWND  g_brOwner5BC72C;           /* 0x105BC72C  the game window    */
extern const GUID DAT_10078a18;         /* CLSID_DirectSound              */
extern const GUID DAT_10078a38;         /* IID_IDirectSound               */

void BrSndBankClear(void);              /* 0x1006BFD0 */

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

#endif /* BR_MATCHING_BUILD */
