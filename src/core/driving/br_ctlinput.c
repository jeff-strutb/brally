/* br_ctlinput.c -- driving: apply this frame's player input to one car.
 *
 *   0x1005AFF0  3210 B   BrCtlInputApply   (D3D 0x10061F70, shared by prefix)
 *
 * Neighbours: BrCarPhysStep 0x1005A7A0, BrCtlHumanBody 0x1005C8B0.
 * Matching arm only; the port TU is empty.
 *
 */

#define _CRTIMP __declspec(dllimport)

#include <math.h>

#include "br_match.h"


void BrMat4MulVec3(float *pOut, float *pM, float *pV);
void __fastcall BrBitLatchTake(void *pThis, void *_edx, unsigned mask);

extern unsigned short *DAT_10b71534;
extern int DAT_100b2e6c;
extern float DAT_100b2e70;
extern float DAT_100b2e74;
extern float DAT_100b2e78;
extern short DAT_10ac67cc;
extern int DAT_100a9360;
extern int DAT_100b2f00;
extern int DAT_10af0858[];

extern float DAT_10077780;
extern float DAT_10077784;
extern float DAT_10077788;
extern float DAT_10077798;
extern float DAT_1007779c;
extern float DAT_100777a0;
extern float DAT_100777a4;
extern float DAT_100777a8;
extern float DAT_100777ac;
extern float DAT_100777b8;
extern float DAT_100777c0;
extern float DAT_100777c8;
extern float DAT_100777d0;
extern float DAT_100777e4;
extern float DAT_100777e8;
extern float DAT_100777ec;
extern float DAT_100777f0;
extern float DAT_100777f4;
extern float DAT_100777f8;
extern float DAT_100777fc;
extern float DAT_10077800;
extern float DAT_10077804;
extern double DAT_10077810;
extern float DAT_10077818;
extern float DAT_1007781c;
extern float DAT_10077820;
extern double DAT_10077828;
extern float DAT_10077830;
extern float DAT_10077834;
extern float DAT_10077838;
extern float DAT_1007783c;
extern float DAT_10077840;
extern float DAT_10077844;
extern float DAT_1007784c;
extern float DAT_10077850;
extern float DAT_10077854;
extern float DAT_10077858;
extern float DAT_1007785c;
extern float DAT_10077860;
extern float DAT_10077864;
extern float DAT_10077868;
extern float DAT_1007786c;
extern float DAT_10077870;
extern float DAT_10077874;
extern float DAT_10077878;
extern float DAT_1007787c;
extern float DAT_10077880;
extern float DAT_10077884;
extern float DAT_10077888;

/* BrCtlInputApply (d3d 0x10061F70): its Glide body is the C++ lane's
 * src/core/driving/BrCtlInputApply_1005AFF0.cpp. */
void BR_THISCALL1 BrCtlInputApply(unsigned char *pCar);
