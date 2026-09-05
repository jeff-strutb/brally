/* br_camframe.c -- drawing: seating the chase camera's frames on a car.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.  The
 * camera is here for the same reason br_chasecam.c is: what it produces is
 * the transform a frame draws through.
 *
 * Glide 0x10002310 is __thiscall on the car record.  The port's cdecl twin,
 * BrCamFrameInitB(void *pCar) in slice2_11.c, stores the selected frame's
 * byte OFFSET in the two selector slots; the original stores its ADDRESS,
 * and that is what this arm does.  The header's prototype for the port
 * twin is renamed out of the way for this translation unit.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrCamFrameInitB BrCamFrameInitB_port
#include "slice2_11.h"
#undef BrCamFrameInitB

/* The slice of the car record this function writes, as MEMBERS: a struct
 * copy into `car->m` is emitted as `mov [esi+disp],reg` (the original's
 * form), while a copy into `*(BrVec3 *)(p + disp)` makes VC5 form the
 * destination address in a register first -- three `lea`, 8 bytes short.
 * Scalar stores do not care; the vector copies do. */
typedef struct BrCarCamSlice {
    unsigned char pad0[BR_CAR_OFF_CAM_D + 0x30];
    BrVec3        camDPos;                          /* 0x2838 */
    unsigned char pad1[BR_CAR_OFF_SLEW - (BR_CAR_OFF_CAM_D + 0x30 + 0xC)];
    float         slew;                             /* 0x28DC */
    unsigned char pad2[BR_CAR_OFF_PREVPOS - (BR_CAR_OFF_SLEW + 4)];
    BrVec3        prevPos;                          /* 0x28EC */
    float         shake;                            /* 0x28F8 */
    unsigned char pad3[BR_CAR_OFF_V2900 - (BR_CAR_OFF_SHAKE + 4)];
    BrVec3        v2900;                            /* 0x2900 */
} BrCarCamSlice;

/* WHAT IT DOES: seats the "B" chase camera on a car: points both selector
 * slots at the frame the game mode picks (B in mode 5, A otherwise), puts
 * B's position at the car's position plus four times its forward vector
 * (plus ten times its side vector in one mode) minus the car's position
 * again, copies that position into frame D, the previous-position record
 * and the +0x2900 record, zeroes the shake and starts the slew at 2.
 *
 * Only ONE pointer local -- B's position, the argument of all three vector
 * calls and the source of the three copies (edi).  The selected frame is a
 * bare conditional, not a reused B pointer: keeping B in a register costs
 * a `mov eax,ecx`. */
/* @implements 0x10002310 glide BrCamFrameInitB */
void __fastcall BrCamFrameInitB(unsigned char *p)
{
    BrCamFrame    *pF   = (BrCamFrame *)p;
    BrCarCamSlice *car  = (BrCarCamSlice *)p;
    BrVec3        *pPos;
    BrCamFrame    *pSel;

    pSel = (g_brMode0AA010 == 5) ? (BrCamFrame *)(p + BR_CAR_OFF_CAM_B)
                                 : (BrCamFrame *)(p + BR_CAR_OFF_CAM_A);
    *(BrCamFrame **)(p + BR_CAR_OFF_ACTIVECAM)  = pSel;
    *(BrCamFrame **)(p + BR_CAR_OFF_ACTIVECAM2) = pSel;

    /* Assigned HERE, after the selector: initialised at its declaration
     * the `lea edi,[esi+0x27b0]` is hoisted above the `cmp eax,5`
     * (32 diff bytes of shifted encodings, same instructions). */
    pPos = &((BrCamFrame *)(p + BR_CAR_OFF_CAM_B))->f30;
    BrVec3MulAdd(pPos, &pF->f30, &pF->f20, 4.0f);
    if (g_brFlag6909E0 != 0)
        BrVec3MulAddTo(pPos, &pF->f00, 10.0f);
    BrVec3SubFrom(pPos, &pF->f00);

    /* Three SCALAR copies per vector, not a struct assignment: the original
     * moves each float through a GP register straight into the car
     * (`mov edx,[edi]; mov [esi+0x2838],edx`), while a BrVec3 assignment
     * makes VC5 form the destination address first (+3 lea, 8 B short). */
    car->camDPos.x = pPos->x;
    car->camDPos.y = pPos->y;
    car->camDPos.z = pPos->z;
    car->prevPos.x = pPos->x;
    car->prevPos.y = pPos->y;
    car->prevPos.z = pPos->z;
    car->shake     = 0.0f;
    car->v2900.x   = pPos->x;
    car->v2900.y   = pPos->y;
    car->v2900.z   = pPos->z;
    car->slew      = 2.0f;
}
#endif /* BR_MATCHING_BUILD */
