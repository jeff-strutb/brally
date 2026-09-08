/* br_dplayenum.c -- net.
 *
 * DirectPlay EnumSessions callback: the lobby hears about one advertised
 * game and tries to join it.
 *
 * @t4-pass 0x10036220 1 2026-09-08 probes 6 bytes 240 insns 80 regions 1 rows 20 census no
 *
 * PARKED T2 after 6 probes. Control flow and the join tail are present.
 * Residue vs orig 224 B / 76 insns:
 *   - ebp frame from the 5 thiscall-wrapper structs (orig: esi/edi only, no
 *     sub esp)
 *   - vtable +0x10 4th stack arg is `push 0` instead of `push offset
 *     DAT_100aabe8` (tried &extern, char[], packed 5-int struct, literal)
 *   - nKind in esi not eax; flag byte stored as immediate 1 not `al`
 *   - FUN_100361a0 hook arg `push 0` instead of `push 0x10036130`
 * Dead: shared join tail (272->240), own TU (no change), packed struct
 * (287, worse), 0x100AABE8 literal (239, FIRSTDIV +6).
 */

#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <windows.h>

extern int g_brPAA29D4;
extern int DAT_10ac5bf0;
extern int DAT_100aabe8;
extern int FUN_100361a0(void *pJoin, void *pfn, void *pCtx, int z);

typedef struct { int v; } BrDpI;
typedef void (__fastcall *BrDpListF10)(void *pThis, BrDpI a, BrDpI b, BrDpI c,
                                       BrDpI d, BrDpI e);
typedef void (__fastcall *BrDpListF28)(void *pThis, BrDpI a, BrDpI b, BrDpI c);
typedef struct { int a, b, c, d; } BrSessionGuid;

/* WHAT IT DOES: DirectPlay EnumSessions callback. On a live session (not a
 * timeout) it tells the lobby list widget about the game, copies the 16-byte
 * session id, and hands that id to the join helper. Returns 1 to keep
 * enumerating, 0 if there is no lobby object or DirectPlay timed out. */
/* @implements 0x10036220 glide BrNetEnumSessionCb */
int __stdcall BrNetEnumSessionCb(void *pDesc, void *pUnused, unsigned flags,
                                 void *pCtx)
{
  unsigned char *pList;
  void *pMem;
  BrDpI a, b, c, d, e;

  (void)pUnused;
  if (g_brPAA29D4 == 0)
    return 0;
  if ((flags & 1) != 0)
    return 0;

  pMem = (void *)flags;
  if (DAT_10ac5bf0 != 0) {
    if (*(unsigned *)((char *)pDesc + 0x2c) < 8u
        && (*(unsigned char *)((char *)pDesc + 4) & 0x20) == 0) {
      b.v = 1;
      *(unsigned char *)&flags = 1;
    } else {
      b.v = 0x11;
      *(unsigned char *)&flags = 0;
    }
    pList = (unsigned char *)g_brPAA29D4 + 0x3838;
    a.v = *(int *)((char *)pDesc + 0x30);
    c.v = (int)flags;
    d.v = (int)&DAT_100aabe8;
    e.v = 1;
    (*(BrDpListF10 *)(*(int *)pList + 0x10))(pList, a, b, c, d, e);

    pMem = GlobalLock(GlobalAlloc(0x42, 0x10));
    if (pMem == 0)
      return 1;
    *(BrSessionGuid *)pMem = *(BrSessionGuid *)((char *)pDesc + 8);
    a.v = (int)pMem;
    b.v = 0x10;
    c.v = -1;
    pList = (unsigned char *)g_brPAA29D4 + 0x3838;
    (*(BrDpListF28 *)(*(int *)pList + 0x28))(pList, a, b, c);
  }
  FUN_100361a0(pMem, (void *)0x10036130, pCtx, 0);
  return 1;
}
#endif /* BR_MATCHING_BUILD */
