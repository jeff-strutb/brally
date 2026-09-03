/* Auto-generated from Ghidra decompilation — 0x10017F30 */
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
extern int DAT_100a7514;
extern int DAT_100a7518;
extern int DAT_104b16a4;
extern int DAT_104b16a8;


/* The slice2_16.c body models these four as fields of a BrFadeState the
 * original does not have: it takes no argument and reads nothing off the
 * stack, addressing all four absolutely (the "state-pointer argument that the
 * original never loads is absolute globals" idiom).  The port-friendly
 * struct body stays in slice2_16.c untagged, exactly as BrFadeRelease
 * (0x10017F10) is split.
 *
 * 0x100A7514 and 0x100A7518 are the grSstWinOpen screen width and height
 * (d3d 0x100A81C0 / 0x100A81C4, named g_Br0A81C0 / g_Br0A81C4 in
 * slice3_39.h); the destinations are d3d 0x105754FC / 0x10575500, the pair
 * slice2_16.h calls f5754FC and pos.  Note the crossed order -- the WIDTH
 * goes to the HIGHER destination.
 *
 * The 16-byte `jmp +0x0b` / 11-nop link-stage preamble in front of the body
 * is in config/preambles.csv and is never spelled here. */
/* @implements 0x10017F30 glide BrFadeLatch */
void BrFadeLatch(void)

{
  DAT_104b16a8 = DAT_100a7514;
  DAT_104b16a4 = DAT_100a7518;
}


#endif /* BR_MATCHING_BUILD */
