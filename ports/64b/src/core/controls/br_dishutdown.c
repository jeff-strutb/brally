/* br_dishutdown.c -- controls: giving DirectInput back to Windows.
 *
 * RESPONSIBILITY: reading what the player is doing -- specifically the last
 * step of it. The bring-up side of this pair lives in
 * src/core/cpp/0x10059350.cpp (Input59350::CreateDevice), which creates the
 * device off the same IDirectInput root this file releases.
 *
 * The root object is reference-counted by the game, not just by COM: several
 * subsystems ask for DirectInput and the last one to leave is the one that
 * actually tears it down. Only IUnknown::Release (vtable +0x08) is called
 * here, so the interface is declared with just that slot.
 */
#include "slice1_10.h"   /* br_globals: its objects */
#include "slice3_39.h"   /* br_globals: its objects */
#include "br_match.h"


struct BrDI;

typedef struct BrDIVtbl {
    void *QueryInterface;                               /* +0x00 */
    void *AddRef;                                       /* +0x04 */
    unsigned long(__stdcall *Release)(struct BrDI *);   /* +0x08 */
} BrDIVtbl;

typedef struct BrDI {
    BrDIVtbl *lpVtbl;
} BrDI;

/* 0x118EEF00 -- how many subsystems still hold DirectInput open. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 0x118EEE88 -- the IDirectInput root; named g_pDInput in 0x10059350.cpp. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 0x10AC61E0 -- the input record built on top of that root. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x10059320, a thiscall with no stack argument: the input record's own
 * teardown, matched in src/core/cpp/0x10059320.cpp as BrNavRelease_10059320. */
/* BrNavRelease_10059320: prototype in br_funcs.h */
/* 0x1007456C -- the linker's jmp[IAT] stub for MSVCRT's operator delete.
 * Fenced in config/fenced.csv as an import thunk, so it is declared, never
 * defined, and the call is a plain cdecl one. */
/* BrOperatorDelete: prototype in br_funcs.h */

/* WHAT IT DOES: drop one user's claim on DirectInput and, when that was the
 * last one, shut it down: destroy and free the input record built on the
 * root, then Release the IDirectInput root itself and clear both pointers.
 * Always reports success, including when it did nothing. */
/* @implements 0x100720A0 glide BrDInputShutdown */
int BrDInputShutdown(void)
{
    void *pRec;
    BrDIVtbl *vt;

    if (--g_BrDInputUsers == 0 && g_pDInput != 0) {
        pRec = g_pBrAC61E0;
        if (pRec != 0) {
            BrNavRelease_10059320(pRec);
            BrOperatorDelete(pRec);
        }
        /* The vtable MUST be captured into a local before the null-out:
         * that is what lets VC5 schedule the store INTO the call setup,
         * between `push eax` and `call [ecx+8]`, the way the original has
         * it. Writing the call as one `g_pDInput->lpVtbl->Release(...)`
         * expression keeps the store in source order and costs 8 bytes. */
        vt = g_pDInput->lpVtbl;
        g_pBrAC61E0 = 0;
        vt->Release(g_pDInput);
        g_pDInput = 0;
    }
    return 1;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
typedef int (__stdcall *CC_std_1)();   /* COM method: this + arguments */

/* WHAT IT DOES: the same reference-counted release for the joystick/force-
 * feedback devices -- frees both interfaces when the last user goes, and
 * ignores an unmatched call. */
/* @implements 0x10072840 glide BrExt_10079550 */
void BrExt_10079550(void)

{
  DAT_118eef18 = DAT_118eef18 + -1;
  if (DAT_118eef18 < 0) {
    DAT_118eef18 = 0;
    return;
  }
  if (DAT_118eef18 == 0) {
    if (DAT_118eef14 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eef14) + 8))(DAT_118eef14);
      DAT_118eef14 = (int *)0x0;
    }
    if (DAT_118eef04 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eef04) + 8))(DAT_118eef04);
      DAT_118eef04 = (int *)0x0;
    }
    if (DAT_118eeeec != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 32))(DAT_118eeeec);
      (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 8))(DAT_118eeeec);
      DAT_118eeeec = (int *)0x0;
    }
  }
  return;
}
