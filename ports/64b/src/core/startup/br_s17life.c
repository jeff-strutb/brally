/* br_s17life.c -- startup: bring the S17 subsystem up and take it down.
 *
 * Filed out of the address batch slice2_17.c.  The cross-slice declarations
 * below are that file's, copied verbatim; the frame-timer object itself is
 * still DEFINED there (it is named only by its address, and slice8_86.c
 * reaches it too), so it is declared here.
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_17.h"
#undef BrPtrListContains

/* 0x106806B0 -- the 0x24-byte frame-timer object 0x100751D0 / 0x10075240
 * operate on.  Defined in slice2_17.c. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* XSLICE 0x100751D0
 * 0x1002C2A0 tail-jumps into it with the object in ecx and nothing on the
 * stack: it is a C++ __thiscall method. MSVC 5.0's C front end cannot spell
 * __thiscall, but for a single pointer argument __fastcall is byte-identical
 * at the call site (arg1 in ecx, no stack cleanup), so that is what the
 * matching build uses. Off MSVC the qualifier vanishes and it is an ordinary
 * one-argument function. */
#define BRS17_THISCALL __fastcall
/* BrX100751D0: prototype in br_funcs.h */
/* XSLICE 0x1002C2C0 */
/* BrX1002C2C0: prototype in br_funcs.h */
/* 0x1007E8B0 is the CRT's atexit (0x1007E820 wrapped, returning 0 or -1). */
/* BrXAtExit: prototype in br_funcs.h */

/* 0x1002C2A0 */
/* WHAT IT DOES: lets go of one particular long-lived object. What that object
 * is was not established here. */
/* @implements 0x1002C2A0 d3d BrS17Release */
void BrS17Release(void)
{
    /* `mov ecx, 0x106806B0 / jmp 0x100751D0`. The operand is an IMMEDIATE --
     * the ADDRESS of the frame-timer object, not a pointer loaded out of a
     * field -- so the object itself is named here and its address taken.
     * Routing this through g_s17.pThis6806B0 costs a `mov eax, [mem]` the
     * original does not have. */
    BrX100751D0(g_br6806B0);
}

/* 0x1002C2B0 */
/* WHAT IT DOES: books a tidy-up routine to run automatically when the program
 * exits, and reports whether the booking was accepted. */
/* @implements 0x10019820 glide BrS17RegisterAtExit */
int BrS17RegisterAtExit(void)
{
    return BrXAtExit(BrX1002C2C0);
}


/* WHAT IT DOES: release the S17 subsystem and register its atexit handler. */
/* @implements 0x10019800 glide BrS17Init */

int BrS17Init(void)

{
  BrS17Release();
  BrS17RegisterAtExit();
  return;
}

