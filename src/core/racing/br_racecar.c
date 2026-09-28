/* br_racecar.c -- racing: putting a race car back on the grid.
 *
 * The entrant's car pick (0x1005C490, T3) and the between-races car reset
 * (0x1005C560) that calls it; BrEntSlotsReset in br_racestart.c calls the
 * reset for every slot.  Kept as their own translation unit: br_racestart.c
 * declares the menu record and the car table with different types, and the
 * T3 pick's layout depends on what precedes it.
 *
 * The header set below (<windows.h>, <mmsystem.h>, <math.h>, <stdio.h>) is
 * LOAD-BEARING although neither body uses it: dropping those four changes
 * 0x1005C490's code (measured 2026-09-28).  It is TU state, not a need.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>
#include "slice2_15.h"    /* BR_CAR_STRIDE */

#ifndef true
#define true 1
#define false 0
#endif

/* ------------------------------------------------------------------ */
/* 0x1005C490                                                         */
/* ------------------------------------------------------------------ */

typedef int (*funcptr)();

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
 * of (1). Layout is the lever; not found in ~30 minutes.
 * 2026-09-05: the tree had regressed to the wrapper form (188 B, regnorm
 * 3+11, 8 insns short); restored to the flat chain here (204 B, regnorm
 * 3+3, 78/78 insns). Re-confirmed (2) is an addressing-mode fold, not
 * source-permutable: an explicit `off = lvl*24 - count; base[off+pos]`
 * temp still emits `lea [edi+edx*8]; sub` rather than `shl; sub;
 * [edx+edi+base]`. The three sites are one layout wall; T3a. */
/* WHAT IT DOES: choose which entity (car model) a race entrant drives, from
 * the difficulty level in the menu record. Entrants below the entrant count
 * take the level's first table index (single-player only); in modes 1 and
 * 6 the car just copies a global into its +0x29A8 slot; otherwise the car's
 * slot above the entrant count picks the level's index byte, falling back
 * to the lowest bit set in the level's mask (5 if the mask is empty). */
/* @t4-pass 0x1005C490 1 2026-09-07 probes 148 bytes 204 insns 78 regions 3 rows 6 census yes  (tools/crank.py) */
/* @t4-pass 0x1005C490 2 2026-09-07 probes 101 bytes 204 insns 78 regions 3 rows 6 census yes  (tools/crank.py) */
/* @t3 0x1005C490 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 204/201 insns 78/78 rows 3+3 regions 3 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * RESIDUE: one addressing-mode fold across three table-index sites (the
 * `lea;sub` vs `shl;sub;[base+idx]` layout wall in the dossier above), not
 * source-permutable. Do not reopen before the end-grind (project rule 12). */
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
  if (pos < DAT_100b3858) {
    if (DAT_100a9360 == 0) {
      arg.n = DAT_100b3024[lvl].aIdx[0];
      BrEntitySetIndex(pCar, arg);
    }
  } else if (DAT_100a9360 == 1 || DAT_100a9360 == 6) {
    *(int *)(pCar + 0x29a8) = DAT_10af3bb0;
  } else {
    m = DAT_100b3024[cl].mask;
    for (i = 0; i < 16; i++) {
      if (m & 1) break;
      m >>= 1;
    }
    if (i == 16) i = 5;
    if (pos > DAT_100b3858) {
      arg.n = i;
      BrEntitySetIndex(pCar, arg);
    } else {
      arg.n = DAT_100b3024[lvl].aIdx[pos - DAT_100b3858];
      BrEntitySetIndex(pCar, arg);
    }
  }
}

/* ------------------------------------------------------------------ */
/* 0x1005C560                                                         */
/* ------------------------------------------------------------------ */

/* 0x1005C560 -- a car's between-races reset. */

/* Both callees are thiscall(this) -- __fastcall carries the one register
 * argument exactly (see 0x1005C490.c). */
void __fastcall FUN_1006ff00(void *pCar);
void __fastcall BrRaceCarPickIndex(unsigned char *pCar);   /* 0x1005C490 */

extern unsigned char DAT_10af1208[];   /* car0 base                        */
extern int           DAT_100a9360;     /* the game mode                    */
extern unsigned char DAT_100b2fd8[];   /* 3 bytes of livery colour per car */
extern unsigned char DAT_10b1cbf0[];   /* 0x14C-byte comms records, 2 of   */
extern int           DAT_100aa2a8;     /* entries in the comms tables      */

/* WHAT IT DOES: puts one car back to its starting state between races.
 * Works out which car this is from its own address, restores its livery
 * colour from the table (skipped in mode 6), and for the two player cars
 * wires up and wipes the comms record -- header fields to their fixed
 * values and both per-entry tables to zero. Non-player cars drop their
 * record instead. Ends by re-picking which model the car drives. */
/* @implements 0x1005C560 glide BrRaceCarReset */
void __fastcall BrRaceCarReset(unsigned char *pCar)
{
    int idx;
    int i;

    FUN_1006ff00(pCar);
    idx = (int)(pCar - DAT_10af1208) / (int)BR_CAR_STRIDE;
    *(int *)(pCar + 0x140) = idx;

    if (DAT_100a9360 != 6) {
        /* Three int-typed locals: the original zero-extends all three
         * bytes into registers before any store. */
        int c2 = DAT_100b2fd8[idx * 3 + 2];
        int c1 = DAT_100b2fd8[idx * 3 + 1];
        int c0 = DAT_100b2fd8[idx * 3];
        pCar[0x29ad] = (unsigned char)c1;
        pCar[0x29ac] = (unsigned char)c0;
        pCar[0x29ae] = (unsigned char)c2;
    }

    if (idx < 2) {
        *(unsigned char **)(pCar + 0xe8c) = &DAT_10b1cbf0[idx * 0x14c];
        **(int **)(pCar + 0xe8c) = 0;
        (*(unsigned char **)(pCar + 0xe8c))[4] = 0;
        (*(unsigned char **)(pCar + 0xe8c))[5] = 0;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf0)  = 0x102;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf2)  = 4;
        *(short *)(*(int *)(pCar + 0xe8c) + 0xf4)  = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0xfc)  = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0xf8)  = 0;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x104) = 1;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x100) = 2;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x108) = 0;
        *(int *)(*(int *)(pCar + 0xe8c) + 0x108) = 5;
        for (i = 0; i < DAT_100aa2a8; i++) {
            *(int *)(*(int *)(pCar + 0xe8c) + 0xb0 + i * 4)  = 0;
            *(int *)(*(int *)(pCar + 0xe8c) + 0x10c + i * 4) = 0;
        }
    } else {
        *(int *)(pCar + 0xe8c) = 0;
    }

    BrRaceCarPickIndex(pCar);
    *(int *)(pCar + 0xe88) = 0;
}

#endif /* BR_MATCHING_BUILD */
