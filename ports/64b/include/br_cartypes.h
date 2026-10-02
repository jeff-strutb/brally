/* br_cartypes.h: the small types the car record (BrDriverCar, slice3_41.h)
 * embeds by value, shared by every module that reaches the car. */
#ifndef BR_CARTYPES_H
#define BR_CARTYPES_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>
#include "slice3_44.h"      /* BrRbBody */

struct BrAiPathNode;
struct BrDriver;
struct BrRaceCtl;

/* The AI's waypoint cursor (car +0xF8C / +0xF90). One-member structs so the
 * corridor scan can take them by value. */
typedef struct { uint32_t v; }                 BrAiIdxArg;
typedef struct { struct BrAiPathNode *p; }     BrAiNodeArg;

/* One node of a rigid body's force list (the car holds sixteen at +0xBA0). */
typedef struct BrRbForce {
    struct BrRbForce *pNext;                 /* +0x0000 */
    int32_t kind;                            /* +0x0004 */
    struct BrVec3 f;                         /* +0x0008 */
    struct BrVec3 r;                         /* +0x0014 */
} BrRbForce;

/* One interpolation snapshot of a matrix (car +0x273C, six of them). */
typedef struct BrSnapMtx {
    float m[4][4];
    float f40;
} BrSnapMtx;                                    /* 0x44 */

/* One of the car's five physics sub-objects (car +0x164 + i*0x20C): i = 0 is
 * the body, 1..4 the wheels in memory order LF, LR, RF, RR. */
typedef struct BrCarBody {
    BrRbBody rb;                             /* +0x0000 */
    float f01DC;                             /* +0x01DC */
    float f01E0;                             /* +0x01E0 */
    float f01E4;                             /* +0x01E4 */
    float f01E8;                             /* +0x01E8 */
    float f01EC;                             /* +0x01EC */
    float f01F0;                             /* +0x01F0 */
    float f01F4;                             /* +0x01F4 */
    int f01F8;                               /* +0x01F8 */
    uint8_t f01FC;                           /* +0x01FC */
    uint8_t _pad01FD[0x1];
    char f01FE;                              /* +0x01FE */
    uint8_t f01FF;                           /* +0x01FF */
    uint8_t f0200;                           /* +0x0200 */
    uint8_t _pad0201[0x1];
    char f0202;                              /* +0x0202 */
    char f0203;                              /* +0x0203 */
    char f0204;                              /* +0x0204 */
    char f0205;                              /* +0x0205 */
    char f0206;                              /* +0x0206 */
    char f0207;                              /* +0x0207 */
    char f0208;                              /* +0x0208 */
    char f0209;                              /* +0x0209 */
    uint8_t _pad020A[0x2];
} BrCarBody;

/* The control block the car points at (car +0x29C0): the per-frame control
 * word and steering command, and the one-time race-setup replay records. */
#ifndef BR_RACEBEGIN_MAXREC
#define BR_RACEBEGIN_MAXREC   2
#endif
typedef struct BrRaceCtl {
    uint32_t ctl;                            /* +0x0000  the car's control word: brake/finish bits (BR_DRIVERCAR_CTL_*) */
    uint8_t _pad0004[0x1C];
    float steer;                             /* +0x0020  the controller's steering command, -1..1 */
    uint8_t b24;                             /* +0x0024 */
    uint8_t b25;                             /* +0x0025 */
    uint8_t _pad0026[0x6];
    void * apRec[BR_RACEBEGIN_MAXREC];       /* +0x002C  per-entrant replay record */
    int32_t aLen[BR_RACEBEGIN_MAXREC];       /* +0x0034 */
    int32_t aCap[BR_RACEBEGIN_MAXREC];       /* +0x003C */
    uint8_t *pHdr;                           /* +0x0044  the 8-byte replay header */
    int32_t f48;                             /* +0x0048 */
    int32_t f4C;                             /* +0x004C */
    uint8_t _pad0050[0x104];
    int32_t f154;                            /* +0x0154  own index */
    struct BrEntRec *f158;                   /* +0x0158  &g_aBrEntRecs[f154] */
} BrRaceCtl;                                 /* 0x15C: also slice1_05.h's BrEnt */

#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
