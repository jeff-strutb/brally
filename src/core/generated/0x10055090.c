/* Auto-generated from Ghidra decompilation — 0x10055090 */
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
/* 0x100AAD08: the UI sprite table, 24-byte entries (see include/br_uispr.h).
 * +0x00 is read as a WORD and pushed as-is, so the callee's third parameter
 * is declared short here; +0x04 is the source rect, +0x14 the blit flag. */
extern unsigned char g_aBrUiSprite[];
int BrSprFontDraw(int, int, short, void *, int);

/* WHAT IT DOES: draw one UI sprite from the sprite table at a screen
 * position given in floats: looks the entry up by index, truncates x and y
 * to integers and hands its image, source rect and blit flag to the clipped
 * sprite blitter. Always reports success. */
/* @implements 0x10055090 glide BrUiSprDrawAt */
int __stdcall BrUiSprDrawAt(unsigned short iSpr, float x, float y)
{
  unsigned int off = (unsigned int)iSpr * 3u;
  off <<= 3;
  BrSprFontDraw((int)x, (int)y, *(short *)(g_aBrUiSprite + off),
                g_aBrUiSprite + off + 4u,
                *(int *)(g_aBrUiSprite + off + 0x14u));
  return 1;
}


#endif /* BR_MATCHING_BUILD */
