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

/* ------------------------------------------------------------------ */
/* 0x10036300 -- the EnumSessions initiator                            */
/* ------------------------------------------------------------------ */

#include <string.h>

extern int   DAT_10077500;    /* the application GUID, four dwords */
extern int   DAT_10077504;
extern int   DAT_10077508;
extern int   DAT_1007750c;
extern int   DAT_10ac5bcc;    /* "enumeration in progress" flag    */
extern int   DAT_10ac5d30;    /* the lobby dialog object           */
extern int   DAT_10ac5d2c;    /* the session table                 */
extern int   DAT_10ac5bd8;    /* our row in the session table      */
extern void *g_brP680584;     /* the lobby window                  */
void BrSub1003D070(void);     /* 0x10036700 -- resets the list     */
int __stdcall FUN_10036130(int, int, int, int, int);

typedef int (__stdcall *BrDpEnumSessionsFn)(void *, void *, int, void *,
                                            void *, int);

/* WHAT IT DOES: kicks off the search for advertised games -- resets the
 * session list, builds a fresh session description carrying our
 * application id, and asks DirectPlay to enumerate matching sessions
 * (each hit arrives in the callback above). Afterwards it greys the
 * lobby's join button in or out: joining stays possible only while a
 * session table row exists, it has players, and our own row is not
 * already marked joined. */
/* @t4-pass 0x10036300 1 2026-09-12 probes 10 bytes 290 insns 83 regions 3 rows 1 census no  (hand: desc store orders x3, memset spellings x2, decl orders x2, err-constant cast, word !=0, RMW written out -- all inert) */
/* @t4-pass 0x10036300 2 2026-09-12 probes 10 bytes 290 insns 83 regions 3 rows 1 census yes  (hand: vtable two-step, nested-if tail, int-typed param/volatile view, mul operand order, char-typed flag store, guid byte-lea, typed vcall arg, memset 20*4, swap 6/9, swap 7/8 -- all inert; census push 12 jcc 6 call 3 repstosd 1 store32 9 store8 2 IDENTICAL both streams) */
/* @t3 0x10036300 2026-09-12 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 290/289 insns 83/82 rows 0+1 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is placement only: (1) `push edi` sits above the NULL test where
 * the original shrink-wraps it below (+1 insn, the early return pops it);
 * (2) the three GUID-dword stores pair registers to slots in a rotated
 * order (shapes identical, relocs masked); (3) the tail A `pop edi` sits
 * at the block end, the original lifts it two insns. The one msetdiff row
 * is the missing sunk-else `jmp` twin of (1). Instruction census is
 * identical class-for-class. Do not reopen before the end-grind. */
/* @implements 0x10036300 glide BrNetEnumSessionsStart */
int BrNetEnumSessionsStart(void *pIface)
{
    void *p = pIface;                /* the working copy; the else arm below
                                      * reads the PARAMETER, which is why the
                                      * original reloads [esp+arg] there */
    int   desc[20];
    int   r;

    if (p == NULL) {
        return (int)0x88770082;
    }
    BrSub1003D070();
    memset(desc, 0, 0x50);
    desc[6] = DAT_10077500;
    desc[9] = DAT_1007750c;
    desc[0] = 0x50;
    desc[7] = DAT_10077504;
    desc[8] = DAT_10077508;
    DAT_10ac5bcc = 1;
    if (DAT_10ac5bf0 != 0) {
        r = (*(BrDpEnumSessionsFn *)(*(int *)p + 0x34))
                (p, desc, 0, (void *)BrNetEnumSessionCb,
                 g_brP680584, 0x91);
    } else {
        /* The bytes prove a real reload of the argument slot here; a plain
         * read is value-numbered back to the register copy (as VC5 always
         * does -- see 0x100540D0's park note), so the load is pinned the
         * same way the dead-store class is: through a volatile view. */
        r = (int)*(void *volatile *)&pIface;
    }
    /* The join helper gets the enumerated session id -- desc+8. */
    FUN_100361a0(&desc[2], (void *)FUN_10036130, g_brP680584, 0);
    DAT_10ac5bcc = 0;

    if (DAT_10ac5d30 != 0
        && *(unsigned short *)(DAT_10ac5d2c + 0x1e164) > 0u
        && (*(unsigned char *)(DAT_10ac5d2c + DAT_10ac5bd8 * 0x438
                               + 0x3868) & 0x10) == 0) {
        *(unsigned int *)(DAT_10ac5d30 + 0x1c) &= 0xffffffef;
        *(unsigned char *)(DAT_10ac5d30 + 0x2b64) = 1;
        return r;
    }
    *(unsigned int *)(DAT_10ac5d30 + 0x1c) |= 0x10;
    *(unsigned char *)(DAT_10ac5d30 + 0x2b64) = 0;
    return r;
}
#endif /* BR_MATCHING_BUILD */
