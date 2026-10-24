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
    if ((*(int *)&DAT_100b51e4[1036]) == 0)
        return 1;
    BrSndG18290FC++;
    if (BrSndG18290FC != 1)
        return 1;
    memset((*(void * (*)[15])&g_apBrSfxChanVoice), 0, sizeof((*(void * (*)[15])&g_apBrSfxChanVoice)));
    BrSndBankClear();
    /* the original made its DirectSound object here, with a primary buffer
       of 22 kHz 16-bit stereo it kept playing; the output is the host's */
    BrSndPDS = br_snd_open();
    return BrSndPDS != NULL;
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
  if (BrSndPDS != NULL) {
    br_snd_close();
    BrSndPDS = NULL;
  }
  if (DAT_1184c2a8 != NULL) {
    free(DAT_1184c2a8);
    DAT_1184c2a8 = NULL;
  }
  return 1;
}
