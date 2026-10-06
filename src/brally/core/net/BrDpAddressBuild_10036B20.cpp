/* WHAT IT DOES: builds a DirectPlay compound address for the connection
 * the player picked (modem, serial or TCP/IP), asking the lobby object for
 * the size first and then filling a freshly allocated block; hands back the
 * block and its size, or the COM error. */
/* @implements 0x10036B20 glide BrDpAddressBuild
 * @cpp_kind free
 * @cpp_symbol _BrDpAddressBuild
 *
 * cdecl, two arguments, 805 B.  Lives in the C++ lane: the original was
 * built by C1XX against the C++ DirectPlayLobby interface (CreateCompound
 * Address as a virtual method), the shape the DirectX SDK samples use.  In
 * the TCP/IP arm the address element's GUID and size are stored at the head
 * of BOTH arms of the provider test, which is what lets VC5 schedule the
 * GUID copy between the memcmp and its branch as the original does.
 */
#define _CRTIMP __declspec(dllimport)
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
extern "C" {
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

/* ------------------------------------------------------------------ */
/* 0x10035400                                                         */
/* ------------------------------------------------------------------ */

/* Matching TU for 0x10035400: DirectPlay init (prefix of a map-split
 * function; 0x10035533 is the internal join, not a separate C function).
 * Inferred from orig bytes: two lstrcpyA via IAT-in-esi, /Oi memset of
 * 8+16 dwords, stride-0xe0 zero, nested GetModuleHandleA into
 * FUN_10009b00, FUN_10037260(FUN_1006d280(id), hr). */

int BrDPlayStartup(void *hInst, void *pCtx);   /* 0x10009B00, net/br_dplay.c */
int FUN_10032320(void *pCtx);
int FUN_10035bb0(void *pCtx);
int FUN_1006d280(int);
int FUN_10037260(int, int);
int FUN_10036e50(int **);
int FUN_10036300(int *);
int __stdcall FUN_10035ac0(int, int, int, int, int, int);

extern char DAT_10ac3e80[];
extern char DAT_10ac3f88[];
extern char DAT_10396f08[];
extern char DAT_10b71aa0[];
extern char DAT_10b71ac0[];
extern int DAT_10ac315c;
extern int DAT_10273334;
extern int DAT_10226a48;
extern int DAT_10ac5bdc;
extern int DAT_10ac4090;
extern int DAT_10ac4098;
extern int DAT_10077500;
extern int DAT_105bc72c;
extern int *g_brP277B40;             /* 0x10273328: the IDirectPlay4A interface */
extern int *DAT_10ac3068;            /* the IDirectPlayLobby interface */
extern int DAT_10ac5d2c;
extern int DAT_10ac5bf0;
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern char s_Could_not_create_DirectPlay_obje_100aa4d8[];

int FUN_10036650(char **);

/* A 16-byte GUID as four ints; the copy is a structure assignment, which is
 * what makes the compiler pair the loads and stores (and form one destination
 * pointer when the element index is variable). */
typedef struct { int a, b, c, d; } BrDpGuid;

/* One DPCOMPOUNDADDRESSELEMENT: data-type GUID, byte count, data pointer. */
typedef struct { BrDpGuid g; int cb; char *pv; } BrDpAddrElem;

/* CreateCompoundAddress lives at +0x38 in the IDirectPlayLobby vtable. */
typedef int (__stdcall *BrDpCreateAddr5)(void *, BrDpAddrElem *, int,
                                         void *, unsigned int *);

extern unsigned char DAT_10078898[];   /* modem provider GUID  */
extern unsigned char DAT_10078878[];   /* serial provider GUID */
extern unsigned char DAT_10078868[];   /* tcp/ip provider GUID */
extern BrDpGuid DAT_10078978;          /* DPAID_ServiceProvider */
extern BrDpGuid DAT_100789b8;
extern BrDpGuid DAT_10078998;
extern BrDpGuid DAT_100789d8;
extern BrDpGuid DAT_100789f8;

struct IBrDPLobby {
    virtual long __stdcall s00(void) = 0;
    virtual long __stdcall s01(void) = 0;
    virtual long __stdcall s02(void) = 0;
    virtual long __stdcall s03(void) = 0;
    virtual long __stdcall s04(void) = 0;
    virtual long __stdcall s05(void) = 0;
    virtual long __stdcall s06(void) = 0;
    virtual long __stdcall s07(void) = 0;
    virtual long __stdcall s08(void) = 0;
    virtual long __stdcall s09(void) = 0;
    virtual long __stdcall s10(void) = 0;
    virtual long __stdcall s11(void) = 0;
    virtual long __stdcall s12(void) = 0;
    virtual long __stdcall s13(void) = 0;
    virtual long __stdcall CreateCompoundAddress(void *pElem, int n, void *pv, unsigned int *pcb) = 0;
};
int BrDpAddressBuild(int *param_1, unsigned int *param_2)

{
  int hr;
  int iVar3;
  unsigned int uVar5;
  void *pvVar4;
  void *pMem;
  int iVar6;
  char *pcVar7;
  void *pObj;
  int *vt;
  unsigned int local_50;
  char *local_4c;
  BrDpAddrElem aElem [3];

  pMem = 0;
  local_50 = 0;
  hr = FUN_10036650(&local_4c);
  if (hr < 0)
    goto FAILURE;
  {
    if (memcmp(local_4c, DAT_10078898, 0x10) == 0) {
      aElem[0].g = DAT_10078978;
      iVar6 = 1;
      aElem[0].cb = 0x10;
      aElem[0].pv = (char *)DAT_10078898;
      uVar5 = strlen(DAT_10ac3f88);
      if (1 < uVar5) {
        aElem[1].g = DAT_100789b8;
        aElem[1].cb = lstrlenA(DAT_10ac3f88) + 1;
        aElem[1].pv = DAT_10ac3f88;
        iVar6 = 2;
      }
      pcVar7 = DAT_10ac3e80;
      if (pcVar7 != 0) {
        aElem[iVar6].g = DAT_10078998;
        iVar3 = lstrlenA(DAT_10ac3e80);
        aElem[iVar6].cb = iVar3 + 1;
        aElem[iVar6].pv = DAT_10ac3e80;
        iVar6 = iVar6 + 1;
      }
    }
    else if (memcmp(local_4c, DAT_10078878, 0x10) == 0) {
      aElem[0].g = DAT_10078978;
      aElem[0].cb = 0x10;
      aElem[0].pv = (char *)DAT_10078878;
      uVar5 = strlen(DAT_10b71ac0);
      if (uVar5 <= 1) {
        lstrcpyA(DAT_10b71ac0, DAT_10396f08);
      }
      aElem[1].g = DAT_100789d8;
      aElem[1].cb = lstrlenA(DAT_10b71ac0) + 1;
      aElem[1].pv = DAT_10b71ac0;
      uVar5 = strlen(DAT_10b71aa0);
      if (uVar5 <= 1) {
        lstrcpyA(DAT_10b71aa0, DAT_10396f08);
      }
      aElem[2].g = DAT_100789f8;
      aElem[2].cb = lstrlenA(DAT_10b71aa0) + 1;
      aElem[2].pv = DAT_10b71aa0;
      iVar6 = 3;
    }
    else {
      if (memcmp(local_4c, DAT_10078868, 0x10) == 0) {
        aElem[0].g = DAT_10078978;
        aElem[0].cb = 0x10;
        aElem[0].pv = (char *)DAT_10078868;
      }
      else {
        aElem[0].g = DAT_10078978;
        aElem[0].cb = 0x10;
        aElem[0].pv = (char *)&local_4c;
      }
      iVar6 = 1;
    }
    hr = ((IBrDPLobby *)DAT_10ac3068)->CreateCompoundAddress(aElem, iVar6, 0, &local_50);
    if (hr != (int)0x8877001e)
      goto FAILURE;
    pMem = (void *)GlobalLock(GlobalAlloc(0x42, local_50));
    if (pMem == 0) {
      hr = (int)0x8007000e;
      goto FAILURE;
    }
    hr = ((IBrDPLobby *)DAT_10ac3068)->CreateCompoundAddress(aElem, iVar6, pMem, &local_50);
    if (hr < 0)
      goto FAILURE;
    *param_1 = (int)pMem;
    *param_2 = local_50;
    return 0;
  }
FAILURE:
  if (pMem != 0) {
    GlobalUnlock(GlobalHandle(pMem));
    GlobalFree(GlobalHandle(pMem));
  }
  return hr;
}
}
