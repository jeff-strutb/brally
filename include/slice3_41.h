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

#include <stdint.h>
#include <stddef.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_pool.h"

/* ---------------------------------------------------------------------
 * Cross-slice symbols.  Declared, never defined here.
 * ------------------------------------------------------------------- */

/* XSLICE 0x10035BBA -- same declaration as slice2_18.h. */
extern void BrFatal(const char *pszMsg);

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
extern float g_f6C2CFC;
#define g_BrAnimDt g_f6C2CFC

/* 0x100B380C -- a mode selector.  WARNING: this one address already carries
 * THREE names in port/include (BrG_0B380C in slice2_18.h, g_br0B380C in
 * slice2_25.h, g_Br0B380C in slice2_19.h).  slice2_19.h's spelling is used
 * here; integration has to collapse the other two. */
extern int32_t g_Br0B380C;

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

#ifdef BR_MATCHING_BUILD
/* ============================ TRUE LAYOUT ================================
 * The original record is 0x2B68 bytes and every field below sits at the
 * offset the original instructions use.  The port arm further down keeps the
 * fields packed sequentially with their offsets in comments only, which is
 * why a store the original writes at +0xF78 came out at +0x84 and blocked
 * BrRaceCarPre (0x10061430) from matching.  Displacement errors of that class
 * cannot be fixed one function at a time -- the layout has to be right.
 *
 * WHY THIS ARM IS CONDITIONAL. The original is 32-bit, so its pointers are
 * 4 bytes. pEquip sits at +0xE8C with fE90 immediately after it at +0x E90;
 * on the 64-bit port an 8-byte pointer would overwrite that neighbour. True
 * offsets are therefore only expressible where pointers are 4 bytes, which
 * is exactly the build that needs them.
 *
 * The paddings are not trusted: every offset is asserted below, so a wrong
 * one is a compile error rather than silent corruption.
 *
 * KNOWN GAP -- the three f29C0* fields are NOT at true offsets. In the
 * original they are the +0x00, +0x20 and +0x24 of the block pCtl points at,
 * not members of this record. They are parked in tail padding so the six
 * call sites in br_racestep.c keep compiling. Moving them into BrRaceCtl is
 * the next step and should HELP those functions, because the original reads
 * them through the pointer and the flattened form does not. */
typedef struct BrDriverCar {
    uint8_t  _pad000[0x030];
    BrVec3   pos;                           /* +0x030 */
    uint8_t  _pad03C[0x140 - 0x03C];
    int32_t  f140;                          /* +0x140 */
    uint8_t  _pad144[0x1E8 - 0x144];
    BrVec3   f1E8;                          /* +0x1E8 */
    uint8_t  _pad1F4[0x360 - 0x1F4];
    uint8_t  b360;                          /* +0x360 */
    uint8_t  _pad361[0x730 - 0x361];
    int32_t  f730;                          /* +0x730 */
    uint8_t  _pad734[0xE70 - 0x734];
    int32_t  fE70;                          /* +0xE70 */
    uint8_t  _padE74[0xE88 - 0xE74];
    int32_t  fE88;                          /* +0xE88 */
    uint8_t *pEquip;                        /* +0xE8C */
    int32_t  fE90;                          /* +0xE90 */
    int32_t  fE94;                          /* +0xE94 */
    int32_t  fE98;                          /* +0xE98 */
    int32_t  fE9C;                          /* +0xE9C */
    uint8_t  _padEA0[0xF00 - 0xEA0];
    int32_t  fF00;                          /* +0xF00 */
    int32_t  fF04;                          /* +0xF04 */
    void   (*pfnControl)(struct BrDriverCar *); /* +0xF08 */
    uint8_t  _padF0C[0xF78 - 0xF0C];
    int32_t  fF78;                          /* +0xF78 */
    int32_t  fF7C;                          /* +0xF7C */
    BrVec3   posPrev;                       /* +0xF80 */
    uint8_t  _padF8C[0xFA0 - 0xF8C];
    int32_t  gateHi;                        /* +0xFA0 */
    int32_t  gate;                          /* +0xFA4 */
    int32_t  lap;                           /* +0xFA8 */
    int32_t  lapB;                          /* +0xFAC */
    float    tRun;                          /* +0xFB0 */
    float    aLapTime[BR_RACE_LAPTIME_MAX]; /* +0xFB4 */
    float    tBest;                         /* +0xFE4 */
    int32_t  lapBest;                       /* +0xFE8 */
    float    tFinal;                        /* +0xFEC */
    float    fFF0;                          /* +0xFF0 */
    float    fFF4;                          /* +0xFF4 */
    int32_t  fFF8;                          /* +0xFF8 */
    /* +0xFFC and +0x1004 hold CHARACTER POINTERS -- either a string-table
     * result out of BrStrGet or the address of one of the record's own two
     * text buffers.  They keep their int32 type because 0x1001A644 zeroes
     * them as integers; 0x1005FF00 casts at each store, which on a 32-bit
     * build is free. */
    int32_t  fFFC;                          /* +0xFFC */
    float    f1000;                         /* +0x1000 */
    int32_t  f1004;                         /* +0x1004 */
    float    f1008;                         /* +0x1008 */
    /* The HUD banner buffer -- "FINAL LAP", "LAP 3", "1st".  0x1005FF00
     * sprintf()s into it and points +0xFFC at it. */
    char     sz100C[0x1024 - 0x100C];       /* +0x100C */
    BrVec3   f1024;                         /* +0x1024 */
    float    f1030;                         /* +0x1030 */
    float    f1034;                         /* +0x1034 */
    uint8_t  _pad1038[0x29A4 - 0x1038];
    int32_t  f29A4;                         /* +0x29A4 */
    int32_t  f29A8;                         /* +0x29A8 */
    uint8_t  _pad29AC[0x29AF - 0x29AC];
    uint8_t  b29AF;                         /* +0x29AF */
    float    f29B0;                         /* +0x29B0 */
    uint8_t  _pad29B4[0x29C0 - 0x29B4];
    struct BrRaceCtl *pCtl;                 /* +0x29C0 */
    /* NOT true offsets -- see KNOWN GAP above. */
    uint32_t f29C0Ctl;
    float    f29C0Steer;
    uint8_t  b29C024;
    uint8_t  _padTail[0x2ABC - 0x29CD];
    /* +0x2ABC: the banner's SECOND buffer.  0x1005FF00's standings pass
     * copies +0x100C into it and repoints +0xFFC, so the text survives the
     * next sprintf into +0x100C. */
    char     sz2ABC[0x2B68 - 0x2ABC];       /* +0x2ABC */
} BrDriverCar;

/* Compile-time proof, in the project's usual idiom.  A wrong padding is a
 * negative array size here rather than a runtime memory bug. */
#define BR_DC_AT(name, off) \
    typedef char BrDriverCarAt_##name[(offsetof(BrDriverCar, name) == (off)) ? 1 : -1]
BR_DC_AT(pos,        0x030);
BR_DC_AT(f140,       0x140);
BR_DC_AT(f1E8,       0x1E8);
BR_DC_AT(b360,       0x360);
BR_DC_AT(f730,       0x730);
BR_DC_AT(fE70,       0xE70);
BR_DC_AT(fE88,       0xE88);
BR_DC_AT(pEquip,     0xE8C);
BR_DC_AT(fE90,       0xE90);
BR_DC_AT(fE94,       0xE94);
BR_DC_AT(fE98,       0xE98);
BR_DC_AT(fE9C,       0xE9C);
BR_DC_AT(fF00,       0xF00);
BR_DC_AT(fF04,       0xF04);
BR_DC_AT(pfnControl, 0xF08);
BR_DC_AT(fF78,       0xF78);
BR_DC_AT(fF7C,       0xF7C);
BR_DC_AT(posPrev,    0xF80);
BR_DC_AT(gateHi,     0xFA0);
BR_DC_AT(gate,       0xFA4);
BR_DC_AT(lap,        0xFA8);
BR_DC_AT(lapB,       0xFAC);
BR_DC_AT(tRun,       0xFB0);
BR_DC_AT(aLapTime,   0xFB4);
BR_DC_AT(tBest,      0xFE4);
BR_DC_AT(lapBest,    0xFE8);
BR_DC_AT(tFinal,     0xFEC);
BR_DC_AT(fFF4,       0xFF4);
BR_DC_AT(fFF8,       0xFF8);
BR_DC_AT(fFF0,       0xFF0);
BR_DC_AT(fFFC,       0xFFC);
BR_DC_AT(f1000,     0x1000);
BR_DC_AT(f1004,     0x1004);
BR_DC_AT(f1008,     0x1008);
BR_DC_AT(sz100C,    0x100C);
BR_DC_AT(sz2ABC,    0x2ABC);
BR_DC_AT(f1024,     0x1024);
BR_DC_AT(f1030,     0x1030);
BR_DC_AT(f1034,     0x1034);
BR_DC_AT(f29A4,     0x29A4);
BR_DC_AT(f29A8,     0x29A8);
BR_DC_AT(b29AF,     0x29AF);
BR_DC_AT(f29B0,     0x29B0);
BR_DC_AT(pCtl,      0x29C0);
typedef char BrDriverCarOrigSize[(sizeof(BrDriverCar) == 0x2B68) ? 1 : -1];

#else  /* ---- port arm: fields packed sequentially, offsets in comments ---- */

typedef struct BrDriverCar {
    BrVec3  pos;            /* +0x030  position this frame                */
    BrVec3  posPrev;        /* +0xF80  position last frame                */
    int32_t gateHi;         /* +0xFA0  furthest gate reached (unwrapped)  */
    int32_t gate;           /* +0xFA4  current gate (unwrapped, may be <0)*/
    int32_t lap;            /* +0xFA8  lap counter                        */
    int32_t lapB;           /* +0xFAC  "technically earned" lap counter   */
    float   tRun;           /* +0xFB0  time accumulated in the lap so far */
    float   aLapTime[BR_RACE_LAPTIME_MAX];  /* +0xFB4  "lapTimeFinal[]"   */
    float   tBest;          /* +0xFE4  best lap time, 0 == none yet       */
    int32_t lapBest;        /* +0xFE8  lap the best was set on            */
    float   tFinal;         /* +0xFEC  race time, trimmed at the flag     */
    float   fFF4;           /* +0xFF4  sort key / distance along track    */
    int32_t fFF8;           /* +0xFF8  rank output -- AND the finishing
                             *         order, which 0x1005FF00 overwrites
                             *         it with at the flag                */
    float   f29B0;          /* +0x29B0 set to 0.9f at the flag            */

    /* ---- EXTENDED AGAIN, by br_racestep.h's pass ------------------------
     * Same rule as the extension above, and for the same reason: 0x10061F60
     * and 0x100623E0 read and write these on the SAME 0x2B68 record the two
     * blocks above model, so a rival struct would be a second view of one
     * original object.  Every one carries its original offset.
     *
     * +0x29C0 is a POINTER to a control block in the original; only its
     * first dword, its +0x20 and its +0x24 are ever touched from these two
     * functions, so the three are members here rather than a block. */
    int32_t f140;           /* +0x140   compared against the entrant count
                             *          before the difficulty lookup       */
    int32_t fE70;           /* +0xE70   zeroed either side of the
                             *          controller call on the frozen arm  */
    int32_t fF00;           /* +0xF00   non-zero == this car body is live; the
                             *          phantom arm tests the car it borrows */
    int32_t fF04;           /* +0xF04   a frame countdown 0x100623E0 bleeds */
    int32_t fF78;           /* +0xF78   zeroed once a frame by 0x10061430,
                             *          whose eleven bytes are that store   */
    void  (*pfnControl)(struct BrDriverCar *);  /* +0xF08 the controller;
                             *          0x1005D050 for a human slot,
                             *          0x1005E690 for an AI one           */
    BrVec3  f1024;          /* +0x1024  world velocity, (pos - posPrev)/dt */
    float   f1030;          /* +0x1030  speed                              */
    float   f1034;          /* +0x1034  distance run, f1030 * dt integrated*/
    uint8_t b29AF;          /* +0x29AF  the finish / recovery state byte   */
    uint32_t f29C0Ctl;      /* +0x29C0 -> +0x00, the control bit word      */
    float   f29C0Steer;     /* +0x29C0 -> +0x20, the steering command      */
    uint8_t b29C024;        /* +0x29C0 -> +0x24                            */
    uint8_t b360;           /* +0x360   the skid-trail sample count        */

    /* ---- EXTENDED AGAIN, by br_racebegin.h's pass -----------------------
     * Same rule and the same reason as the two extensions above: Glide
     * 0x10019A70's ONE-TIME ARM reads and writes these on the SAME 0x2B68
     * record, so a rival struct would be a second view of one original
     * object.  Every one carries its original offset and the instruction
     * that pins it.
     *
     * The four equipment slots are named by TWO independent readings that
     * agree: 0x1001A490 copies them out of the equipment record whose
     * offsets br_racestart.h pins from 0x100628B0, and 0x10019EBD reads the
     * same four out of the eight-byte replay header 0x1001A14A builds.
     * car+0xE94 being the suspension also agrees with CONVENTIONS.md's
     * spring rate, `(20 - n*-4) * 16000` with `n` at car+0xE94. */
    int32_t  f730;          /* +0x730   0x100199AC gates the speed update  */
    BrVec3   f1E8;          /* +0x1E8   the velocity 0x100199BC takes the
                             *          length of.  Its components are read
                             *          in the order +0x1EC, +0x1E8, +0x1F0
                             *          and summed (y*y + x*x) + z*z, which
                             *          is the order the port keeps          */
    int32_t  fE88;          /* +0xE88   0x1001A619 gates the 0x1006FCE0
                             *          call on this being zero            */
    uint8_t *pEquip;        /* +0xE8C   -> the 0x200-byte equipment record
                             *          slice5_60.h reaches as
                             *          g_BrCarEquipTarget.  RAW BYTES: the
                             *          record is written to disc verbatim  */
    int32_t  fE90;          /* +0xE90   tire type,        0x1001A4A3       */
    int32_t  fE94;          /* +0xE94   suspension type,  0x1001A498       */
    int32_t  fE98;          /* +0xE98   handling type,    0x1001A4B9       */
    int32_t  fE9C;          /* +0xE9C   transmission,     0x1001A49B       */
    int32_t  fF7C;          /* +0xF7C   0x1001A641                         */
    int32_t  fFFC;          /* +0xFFC   0x1001A644                         */
    int32_t  f1004;         /* +0x1004  0x1001A64A, and the byte count
                             *          0x1001A8D0's download returns       */
    int32_t  f29A4;         /* +0x29A4  the car model REQUESTED; 0x10019EA7
                             *          writes it and passes the same value
                             *          to 0x1006FD50                       */
    int32_t  f29A8;         /* +0x29A8  the car model APPLIED; 0x1001A127
                             *          records THIS into the replay header
                             *          and 0x1001A61E passes it on         */

    /* +0x29C0's POINTEE.  The three fields above (f29C0Ctl, f29C0Steer,
     * b29C024) are that block's +0x00, +0x20 and +0x24 flattened into this
     * record; br_racebegin.h's BrRaceCtl deliberately does NOT repeat them,
     * so there is exactly one model of each dword.  A host that supplies a
     * real block has to decide which object owns those three. */
    struct BrRaceCtl *pCtl; /* +0x29C0                                     */
} BrDriverCar;

#endif /* BR_MATCHING_BUILD -- true layout vs port layout */

/* The three bits of car+0x29C0's first dword that 0x10061F60 writes.  br_ai.h
 * names the same word's 0x10000 / 0x20000 / 0x40000 from the controller's
 * side; these are the two spellings 0x10061F60 uses. */
#define BR_DRIVERCAR_CTL_BRAKE  0x00040000u   /* 0x10062035, frozen arm    */
#define BR_DRIVERCAR_CTL_FIN    0x000C0000u   /* 0x100620B9, finished arm  */

#define BR_DRIVER_SKIP  2u      /* +0x68 bit 1: slot takes no rank        */

typedef struct BrDriver {
    BrVec3       f00;
    BrVec3       f0C;
    BrVec3       f18;
    int32_t      f24;
    int32_t      f28;
    int32_t      f2C;
    /* +0x30 and +0x34 were `int32_t` here on the strength of the constructor,
     * which only zeroes them.  0x1005FF00 reads and writes both with
     * fld/fstp: they are the running lap time and the best lap time.  +0x38
     * is still only ever zeroed, so it stays an int32. */
    float        f30;       /* running lap time                           */
    float        f34;       /* best lap time, 0 == none set yet           */
    int32_t      f38;
    int32_t      f3C;
    int32_t      f40;       /* lap                                        */
    int32_t      f44;       /* "technically earned" lap                   */
    int32_t      f48;       /* furthest gate reached (unwrapped)          */
    int32_t      f4C;       /* current gate (unwrapped, may be negative)  */
    float        f50;       /* sort key                                   */
    int32_t      f54;       /* rank, written only when pCar == NULL       */
    int32_t      f58;
    uint8_t      f5C, f5D, f5E, f5F;
    BrDriverCar *pCar;      /* +0x60                                      */
    int32_t      f64;       /* own slot index                             */
    uint32_t     f68;       /* flags                                      */
    int32_t      f6C;
    int32_t      f70;
    int32_t      f74;
    int32_t      f78;
    int32_t      f7C;
} BrDriver;

/* 0x10066620  qsort comparator over 8-byte {float key; int32_t idx;} pairs.
 *
 * Returns +1 when a > b, -1 when a < b, 0 otherwise -- but the original does
 * the comparison TWICE and reads different status bits each time, so an
 * unordered (NaN) pair takes the "-1" exit rather than "0".  Reproduced. */
int BrRankCmpKey(const void *pA, const void *pB);

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
void BrRankAssign(BrDriver *pSlots, int32_t n);

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
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail);

/* 0x10067900  the inverse.  GOTCHA: no size argument and no check at all --
 * it reads exactly as many bytes from pSrc as the table describes. */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc);

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
float BrSndDoppler(const BrVec3 *pSrcPos, const BrVec3 *pSrcPrev,
                   const BrVec3 *pLisPos, const BrVec3 *pLisPrev);

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
void BrSndPan(const BrVec3 *pSrcPos, const BrMat4 *pListener,
              float *pGainA, float *pGainB, int32_t *pVol, int32_t fNarrow);

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

extern BrSndNearest g_BrSndNearest;

/* 0x10AA3470 -- an index into the 25-entry, 24-byte-stride table at
 * 0x100B3AA8 (geometry pinned by the clear loop in 0x100682A0).  -1 means
 * "none".  Set by 0x10067D40, cleared by BrSndNearestReset. */
extern int32_t g_BrSndAA3470;

/* The sentinel the distance metric starts at: 0x50000000 == 2^33. */
#define BR_SND_NEAREST_FAR  8589934592.0f

/* 0x10067DA0  per-frame reset: metric back to the sentinel, f84 and f8C to
 * -1.  GOTCHA: it deliberately leaves f88 and f90 (the committed halves)
 * alone -- that is how 0x10067ED0 detects a source that stopped being
 * offered. */
void BrSndNearestInvalidate(void);

/* 0x10067DC0  full reset, plus g_BrSndAA3470 = -1.
 * GOTCHA: f98 (the base frequency) is NOT cleared by either reset. */
void BrSndNearestReset(void);

/* 0x10067E50  offer a candidate; keep it only if it is strictly nearer than
 * the current best.  Note the argument order -- the ids come first and the
 * geometry last, and f8C/f84/f9C/f98 land in a scrambled order in the block.
 * The "Prev" and "committed" fields are NOT touched here. */
void BrSndNearestOffer(int32_t f8C, int32_t f84, int32_t f9C, float f98,
                       const BrVec3 *pPos, const BrMat4 *pListener);

/* 0x10068210  offer with the fixed parameters this build uses: set index 15,
 * volume scale 0x180, base frequency 11000 Hz -- but ONLY when g_Br0B380C is
 * 4 or 10.  Otherwise it does nothing at all: the id stays -1 and the two
 * "default" values 0x80 / -1 the function starts with are never used. */
void BrSndNearestOfferDefault(int32_t f8C, const BrVec3 *pPos,
                              const BrMat4 *pListener);

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
void *BrPool16Alloc(void);
void *BrPool32Alloc(void);

/* The 64-byte pool's counter (0x10B01C40) belongs to br_pool.h, whose
 * BrPoolAlloc takes an explicit BrPool * and so has no global instance for
 * BrGfx69580 -- which takes no arguments -- to reach.  The integration points
 * this at whichever BrPool it makes canonical; NULL means "not wired". */
extern BrPool *g_pBrPool64;

/* 0x10069580.  Name fixed by slice2_18.h, which already declares it. */
void BrGfx69580(void);

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

#endif /* SLICE3_41_H */
