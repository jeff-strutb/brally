/* br_carstep.c -- matching arm for BrCarStep (0x1006F170).
 *
 * Per-car frame step, called once per car by the race step and the AI.  On the
 * live path it casts the wheel collision ray, applies the control input, then
 * writes the four wheels' suspension force / slip fields; on the replay path
 * (pCar->f7c set) it drives the entity straight from the recorded transform.
 * Either way it then hands off to the net/steer sub-steps, recomputes the
 * scalar speed, and -- for the local camera car -- advances the chase camera.
 */
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

/* callees */
int   BrCollRayCast_1006EC30(void *a, void *b, void *c, void *d, void *e,
                             void *f, void *g, void *h, void *i);
void  BR_THISCALL1 BrCtlInputApply(unsigned char *pCar);
void  BR_THISCALL1 BrCarSub1005A7A0(unsigned char *pCar);
void  BR_THISCALL1 BrCarWheelSteerStep_1005ACE0(unsigned char *pCar);
void  BrCarNetSendState(unsigned char *pCar);
void  BR_THISCALL1 BrEntSetPos(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetVel(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetAngVel(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetOrientation(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrCamChaseStep(unsigned char *pCar);
void  BrVec3Scale(float *pOut, void *pV, float s);
void  BrVec3AddTo(void *pDst, float *pSrc);
void  BrVec3Cross(void *pOut, void *pA, void *pB);
/* 0x1002F640 -- thiscall (`this` in ecx = the latch at *(pCar+0x29C0), edx
 * unread, one stack arg, `ret 4`); same arm as br_ctlinput.c:21. */
void  __fastcall BrBitLatchTake(void *pThis, void *_edx, unsigned mask);
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
extern void  BrCtlHuman(void);   /* 0x1005D050 -- compared as a function pointer */

/* BrCarStep (0x1006F170) is matched in the C++ lane as BrCar::Step:
 * src/brally/core/driving/BrCarStep_1006F170.cpp.  The declarations above stay for
 * the Mac port spec, which supplies its own body. */

