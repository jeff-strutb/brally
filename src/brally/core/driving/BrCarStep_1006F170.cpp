/* WHAT IT DOES: step one car for the frame -- cast the wheel collision ray and
 * apply control input (live) or replay the recorded transform, write the four
 * wheels' suspension/slip fields from the body slip and spin, run the net and
 * steer sub-steps, recompute the scalar speed, and advance the chase camera for
 * the entry that owns it. */
/* @implements 0x1006F170 glide BrCarStep
 * @cpp_kind method
 * @cpp_symbol ?Step@BrCar@@QAEXXZ
 *
 * A BrCar member (thiscall, `this` = the car).  The callees are members too:
 * a real thiscall pushes its stack arguments and loads ecx last, which the
 * C lane's __fastcall stand-in cannot do (it loads ecx before the zero pushes
 * of SetVel/SetAngVel), and BrBitLatch::Take has no dummy edx to zero.
 * Four other source shapes are load bearing:
 *  - the drive-mode branch is a switch (`sub eax,ebp / je`, `dec eax / je`);
 *  - the e68 reads are volatile: the C++ front end otherwise orders each
 *    `e68 * K` product constant first (`fld K; fmul e68`), the original loads
 *    the field and multiplies by the constant;
 *  - the |1cc| test is a ternary, so the compare reads the field before the
 *    value is loaded for the fchs;
 *  - the camera-owner search indexes the table (`tbl[i * 0x16]`), which VC5
 *    strength-reduces to a pointer initialised after the count guard.
 * The two-constant products are parenthesised (x * K1) * K2: unparenthesised,
 * the C++ front end multiplies by K2 first.
 * The mode test is two ifs (mode 4 tested again in the second), the net/steer
 * branch puts the net arm first, and the speed sum runs e8, ec, f0.
 */
/* @t4-pass 0x1006F170 1 2026-09-20 probes 11 bytes 1289 insns 319 regions 10 rows 30 census no  (fn.py: loop-bound spelling, compare operand order (e68/e6c), puVar1 decl split, bool test, zero-cast args, O2/O2y/O2p/O1/Ox.  Best -6 bytes/-4 insns at O2; none reached 0.  Residue is the zero-register mode compare + x87 reassociation + peeled tail with duplicated epilogue.) */
/* @t4-pass 0x1006F170 2 2026-09-20 probes 11 bytes 1289 insns 319 regions 10 rows 30 census yes  (census of unpaired multiset: MISSING sub R,R / dec R (mode-byte compare reusing the ebp zero) pair with EXTRA cmp R,R / cmp R,1; MISSING fld [M+0x1f0] + EXTRA fld [M+0x1e8] = commutative x87 operand order in the speed sum; MISSING add esp,0x14 + jl vs EXTRA jle = the duplicated tail epilogue and loop-bound polarity.  Every divergent row is register allocation, x87 reassociation or branch/epilogue shape of identical logic.  A5 oracle EQUIVALENT on 64 seeds.) */
#include <math.h>
#include <string.h>

class BrBitLatch {
public:
    void Take(unsigned mask);                       /* 0x1002F640 */
};

class BrCar {
public:
    void Step();
    void SetPos(float x, float y, float z);         /* 0x1006F680 */
    void SetVel(float x, float y, float z);         /* 0x1006FA10 */
    void SetAngVel(float x, float y, float z);      /* 0x1006FC10 */
    void SetOrientation(float x, float y, float z); /* 0x1006FA90 */
    void CtlInputApply();                           /* 0x1005AFF0 */
    void PhysStep();                                /* 0x1005A7A0 BrCarPhysStep */
    void WheelSteerStep();                          /* 0x1005ACE0 */
    void CamChaseStep();                            /* 0x10001CF0 */
};
#define pCar ((unsigned char *)this)

extern "C" {
int   BrCollRayCast_1006EC30(void *a, void *b, void *c, void *d, void *e,
                             void *f, void *g, void *h, void *i);
void  BrCarNetSendState(unsigned char *p);
void  BrVec3Scale(float *pOut, void *pV, float s);
void  BrVec3AddTo(void *pDst, float *pSrc);
void  BrVec3Cross(void *pOut, void *pA, void *pB);
float BrSqrtF(float x);

extern float DAT_106e9d8c;
extern float DAT_10077c60;
extern float DAT_10077c64;
extern float DAT_10077c68;
extern float DAT_10077c6c;
extern float DAT_10077c70;
extern float DAT_10077c74;
extern float DAT_10077c78;
extern float DAT_10077c38;
extern int   DAT_105ccb88;
extern int   DAT_100a9360;
extern int   DAT_10226a44;
extern int   DAT_10226a48;
extern int   DAT_100aa044;
extern int   DAT_106e86c8;
extern void  BrCtlHuman(void);
}

void BrCar::Step()
{
  float local_c[3];
  float fVar2;
  int cVar3;
  int iVar5;
  int *piVar6;

  if (*(int *)(pCar + 0xf7c) != 0) {
    BrVec3Scale(local_c, pCar,
                (*(float *)(pCar + 0x2728) * DAT_106e9d8c) * DAT_10077c60);
    BrVec3AddTo(pCar + 0x30, local_c);
    SetPos(*(float *)(pCar + 0x30), *(float *)(pCar + 0x34),
                *(float *)(pCar + 0x38));
    SetVel(0.0f, 0.0f, 0.0f);
    SetAngVel(0.0f, 0.0f, 0.0f);
    *(int *)(pCar + 0xe24) = 0;
    SetOrientation((*(float *)(pCar + 0x2720) * DAT_106e9d8c) * DAT_10077c64,
                   (*(float *)(pCar + 0x2724) * DAT_106e9d8c) * DAT_10077c64,
                        (*(float *)(pCar + 0x272c) * DAT_106e9d8c) * DAT_10077c64);
    if ((**(unsigned char **)(pCar + 0x29c0) & 0x10) != 0) {
      int *puVar1 = (int *)(pCar + 0x20);
      *puVar1 = 0;
      puVar1[1] = 0;
      puVar1[2] = 0x3f800000;
      BrVec3Cross(pCar + 0x10, puVar1, pCar);
      BrVec3Cross(puVar1, pCar, pCar + 0x10);
      (*(BrBitLatch **)(pCar + 0x29c0))->Take(0x10);
    }
  } else {
    *(int *)(pCar + 0x1020) =
        BrCollRayCast_1006EC30(pCar + 0x30, pCar + 0x20, pCar + 0x30, pCar + 0x290c,
                               pCar + 0x294c, pCar + 0x2950, pCar + 0x2990,
                               pCar + 0x2994, pCar + 0x2998);
    *(int *)(*(int *)(pCar + 0x16c) + 0x1c0) = 0;
    *(int *)(*(int *)(pCar + 0x168) + 0x1c0) = 0;
    CtlInputApply();
    *(int *)(*(int *)(pCar + 0x174) + 0x1c0) = *(int *)(pCar + 0xe20);
    *(int *)(*(int *)(pCar + 0x170) + 0x1c0) = *(int *)(*(int *)(pCar + 0x174) + 0x1c0);
    *(int *)(*(int *)(pCar + 0x168) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x16c) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x170) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x174) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x168) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x16c) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x170) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x174) + 0x1d0) = 0;
    if (*(float *)(pCar + 0xe68) > DAT_10077c38) {
      if ((**(unsigned int **)(pCar + 0x29c0) & 0x20000) != 0) {
        *(float *)(pCar + 0xe68) = -*(float *)(pCar + 0xe68);
      }
      switch (*(char *)(*(int *)(pCar + 0x29c4) + 0xd9)) {
      case 0:
          *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c6c;
          *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c6c;
          *(float *)(*(int *)(pCar + 0x170) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c70;
          *(float *)(*(int *)(pCar + 0x174) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c70;
        break;
      case 1:
          *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c68;
          *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = *(volatile float *)(pCar + 0xe68) * DAT_10077c68;
          *(int *)(*(int *)(pCar + 0x170) + 0x1cc) = 0;
          *(int *)(*(int *)(pCar + 0x174) + 0x1cc) = 0;
        break;
      default:
          *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = -*(float *)(pCar + 0xe68);
          *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = -*(float *)(pCar + 0xe68);
          *(float *)(*(int *)(pCar + 0x170) + 0x1cc) = -*(float *)(pCar + 0xe68);
          *(float *)(*(int *)(pCar + 0x174) + 0x1cc) = -*(float *)(pCar + 0xe68);
        break;
      }
    }
    if (*(float *)(pCar + 0xe6c) < DAT_10077c38) {
      *(float *)(*(int *)(pCar + 0x168) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      *(float *)(*(int *)(pCar + 0x16c) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      *(int *)(*(int *)(pCar + 0x168) + 0x1cc) = 0;
      *(int *)(*(int *)(pCar + 0x16c) + 0x1cc) = 0;
      iVar5 = *(int *)(pCar + 0x170);
      fVar2 = (*(float *)(iVar5 + 0x1cc) < DAT_10077c38)
                  ? -*(float *)(iVar5 + 0x1cc) : *(float *)(iVar5 + 0x1cc);
      if (fVar2 < DAT_10077c74) {
        *(float *)(iVar5 + 0x1d0) = -*(float *)(pCar + 0xe6c);
        *(float *)(*(int *)(pCar + 0x174) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      }
    }
    if ((DAT_105ccb88 == 0) &&
        (((DAT_100a9360 != 2 || (*(int *)(pCar + 0x140) != 1)) ||
          (*(int *)(*(int *)(pCar + 0x29c0) + 0x44) == 0)))) {
      if (DAT_100a9360 == 4 && *(int *)(pCar + 0x140) == 0 &&
          *(int *)(*(int *)(pCar + 0x29c0) + 0x44) != 0)
        goto LAB_net;
      if (DAT_100a9360 != 4 && DAT_100a9360 != 5 && DAT_10226a44 == 0 &&
          0x5a < *(int *)(pCar + 0x730))
        goto LAB_net;
      PhysStep();
    }
  }
LAB_net:
  if (DAT_10226a48 != 0) {
    if ((*(void **)(pCar + 0xf08) == (void *)BrCtlHuman) && (DAT_105ccb88 == 0)) {
      BrCarNetSendState(pCar);
    }
  } else if (DAT_105ccb88 == 0) {
    WheelSteerStep();
  }
  if (*(int *)(pCar + 0x730) != 0) {
    *(float *)(pCar + 0x1030) =
        BrSqrtF(*(float *)(pCar + 0x1e8) * *(float *)(pCar + 0x1e8) +
                *(float *)(pCar + 0x1ec) * *(float *)(pCar + 0x1ec) +
                *(float *)(pCar + 0x1f0) * *(float *)(pCar + 0x1f0)) * DAT_10077c78;
  }
  if (DAT_10226a48 == 0) {
    for (iVar5 = 0; iVar5 < DAT_100aa044; iVar5++) {
      if (*(int *)(pCar + 0x140) == (&DAT_106e86c8)[iVar5 * 0x16]) {
        CamChaseStep();
        return;
      }
    }
  }
  return;
}
