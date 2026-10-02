/* slice3_41.h -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * Packet range 0x100661B0 - 0x100695A0 (33 functions in the listing).  Only
 * the parts that could be resolved with confidence are ported; the rest are
 * listed under "NOT PORTED" at the bottom of this file.
 *
 * What is in here:
 *
 *   1. The 0x80-byte "driver" record at 0x10ACD498 and the race-position
 *      (rank) sort that ranks them              (0x10066620, 0x10066510 part)
 *   2. The variable-block save / restore pair used by the replay and
 *      state-snapshot code                          (0x10067880, 0x10067900)
 *   3. Positional-audio maths: the Doppler ratio and the stereo pan / volume
 *      solver                                       (0x10067AE0, 0x10067BC0)
 *   4. The "nearest sound source this frame" tracker and its two resets
 *              (0x10067DA0, 0x10067DC0, 0x10067E50, 0x10068210)
 *   5. Two more per-frame slot banks (16- and 32-byte slots) built exactly
 *      like br_pool.h's 64-byte one, plus the counter reset that clears all
 *      three                     (0x100694E0, 0x10069530, 0x10069580)
 *
 * Field names that could not be justified are positional (fNN = the byte
 * offset in the ORIGINAL layout).  This port does NOT reproduce the original
 * byte layout -- pointers are wider here -- so every struct is indexed by
 * member, never by byte.
 */
#ifndef SLICE3_41_H
#define SLICE3_41_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif

#include <stdint.h>
#include <stddef.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_pool.h"
#include "br_cartypes.h"

/* ---------------------------------------------------------------------
 * Cross-slice symbols.  Declared, never defined here.
 * ------------------------------------------------------------------- */

/* XSLICE 0x10035BBA -- same declaration as slice2_18.h. */
/* BrFatal: prototype in br_funcs.h */

/* 0x106C2CFC -- seconds elapsed this frame.  slice2_19.h already declares
 * this address under this name, so the name is reused verbatim (a duplicate
 * extern of identical type is legal even if both headers are included).
 * Confirmed as a time delta independently here: 0x10068EF0 does
 * car->f1034 += car->f1030 * g_BrAnimDt, and the contract fixes car+0x1030
 * as speed.
 *
 * ALIAS RESOLVED (third name for this address): slice2_20.c calls it
 * g_f6C2CFC. The storage is now defined once, in port/src/br_data.c, and both
 * this header and slice2_19.h spell it as g_BrAnimDt. .bss in the original,
 * so 0 at boot is the original's own value. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define g_BrAnimDt g_brRaceFlyStep

/* 0x100B380C -- a mode selector.  WARNING: this one address already carries
 * THREE names in port/include (BrG_0B380C in slice2_18.h, g_br0B380C in
 * slice2_25.h, g_Br0B380C in slice2_19.h).  slice2_19.h's spelling is used
 * here; integration has to collapse the other two. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* =====================================================================
 * 1.  Driver records and the race-position sort
 *
 * 0x10ACD498 is an array of 0x80-byte records, one per driver slot.  The
 * stride is pinned by 0x100662A0, whose constructor computes its own index
 * with  (this - 0x10ACD498) >> 7  and stores it at +0x64.  Do not confuse
 * this array with the 0x2B68-byte entity/car records the contract mentions:
 * +0x60 of a driver record POINTS AT one of those.
 *
 * Offsets established by 0x100662A0 / 0x10068EF0 / 0x10066510:
 *
 *   +0x00 BrVec3   copied from 0x10AF9B38..0x10AF9B40
 *   +0x0C BrVec3   copy of +0x00
 *   +0x18 BrVec3   scratch (destination of a BrVec3Sub in 0x10068EF0)
 *   +0x28 int32    from 0x10AF988C
 *   +0x2C int32    from 0x10ACD490
 *   +0x30 int32 x3 zeroed by the constructor
 *   +0x3C int32    from a table selected by 0x100B380C
 *   +0x40 int32    from 0x10AF9B44
 *   +0x44 int32    from 0x10AF9B44, read as an integer counter (fild) later
 *   +0x48 int32    from 0x10AF96C0
 *   +0x4C int32    from 0x10AF96C0
 *   +0x50 float    the sort key ("progress")
 *   +0x54 int32    the rank, used only when +0x60 is NULL
 *   +0x5C 3 bytes  from the 3-byte-stride table at 0x100B37D0
 *   +0x60 ptr      the car record, or NULL
 *   +0x64 int32    this record's own index
 *   +0x68 int32    flags; bit0 and bit1 are both tested
 *   +0x74 int32
 *   +0x78 int32
 *   +0x7C int32
 * ===================================================================== */

/* The car fields the driver record mirrors.  In the original these all live
 * inside the 0x2B68-byte entity record; when integration lands a full car
 * type, BrDriver::pCar should be re-pointed at it.
 *
 * EXTENDED by br_race.h's pass.  It was two fields, because the rank sort was
 * the only consumer in this packet.  The lap/gate state machine (D3D
 * 0x10066E90 == Glide 0x1005FF00 -- the "NOT PORTED" entry below) mirrors
 * SEVEN more of them into the driver record on entry and back out on exit, so
 * a second car model would have been a second view of one original object --
 * exactly the aliased-storage bug CONVENTIONS.md records.  Extending is the
 * fix; adding a rival struct is not.  Nothing here is overlaid on a foreign
 * buffer, so the widened layout is harmless.
 *
 * fFF4 keeps its positional name because it has two readings that the port
 * cannot yet collapse: 0x10066510 sorts on it as an opaque key, and
 * 0x1005FF00 adjusts it by the track's lap length -- i.e. it is distance
 * along the track, accumulated across laps. */
#define BR_RACE_LAPTIME_MAX  12  /* (0xFE4 - 0xFB4) / 4 -- see br_race.h */

/* The original record is 0x2B68 bytes. This definition is generated from
 * ports/64b/types/BrDriverCar.spec, which lists every field at the offset
 * the original instructions use; the padding between them is byte arrays, so
 * the 64-bit compiler lays each pointer out at its own width and nothing that
 * follows it is overwritten. Code reaches every field by name. The control
 * word and steering command the per-frame passes write live in the block
 * pCtl points at (BrRaceCtl), as they do in the original. */
typedef struct BrDriverCar {
    BrVec3 fwd;                              /* +0x0000  world frame row 0 (the body matrix is rows 0..3) */
    float f0C;                               /* +0x000C */
    BrVec3 right;                            /* +0x0010  frame row 1 */
    float f1C;                               /* +0x001C */
    BrVec3 up;                               /* +0x0020  frame row 2 */
    float f2C;                               /* +0x002C */
    BrVec3 pos;                              /* +0x0030  frame row 3: the car's position */
    float f3C;                               /* +0x003C */
    BrMat4 aWheel[4];                        /* +0x0040  one matrix per wheel */
    int32_t f140;                            /* +0x0140  player/car index */
    int32_t iNetPlayer;                      /* +0x0144  the DirectPlay player this car belongs to */
    char szName[26];                         /* +0x0148  the driver name shown on the HUD (%s) */
    short f0162;                             /* +0x0162 */
    BrCarBody aBody[5];                      /* +0x0164  physics: [0] the body, [1..4] the wheels LF, LR, RF, RR */
    BrRbForce aForce[16];                    /* +0x0BA0  the bodies' force-list nodes (linked through pNext) */
    uint8_t _pad0DA0[0x80];
    float f0E20;                             /* +0x0E20 */
    float f0E24;                             /* +0x0E24 */
    float f0E28;                             /* +0x0E28 */
    float f0E2C;                             /* +0x0E2C */
    int32_t f0E30;                           /* +0x0E30  seen in the trace */
    int32_t f0E34;                           /* +0x0E34  seen in the trace */
    int32_t f0E38;                           /* +0x0E38  seen in the trace */
    int32_t f0E3C;                           /* +0x0E3C  seen in the trace */
    int32_t f0E40;                           /* +0x0E40  seen in the trace */
    float f0E44;                             /* +0x0E44 */
    float f0E48;                             /* +0x0E48 */
    float f0E4C;                             /* +0x0E4C */
    float f0E50;                             /* +0x0E50 */
    float f0E54;                             /* +0x0E54 */
    int f0E58;                               /* +0x0E58 */
    int32_t f0E5C;                           /* +0x0E5C  seen in the trace */
    int f0E60;                               /* +0x0E60 */
    int32_t f0E64;                           /* +0x0E64  seen in the trace */
    float fE68;                              /* +0x0E68 */
    float f0E6C;                             /* +0x0E6C */
    int32_t fE70;                            /* +0x0E70 */
    float f0E74;                             /* +0x0E74 */
    unsigned char f0E78;                     /* +0x0E78 */
    uint8_t _pad0E79[0x3];
    float f0E7C;                             /* +0x0E7C */
    unsigned char f0E80;                     /* +0x0E80 */
    char f0E81;                              /* +0x0E81 */
    uint8_t _pad0E82[0x2];
    int f0E84;                               /* +0x0E84 */
    int32_t fE88;                            /* +0x0E88 */
    uint8_t *pEquip;                         /* +0x0E8C  the equipment record */
    int32_t fE90;                            /* +0x0E90 */
    int32_t fE94;                            /* +0x0E94 */
    int32_t fE98;                            /* +0x0E98 */
    int32_t fE9C;                            /* +0x0E9C */
    int32_t cHoldFwd;                        /* +0x0EA0 */
    int32_t cHoldRev;                        /* +0x0EA4 */
    int32_t cRevRun;                         /* +0x0EA8 */
    int32_t cFwdRun;                         /* +0x0EAC */
    struct BrDriver * apRank[16];            /* +0x0EB0  the field in race order (the lap-save restore writes it) */
    uint8_t _pad0EF0[0x10];
    struct BrDriver *pProfile;               /* +0x0F00  the entrant/profile record */
    int32_t fF04;                            /* +0x0F04 */
    void (*pfnControl)(struct BrDriverCar *); /* +0x0F08 */
    BrVec3 aim;                              /* +0x0F0C  the AI's smoothed aim point */
    BrVec3 pathPos;                          /* +0x0F18  the path frame's blended position */
    BrVec3 tangent;                          /* +0x0F24 */
    BrVec3 lateral;                          /* +0x0F30 */
    BrVec3 pathUp;                           /* +0x0F3C */
    float fF48;                              /* +0x0F48 */
    BrVec3 d;                                /* +0x0F4C */
    float fF58;                              /* +0x0F58 */
    int f0F5C;                               /* +0x0F5C */
    int32_t f0F60;                           /* +0x0F60  seen in the trace */
    int32_t f0F64;                           /* +0x0F64  seen in the trace */
    int f0F68;                               /* +0x0F68 */
    int f0F6C;                               /* +0x0F6C */
    int f0F70;                               /* +0x0F70 */
    float f0F74;                             /* +0x0F74 */
    int32_t fF78;                            /* +0x0F78 */
    int32_t fF7C;                            /* +0x0F7C */
    BrVec3 posPrev;                          /* +0x0F80 */
    BrAiNodeArg pNode;                       /* +0x0F8C  the AI's waypoint cursor: node */
    BrAiIdxArg iPt;                          /* +0x0F90  ... and point */
    float f0F94;                             /* +0x0F94 */
    float f0F98;                             /* +0x0F98 */
    float f0F9C;                             /* +0x0F9C */
    int32_t gateHi;                          /* +0x0FA0 */
    int32_t gate;                            /* +0x0FA4 */
    int32_t lap;                             /* +0x0FA8 */
    int32_t lapB;                            /* +0x0FAC */
    float tRun;                              /* +0x0FB0 */
    float aLapTime[12];                      /* +0x0FB4 */
    float tBest;                             /* +0x0FE4 */
    int32_t lapBest;                         /* +0x0FE8 */
    float tFinal;                            /* +0x0FEC */
    float fFF0;                              /* +0x0FF0 */
    float fFF4;                              /* +0x0FF4  distance along the track, across laps */
    int32_t fFF8;                            /* +0x0FF8  ranking key */
    char *pszBanner;                         /* +0x0FFC  a string-table result or one of the two banner buffers */
    float f1000;                             /* +0x1000 */
    union {                                  /* +0x1004 */
        char *psz1004;  /* a second banner string ... */
        int32_t n1004;  /* ... and, during race setup, the last replay row's size */
    };
    float f1008;                             /* +0x1008 */
    char sz100C[24];                         /* +0x100C  the HUD banner buffer */
    BrVec3 f1024;                            /* +0x1024  velocity */
    float f1030;                             /* +0x1030  speed */
    float f1034;                             /* +0x1034 */
    uint32_t f1038[2];                       /* +0x1038  seen in the trace */
    uint32_t f1040[2];                       /* +0x1040  seen in the trace */
    uint32_t f1048[2];                       /* +0x1048  seen in the trace */
    uint32_t f1050[2];                       /* +0x1050  seen in the trace */
    int32_t f1058;                           /* +0x1058  seen in the trace */
    float f105C;                             /* +0x105C */
    uint32_t f1060[2];                       /* +0x1060  seen in the trace */
    int32_t f1068;                           /* +0x1068  seen in the trace */
    float f106C[4];                          /* +0x106C  the four floats the table init writes 0x44 back */
    int32_t f107C;                           /* +0x107C  seen in the trace */
    int32_t f1080;                           /* +0x1080  seen in the trace */
    int32_t f1084;                           /* +0x1084  seen in the trace */
    int32_t f1088;                           /* +0x1088  seen in the trace */
    int32_t f108C;                           /* +0x108C  seen in the trace */
    int32_t f1090;                           /* +0x1090  seen in the trace */
    int32_t f1094;                           /* +0x1094  seen in the trace */
    int32_t f1098;                           /* +0x1098  seen in the trace */
    int32_t f109C;                           /* +0x109C  seen in the trace */
    int32_t f10A0;                           /* +0x10A0  seen in the trace */
    int32_t f10A4;                           /* +0x10A4  seen in the trace */
    int32_t f10A8;                           /* +0x10A8  seen in the trace */
    int32_t a10AC[4];                        /* +0x10AC  (float on the first pass, then cleared) */
    int32_t a10BC[4];                        /* +0x10BC */
    int32_t a10CC[4];                        /* +0x10CC */
    int32_t a10DC[4];                        /* +0x10DC */
    BrVec3 aWheelPrev[4];                    /* +0x10EC  each wheel's previous position */
    uint8_t _pad111C[0x4];
    int32_t aHist[0x90][8];                  /* +0x1120  144 history records of 0x20 bytes */
    int16_t aWHist[0x90][3];                 /* +0x2320  144 three-short entries */
    uint16_t a2680[0x24];                    /* +0x2680  pairs, set to 2,2 each */
    float f26C8;                             /* +0x26C8 */
    float f26CC;                             /* +0x26CC */
    float f26D0;                             /* +0x26D0 */
    uint32_t f26D4[2];                       /* +0x26D4  seen in the trace */
    uint32_t f26DC[2];                       /* +0x26DC  seen in the trace */
    uint32_t f26E4[2];                       /* +0x26E4  seen in the trace */
    uint32_t f26EC[2];                       /* +0x26EC  seen in the trace */
    uint32_t f26F4[2];                       /* +0x26F4  seen in the trace */
    uint32_t f26FC[2];                       /* +0x26FC  seen in the trace */
    uint32_t f2704[2];                       /* +0x2704  seen in the trace */
    uint32_t f270C[2];                       /* +0x270C  seen in the trace */
    int32_t i2714;                           /* +0x2714 */
    float f2718;                             /* +0x2718 */
    uint8_t _pad271C[0x4];
    float f2720;                             /* +0x2720 */
    float f2724;                             /* +0x2724 */
    float aimFwd;                            /* +0x2728 */
    float f272C;                             /* +0x272C */
    float f2730;                             /* +0x2730 */
    BrSnapMtx *pMatA;                        /* +0x2734 */
    BrSnapMtx *pMatB;                        /* +0x2738 */
    BrSnapMtx aSnap[6];                      /* +0x273C */
    int32_t f28D4;                           /* +0x28D4  seen in the trace */
    int32_t f28D8;                           /* +0x28D8  seen in the trace */
    float f28DC;                             /* +0x28DC */
    float f28E0;                             /* +0x28E0 */
    float f28E4;                             /* +0x28E4 */
    float f28E8;                             /* +0x28E8 */
    uint32_t f28EC[2];                       /* +0x28EC  seen in the trace */
    int32_t f28F4;                           /* +0x28F4  seen in the trace */
    int32_t f28F8;                           /* +0x28F8  seen in the trace */
    uint8_t _pad28FC[0x4];
    int32_t f2900;                           /* +0x2900  seen in the trace */
    int32_t f2904;                           /* +0x2904  seen in the trace */
    int32_t f2908;                           /* +0x2908  seen in the trace */
    uint16_t aNearIds[32];                   /* +0x290C  ray-cast: near face ids (the first indexes the 84-byte records) */
    int32_t gotHit;                          /* +0x294C  ray-cast: a near hit was found */
    uint16_t aFarIds[32];                    /* +0x2950  ray-cast: far face ids */
    int32_t farCount;                        /* +0x2990 */
    float fHitDist;                          /* +0x2994 */
    int32_t iHitFace;                        /* +0x2998 */
    short f299C;                             /* +0x299C */
    short f299E;                             /* +0x299E */
    short f29A0;                             /* +0x29A0 */
    short f29A2;                             /* +0x29A2 */
    int32_t f29A4;                           /* +0x29A4 */
    int32_t f29A8;                           /* +0x29A8 */
    unsigned char f29AC;                     /* +0x29AC */
    unsigned char f29AD;                     /* +0x29AD */
    unsigned char f29AE;                     /* +0x29AE */
    uint8_t b29AF;                           /* +0x29AF  draw class; 2 is the translucent pass */
    float f29B0;                             /* +0x29B0  alpha */
    int32_t i29B4;                           /* +0x29B4 */
    int f29B8;                               /* +0x29B8 */
    unsigned char f29BC;                     /* +0x29BC */
    unsigned char f29BD;                     /* +0x29BD */
    uint8_t _pad29BE[0x2];
    struct BrRaceCtl *pCtl;                  /* +0x29C0  the control block (br_racebegin.h) */
    void *pModel;                            /* +0x29C4  the car's model record */
    uint32_t f29C8[2];                       /* +0x29C8  seen in the trace */
    uint32_t f29D0[2];                       /* +0x29D0  seen in the trace */
    uint16_t f29D8;                          /* +0x29D8  seen in the trace */
    uint8_t _pad29DA[0x96];
    int32_t f2A70;                           /* +0x2A70  seen in the trace */
    int32_t f2A74;                           /* +0x2A74  seen in the trace */
    int32_t f2A78;                           /* +0x2A78  seen in the trace */
    int32_t f2A7C;                           /* +0x2A7C  seen in the trace */
    int32_t f2A80;                           /* +0x2A80  seen in the trace */
    int32_t f2A84;                           /* +0x2A84  seen in the trace */
    int32_t f2A88;                           /* +0x2A88  seen in the trace */
    int32_t f2A8C;                           /* +0x2A8C  seen in the trace */
    uint32_t f2A90[2];                       /* +0x2A90  seen in the trace */
    uint32_t f2A98[2];                       /* +0x2A98  seen in the trace */
    uint32_t f2AA0[2];                       /* +0x2AA0  seen in the trace */
    uint32_t f2AA8[2];                       /* +0x2AA8  seen in the trace */
    float f2AB0;                             /* +0x2AB0 */
    float f2AB4;                             /* +0x2AB4 */
    float f2AB8;                             /* +0x2AB8 */
    char sz2ABC[172];                        /* +0x2ABC  the banner's second buffer */
} BrDriverCar;

/* The offsets are the original's 32-bit layout; the 64-bit core lays the
 * struct out by its field types, so the check only names the field. */






















/* The three bits of car+0x29C0's first dword that 0x10061F60 writes.  br_ai.h
 * names the same word's 0x10000 / 0x20000 / 0x40000 from the controller's
 * side; these are the two spellings 0x10061F60 uses. */
#define BR_DRIVERCAR_CTL_BRAKE  0x00040000u   /* 0x10062035, frozen arm    */
#define BR_DRIVERCAR_CTL_FIN    0x000C0000u   /* 0x100620B9, finished arm  */

#define BR_DRIVER_SKIP  2u      /* +0x68 bit 1: slot takes no rank        */

typedef struct BrDriver {
    struct BrVec3 f00;                       /* +0x0000 */
    struct BrVec3 f0C;                       /* +0x000C */
    struct BrVec3 f18;                       /* +0x0018 */
    int32_t f24;                             /* +0x0024 */
    struct BrAiPathNode *pPathNode;          /* +0x0028  the car's path node, parked here between races */
    int32_t f2C;                             /* +0x002C */
    float f30;                               /* +0x0030 */
    float f34;                               /* +0x0034 */
    int32_t f38;                             /* +0x0038 */
    int32_t f3C;                             /* +0x003C */
    int32_t f40;                             /* +0x0040 */
    int32_t f44;                             /* +0x0044 */
    int32_t f48;                             /* +0x0048 */
    int32_t f4C;                             /* +0x004C */
    float f50;                               /* +0x0050 */
    int32_t f54;                             /* +0x0054 */
    int32_t f58;                             /* +0x0058 */
    uint8_t f5C;                             /* +0x005C */
    uint8_t f5D;                             /* +0x005D */
    uint8_t f5E;                             /* +0x005E */
    uint8_t f5F;                             /* +0x005F */
    BrDriverCar *pCar;                       /* +0x0060 */
    int32_t f64;                             /* +0x0064 */
    uint32_t f68;                            /* +0x0068 */
    int32_t f6C;                             /* +0x006C */
    int32_t f70;                             /* +0x0070 */
    int32_t f74;                             /* +0x0074 */
    void **aptex;                            /* +0x0078  the colour-panel textures (count +0x7C) */
    int32_t cptex;                           /* +0x007C */
} BrDriver;

/* 0x10066620  qsort comparator over 8-byte {float key; int32_t idx;} pairs.
 *
 * Returns +1 when a > b, -1 when a < b, 0 otherwise -- but the original does
 * the comparison TWICE and reads different status bits each time, so an
 * unordered (NaN) pair takes the "-1" exit rather than "0".  Reproduced. */
/* BrRankCmpKey: prototype in br_funcs.h */

/* The g_22AF18 == 0 half of 0x10066510: sort the non-skipped driver slots by
 * ascending key and hand out ranks.
 *
 * Slot j of the sorted order gets rank  n - j - 1, so the LOWEST key gets the
 * HIGHEST rank number.  Written to pCar->fFF8 when the slot has a car and to
 * the slot's own f54 when it does not.
 *
 * GOTCHA: the rank counts down from `n`, the number of SLOTS, not from the
 * number of slots that actually took part.  Skipping k slots therefore leaves
 * ranks 0..k-1 unused and the leader ranked k, not 0.
 *
 * GOTCHA: the original's pair buffer is a 0xA0-byte stack array, i.e. exactly
 * 20 pairs, with no bound check.  See the DEVIATION in the .c file. */
#define BR_RANK_MAX  20
/* BrRankAssign: prototype in br_funcs.h */

/* =====================================================================
 * 2.  Variable-block save / restore  (0x10067880, 0x10067900)
 *
 * A table of {pointer, byte count} pairs terminated by a NULL pointer.  Save
 * concatenates every block into one buffer; load scatters a buffer back.
 * ===================================================================== */

typedef struct BrVarBlock {
    void    *pData;     /* +0x00 -- NULL terminates the table              */
    uint32_t cb;        /* +0x04                                           */
} BrVarBlock;

/* 0x10067880  pack pTable into pDst; BrFatal if it needs more than cbAvail.
 * The check is `used > cbAvail` on SIGNED ints and happens only AFTER every
 * block has already been written, so the overflow it reports has already
 * happened. */
/* BrVarSave: prototype in br_funcs.h */

/* 0x10067900  the inverse.  GOTCHA: no size argument and no check at all --
 * it reads exactly as many bytes from pSrc as the table describes. */
/* BrVarLoad: prototype in br_funcs.h */

/* =====================================================================
 * 3.  Positional audio maths
 * ===================================================================== */

/* 0x10067AE0  Doppler frequency ratio.
 *
 *      u  = normalise(srcPos - lisPos)          (skipped when |u| == 0)
 *      vs = (srcPos - srcPrev) / g_BrAnimDt     source velocity
 *      vl = (lisPos - lisPrev) / g_BrAnimDt     listener velocity
 *      return (1 + (vl.u)/c) / (1 + (vs.u)/c)   c = 343 m/s
 *
 * The 343 shows up in the image as the pair of constants 0x1008F9EC /
 * 0x1008F9F0 = -/+0.0029154520f = -/+1/343, which is what identifies the
 * function.  Argument order is source-first, listener-second, and within each
 * pair current-first, previous-second.
 *
 * GOTCHA: divides by g_BrAnimDt with no guard, and there is no guard on the
 * denominator either -- a source closing faster than c drives it through zero
 * and the ratio comes back negative.  The two 1/343 constants are also only
 * float-accurate, so the pole sits a hair off 343.  Preserved.
 *
 * The |u| == 0 case IS guarded: the normalise is skipped, u stays the zero
 * vector, both dot products vanish and the function returns exactly 1. */
/* BrSndDoppler: prototype in br_funcs.h */

/* 0x10067BC0  stereo pan gains + distance volume for one source.
 *
 * The listener is an object whose first 64 bytes are a BrMat4 (rows 0..2 are
 * the basis, row 3 is the position -- that is how 0x10069370 and 0x10068EF0
 * use it).  The pan axis is row 1.
 *
 *      d    = srcPos - lis->m[3]
 *      proj = dot(lis->m[1], d)   clamped to [-10, +10]
 *      if (fNarrow) proj *= 0.4            -> the pan range narrows to +-4
 *      p    = (proj + 10) * 0.05           -> [0, 1]
 *      q    = 1 - p
 *      if p and q are both within [0.49, 0.51] both snap to exactly 0.5
 *      the LARGER of p/q becomes  x + 0.6*(1-x); the smaller becomes x*1.6
 *      *pGainA = the p-derived value, *pGainB = the q-derived value
 *      *pVol   = (int32_t)(1024 / max(|d|, 32))
 *
 * Dead-centre therefore yields 0.8 / 0.8, and the two curves meet there, so
 * the law is continuous.  Which output is physically left and which is right
 * could not be established, hence the positional A/B names.
 *
 * GOTCHA: the minimum-distance clamp compares against a DOUBLE 32.0 at
 * 0x1008FA08 but substitutes the FLOAT 32.0 at 0x1008FA10. */
/* BrSndPan: prototype in br_funcs.h */

/* =====================================================================
 * 4.  Nearest-source tracker  (the 0x10AF9B58..0x10AF9BA3 block)
 *
 * Candidates are offered during the frame; the one with the smallest
 * listener distance wins.  0x10067ED0 (NOT ported, see below) is the commit
 * step that reads the winner, drives BrSndPan / BrSndDoppler with it and
 * fills in the "Prev" and "committed" halves of this block.
 * ===================================================================== */

typedef struct BrSndNearest {
    BrVec3          pos;        /* 0x10AF9B58  winning source position      */
    BrVec3          posPrev;    /* 0x10AF9B64  ... as of the last commit    */
    const BrMat4   *pObj;       /* 0x10AF9B70  winning listener             */
    const BrMat4   *pObjPrev;   /* 0x10AF9B74  ... as of the last commit    */
    BrVec3          objPosPrev; /* 0x10AF9B78  pObj->m[3] at the last commit*/
    int32_t         f84;        /* 0x10AF9B84  candidate set index          */
    int32_t         f88;        /* 0x10AF9B88  committed set index          */
    int32_t         f8C;        /* 0x10AF9B8C  candidate id                 */
    int32_t         f90;        /* 0x10AF9B90  committed id                 */
    float           metric;     /* 0x10AF9B94  best distance so far         */
    float           f98;        /* 0x10AF9B98  base frequency (Hz)          */
    int32_t         f9C;        /* 0x10AF9B9C  volume scale, >>8 after use  */
    int32_t         fA0;        /* 0x10AF9BA0                               */
} BrSndNearest;

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x10AA3470 -- an index into the 25-entry, 24-byte-stride table at
 * 0x100B3AA8 (geometry pinned by the clear loop in 0x100682A0).  -1 means
 * "none".  Set by 0x10067D40, cleared by BrSndNearestReset. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The sentinel the distance metric starts at: 0x50000000 == 2^33. */
#define BR_SND_NEAREST_FAR  8589934592.0f

/* 0x10067DA0  per-frame reset: metric back to the sentinel, f84 and f8C to
 * -1.  GOTCHA: it deliberately leaves f88 and f90 (the committed halves)
 * alone -- that is how 0x10067ED0 detects a source that stopped being
 * offered. */
/* BrSndNearestInvalidate: prototype in br_funcs.h */

/* 0x10067DC0  full reset, plus g_BrSndAA3470 = -1.
 * GOTCHA: f98 (the base frequency) is NOT cleared by either reset. */
/* BrSndNearestReset: prototype in br_funcs.h */

/* 0x10067E50  offer a candidate; keep it only if it is strictly nearer than
 * the current best.  Note the argument order -- the ids come first and the
 * geometry last, and f8C/f84/f9C/f98 land in a scrambled order in the block.
 * The "Prev" and "committed" fields are NOT touched here. */
/* BrSndNearestOffer: prototype in br_funcs.h */

/* 0x10068210  offer with the fixed parameters this build uses: set index 15,
 * volume scale 0x180, base frequency 11000 Hz -- but ONLY when g_Br0B380C is
 * 4 or 10.  Otherwise it does nothing at all: the id stays -1 and the two
 * "default" values 0x80 / -1 the function starts with are never used. */
/* BrSndNearestOfferDefault: prototype in br_funcs.h */

/* =====================================================================
 * 5.  Per-frame slot banks
 *
 * Built exactly like br_pool.h's 64-byte pool: a bank per frame holding
 * `nUsable` real slots plus one shared overflow slot, handed out round-robin
 * until the frame's quota runs out, after which every further request
 * returns that one overflow slot.  Never fails, never returns NULL, and the
 * counter keeps incrementing past the limit.
 *
 * The original uses two literal base addresses per pool, but they differ by
 * exactly nUsable*cbSlot, which is what proves the overflow slot is simply
 * index `nUsable` of the same bank:
 *
 *   0x100694E0  16-byte slots, 20 usable, base 0x10B02190, ovf 0x10B022D0
 *               counter 0x10B01C48
 *   0x10069530  32-byte slots, 20 usable, base 0x10B01C50, ovf 0x10B01ED0
 *               counter 0x10B01C44
 *
 * The gap between the 32-byte pool's base and the 16-byte pool's base is
 * 0x540 == 2 * 21 * 32, so the frame index only ever takes the values 0 and
 * 1 -- the banks are double-buffered.
 *
 * All three pools (these two and br_pool.h's) share the single frame counter
 * at 0x106C65EC.  This port gives each bank its own copy; integration
 * must keep them in step. */
typedef struct BrFrameBank {
    uint8_t *pBase;     /* first usable slot of frame 0                    */
    int32_t  cbSlot;    /* 16 or 32                                        */
    int32_t  nUsable;   /* 20                                              */
    int32_t  nBank;     /* nUsable + 1                                     */
    int32_t  frame;     /* mirrors 0x106C65EC                              */
    int32_t  count;     /* slots taken this frame, INCLUDING overflows     */
} BrFrameBank;

extern BrFrameBank g_BrPool16;      /* 0x100694E0's state */
extern BrFrameBank g_BrPool32;      /* 0x10069530's state */

void *BrFrameBankAlloc(BrFrameBank *pBank);

/* 0x100694E0 / 0x10069530 -- the zero-argument forms the original exports. */
/* BrPool16Alloc: prototype in br_funcs.h */
/* BrPool32Alloc: prototype in br_funcs.h */

/* The 64-byte pool's counter (0x10B01C40) belongs to br_pool.h, whose
 * BrPoolAlloc takes an explicit BrPool * and so has no global instance for
 * BrGfx69580 -- which takes no arguments -- to reach.  The integration points
 * this at whichever BrPool it makes canonical; NULL means "not wired". */
extern BrPool *g_pBrPool64;

/* 0x10069580.  Name fixed by slice2_18.h, which already declares it. */
/* BrGfx69580: prototype in br_funcs.h */

/* =====================================================================
 * NOT PORTED -- and why
 *
 *   0x100661B0  four-way glue over 0x10074F70 with unresolved field offsets
 *               into the +0x29C4 sub-object.
 *   0x100662A0  driver-record constructor.  Reads eleven globals whose types
 *               are not established and calls the 0x10008B80 stub; the field
 *               offsets it writes are documented above instead.
 *   0x10066510  only the g_22AF18 == 0 half is ported, as BrRankAssign.  The
 *               other half rewrites car+0x1A08 from a netplay accessor
 *               (0x10005E40) over the 0x2B68-stride array and needs a car
 *               type this packet does not pin down.
 *   0x10066650  2103 bytes of driver AI over ~15 opaque globals.  NOT AI:
 *               the Glide twin 0x1005F6C0 carries "saving lap (%d/%d) and
 *               gate (%d/%d)" and "restoring lap (%d/%d) and gate (%d/%d)",
 *               so this is the lap/gate snapshot pair.  Still unported.
 *   0x10066E90  2534 bytes, likewise -- and likewise NOT AI.  The Glide twin
 *               0x1005FF00 is the GATE/LAP/FINISH state machine, and the
 *               lap-counting, finish-condition and car-mirror parts of it are
 *               now ported in port/src/br_race.c.  What is still missing from
 *               that file is listed in br_race.h: the debug prints, the HUD
 *               banner, the two per-track record tables, and the standings
 *               recompute at 0x1006044B.
 *   0x10067940  \
 *   0x10067960   |  four-line wrappers whose entire content is the address of
 *   0x10067980   |  a global BrVarBlock table (0x100B39B0, 0x100B3A68) and a
 *   0x100679A0  /   buffer.  Constants worth keeping: the first pair saves
 *               <obj>+0x7080 with a 0x15F88-byte budget, the second saves the
 *               64 bytes at 0x10AF9848.  No table contents are in .rdata, so
 *               there is nothing to port.
 *   0x100679C0  race-start reset; five globals plus a 0x2B68-stride walk.
 *   0x10067D40  selects an entry of the 0x100B3AA8 table (25 entries, stride
 *               24, from 0x100682A0's clear loop) and forwards three of its
 *               fields to 0x100752D0.  Pure global glue.
 *   0x10067D80  \  one-line callers of 0x10067D40 with the constants 0xD and
 *   0x10067D90  /  0xE.
 *   0x10067DC0  ported.
 *   0x10067ED0  the commit step for the nearest-source block.  It is coherent
 *               -- it drives BrSndPan/BrSndDoppler and packs the results into
 *               a 64-bit pitch accumulator at 0x118AC770 and a pair of packed
 *               16-bit volumes at 0x118AC77C (the `sar 1; and 0x7FFF7FFF` is
 *               a packed halve) -- but the meaning of the 0x118AC7xx block
 *               and of three of the globals that gate it could not be
 *               established, so it is left out rather than guessed.
 *   0x10068260  glue over 0x10073080/0x100730A0/0x10075300.
 *   0x100682A0  MakeEnemyCarColorPanels__1; texture/panel setup, all globals.
 *   0x100683D0  a loop calling 0x10072B80(0x18, i, 0).
 *   0x10068EF0  1073 bytes of driver update over the un-typed car record.
 *   0x10069330  glue over three unported calls.
 *   0x10069370  glue over eight unported calls.
 *   0x10069490  BrPoolAlloc -- already in br_pool.h.
 *   0x100695A0  two-line glue: 0x100765E0(b, a) then a->f10 = b->m[3].
 *               Neither argument's type is pinned down by this packet.
 * ===================================================================== */










/* One interpolation snapshot of the race: every car, as br_snapinterp.c
 * blends it. */
typedef struct BrSnap {
    int32_t     stamp;                      /* +0x00000  sequence number    */
    BrSnapDrv   drv[20];                    /* +0x00004                     */
    int32_t     fA04;                       /* +0x00A04                     */
    BrDriverCar car[16];                    /* +0x00A08 .. +0x2C088         */
    BrSnapTailA tailA;                      /* +0x2C088                     */
    int32_t     tailB;                      /* +0x2C0E0                     */
    BrSnapTailC tailC;                      /* +0x2C0E4 .. +0x2E0EC         */
    int32_t     f2E0EC;                     /* +0x2E0EC                     */
} BrSnap;






















#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif



























/* BR_GLOBALS_BEGIN: generated by ports/64b/tools/unify.py */
#ifdef __cplusplus
extern "C" {
#endif
#pragma push_macro("g_aBrSnap")
#undef g_aBrSnap
extern BrSnap g_aBrSnap[6];  /* 0x10396F48 */
#pragma pop_macro("g_aBrSnap")
#pragma push_macro("g_aBrRaceDriver")
#undef g_aBrRaceDriver
extern BrDriver g_aBrRaceDriver[20];  /* 0x10AF07F8 */
#pragma pop_macro("g_aBrRaceDriver")
#pragma push_macro("g_aBrRaceCar")
#undef g_aBrRaceCar
extern BrDriverCar g_aBrRaceCar[16];  /* 0x10AF1208 */
#pragma pop_macro("g_aBrRaceCar")
#pragma push_macro("g_BrSndNearest")
#undef g_BrSndNearest
extern BrSndNearest g_BrSndNearest;  /* 0x10B1CEB8 */
#pragma pop_macro("g_BrSndNearest")
#ifdef __cplusplus
}
#endif
/* BR_GLOBALS_END */
#endif /* SLICE3_41_H */
