/* Auto-generated from Ghidra decompilation — 0x1006FFC0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */



/* WaveOpenFile (DX5 wave.c, CSE'd PCM/extra alloc). Ghidra shredded
 * PCMWAVEFORMAT into 4 ints so mmioRead's HPSTR was only known to touch
 * the first dword — /O2 frame 0x18 vs orig 0x24. cbExtraBytes lives in
 * the dead pszFileName slot. Success returns the mmioAscend result, not
 * a fresh 0; cleanup nulls hmmio then stores it. */
/* WHAT IT DOES: open a .WAV file and get it ready to read -- walks the RIFF
 * chunks, checks it really is PCM audio, hands back the format description
 * and leaves the file positioned at the start of the samples. */
/* @implements 0x1006FFC0 glide FUN_1006ffc0 */
MMRESULT FUN_1006ffc0(LPSTR param_1,int *param_2,int *param_3,LPMMCKINFO param_4)

{
  int *piVar1;
  LPMMCKINFO pmmckiParent;
  HMMIO hmmio;
  LONG LVar2;
  int *puVar3;
  unsigned int uVar4;
  MMRESULT MVar5;
  PCMWAVEFORMAT pcmWaveFormat;
  MMCKINFO local_14;
  
  *param_3 = 0;
  hmmio = mmioOpenA(param_1,(LPMMIOINFO)0x0,0x10000);
  pmmckiParent = param_4;
  if (hmmio == 0) {
    MVar5 = 0xe100;
    goto LAB_10070133;
  }

    MVar5 = mmioDescend(hmmio,param_4,(MMCKINFO *)0x0,0);
    if (MVar5 == 0) {
      if ((pmmckiParent->ckid == 0x46464952) && (pmmckiParent->fccType == 0x45564157)) {
        local_14.ckid = 0x20746d66;
        MVar5 = mmioDescend(hmmio,&local_14,pmmckiParent,0x10);
        if (MVar5 != 0) goto LAB_10070133;
        if (local_14.cksize >= sizeof(PCMWAVEFORMAT)) {
          LVar2 = mmioRead(hmmio,(HPSTR)&pcmWaveFormat,sizeof(PCMWAVEFORMAT));
          if (LVar2 != (LONG)sizeof(PCMWAVEFORMAT)) {
            MVar5 = 0xe102;
            goto LAB_10070133;
          }
          if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM) {
            param_1 = (LPSTR)0x0;
          }
          else {
            LVar2 = mmioRead(hmmio,(HPSTR)&param_1,2);
            if (LVar2 != 2) {
              MVar5 = 0xe102;
              goto LAB_10070133;
            }
          }
          puVar3 = GlobalAlloc(0,((unsigned int)param_1 & 0xffff) + sizeof(WAVEFORMATEX));
          *param_3 = (int)puVar3;
          if (puVar3 == (int *)0x0) {
            MVar5 = 0xe000;
            goto LAB_10070133;
          }
          *(PCMWAVEFORMAT *)puVar3 = pcmWaveFormat;
          *(short *)(*param_3 + 0x10) = (short)param_1;
          if (((short)param_1 == 0) ||
             (uVar4 = mmioRead(hmmio,(HPSTR)(*param_3 + 0x12),(unsigned int)param_1 & 0xffff),
             uVar4 == ((unsigned int)param_1 & 0xffff))) {
            MVar5 = mmioAscend(hmmio,&local_14,0);
            if (MVar5 != 0) goto LAB_10070133;
            goto TEMPCLEANUP;

          }
        }
      }
      MVar5 = 0xe101;
    }
  
LAB_10070133: ;
  piVar1 = param_3;
  if ((HGLOBAL)*param_3 != (HGLOBAL)0x0) {
    GlobalFree((HGLOBAL)*param_3);
    *piVar1 = 0;
  }
  if (hmmio != (HMMIO)0x0) {
    mmioClose(hmmio,0);
    hmmio = (HMMIO)0x0;
  }
TEMPCLEANUP:
  *param_2 = (int)hmmio;
  return MVar5;
}


#endif /* BR_MATCHING_BUILD */
