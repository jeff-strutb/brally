/* Auto-generated from Ghidra decompilation — 0x10036B20 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <string.h>

/* Forward declarations for unknown functions/globals */
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
extern char DAT_10ac3f88[];
extern char DAT_10ac3e80[];
extern char DAT_10b71ac0[];
extern char DAT_10b71aa0[];
extern char DAT_10396f08[];
extern void *DAT_10ac3068;             /* the IDirectPlayLobby interface */

/* WHAT IT DOES: builds a DirectPlay compound address for the connection the
 * player selected, then asks the lobby interface (vtable +0x38,
 * CreateCompoundAddress) for its packed size, allocates a movable global
 * buffer, and calls it again to fill the buffer. The element list always
 * starts with the service-provider GUID; a modem connection appends the
 * modem name (if set) and the phone number, a serial connection fills in
 * defaulted device/baud strings and appends both, tcp/ip appends the host
 * address (or points at the queried blob when the GUIDs differ). On success
 * returns 0 and hands back the buffer and its size; on failure unlocks and
 * frees the buffer and returns the error code. */
/* T2 RESIDUE (805 orig / 805 recomp B, insns 232/231, raw 14+15, regnorm 2+3):
 * size-exact; byte-exact through branch 1 and branch 2 (offsets align to
 * +0x235). Two coupled defects remain, both in the certified either-or
 * layout class (see 0x10036810 BrComGetAlloc, T3 2026-09-09):
 *  - branch 3 (the tcp/ip arm): the original hoists the memcmp's cmpsb above
 *    the element GUID/cb stores and delays the jne 14 insns (flags held
 *    across the movs, GUID store order pair-swapped 1,0,3,2); ours keeps
 *    source order (stores, then cmpsb+jne adjacent). Ternary, temp-variable,
 *    duplicated-arm and member-wise-copy spellings all fail to reproduce the
 *    hoist (probes w3, w4, w6) — the temp materializes an sbb pair instead.
 *  - the tail: the original lays out [err][jmp cleanup][2nd-call][cleanup]
 *    [success]; ours emits [err][cleanup][2nd-call][success] with a
 *    backward jl, eliding the jmp (one instruction fewer, same bytes).
 *    goto-shaped and arm-swapped restructures compile to identical bytes
 *    (probes w2, w5) — the layout is not source-reachable from here.
 * Proven levers already in this file: null pointers spelled bare 0 (any
 * (void*)0x0 cast flips the whole tail layout AND the strlen guards, +11 B);
 * pObj/vt COM locals per the byte-exact sibling 0x10036F40; strlen guards
 * through a materialized unsigned temp; single count variable incremented in
 * place; one result variable across both vtable calls.
 * @t4-pass 0x10036B20 1 2026-09-09 probes 6 bytes 805 insns 231 regions 1 rows 5 census no  (fn.py variants: null spellings, goto tail, duplicated arms, memcmp temp, inverted pMem arms, member-wise GUID copy) */
/* @t4-pass 0x10036B20 2 2026-09-09 probes 10 bytes 805 insns 231 regions 3 rows 3 census no  (statement orders, commutes, ++, nested calls, pMem goto: 8 identical, 2 regressions) */
/* @t4-pass 0x10036B20 3 2026-09-09 probes 10 bytes 805 insns 231 regions 3 rows 3 census yes  (polarity, arm swap, &aElem[0], decl/init orders: 9 identical, 1 regression; histograms equal except the layout triple jge 1/0, jl 1/2, jmp 4/3) */
/* @t3 0x10036B20 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 805/805 insns 231/232 rows 2+1 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * Residue: the tail's either-or layout fork and the branch-3 cmpsb hoist --
 * both in the certified layout class; dead lists in the T2 RESIDUE note
 * above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10036B20 glide BrDpAddressBuild */
int BrDpAddressBuild(int *param_1, unsigned int *param_2)

{
  char cVar1;
  int iVar2;
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
  iVar2 = FUN_10036650(&local_4c);
  if (0 <= iVar2) {
    if (memcmp(local_4c, DAT_10078898, 0x10) == 0) {
      aElem[0].g = DAT_10078978;
      iVar6 = 1;
      aElem[0].cb = 0x10;
      aElem[0].pv = DAT_10078898;
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
      aElem[0].pv = DAT_10078878;
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
      aElem[0].g = DAT_10078978;
      aElem[0].cb = 0x10;
      if (memcmp(local_4c, DAT_10078868, 0x10) == 0) {
        aElem[0].pv = DAT_10078868;
      }
      else {
        aElem[0].pv = (char *)&local_4c;
      }
      iVar6 = 1;
    }
    pObj = DAT_10ac3068;
    vt = *(int **)pObj;
    iVar2 = (*(BrDpCreateAddr5 *)((char *)vt + 0x38))
                (pObj, aElem, iVar6, 0, &local_50);
    if (iVar2 == (int)0x8877001e) {
      pvVar4 = GlobalAlloc(0x42, local_50);
      pMem = GlobalLock(pvVar4);
      if (pMem == 0) {
        iVar2 = (int)0x8007000e;
      }
      else {
        pObj = DAT_10ac3068;
        vt = *(int **)pObj;
        iVar2 = (*(BrDpCreateAddr5 *)((char *)vt + 0x38))
                    (pObj, aElem, iVar6, pMem, &local_50);
        if (0 <= iVar2) {
          *param_1 = (int)pMem;
          *param_2 = local_50;
          return 0;
        }
      }
    }
  }
  if (pMem != 0) {
    pvVar4 = GlobalHandle(pMem);
    GlobalUnlock(pvVar4);
    pvVar4 = GlobalHandle(pMem);
    GlobalFree(pvVar4);
  }
  return iVar2;
}


#endif /* BR_MATCHING_BUILD */
