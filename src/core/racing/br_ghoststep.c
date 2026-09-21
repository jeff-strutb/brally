/* br_ghoststep.c -- matching arm for BrGhostPlaybackStep (0x10061F60).
 *
 * Advance one ghost/replay playback controller for the frame: drive the ghost
 * car's fade in/out state, snapshot its position for the next frame, run the
 * car's own update callback, and -- when the recorded path is active -- step the
 * keyframe cursor and interpolate the along-path time.
 */
#ifdef BR_MATCHING_BUILD
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

void  BrRacePathAdvance(int keyA, int keyB, float t, float w);
float BrVec3Length(void *pV);
void  BrVec3Sub(void *pOut, void *pA, void *pB);
void  BrVec3ScaleBy(void *pV, float s);
void  BrRaceGateStep(void);
extern void BrCtlAi(void);   /* 0x1005E690 -- compared as a function pointer */

extern int   DAT_10226a48;
extern int   DAT_100a9360;
extern int   DAT_105ccb5c;
extern int   DAT_100b3858;
extern float DAT_106e9d8c;
extern int   DAT_106eed48;
extern float DAT_10077a0c;
extern float DAT_10077a10;
extern float DAT_10077a14;
extern float DAT_100778f8;
extern float DAT_100778d8;
extern int   DAT_10b1cbec;
extern int   DAT_10af07f0;
extern int   DAT_10b1ce98;
extern int   DAT_10b1ce9c;
extern int   DAT_10b1cea0;
extern unsigned char DAT_10af2108;
extern unsigned char DAT_10af222c;

extern int DAT_118eef48, DAT_118eef4c, DAT_118eef54, DAT_118eef60, DAT_118eef64;
extern int DAT_118eef6c, DAT_118eef78, DAT_118eef7c, DAT_118eef84, DAT_118eef90;
extern int DAT_118eef94, DAT_118eef9c, DAT_118eefa8, DAT_118eefac, DAT_118eefb4;

/* WHAT IT DOES: advance one ghost playback controller -- fade the ghost car in
 * or out per the race state, copy its position into the previous-frame slot, run
 * its update callback, and while the recorded path is live step the keyframe
 * cursor and interpolate the path time; clears the shared ghost scratch when the
 * playback is disabled. */
/* RESIDUE (2026-09-20): bytes 1087/1073, insn gap 1, 5 masked regions.  Hand
 * transcription (this function had only a Ghidra draft, no project code).
 * Reading the accumulated path time as a float, casting the frame counter
 * signed before the widen, ordering the fade comparisons so the car value loads
 * first, and pinning the path constant to 2.22f closed the gap to a single
 * instruction over.  The remainder is colouring, read out of the diff:
 *   - guard polarity on the flags-word branches (je vs jne / jg fall-through);
 *   - the large-stride index multiplies (*0x28 / *0xada / *0x2b68) chosen as
 *     lea+shl chains here vs the original's shl sequence;
 *   - the path-time divide as fdivp vs fdivr (operand order on the x87 stack).
 * A5 oracle EQUIVALENT on 48 seeds (oracle_profiles _gs_bss/_gs_buf): the fade
 * state machine, the flag-word writes, the position copy and the speed
 * accumulation are all verified -- negative controls on each fire DIFF; the car
 * update vtable slot is pinned to BrCtlAi and that callback plus the path /
 * vector / gate helpers are black-boxed, while BrVec3Length runs.  No source
 * lever moved the byte count off 1087 across 12 spellings and 4 opt levels. */
/* @t4-pass 0x10061F60 1 2026-09-20 probes 12 bytes 1087 insns 307 regions 5 rows 31 census no  (fn.py: &-mask forms, bool pointer test, add/and/or commutes, keyframe-index reassoc, O2/O2y/O2p/Ox.  Best +14 bytes/+1 insn at O2; none reached 0.  Residue is guard polarity + large-stride index lea/shl + fdivp-vs-fdivr.) */
/* @t4-pass 0x10061F60 2 2026-09-20 probes 12 bytes 1087 insns 307 regions 5 rows 31 census yes  (census of unpaired multiset: EXTRA jne + MISSING je = guard polarity on the flags branches; EXTRA lea [R+R*K]/[R*K] + sub R,R vs MISSING shl R,3 = the *0x28/*0xada/*0x2b68 stride multiplies as lea chains vs shifts; EXTRA fdivp vs MISSING fdivr = x87 divide operand order.  Every divergent row is branch shape, an index-multiply idiom, or x87 operand order of identical logic.  A5 oracle EQUIVALENT on 48 seeds.) */
/* @t3 0x10061F60 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1087/1073 insns 307/306 rows 15+16 regions 5 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 */
/* @implements 0x10061F60 glide BrGhostPlaybackStep */
void BR_THISCALL1 BrGhostPlaybackStep(unsigned int *param_1)
{
  int iVar1;
  unsigned int *puVar2;
  void (*pcVar3)(int);
  float fVar4;
  float fVar5;

  if ((DAT_10226a48 == 0) || (param_1[0x18] != 0)) {
    iVar1 = param_1[0x18];
    if ((iVar1 != 0) && ((*(void **)(iVar1 + 0xf08) == (void *)BrCtlAi && (DAT_100a9360 != 5)))) {
      **(unsigned int **)(iVar1 + 0x29c0) = **(unsigned int **)(iVar1 + 0x29c0) & 0xf000000;
      *(int *)(*(int *)(param_1[0x18] + 0x29c0) + 0x20) = 0;
    }
    if (DAT_105ccb5c != 0) {
      DAT_118eef48 = 0;
      DAT_118eef4c = 0;
      DAT_118eef54 = 0;
      DAT_118eef60 = 0;
      DAT_118eef64 = 0;
      DAT_118eef6c = 0;
      DAT_118eef78 = 0;
      DAT_118eef7c = 0;
      DAT_118eef84 = 0;
      DAT_118eef90 = 0;
      DAT_118eef94 = 0;
      DAT_118eef9c = 0;
      DAT_118eefa8 = 0;
      DAT_118eefac = 0;
      DAT_118eefb4 = 0;
      return;
    }
    if ((param_1[0x1a] & 1) == 0) {
      iVar1 = param_1[0x18];
      if ((param_1[0x1a] & 2) == 0) {
        if (iVar1 != 0) {
          if (*(char *)(iVar1 + 0x29af) == '\x02') {
            *(float *)(iVar1 + 0x29b0) = *(float *)(iVar1 + 0x29b0) - DAT_106e9d8c * DAT_10077a0c;
            if ((DAT_100a9360 == 2) && (param_1[0x19] != 0)) {
              if (*(float *)(param_1[0x18] + 0x29b0) > DAT_10077a10) {
                *(int *)(param_1[0x18] + 0x29b0) = 0x3ec00000;
              }
            } else if (*(float *)(param_1[0x18] + 0x29b0) >= DAT_100778f8) {
              *(int *)(param_1[0x18] + 0x29b0) = 0x3f800000;
              *(unsigned char *)(param_1[0x18] + 0x29af) = 0;
            }
          }
          iVar1 = param_1[0x18];
          *(int *)(iVar1 + 0xf80) = *(int *)(iVar1 + 0x30);
          *(int *)(iVar1 + 0xf84) = *(int *)(iVar1 + 0x34);
          *(int *)(iVar1 + 0xf88) = *(int *)(iVar1 + 0x38);
          pcVar3 = *(void (**)(int))(param_1[0x18] + 0xf08);
          if (pcVar3 != (void (*)(int))0x0) {
            (*pcVar3)(param_1[0x18]);
          }
          iVar1 = param_1[0x18];
          *(float *)(iVar1 + 0x1034) =
               *(float *)(iVar1 + 0x1030) * DAT_106e9d8c + *(float *)(iVar1 + 0x1034);
          return;
        }
        param_1[3] = *param_1;
        param_1[4] = param_1[1];
        param_1[5] = param_1[2];
        iVar1 = param_1[10] + param_1[0xb] * 0x28;
        fVar4 = (((float)((int)param_1[0x11] + 1) * *(float *)(DAT_106eed48 + 100) - *(float *)(param_1 + 0x14)) -
                 *(float *)(iVar1 + 0x8c)) /
                (*(float *)(iVar1 + 100) - *(float *)(iVar1 + 0x8c));
        if ((&DAT_10af2108)[(DAT_100b3858 + param_1[0x1d]) * 0xada] == 0) {
          BrRacePathAdvance(param_1[10], param_1[0xb], fVar4, 2.22f);
          fVar5 = *(float *)(param_1 + 0x14) - DAT_10077a14;
        } else {
          fVar5 = BrVec3Length(&DAT_10af222c + (DAT_100b3858 + param_1[0x1d]) * 0x2b68);
          BrRacePathAdvance(param_1[10], param_1[0xb], fVar4, fVar5 * DAT_106e9d8c);
          fVar5 = BrVec3Length(&DAT_10af222c + (DAT_100b3858 + param_1[0x1d]) * 0x2b68);
          fVar5 = fVar5 * DAT_106e9d8c + *(float *)(param_1 + 0x14);
        }
        *(float *)(param_1 + 0x14) = fVar5;
        param_1[10] = DAT_10b1cbec;
        param_1[0xb] = DAT_10af07f0;
        *param_1 = DAT_10b1ce98;
        param_1[1] = DAT_10b1ce9c;
        param_1[2] = DAT_10b1cea0;
        BrVec3Sub(param_1 + 6, param_1, param_1 + 3);
        BrVec3ScaleBy(param_1 + 6, DAT_100778f8 / DAT_106e9d8c);
        BrRaceGateStep();
      } else if (iVar1 != 0) {
        if ((DAT_100a9360 != 0) || ((int)param_1[0x19] < DAT_100b3858)) {
          **(unsigned int **)(iVar1 + 0x29c0) = 0xc0000;
          *(unsigned char *)(*(int *)(param_1[0x18] + 0x29c0) + 0x24) = 0x81;
          *(int *)(*(int *)(param_1[0x18] + 0x29c0) + 0x20) = 0xbf800000;
        }
        if ((DAT_100a9360 == 0) && (DAT_100b3858 <= (int)param_1[0x19])) {
          *(unsigned char *)(param_1[0x18] + 0x29af) = 2;
          *(float *)(param_1[0x18] + 0x29b0) = *(float *)(param_1[0x18] + 0x29b0) - DAT_106e9d8c;
          if (*(float *)(param_1[0x18] + 0x29b0) < DAT_100778d8) {
            *(int *)(param_1[0x18] + 0x29b0) = 0;
          }
        }
        iVar1 = param_1[0x18];
        *(int *)(iVar1 + 0xf80) = *(int *)(iVar1 + 0x30);
        *(int *)(iVar1 + 0xf84) = *(int *)(iVar1 + 0x34);
        *(int *)(iVar1 + 0xf88) = *(int *)(iVar1 + 0x38);
        pcVar3 = *(void (**)(int))(param_1[0x18] + 0xf08);
        if (pcVar3 != (void (*)(int))0x0) {
          (*pcVar3)(param_1[0x18]);
          return;
        }
      }
    } else if (param_1[0x18] != 0) {
      puVar2 = *(unsigned int **)(param_1[0x18] + 0x29c0);
      *puVar2 = *puVar2 | 0x40000;
      *(int *)(param_1[0x18] + 0xe70) = 0;
      iVar1 = param_1[0x18];
      *(int *)(iVar1 + 0xf80) = *(int *)(iVar1 + 0x30);
      *(int *)(iVar1 + 0xf84) = *(int *)(iVar1 + 0x34);
      *(int *)(iVar1 + 0xf88) = *(int *)(iVar1 + 0x38);
      pcVar3 = *(void (**)(int))(param_1[0x18] + 0xf08);
      if (pcVar3 != (void (*)(int))0x0) {
        (*pcVar3)(param_1[0x18]);
      }
      *(int *)(param_1[0x18] + 0xe70) = 0;
      return;
    }
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
