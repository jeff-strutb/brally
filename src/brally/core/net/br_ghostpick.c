/* br_ghostpick.c -- matching arm for BrGhostPickBlend (0x10005810).
 *
 * Under the shared network mutex, pick the two freshest remote snapshots for a
 * peer slot, interpolate the local car state between them (weighted by how far
 * the newest snapshot has aged), fold the along-track delta into the result,
 * then renormalise the car's orientation quaternion and wrap its angles.
 */
#include <windows.h>
#include <string.h>

int   BrTicks30FromMs(void);
void  BrCarStateLerp(float *pOut, float t, void *pA, void *pB);
float BrVec3Length_100682C0(float *pV);
void  BrQuatNormalise_1006D410(float *pQuat);
void  BrAngleWrap_10005C40(float *pAngle);
void  BrAngleWrap_10005C70(float *pAngle);
void  BrAngleWrap_10005CA0(float *pAngle);

extern HANDLE DAT_10226a64;
extern int    DAT_1007b264;
extern unsigned int DAT_1021ce58;
extern float  DAT_100770b0;

/* @t4-pass 0x10005810 1 2026-09-20 probes 11 bytes 1000 insns 310 regions 2 rows 76 census no  (fn.py: loop-condition and decrement spellings, compare operand order, += fold, address-of vs +1, cast drop, O2/O2y/O2p/Ox/O1.  Best -14 bytes at O2p, -70 at O2; none reached 0.  Residue is per-slot register-vs-stack spilling + rep-movsd-vs-loop + x87 divide width.) */
/* @t4-pass 0x10005810 2 2026-09-20 probes 11 bytes 1000 insns 310 regions 2 rows 76 census yes  (census of unpaired multiset: MISSING mov [esp+S],R + mov R,[base+I] = the original's spill-and-reread of the per-slot scalars vs our register residency; MISSING rep movsd = the snapshot block copy vs our unrolled loop; EXTRA fild qword / fstp reg vs MISSING fld reg + fidiv = x87 conversion-width in the age-weight divide; the add/sub esp deltas are the frame size that follows from the spill choice.  Every divergent row is register allocation, a copy idiom, or x87 width of identical logic.  A5 oracle EQUIVALENT on 48 seeds.) */
/* One blend step: interpolate between snapshot iOld and the newest snapshot
 * iNew by how far iNew has aged (capped at six ticks), then fold the change in
 * along-track distance into the result.  Written as an __inline helper and
 * expanded at each of its three call sites: VC5 never cross-jumps inline
 * expansions, which is why the original keeps three byte-identical copies
 * where a plain repeated block would be tail-merged. */
static __inline void BrGhostBlendStep(float *pOut, unsigned int *pSlot,
                                      int iNew, int iOld, int dt)
{
  int t;
  float a, b;
  float v[3];

  t = BrTicks30FromMs();
  t = t - ((int *)pSlot)[3 + iNew];
  if (6 < t) {
    t = 6;
  }
  BrCarStateLerp(pOut, (float)(t + dt) / (float)dt, pSlot + iOld * 0x28 + 0x16,
                 pSlot + iNew * 0x28 + 0x16);
  v[0] = *(float *)&pSlot[iNew * 0x28 + 0x1a];
  v[1] = *(float *)&pSlot[iNew * 0x28 + 0x1b];
  v[2] = *(float *)&pSlot[iNew * 0x28 + 0x1c];
  a = BrVec3Length_100682C0(v);
  v[0] = pOut[4];
  v[1] = pOut[5];
  v[2] = pOut[6];
  b = BrVec3Length_100682C0(v);
  pOut[6] = (a - b) + pOut[6];
}

/* WHAT IT DOES: under the network mutex, choose the two freshest snapshots for
 * this peer, interpolate the car state between them by the newest snapshot's
 * age, add the along-track distance delta, release the mutex and renormalise
 * the car's quaternion and wrap its three angle channels; returns 0 if the slot
 * has too few snapshots, else 1.
 *
 * pOut is used directly, never through a copy: VC5 caches it in ebp for the
 * body and the early-out block re-reads it from its argument slot, while the
 * best-time locals of the two picks pack into that slot. */
/* @implements 0x10005810 glide BrGhostPickBlend */
unsigned int BrGhostPickBlend(float *pOut, int slot)
{
  unsigned int *pSlot;
  unsigned int bestT;
  int k;              /* first pick's counter, then the older snapshot */
  int j;              /* second pick's counter, then the age difference */
  unsigned int *pTime;
  int iNew;
  float *pQ3;
  float *pQ1;
  float *pQ2;
  HANDLE ahWait[2];     /* the two mutexes, waited on together */

  pSlot = &DAT_1021ce58 + slot * 0x25e;
  ahWait[0] = DAT_10226a64;
  ahWait[1] = (HANDLE)*pSlot;
  WaitForMultipleObjects(2, ahWait, 1, 0xffffffff);
  if (slot == DAT_1007b264)
    goto done;
  if ((int)((int *)pSlot)[0x156] >= 2) {
    iNew = 0;
    k = 0;
    bestT = 0;
    pTime = pSlot + 3;
    do {
      if ((pTime[0xb] != 0) && (*pTime > bestT)) {
        iNew = k;
        bestT = *pTime;
      }
      k = k + 1;
      pTime = pTime + 1;
    } while (k < 8);
    k = 0;
    j = 0;
    bestT = 0;
    pTime = pSlot + 3;
    do {
      if ((pTime[0xb] != 0) && (*pTime > bestT) && (j != iNew)) {
        k = j;
        bestT = *pTime;
      }
      j = j + 1;
      pTime = pTime + 1;
    } while (j < 8);
    j = ((int *)pSlot)[3 + iNew] - ((int *)pSlot)[3 + k];
    if (((int *)pSlot)[0x158] == iNew) {
      if ((int)((int *)pSlot)[0x15a] < 0xf) {
        ((int *)pSlot)[0x15a] = ((int *)pSlot)[0x15a] + 1;
        ((int *)pSlot)[0x159] = ((int *)pSlot)[0x159] + 1;
      }
      if (j != 0) {
        BrGhostBlendStep(pOut, pSlot, iNew, k, j);
      } else {
        memcpy(pOut, pSlot + ((int *)pSlot)[0x158] * 0x28 + 0x16, 0xa0);
        goto done;
      }
    } else {
      ((int *)pSlot)[0x158] = iNew;
      if ((((unsigned int)((int *)pSlot)[3 + iNew] < (unsigned int)(((int *)pSlot)[0x159] + 1)) &&
           ((int)((int *)pSlot)[0x15b] < 0x14)) && (j != 0)) {
        ((int *)pSlot)[0x15a] = 1;
        ((int *)pSlot)[0x15b] = ((int *)pSlot)[0x15b] + 1;
        ((int *)pSlot)[0x159] = ((int *)pSlot)[0x159] + 1;
        BrGhostBlendStep(pOut, pSlot, iNew, k, j);
      } else {
        ((int *)pSlot)[0x15a] = 0;
        ((int *)pSlot)[0x15b] = 0;
        ((int *)pSlot)[0x159] = ((int *)pSlot)[3 + iNew];
        BrGhostBlendStep(pOut, pSlot, iNew, k, j);
      }
    }
  } else {
    pOut[0x1f] = 400.0f;
    ReleaseMutex((HANDLE)*pSlot);
    ReleaseMutex(DAT_10226a64);
    return 0;
  }

done:
  ReleaseMutex((HANDLE)*pSlot);
  ReleaseMutex(DAT_10226a64);
  BrAngleWrap_10005C40(pOut);
  pQ1 = pOut + 1;
  BrAngleWrap_10005C40(pQ1);
  pQ2 = pOut + 2;
  BrAngleWrap_10005C40(pQ2);
  pQ3 = pOut + 3;
  BrAngleWrap_10005C40(pQ3);
  if (*pOut + *pQ1 + *pQ3 + *pQ2 == DAT_100770b0) {
    *pOut = 1.0f;
    *pQ1 = 0.0f;
    *pQ2 = 0.0f;
    *pQ3 = 0.0f;
  } else {
    BrQuatNormalise_1006D410(pOut);
  }
  BrAngleWrap_10005C70(pOut + 4);
  BrAngleWrap_10005C70(pOut + 5);
  BrAngleWrap_10005CA0(pOut + 6);
  return 1;
}
