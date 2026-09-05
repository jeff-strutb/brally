/* Auto-generated from Ghidra decompilation — 0x10058A30 */
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
extern int DAT_1007b324;            /* the menu's picks (g_br094354..5C)   */
extern int DAT_1007b328;
extern int DAT_1007b32c;
extern int DAT_10ac5b44;            /* the live race settings              */
extern int DAT_10ac5b48;
extern int DAT_10ac5b4c;
extern int DAT_10ac5b50;
extern int DAT_10ac5bf8;
extern int DAT_10ac5bfc;            /* g_brAA28A4                          */
extern int DAT_10ac5c10;
extern int DAT_10b71530;            /* g_brB4E1D0                          */
extern int DAT_10ac5a48;            /* g_brAA26F0                          */
extern unsigned char DAT_10ac5a4c;  /* g_aBrAA26F4[0]                      */
extern unsigned char DAT_10ac5a4d;  /* g_aBrAA26F4[1]                      */
extern int DAT_100a9360;            /* g_br0AA010, the game mode           */
extern unsigned char DAT_100b3028[][12][2];/* g_aBr0B3820: [level][pick][2]  */
extern int DAT_100b3014;            /* g_br0B380C                          */
extern int DAT_10226e80;            /* g_br22B350                          */
extern int DAT_10ac5b38;            /* the low mask word                   */
extern unsigned short DAT_10ac5b3a; /* the high mask word                  */
extern int DAT_10ac5d68;            /* g_brAA2A10                          */
extern int DAT_10ac5d6c;            /* g_brAA2A14                          */

/* WHAT IT DOES: commit the menu's choices into the live race settings --
 * copies the three picked values and the two-byte difficulty pair across,
 * looks the pair up in the 12-wide byte table to set the two derived
 * globals (single-player mode only), and merges the two selection mask
 * words into the accumulated masks. The Glide twin of the tail of
 * slice5_63.c's menu function. Fields are written in ADDRESS order
 * (0x10ac5a48 group, then 0x10ac5b44 group) and the pair table is a
 * [level][pick][2] byte array -- both decide the register schedule. */
/* @implements 0x10058A30 glide BrRaceSettingsCommit */
void BrRaceSettingsCommit(void)
{
  DAT_10ac5a48 = DAT_10ac5bf8;
  DAT_10ac5a4c = (unsigned char)DAT_10ac5c10;
  DAT_10ac5a4d = (unsigned char)DAT_10ac5bfc;
  DAT_10ac5b44 = DAT_1007b324;
  DAT_10ac5b48 = DAT_1007b32c;
  DAT_10ac5b4c = DAT_1007b328;
  DAT_10ac5b50 = DAT_10b71530;
  if (DAT_100a9360 == 0) {
    DAT_100b3014 = DAT_100b3028[DAT_10ac5a4c][DAT_10ac5a4d][0];
    DAT_10226e80 = DAT_100b3028[DAT_10ac5a4c][DAT_10ac5a4d][1];
  }
  DAT_10ac5d68 |= DAT_10ac5b38 & 0xffff;
  DAT_10ac5d6c |= DAT_10ac5b3a;
}


#endif /* BR_MATCHING_BUILD */
