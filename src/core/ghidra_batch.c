/* Ghidra-decompiled functions verified bitexact by the auto-decomp pipeline.
 * Filed here pending identification of their named modules.
 */
#ifdef BR_MATCHING_BUILD

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
static unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for Ghidra-generated function names */
int BrSub10073980();
int BrSub100739B0();
int FUN_100014e0();
int FUN_100027e0();
int FUN_10002dc0();
int FUN_1001dd80();
int FUN_1001dfb0();
int FUN_10027b60();
int FUN_10027fb0();
int FUN_100283c0();
int FUN_1002db88();
int FUN_1002f282();
int FUN_10032500();
int FUN_10035400();
int FUN_100356b0();
int FUN_10036300();
int FUN_10037130();
int FUN_10069a80();
int FUN_1006a330();
int FUN_1006b420();
int FUN_1006b490();
int FUN_1006b4c0();
int FUN_1006b790();
int FUN_1006b970();
int FUN_1006bf90();
int FUN_1006c010();
int FUN_1006e1d0();
int FUN_1006f840();
int FUN_100776c0();
int FUN_100776f0();
int FUN_10077750();

/* Extern globals */
extern int BrG_6C661C;
extern int BrG_6C6624;
extern int BrSndG0B5DE8;
extern int BrSndG18290FC;
extern int BrSndPDS;
extern int DAT_10078828;
extern int DAT_10078858;
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_100abdf4;
extern int DAT_100b22d8;
extern int DAT_100b2f04;
extern int DAT_100b32b0;
extern int DAT_100b32bc;
extern int DAT_100b32c0;
extern int DAT_100b55f8;
extern int DAT_100b64b0;
extern int DAT_100b84a8;
extern int DAT_100ba2d0;
extern int DAT_1021c788;
extern int DAT_1021c81c;
extern int DAT_1021c908;
extern int DAT_1021ce40;
extern int DAT_1021ce4c;
extern int DAT_1021ce58;
extern int DAT_10226a54;
extern int DAT_10226a58;
extern int DAT_10226a5c;
extern int DAT_10226a64;
extern int DAT_10226e78;
extern int DAT_104ab4f0;
extern int DAT_104ab504;
extern int DAT_104abb30;
extern int DAT_104af5c8;
extern int DAT_104b05c8;
extern int DAT_10697a4c;
extern int * DAT_106b7aa0;
extern funcptr DAT_106b7ab8;
extern int DAT_106e7714;
extern int DAT_106e9a2c;
extern int DAT_106ed6a8;
extern int DAT_106ed6b0;
extern int DAT_10ac306c;
extern int DAT_10ac315c;
extern int DAT_10ac408c;
extern int DAT_10ac53e8;
extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;
extern int DAT_10ac5bb4;
extern int DAT_10ac5c40;
extern int DAT_10ac5c4c;
extern int DAT_10ac5c50;
extern int DAT_10ac5c54;
extern int DAT_10ac5cbc;
extern int DAT_10ac5d84;
extern int DAT_10ac5d98;
extern int DAT_10ac5e50;
extern int DAT_10ac6050;
extern int DAT_10ac610c;
extern int DAT_10ac6114;
extern int * DAT_10ac66e8;
extern int * DAT_10ac6720;
extern int * DAT_10ac6730;
extern int DAT_10ac6734;
extern int DAT_10ac6738;
extern int DAT_10ac673c;
extern int DAT_10b73668;
extern int DAT_10cf3668;
extern int DAT_117a5f28;
extern int DAT_117b324c;
extern int DAT_11849e60;
extern int DAT_1184c078;
extern int DAT_1184c07c;
extern int DAT_1184c094;
extern int DAT_1184c470;
extern int DAT_1184c478;
extern int DAT_1184c480;
extern int DAT_1184c484;
extern int DAT_118ed1a0;
extern funcptr DAT_118ed1cc;
extern funcptr DAT_118ed1d0;
extern funcptr PTR_FUN_100776c0;
extern funcptr PTR_FUN_100776f0;
extern funcptr PTR_FUN_10077750;
extern int _DAT_10697a48;
extern int _DAT_10697a50;
extern int _DAT_1184c460;
extern int _DAT_1184c464;
extern int g_0B6C00;
extern int g_178FEE8;
extern int g_AC300;
extern int g_BrReplayCount;
extern int g_BrReplayOn;
extern int g_BrSndAA3470;
extern int g_a220B20;
extern int * g_aBr178FEF8;
extern int * g_aBrPeer71;
extern int g_aBrSndBankVoice;
extern int g_br094294;
extern int g_br18AB118_S_S1499;
extern int g_brAA2854;
extern int g_brAA28D8;
extern int g_brCdEnabled;
extern int g_brCdMediaOk;
extern int g_brCdPlaying;
extern int g_brCdTrackCur;
extern int g_brCdTrackFirst;
extern int g_brH220DDC;
extern int g_brH221324;
extern int g_brH22AF04;
extern int g_brP277B40;
extern int g_brP680584;
extern int * g_brPA9D008;
extern int g_brPAA29D4;
extern int g_brPhaseAA2904;
extern int g_h1022AF30;
extern funcptr g_pfn18AA0B0;


/* @implements 0x10001C90 glide FUN_10001c90 */


int __fastcall FUN_10001c90(int param_1)

{
  int iVar1;
  
  iVar1 = param_1 + 0x2838;
  *(int *)(param_1 + 0x2734) = param_1 + 0x2808;
  BrVec3MulAdd(iVar1,param_1 + 0x30,param_1,0x40c00000);
  BrVec3MulAddTo(iVar1,param_1 + 0x10,0x40000000);
  BrVec3AddTo(iVar1,param_1 + 0x20);
  *(int *)(param_1 + 0xf78) = 2;
  return;
}


/* @implements 0x100027A0 glide FUN_100027a0 */


int FUN_100027a0(void)

{
  return DAT_1021c788;
}


/* @implements 0x10002C50 glide FUN_10002c50 */


int FUN_10002c50(void)

{
  if (g_brCdEnabled == 1) {
    FUN_100027e0();
    return;
  }
  BrCdTrackGetEar();
  return;
}


/* @implements 0x10002C70 glide FUN_10002c70 */


int FUN_10002c70(void)

{
  int iVar1;
  
  if ((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) {
    iVar1 = FUN_10002c50();
    g_brCdTrackCur = iVar1 + -1;
    if (iVar1 + -1 < g_brCdTrackFirst) {
      g_brCdTrackCur = g_brCdTrackFirst;
    }
    BrCdTrackPlay(g_brCdTrackCur);
  }
  return 1;
}


/* @implements 0x10002D30 glide FUN_10002d30 */


int FUN_10002d30(int param_1)

{
  if (g_brCdEnabled == 1) {
    FUN_10002dc0(param_1);
    return;
  }
  BrCdVolumeScale(param_1);
  return;
}


/* @implements 0x10002E80 glide FUN_10002e80 */


int FUN_10002e80(void)

{
  int uVar1;
  
  if (((g_brCdEnabled != 0) && (g_brCdPlaying != 0)) && (g_brCdMediaOk != 0)) {
    uVar1 = BrCdTrackPlay(g_brCdTrackCur);
    return uVar1;
  }
  return 1;
}


/* @implements 0x10005E80 glide FUN_10005e80 */


int FUN_10005e80(void)

{
  HANDLE pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_1021ce58;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *puVar2 = pvVar1;
    puVar2 = puVar2 + 0x25e;
  } while ((int)puVar2 < 0x102265d8);
  DAT_10226a54 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a58 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_10226a5c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_h1022AF30 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce40 = 0;
  DAT_1021c908 = 0;
  BrTimeUpdate();
  DAT_10226a64 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH221324 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH22AF04 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  g_brH220DDC = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021ce4c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  DAT_1021c81c = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  BrNetReset();
  return 1;
}


/* @implements 0x100060A0 glide FUN_100060a0 */


int FUN_100060a0(void)

{
  return g_br094294;
}


/* @implements 0x10007F10 glide FUN_10007f10 */


int FUN_10007f10(void)

{
  MEMORYSTATUS local_20;
  
  local_20.dwLength = 0x20;
  GlobalMemoryStatus(&local_20);
  DAT_10226e78 = local_20.dwTotalPhys;
  return;
}


/* @implements 0x10008D50 glide FUN_10008d50 */


int __fastcall FUN_10008d50(int param_1)

{
  return param_1;
}


/* @implements 0x10008D60 glide FUN_10008d60 */


int FUN_10008d60(void)

{
  return;
}


/* @implements 0x10009C00 glide FUN_10009c00 */


int FUN_10009c00(void)

{
  GetDesktopWindow();
  FUN_10035400();
  return;
}


/* @implements 0x1000DEE0 glide FUN_1000dee0 */


double FUN_1000dee0(int param_1)

{
  return (double)*(float *)(param_1 + 0x10);
}


/* @implements 0x10011D10 glide THUNK_10011D10 */


int THUNK_10011D10(void)

{
  FUN_1006e590();
  return;
}


/* @implements 0x10013F00 glide FUN_10013f00 */


int FUN_10013f00(void)

{
  if (DAT_104ab504 != 0) {
    DAT_104ab504 = 0;
  }
  return;
}


/* @implements 0x10013FC0 glide FUN_10013fc0 */


int FUN_10013fc0(void)

{
  return DAT_104ab4f0;
}


/* @implements 0x100168B0 glide FUN_100168b0 */


int FUN_100168b0(int param_1)

{
  DAT_104abb30 = param_1;
  return;
}


/* @implements 0x10019800 glide FUN_10019800 */


int FUN_10019800(void)

{
  BrS17Release();
  BrS17RegisterAtExit();
  return;
}


/* @implements 0x1001E130 glide FUN_1001e130 */


int FUN_1001e130(int param_1,int param_2)

{
  int iVar1;
  int uVar2;
  
  if ((param_1 == DAT_100a7514) && (param_2 == DAT_100a7518)) {
    DAT_106e7714 = param_1;
    DAT_100a7514 = param_1;
    DAT_106e9a2c = param_2;
    DAT_100a7518 = param_2;
    FUN_1001dfb0(param_1,param_2);
    return 1;
  }
  grSstWinClose();
  DAT_106e7714 = param_1;
  DAT_100a7514 = param_1;
  DAT_106e9a2c = param_2;
  DAT_100a7518 = param_2;
  iVar1 = FUN_1001dd80(param_1,param_2);
  if (iVar1 == 0) {
    DAT_106e7714 = 0x280;
    DAT_100a7514 = 0x280;
    DAT_106e9a2c = 0x1e0;
    DAT_100a7518 = 0x1e0;
    uVar2 = FUN_1001dd80(0x280,0x1e0);
    return uVar2;
  }
  return 1;
}


/* @implements 0x1001E2E0 glide FUN_1001e2e0 */


unsigned int * FUN_1001e2e0(unsigned int *param_1)

{
  (*DAT_118ed1cc)(*param_1 & 0xffffff);
  return param_1 + param_1[1] * 2;
}


/* @implements 0x1001E300 glide FUN_1001e300 */


unsigned int * FUN_1001e300(unsigned int *param_1)

{
  (*DAT_118ed1d0)(*param_1 & 0xffffff,param_1[1]);
  return param_1 + 2;
}






/* @implements 0x100293F0 glide FUN_100293f0 */




int FUN_100293f0(unsigned int *param_1)

{
  _DAT_10697a50 = *param_1 >> 8 & 7;
  _DAT_10697a48 = *param_1 >> 0xb & 7;
  return;
}


/* @implements 0x100316A0 glide FUN_100316a0 */


int FUN_100316a0(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x60);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 100)) {
    do {
      BrTrackFixupRec54(iVar1);
      iVar1 = iVar1 + 0x54;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 100));
  }
  return;
}


/* @implements 0x10031A80 glide FUN_10031a80 */


int FUN_10031a80(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x84);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 0x88)) {
    do {
      BrSwapVec3(iVar1);
      iVar1 = iVar1 + 0xc;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 0x88));
  }
  return;
}


/* @implements 0x100324F0 glide FUN_100324f0 */


int FUN_100324f0(void)

{
  FUN_10032500();
  BrAtexit_10038EA0();
  return;
}


/* @implements 0x10035870 glide FUN_10035870 */


int FUN_10035870(void)

{
  FUN_100356b0();
  DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
  DAT_10ac408c = 1;
  if (g_brPAA29D4 != 0) {
    FUN_10036300(g_brP277B40);
  }
  return 1;
}


/* @implements 0x10035BB0 glide FUN_10035bb0 */


int FUN_10035bb0(int *param_1)

{
  LPVOID local_4;
  
  local_4 = (LPVOID)0x0;
  CoCreateInstance((IID *)&DAT_10078858,(LPUNKNOWN)0x0,1,(IID *)&DAT_10078828,&local_4);
  *param_1 = local_4;
  return;
}


/* @implements 0x10036670 glide FUN_10036670 */


int FUN_10036670(void)

{
  LPCVOID pMem;
  HGLOBAL pvVar1;
  int *puVar2;
  
  puVar2 = &DAT_10ac315c;
  do {
    pMem = (LPCVOID)*puVar2;
    if (pMem != (LPCVOID)0x0) {
      pvVar1 = GlobalHandle(pMem);
      GlobalUnlock(pvVar1);
      pvVar1 = GlobalHandle(pMem);
      GlobalFree(pvVar1);
      *puVar2 = 0;
    }
    puVar2 = puVar2 + 0x38;
  } while ((int)puVar2 < 0x10ac3f5c);
  return;
}



/* @implements 0x10037720 glide FUN_10037720 */


int FUN_10037720(void)

{
  if (((((DAT_10ac6730 == 0) && (DAT_10ac6734 == 0)) && (DAT_10ac6738 == 0)) && (DAT_10ac673c == 0))
     && ((DAT_10ac5bb4 != 0 ||
         (((DAT_10ac5e50 == 0 && (DAT_10ac6050 == 0)) &&
          ((DAT_10ac610c == 0 && (DAT_10ac6114 == 0)))))))) {
    return 0;
  }
  return 1;
}


/* @implements 0x10037780 glide FUN_10037780 */


int FUN_10037780(void)

{
  int iVar1;
  
  iVar1 = BrFn1005FFD0();
  if (iVar1 < 0) {
    iVar1 = FUN_10037720();
    if (iVar1 == 0) {
      return 0;
    }
  }
  return 1;
}


/* @implements 0x100385E0 glide FUN_100385e0 */


int FUN_100385e0(void)

{
  return 1;
}


/* @implements 0x10038980 glide FUN_10038980 */


int FUN_10038980(int param_1)

{
  if (0 < DAT_10ac5a48) {
    switch(DAT_10ac5a48) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x73;
      break;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x72;
      break;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x71;
      break;
    case 4:
      *(short *)(param_1 + 0x1e20c) = 0x70;
      break;
    case 5:
      *(short *)(param_1 + 0x1e20c) = 0x6f;
      break;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  if (DAT_10ac5a48 == 0) {
    switch(DAT_10ac5a4c & 0xff) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x47;
      return 1;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x49;
      return 1;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x4b;
      return 1;
    case 4:
    case 5:
    case 6:
      *(short *)(param_1 + 0x1e20c) = 0x4d;
      return 1;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  return 1;
}


/* @implements 0x10038C60 glide FUN_10038c60 */


int FUN_10038c60(int param_1)

{
  if ((g_brPhaseAA2904 == DAT_10ac5cbc) && (DAT_10ac5c40 == 0)) {
    return 0xfffffffe;
  }
  *(short *)(param_1 + 0x1e20c) = *(short *)(DAT_100abdf4 * 4 + 0x100abd48);
  return 1;
}


/* @implements 0x1003BFF0 glide FUN_1003bff0 */


int FUN_1003bff0(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}


/* @implements 0x1003C050 glide FUN_1003c050 */


int FUN_1003c050(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}


/* @implements 0x10040930 glide FUN_10040930 */


int FUN_10040930(void)

{
  DAT_10ac5c50 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}


/* @implements 0x10040960 glide FUN_10040960 */


int FUN_10040960(void)

{
  DAT_10ac5c54 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}


/* @implements 0x100409C0 glide FUN_100409c0 */


int FUN_100409c0(void)

{
  DAT_10ac5d98 = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}


/* @implements 0x100409F0 glide FUN_100409f0 */


int FUN_100409f0(void)

{
  DAT_10ac5c4c = 1;
  BrSub10072AF0(2,0x200020);
  g_brAA2854 = 2;
  return;
}


/* @implements 0x10041930 glide FUN_10041930 */


int __fastcall FUN_10041930(int *param_1)

{
  *param_1 = &PTR_FUN_100776c0;
  return;
}


/* @implements 0x10041DB0 glide FUN_10041db0 */


int FUN_10041db0(void)

{
  FUN_100014e0(DAT_10ac5d84);
                    
                    
  (*DAT_106b7ab8)();
  return;
}


/* @implements 0x10053E60 glide FUN_10053e60 */


int FUN_10053e60(void)

{
  return 0;
}


/* @implements 0x10053EE0 glide FUN_10053ee0 */


int __fastcall FUN_10053ee0(int *param_1)

{
  *param_1 = &PTR_FUN_100776f0;
  return;
}


/* @implements 0x10055A30 glide FUN_10055a30 */


int __fastcall FUN_10055a30(int *param_1)

{
  *param_1 = &PTR_FUN_10077750;
  return;
}


/* @implements 0x10058380 glide FUN_10058380 */


int FUN_10058380(int param_1,int param_2,unsigned int param_3,int param_4,
                 int param_5)

{
  BrUiSprClip(DAT_10ac5d84,param_1,param_2,(&DAT_10ac53e8)[(param_3 & 0xffff) * 2],param_4,param_5)
  ;
  return;
}


/* @implements 0x10059060 glide FUN_10059060 */


int FUN_10059060(void)

{
  int iVar1;
  
  iVar1 = 0;
  do {
    *(unsigned int *)((int)&DAT_10ac6730 + iVar1) = (unsigned int)(*(int *)((int)&DAT_10ac66e8 + iVar1) == 0);
    *(unsigned int *)((int)&DAT_10ac66e8 + iVar1) = *(unsigned int *)((int)&DAT_10ac6720 + iVar1);
    *(unsigned int *)((int)&DAT_10ac6730 + iVar1) =
         *(unsigned int *)((int)&DAT_10ac6730 + iVar1) & *(unsigned int *)((int)&DAT_10ac6720 + iVar1);
    iVar1 = iVar1 + 4;
  } while (iVar1 < 0x10);
  return;
}


/* @implements 0x1005A070 glide FUN_1005a070 */


int FUN_1005a070(void)

{
  return DAT_100b22d8;
}


/* @implements 0x1005C440 glide THUNK_1005C440 */


int THUNK_1005C440(void)

{
  FUN_1006e590();
  return;
}


/* @implements 0x1005F220 glide FUN_1005f220 */


int FUN_1005f220(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  iVar1 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x7c);
  if (0 < iVar1) {
    do {
      FUN_1006e1d0(*(int *)(*(int *)(param_1 + 0x29c4) + 4 + iVar2 * 4),
                   *(int *)(*(int *)(param_2 + 0x78) + iVar2 * 4));
      iVar2 = iVar2 + 1;
    } while (iVar2 < iVar1);
  }
  iVar2 = *(int *)(param_1 + 0x29c4);
  if ((*(int *)(*(int *)(iVar2 + 0x8014) + 4 + (unsigned int)*(unsigned char *)(iVar2 + 0x811b) * 0x24) != 0) &&
     (g_AC300 == 0)) {
    if (*(int *)(iVar2 + 0x84) != 0) {
      FUN_1006e1d0(*(int *)(iVar2 + 0x84),*(int *)(*(int *)(param_2 + 0x78) + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x88);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 4 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x8c);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 8 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x90);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 0xc + iVar1 * 4));
    }
  }
  return;
}


/* @implements 0x10060DB0 glide FUN_10060db0 */


int FUN_10060db0(int param_1)

{
  BrSfxSrcPlay(3,(&DAT_100b32b0)[param_1 * 6],(&DAT_100b32bc)[param_1 * 6],
               (&DAT_100b32c0)[param_1 * 6]);
  g_BrSndAA3470 = param_1;
  return;
}


/* @implements 0x100612D0 glide FUN_100612d0 */


int FUN_100612d0(int param_1,int param_2)

{
  BrSndBankSetCar(param_1,param_2);
  FUN_1006c010(param_1);
  BrSfxSrcPlaySilent(param_1 * 2,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  return;
}


/* @implements 0x10061440 glide FUN_10061440 */


int FUN_10061440(void)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < DAT_100b2f04) {
    do {
      BrWrap_10072B80(0x18,iVar1,0);
      iVar1 = iVar1 + 1;
    } while (iVar1 < DAT_100b2f04);
  }
  return;
}


/* @implements 0x10062610 glide FUN_10062610 */


int FUN_10062610(int param_1,int param_2)

{
  FUN_1006f840(param_2,param_1);
  *(int *)(param_1 + 0x10) = *(int *)(param_2 + 0x30);
  *(int *)(param_1 + 0x14) = *(int *)(param_2 + 0x34);
  *(int *)(param_1 + 0x18) = *(int *)(param_2 + 0x38);
  return;
}


/* @implements 0x100627B0 glide FUN_100627b0 */


int FUN_100627b0(int param_1)

{
  DAT_106ed6b0 = 0;
  BrG_6C6624 = 0;
  BrG_6C661C = 0;
  switch(param_1) {
  case 0:
    DAT_106ed6a8 = 0;
    return;
  case 1:
    DAT_106ed6a8 = 1;
    return;
  case 2:
    DAT_106ed6a8 = 1;
    BrG_6C6624 = 1;
    return;
  case 3:
    DAT_106ed6a8 = 1;
    DAT_106ed6b0 = 1;
    return;
  case 4:
    DAT_106ed6a8 = 1;
    BrG_6C661C = 1;
  }
  return;
}


/* @implements 0x10062830 glide FUN_10062830 */


int FUN_10062830(int param_1)

{
  FUN_100627b0(param_1);
  FUN_1002db88();
  return;
}


/* @implements 0x10062AC0 glide FUN_10062ac0 */


int FUN_10062ac0(void)

{
  BrCtrlCfgInitGlobal();
  BrAtexit_10069A70();
  return;
}


/* @implements 0x10063A50 glide FUN_10063a50 */


int FUN_10063a50(void)

{
  return g_BrReplayOn;
}


/* @implements 0x10063B40 glide FUN_10063b40 */


char * FUN_10063b40(void)

{
  return &DAT_10b73668;
}


/* @implements 0x10063B50 glide FUN_10063b50 */


int FUN_10063b50(void)

{
  return g_BrReplayCount * 0x18;
}


/* @implements 0x10063DA0 glide FUN_10063da0 */


char * FUN_10063da0(void)

{
  return &DAT_10cf3668;
}


/* @implements 0x10069DC0 glide FUN_10069dc0 */


int FUN_10069dc0(int param_1)

{
  FUN_10069a80(&DAT_117a5f28,param_1);
  return;
}


/* @implements 0x1006A070 glide FUN_1006a070 */


int FUN_1006a070(void)

{
  g_a220B20 = 5;
  FUN_1002f282();
  return;
}


/* @implements 0x1006A4D0 glide FUN_1006a4d0 */




int FUN_1006a4d0(void)

{
  HANDLE pvVar1;
  int iVar2;
  int *puVar3;
  int iVar4;
  
  g_178FEE8 = BrSub10075020();
  DAT_117b324c = BrDelta_100713A0();
  iVar2 = 0;
  do {
    pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
    *(HANDLE *)((int)&g_aBrPeer71 + iVar2) = pvVar1;
    puVar3 = (int *)((int)&g_aBr178FEF8 + iVar2);
    iVar4 = 0x10;
    do {
      pvVar1 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
      *puVar3 = pvVar1;
      puVar3 = puVar3 + 0x25b0;
      iVar4 = iVar4 + -1;
    } while (iVar4 != 0);
    iVar2 = iVar2 + 0x96c;
  } while (iVar2 < 0x96c0);
  FUN_1006a330();
  return;
}


/* @implements 0x1006A540 glide FUN_1006a540 */


int FUN_1006a540(void)

{
  BrWrap_100715E0();
  BrAtexit_10071600();
  return;
}


/* @implements 0x1006B1E0 glide FUN_1006b1e0 */


int FUN_1006b1e0(void)

{
  if (DAT_1184c078 != 0) {
    SetEvent(DAT_11849e60);
    WaitForSingleObject(DAT_1184c07c,0xffffffff);
    CloseHandle(DAT_1184c07c);
    DAT_1184c07c = (HANDLE)0x0;
    CloseHandle(DAT_11849e60);
    DAT_11849e60 = (HANDLE)0x0;
    DAT_1184c078 = 0;
  }
  return;
}


/* @implements 0x1006B5B0 glide FUN_1006b5b0 */


int FUN_1006b5b0(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if ((&g_aBrSndBankVoice)[param_1] != 0) {
      *(int *)((&g_aBrSndBankVoice)[param_1] + 0x18) = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}


/* @implements 0x1006B670 glide FUN_1006b670 */


int FUN_1006b670(int param_1,int param_2)

{
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if (param_1 != 0) {
      *(int *)(param_1 + 0xc) = param_2;
      FUN_1006b420(param_1);
      return 1;
    }
    return 0;
  }
  return 1;
}


/* @implements 0x1006B730 glide FUN_1006b730 */


int FUN_1006b730(int param_1,int param_2)

{
  int iVar1;
  
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    iVar1 = FUN_1006b790((&g_aBrSndBankVoice)[param_1],param_2);
    if (iVar1 != 0) {
      (&DAT_1184c094)[param_1 * 6] = param_2;
      return 1;
    }
    return 0;
  }
  return 1;
}


/* @implements 0x1006B950 glide FUN_1006b950 */


int FUN_1006b950(int param_1,int param_2)

{
  *(int *)(param_1 + 0x18) = param_2;
  FUN_1006b970(param_1);
  return;
}


/* @implements 0x1006BA00 glide FUN_1006ba00 */


int FUN_1006ba00(int param_1,int param_2,int param_3,int param_4)

{
  int uVar1;
  int iVar2;
  
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    uVar1 = (&DAT_100b55f8)[param_2 + param_1 * 0x12];
    iVar2 = FUN_1006b790(uVar1,param_3);
    if ((iVar2 != 0) && (iVar2 = FUN_1006b950(uVar1,param_4), iVar2 == 0)) {
      return 1;
    }
    return 0;
  }
  return 1;
}


/* @implements 0x1006BB60 glide FUN_1006bb60 */


int FUN_1006bb60(int param_1)

{
  int iVar1;
  int *piVar2;
  
  piVar2 = (int *)(param_1 + 0x1a8);
  iVar1 = *(int *)(param_1 + 0x1a8);
  while (iVar1 != 0) {
    FUN_1006b4c0(iVar1);
    piVar2 = (int *)(*piVar2 + 0x1a8);
    iVar1 = *piVar2;
  }
  return 0;
}


/* @implements 0x1006BB90 glide FUN_1006bb90 */


int FUN_1006bb90(int param_1)

{
  int *pMem;
  int *puVar1;
  HGLOBAL pvVar2;
  
  pMem = *(int **)(param_1 + 0x1a8);
  *(int *)(param_1 + 0x1a8) = 0;
  while (pMem != (int *)0x0) {
    FUN_1006b490(pMem);
    pvVar2 = GlobalHandle((LPCVOID)pMem[2]);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)pMem[2]);
    GlobalFree(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)*pMem);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle((LPCVOID)*pMem);
    GlobalFree(pvVar2);
    puVar1 = (int *)pMem[0x6a];
    pvVar2 = GlobalHandle(pMem);
    GlobalUnlock(pvVar2);
    pvVar2 = GlobalHandle(pMem);
    GlobalFree(pvVar2);
    pMem = puVar1;
  }
  return 0;
}


/* @implements 0x1006BF50 glide FUN_1006bf50 */


int FUN_1006bf50(int param_1)

{
  int uVar1;
  
  if (((BrSndG0B5DE8 != 0) && (BrSndPDS != 0)) && (BrSndG18290FC != 0)) {
    if ((&g_aBrSndBankVoice)[param_1] != 0) {
      uVar1 = FUN_1006bf90((&g_aBrSndBankVoice)[param_1]);
      return uVar1;
    }
    return 0;
  }
  return 1;
}


/* @implements 0x1006BFD0 glide FUN_1006bfd0 */


int FUN_1006bfd0(void)

{
  int *puVar1;
  
  puVar1 = &g_0B6C00;
  do {
    puVar1[-0x1b0] = 0;
    *puVar1 = 0;
    puVar1[0x12] = 0;
    puVar1 = puVar1 + 1;
  } while ((int)puVar1 < 0x100b6444);
  return;
}


/* @implements 0x1006C750 glide FUN_1006c750 */


int FUN_1006c750(void)

{
  DAT_1184c470 = (*g_pfn18AA0B0)(&DAT_100ba2d0,0,0x40,0x40,1,4,0,0,1,1,0xf,0xf,1,0);
  DAT_1184c478 = DAT_1184c470;
  return;
}


/* @implements 0x1006C800 glide FUN_1006c800 */


int FUN_1006c800(void)

{
  DAT_1184c484 = FUN_10027fb0(&DAT_100b64b0,0x40,0x40,2);
  return;
}


/* @implements 0x1006C880 glide FUN_1006c880 */


int FUN_1006c880(void)

{
  DAT_1184c480 = (*g_pfn18AA0B0)(&DAT_100b84a8,0,0x40,0x40,0,4,0,0,0,0,0,0,0,0);
  return;
}


/* @implements 0x1006C8B0 glide FUN_1006c8b0 */




int FUN_1006c8b0(void)

{
  _DAT_1184c460 = (*g_pfn18AA0B0)(&DAT_104af5c8,0,0x40,0x40,1,4,0,0,1,1,0,0,1,0);
  _DAT_1184c464 = (*g_pfn18AA0B0)(&DAT_104b05c8,0,0x40,0x40,1,4,0,0,1,1,0,0,1,0);
  return;
}


/* @implements 0x1006D190 glide FUN_1006d190 */


int __fastcall FUN_1006d190(int param_1)

{
  return *(int *)(param_1 + 0x10);
}


/* @implements 0x1006E020 glide FUN_1006e020 */


int FUN_1006e020(int param_1)

{
  DAT_118ed1a0 = param_1;
  return;
}


/* @implements 0x1006E030 glide FUN_1006e030 */


int FUN_1006e030(void)

{
  FUN_1006c750();
  BrFontRegisterPages();
  FUN_1006c800();
  BrSub10073980();
  BrSub100739B0();
  FUN_1006c880();
  FUN_1006c8b0();
  return;
}


/* @implements 0x1006E350 glide FUN_1006e350 */


int FUN_1006e350(void)

{
  return g_br18AB118_S_S1499;
}


/* @implements 0x1006E580 glide THUNK_1006E580 */


int THUNK_1006E580(void)

{
  FUN_1006e590();
  return;
}


/* @implements 0x1006E590 glide FUN_1006e590 */


int FUN_1006e590(void)

{
  return;
}


/* @implements 0x10070170 glide FUN_10070170 */


int FUN_10070170(int *param_1,LPMMCKINFO param_2,MMCKINFO *param_3)

{
  mmioSeek((HMMIO)*param_1,param_3->dwDataOffset + 4,0);
  param_2->ckid = 0x61746164;
  mmioDescend((HMMIO)*param_1,param_2,param_3,0x10);
  return;
}


/* @implements 0x10073714 glide FUN_10073714 */


int FUN_10073714(void)

{
  return;
}


/* @implements 0x10073974 glide FUN_10073974 */




int FUN_10073974(void)

{
                    
  halt_baddata();
}


/* @implements 0x10073979 glide FUN_10073979 */




int FUN_10073979(void)

{
                    
  halt_baddata();
}


/* @implements 0x100747E0 glide FUN_100747e0 */


int FUN_100747e0(int *param_1)

{
  if (*(int *)*param_1 == -0x1f928c9d) {
    func_0x10074aec();
  }
  return 0;
}



#endif /* BR_MATCHING_BUILD */
