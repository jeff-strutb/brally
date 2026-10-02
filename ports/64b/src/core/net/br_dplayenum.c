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

#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include <windows.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_100361a0: prototype in br_funcs.h */

typedef struct { int v; } BrDpI;
typedef void (__fastcall *BrDpListF10)(void *pThis, BrDpI a, BrDpI b, BrDpI c,
                                       BrDpI d, BrDpI e);
typedef void (__fastcall *BrDpListF28)(void *pThis, BrDpI a, BrDpI b, BrDpI c);
typedef struct { int a, b, c, d; } BrSessionGuid;

/* WHAT IT DOES: DirectPlay EnumSessions callback. On a live session (not a
 * timeout) it tells the lobby list widget about the game, copies the 16-byte
 * session id, and hands that id to the join helper. Returns 1 to keep
 * enumerating, 0 if there is no lobby object or DirectPlay timed out. */
/* declared only (the Mac port keeps its own body in ports/macos/patch/); Glide match is src/core/cpp/0x10036220.cpp */
/* BrNetEnumSessionCb: prototype in br_funcs.h */

/* ------------------------------------------------------------------ */
/* 0x10036300 -- the EnumSessions initiator                            */
/* ------------------------------------------------------------------ */

#include <string.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the application GUID, four dwords */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* "enumeration in progress" flag    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the lobby dialog object           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the session table                 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* our row in the session table      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* the lobby window                  */
/* BrSub1003D070: prototype in br_funcs.h */
/* FUN_10036130: prototype in br_funcs.h */

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
                 (*(void * *)&g_brOwner5BC72C), 0x91);
    } else {
        /* The bytes prove a real reload of the argument slot here; a plain
         * read is value-numbered back to the register copy (as VC5 always
         * does -- see 0x100540D0's park note), so the load is pinned the
         * same way the dead-store class is: through a volatile view. */
        r = (int)*(void *volatile *)&pIface;
    }
    /* The join helper gets the enumerated session id -- desc+8. */
    FUN_100361a0(&desc[2], (void *)BrWmHook36130, (*(void * *)&g_brOwner5BC72C), 0);
    DAT_10ac5bcc = 0;

    if (g_brPAA29D8 != 0
        && *(unsigned short *)(g_brPAA29D4 + 0x1e164) > 0u
        && (*(unsigned char *)(g_brPAA29D4 + g_5BD8 * 0x438
                               + 0x3868) & 0x10) == 0) {
        *(unsigned int *)(g_brPAA29D8 + 0x1c) &= 0xffffffef;
        *(unsigned char *)(g_brPAA29D8 + 0x2b64) = 1;
        return r;
    }
    *(unsigned int *)(g_brPAA29D8 + 0x1c) |= 0x10;
    *(unsigned char *)(g_brPAA29D8 + 0x2b64) = 0;
    return r;
}
