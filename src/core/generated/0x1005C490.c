/* Auto-generated from Ghidra decompilation — 0x1005C490 */
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
/* 0x1006FD50 (D3D 0x10076AE0, slice1_09.c): thiscall(this, int) -- spelled
 * as __fastcall plus a struct-typed argument so the value stays on the
 * stack. */
typedef struct { int n; } BrEntityIndexArg;
void __fastcall BrEntitySetIndex(void *pEntity, BrEntityIndexArg index);

/* The difficulty table at 0x100B3024: 24-byte records, one per level.
 * +0x00 two signed bytes (entity indices), +0x02 a bit mask of the entity
 * indices in use, +0x04 the ten two-byte pairs 0x10058A30 reads. */
typedef struct BrLevelRec {
  char aIdx[2];
  short mask;
  unsigned char aPair[20];
} BrLevelRec;
extern BrLevelRec DAT_100b3024[];

extern unsigned char *DAT_10af2094;   /* g_pBrMenuACED34: the menu record  */
extern int DAT_100b3858;              /* g_brRaceNEntrant                  */
extern int DAT_100a9360;              /* g_br0AA010, the game mode         */
extern int DAT_10af3bb0;

/* RESIDUE (2026-09-04): 201 vs 204 B, regnorm 3+3, three sites. (1) The
 * original's pos<count arm (mode==0 -> set(aIdx[0])) is laid out LAST, after
 * the mode-1/6 arm, and `cmp edi,ebp; jl` branches AWAY to it; ours lays it
 * inline with `jge` over it. Every wrapper spelling that moves it to the
 * tail (`if (pos >= count) {...}` with the arm after or as `else`, with or
 * without `return`s, mode test as wrapper or guard, the last test as `>`
 * guard / `<=` guard / if-else) makes VC5 cross-jump the pos>count arm's
 * `push eax; call; pops; ret` into that tail (188 B, 8 insns short); the
 * original keeps all four epilogues and pushes the tail's value from EDX.
 * A flat if / else-if / else chain lays the pos<count arm inline (this
 * file). (2) The pos<=count arm indexes as `lvl*24 - count`, then
 * `[edx+edi+base]` (shl 3 + sub, base folded into the load); ours folds
 * `edi+edx*8` into a lea and subtracts after. (3) The pos<count arm's
 * `movsx` lands in edx in the original, eax here -- probably a consequence
 * of (1). Layout is the lever; not found in ~30 minutes. */
/* WHAT IT DOES: choose which entity (car model) a race entrant drives, from
 * the difficulty level in the menu record. Entrants below the entrant count
 * take the level's first table index (single-player only); in modes 1 and
 * 6 the car just copies a global into its +0x29A8 slot; otherwise the car's
 * slot above the entrant count picks the level's index byte, falling back
 * to the lowest bit set in the level's mask (5 if the mask is empty). */
/* @implements 0x1005C490 glide BrRaceCarPickIndex */
void __fastcall BrRaceCarPickIndex(unsigned char *pCar)
{
  BrEntityIndexArg arg;
  unsigned int lvl;
  int cl;
  int pos;
  int i;
  short m;

  lvl = DAT_10af2094[4];
  cl = lvl;
  if (cl > 3) cl = 3;
  pos = *(int *)(pCar + 0x140);
  if (pos >= DAT_100b3858) {
    if (DAT_100a9360 == 1 || DAT_100a9360 == 6) {
      *(int *)(pCar + 0x29a8) = DAT_10af3bb0;
      return;
    }
    m = DAT_100b3024[cl].mask;
    for (i = 0; i < 16; i++) {
      if (m & 1) break;
      m >>= 1;
    }
    if (i == 16) i = 5;
    if (pos > DAT_100b3858) {
      arg.n = i;
      BrEntitySetIndex(pCar, arg);
      return;
    }
    arg.n = DAT_100b3024[lvl].aIdx[pos - DAT_100b3858];
    BrEntitySetIndex(pCar, arg);
  } else if (DAT_100a9360 == 0) {
    arg.n = DAT_100b3024[lvl].aIdx[0];
    BrEntitySetIndex(pCar, arg);
  }
}


#endif /* BR_MATCHING_BUILD */
