/* br_racebegin.h -- RESPONSIBILITY: the rules of a race.
 *
 * THE OPENING OF Glide 0x10019A70 AND THE WHOLE OF ITS ONE-TIME ARM, plus the
 * seven small functions that sit immediately below it in .text and are only
 * ever reached from it.
 *
 * REFERENCE IS orig/BRGlide.dll.  Every address below was read out of the
 * listing; `tools/manifest.py` was run on every callee before a line was
 * written.
 *
 * ======================================================================
 * WHAT THIS ADDS, AND WHAT IS STILL MISSING
 * ======================================================================
 * 0x10019A70 is 11,223 bytes -- 0x10019A70..0x1001C646, with its four jump
 * tables at 0x1001C648, 0x1001C664, 0x1001C678 and 0x1001C688 immediately
 * after the last `ret`.  br_racestep.c already carries the light machine and
 * the per-frame driver passes and explains, at length, why the address itself
 * carries NO `@implements` line.  That is still true and this module does not
 * change it.  What it adds is the block that note lists first as missing:
 *
 *     0x10019A70..0x10019AF8   the frame clock and the substate branch    144 B
 *     0x10019AFE..0x1001AB70   THE ONE-TIME ARM                         4,209 B
 *
 * so the inventory for 0x10019A70 now reads:
 *
 *     transcribed here          0x10019A70..0x1001AB70        4,353 B
 *     transcribed in br_racestep.c
 *                               0x1001AB71..0x1001B260        1,776 B
 *     STILL NOT TRANSCRIBED     0x1001B261..0x1001C646        5,094 B
 *                               (the replay camera, the HUD, the mirror,
 *                                the per-car render marshalling, the pause
 *                                and camera input, the race exit and the
 *                                frame limiter)
 *
 * 4,353 + 1,776 == 6,129 of 11,223, i.e. 54.6% of the PORT.  A matching
 * claim is whole-function only and needs one C function — see the matching
 * protocol in br_racestep.h.  Do not tag this address on any of these
 * split helpers.
 *
 * The seven neighbours below ARE claimed, in full, because each is a whole
 * function with its own entry address.
 *
 * ======================================================================
 * THE OPENING: A FIVE-FRAME CLOCK, AND THE SUBSTATE BRANCH
 * ======================================================================
 * The first thing the race step does on EVERY frame -- one-time arm or not --
 * is stamp the wall clock and push the elapsed milliseconds into a ring:
 *
 *     0x10019A77   now = 0x1006E280()          a millisecond counter
 *     0x10019A84   dt  = now - 0x105CCB90;  0x105CCB90 = now
 *     0x10019A9A   if (cursor < 0 && len > 0)  fill ALL `len` slots with dt
 *     0x10019AB5   if (++0x105BCAE4 == 0) 0x105CCB7C = 0; else += dt
 *     0x10019ACC   if (++cursor >= len) cursor = 0;  ring[cursor] = dt
 *
 * `len` is 0x100A9358 and the shipped image holds **5**; the cursor 0x100A935C
 * ships as **-1**, which is what makes the first frame prime the whole ring
 * with one measurement instead of leaving four zeroes in it.  The counter at
 * 0x105BCAE4 is set to -1 by 0x10019A40, so the frame AFTER a reset is the one
 * that zeroes the accumulator -- `inc` then `je`, not `cmp`.
 *
 * Then, and this is the shape of the whole function:
 *
 *     0x10019AE4   eax = 0x105CCB94                 the SUBSTATE
 *     0x10019AF8   jne 0x1001AB71                   -> the per-frame arm
 *                  fall through                     -> the ONE-TIME arm
 *
 * The one-time arm ends at 0x1001AA5E by storing 1 into 0x105CCB94, so it runs
 * exactly once per race, and it ends at 0x1001AB6F by jumping to 0x1001AB93 --
 * i.e. it joins the per-frame arm PAST the replay-record block at 0x1001AB71,
 * so the frame in which a race begins does not also record a replay packet.
 *
 * ======================================================================
 * THE ONE-TIME ARM IS A SEVEN-WAY SWITCH ON THE GAME MODE
 * ======================================================================
 * 0x10019BC1 is `jmp dword ptr [eax*4 + 0x1001C648]` with eax = 0x100A9360,
 * guarded by `cmp eax, 6 / ja`.  The table, read out of the image:
 *
 *     mode 0  -> 0x10019BC8   20 drivers, 3 cars      a full grid
 *     mode 1  -> 0x10019BF1   2 drivers, 2 cars       head to head
 *     mode 2  -> 0x10019F0E   THE REPLAY/LINK ARM, the big one
 *     mode 3  -> 0x1001A1EE   the default arm (same target as `ja`)
 *     mode 4  -> 0x10019DC0   1 driver, the cutscene arm
 *     mode 5  -> 0x10019D87   1 driver, no assets
 *     mode 6  -> 0x10019C2A   the networked arm
 *
 * Mode 3 sharing the out-of-range target is the table's own doing and is not
 * a transcription short cut: 0x1001C654 holds 0x1001A1EE, the same address
 * 0x10019BBB's `ja` jumps to.
 *
 * ======================================================================
 * THE REPLAY HEADER IS EIGHT BYTES, AND BOTH ENDS OF IT ARE HERE
 * ======================================================================
 * This is the find worth having out of this pass.  The mode-2 arm BUILDS a
 * record and the mode-4 arm READS one, and putting the two side by side names
 * every byte:
 *
 *   byte  built at        read at        meaning
 *   ----  --------------  -------------  ---------------------------------
 *    0    0x1001A11F      0x10019E94     the track          (0x100B3014)
 *    1    0x1001A131      0x10019EA7     the car model      (car+0x29A8 out,
 *                                        car+0x29A4 in -- see below)
 *    2    0x1001A14A      0x10019EBD     handling      (equip +0xF8)
 *    3    0x1001A163      0x10019ECA     transmission  (equip +0xFC)
 *    4    0x1001A17C      0x10019ED7     tire          (equip +0x100)
 *    5    0x1001A195      0x10019EE4     suspension    (equip +0x104)
 *    6    0x1001A1AE      0x10019EF0     equip +0x108, into ctl->b25
 *    7    0x1001A1C1      0x10019EFF     the weather        (0x10226E80)
 *
 * That independently confirms the four equipment offsets br_racestart.h pins
 * from 0x100628B0 -- including the CROSSED pair, since byte 4 is +0x100 and
 * byte 5 is +0x104 in the same order the writer uses -- and it NAMES THE FIFTH
 * SLOT, +0x108, which slice5_60.h counts and declines to identify.  Here it is
 * the byte that ends up in the control block's +0x25.
 *
 * Byte 1 is asymmetric on purpose, as far as the bytes can say: the builder
 * reads car+0x29A8 and the reader writes car+0x29A4, and the reader then calls
 * 0x1006FD50 (the "put this model on this car" routine) with the same value.
 * The consistent reading is a REQUEST field at +0x29A4 and an APPLIED field at
 * +0x29A8 that 0x1006FD50 writes.  It is recorded as observed rather than as a
 * defect: CONVENTIONS.md's rule is that a claim the original is WRONG needs
 * more evidence than a claim it is right, and there is none here.
 *
 * The equipment values the reader lands on are car+0xE90/0xE94/0xE98/0xE9C,
 * and the SAME four are copied out of the equipment record at 0x1001A490 in
 * the common tail -- which is how the four names are pinned on the car side:
 *
 *     car+0xE90 tire   car+0xE94 suspension   car+0xE98 handling
 *     car+0xE9C transmission
 *
 * car+0xE94 being suspension agrees with CONVENTIONS.md, which already has
 * the spring rate as `(20 - n*-4) * 16000` with `n` at car+0xE94.
 *
 * The defaults the common tail writes for every car past the entrant count
 * (0x1001A568) are tire 2, suspension 1, handling 0, transmission 1.
 *
 * ======================================================================
 * TWO `[esp+N]` READS THAT NAME ONE SLOT, AND THE PUSH BETWEEN THEM
 * ======================================================================
 * CONVENTIONS.md records that a displacement is meaningless without the ESP it
 * is relative to, and that reading two equal displacements as one slot has
 * shipped as a bug twice.  Here the trap runs the other way and the answer is
 * the surprising one:
 *
 *     0x1001A3F6   lea edx, [esp+0x2C]      esp == E   -> E+0x2C
 *     0x1001A3FA   push ecx                 esp == E-4
 *     0x1001A3FB   lea eax, [esp+0x30]      esp == E-4 -> E+0x2C
 *
 * DIFFERENT displacements, SAME slot.  Both arguments of 0x10034870 are the
 * three floats at E+0x2C, seeded to (1, 0, 0) at 0x1001A3D3, so the call is
 * an IN-PLACE transform (dst == src) and not a two-buffer one.  Writing it
 * with two buffers would be silently wrong for any implementation of
 * 0x10034870 that reads its source after its first write.
 *
 * ======================================================================
 * 0x100A6B68 IS NOT (ONLY) A STRING
 * ======================================================================
 * 0x1001A94B stores -1 through a cursor that starts at 0x100A6B68 and steps
 * by 4, once per texture set, `0x100AA044` times.  0x100A6B68 disassembles as
 * the format string "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF%s%d:%02d.%02d" -- so its
 * first EIGHT bytes are two int32 slots already holding -1, and the text
 * begins at 0x100A6B70.  The loop is therefore an ordinary "reset two handles
 * to -1", and it stays inside the -1 prefix for every count the shipped modes
 * can produce (0x100AA044 is 1 when replaying and the entrant count otherwise,
 * and mode 2 -- the only arm that raises the entrant count -- is a two-player
 * race).  A third entrant would write over the '%s'.
 *
 * ======================================================================
 * THE FRAME LIMITER, FOR WHOEVER TAKES THE TAIL
 * ======================================================================
 * Not transcribed here -- it is at 0x1001C583, inside the 5 KB that is still
 * missing -- but the table it reads is cheap to state and pins the game's
 * intended rate: 0x100A95B8 holds { 33, 33, 34 } and 0x1001C598 cycles a
 * cursor 0..2 through it, adding each entry to a millisecond deadline.
 * 33 + 33 + 34 == 100 ms for three frames, i.e. EXACTLY 30 Hz -- which is the
 * same 1/30 s br_racestep.h models as `g_brRaceStepDt` and br_phys.h as
 * `BR_PHYS_DT`, arrived at from the other end.
 *
 * ======================================================================
 * A LIVE ALIAS THIS PASS FOUND: 0x100A9360
 * ======================================================================
 * The game mode has TWO host objects with real storage:
 * `g_brCfgGameMode` (br_appstart.c:44, initialised to 1) and
 * `g_brRaceRules.mode` (br_racestep.c:113).  br_racestart.c reads the first;
 * br_racestep.c reads the second.  Both are right about the address and
 * neither can find the other by grepping its own name -- CONVENTIONS.md's
 * "positional name vs semantic name" pattern exactly.  This module reads
 * `g_brRaceRules.mode`, because it is a continuation of br_racestep.c and
 * because adding a THIRD name is the one thing that is certainly wrong.
 * Resolving it is a separate job: it means deciding which module owns the
 * storage and making the other a macro over it, as slice2_11.h did for
 * 0x10220D68.
 *
 * ======================================================================
 * THE FRONTIER
 * ======================================================================
 * Of the 40-odd distinct callees in this range, `tools/manifest.py` reports
 * exactly THREE as implemented: 0x1002E317 (BrGameStepSet), 0x10031140
 * (BrTrackLoadHandling) and, in the neighbours, nothing.  So the arm is
 * transcribed for its ORDER, its CONDITIONS and its CONSTANTS, and every call
 * out of it goes through `BrRaceBeginOps`.  A NULL entry is SKIPPED AND
 * COUNTED -- `BrRaceBeginSkipped()` reports how many times each was reached,
 * so "the race began" is falsifiable.  Nothing here fabricates a result:
 * the two ops that RETURN a value (`pfnTickMs` and `pfnTexCreate`) hand back
 * 0 and -1 respectively when unwired, and -1 is the same sentinel 0x1001A94B
 * writes into the handle slots, so an unwired run looks like "no texture",
 * not like "a texture".
 *
 * 0x10008D60 is the one call that is NOT a frontier: CONVENTIONS.md records
 * that it is a single `c3`, so the folded empty function it stands for really
 * does nothing and omitting it is EXACT.  It is still counted, because the
 * count is the only evidence that the arm reached the point it stands at.
 */
#ifndef BR_RACEBEGIN_H
#define BR_RACEBEGIN_H

#include <stdint.h>

/* For BrDriverCar / BrDriver and the race globals this arm shares with the
 * per-frame one.  This module is a continuation of br_racestep.c -- it is a
 * separate file only because 4 KB of one-time setup and 2 KB of per-frame
 * state do not belong in one translation unit -- so it uses that module's
 * objects rather than coining a second set. */
#include "br_racestep.h"

/* ======================================================================
 * The control block at car+0x29C0
 * ======================================================================
 * car+0x29C0 is a POINTER in the original.  slice3_41.h models three fields
 * of what it points at -- the block's +0x00, +0x20 and +0x24 -- flattened
 * into BrDriverCar as f29C0Ctl, f29C0Steer and b29C024, because those are the
 * only three the per-frame passes touch.
 *
 * THOSE THREE ARE DELIBERATELY ABSENT BELOW.  This struct carries only the
 * fields the ONE-TIME arm touches, so there is no second model of any dword
 * slice3_41.h already owns (CONVENTIONS.md, "Aliased storage").  A host that
 * ever gives a car a real control block has to decide which object owns
 * +0x00/+0x20/+0x24; nothing here forces that decision.
 *
 * The three arrays are indexed by ENTRANT, and in the original they OVERLAP:
 * 0x1001A104 writes +0x2C+4i, 0x1001A1CA writes +0x34+4i and 0x1001A1D8
 * writes +0x3C+4i, so a third entrant's record pointer would land on the
 * first's length.  The shipped arm that runs this loop is mode 2, a
 * two-player link race, so two is the width the layout supports.  The port
 * CLAMPS instead of overlapping -- a DEVIATION, counted, because reproducing
 * the overlap would need a byte array and the slots hold host pointers.
 */
#define BR_RACEBEGIN_MAXREC   2

/* The eight-byte replay header at ctl->pHdr, byte for byte.  See the banner. */
#define BR_RACEBEGIN_HDR_LEN        8
#define BR_RACEBEGIN_HDR_TRACK      0
#define BR_RACEBEGIN_HDR_CARMODEL   1
#define BR_RACEBEGIN_HDR_HANDLING   2
#define BR_RACEBEGIN_HDR_TRANS      3
#define BR_RACEBEGIN_HDR_TIRE       4
#define BR_RACEBEGIN_HDR_SUSP       5
#define BR_RACEBEGIN_HDR_EQUIP5     6
#define BR_RACEBEGIN_HDR_WEATHER    7

/* The equipment record's offsets, as br_racestart.h names them.  Repeated as
 * their own defines rather than included, because this module reaches the
 * record through an op and must not drag br_racestart.o behind it. */
#define BR_RACEBEGIN_EQ_HANDLING    0xF8
#define BR_RACEBEGIN_EQ_TRANS       0xFC
#define BR_RACEBEGIN_EQ_TIRE        0x100
#define BR_RACEBEGIN_EQ_SUSP        0x104
#define BR_RACEBEGIN_EQ_FIFTH       0x108   /* slice5_60.h's unnamed fifth */

typedef struct BrRaceCtl {
    uint8_t   b25;                          /* +0x25                       */
    void     *apRec[BR_RACEBEGIN_MAXREC];   /* +0x2C + 4i                  */
    int32_t   aLen[BR_RACEBEGIN_MAXREC];    /* +0x34 + 4i                  */
    int32_t   aCap[BR_RACEBEGIN_MAXREC];    /* +0x3C + 4i                  */
    uint8_t  *pHdr;                         /* +0x44, the 8-byte header    */
    int32_t   f48;                          /* +0x48                       */
    int32_t   f4C;                          /* +0x4C                       */
} BrRaceCtl;

/* ======================================================================
 * The frame clock
 * ====================================================================== */

/* 0x100A9358 ships as 5, and the ring at 0x105BC900 runs to 0x105BCAE0 --
 * 0x1E0 bytes, 120 dwords -- so 120 is the layout's ceiling and 5 is what
 * the image asks for.  The array is sized for the ceiling and the length is
 * a variable, exactly as the original has it. */
#define BR_RACEBEGIN_CLOCK_MAX   120
#define BR_RACEBEGIN_CLOCK_LEN   5      /* 0x100A9358's shipped value     */

extern uint32_t g_aBrRaceClockRing[BR_RACEBEGIN_CLOCK_MAX]; /* 0x105BC900 */
extern int32_t  g_brRaceClockLen;      /* 0x100A9358, ships as 5          */
extern int32_t  g_brRaceClockCursor;   /* 0x100A935C, ships as -1         */
extern uint32_t g_brRaceClockLast;     /* 0x105CCB90                      */
extern uint32_t g_brRaceClockAccum;    /* 0x105CCB7C                      */
extern int32_t  g_brRaceClockCount;    /* 0x105BCAE4, -1 after a reset    */

/* ======================================================================
 * The globals this module owns.  Every one was grepped across port/ before
 * being given storage; none had an owner.  Names are POSITIONAL wherever the
 * meaning is not established by an instruction, which is most of them.
 * ====================================================================== */
extern int32_t  g_brRaceBeginSpecialsN;   /* 0x106EEEFC, the special count */
extern int32_t  g_brRaceBeginAirplane;    /* 0x105BC7C0                    */
extern int32_t  g_brRaceBeginAirTrigger;  /* 0x105BC7C4                    */
extern int32_t  g_brRaceBeginAirArmed;    /* 0x105BC7CC                    */
extern int32_t  g_brRaceBeginPathLeft;    /* 0x105BC7DC                    */
extern int32_t  g_brRaceBeginPathRight;   /* 0x105BC7E0                    */
extern int32_t  g_brRaceBeginPathLen;     /* 0x105BC7D4                    */
extern int32_t  g_brRaceBeginPathIdx;     /* 0x105BC7D0                    */
extern float    g_brRaceBeginPathT;       /* 0x105BC7E4                    */
extern float    g_brRaceBeginPathSeg;     /* 0x105BC7E8                    */
extern float    g_brRaceBeginPathScale;   /* 0x105BC7D8                    */
extern int32_t  g_brRaceBeginFxCount;     /* 0x105BCAE8, waterfalls found  */
extern int32_t  g_aBrRaceBeginFx[8];      /* 0x105BC778, stride 4          */
extern int32_t  g_brRaceBeginCamMinus1;   /* 0x105BC7C8, 0x100BCBE8 - 1    */

extern int32_t  g_brRaceBeginNTexSet;     /* 0x100AA044, ships as 1        */
extern int32_t  g_brRaceBeginRecLen;      /* 0x105BC8D8                    */
/* 0x105BC8E0 is the SOURCE the mode-2 arm copies out of and the header the
 * mode-4 arm reads directly (0x10019DEA points ctl->pHdr straight at it).
 * 0x11778850 is a DIFFERENT object -- the incoming record the mode-2 arm
 * copies INTO and then validates.  Modelling them as one would make the two
 * acceptance tests at 0x1001A018 compare a buffer with itself. */
extern uint8_t  g_aBrRaceBeginRec[BR_RACEBEGIN_HDR_LEN];    /* 0x105BC8E0  */
extern uint8_t  g_aBrRaceBeginRecIn[BR_RACEBEGIN_HDR_LEN];  /* 0x11778850  */
extern int32_t  g_brRaceBeginBestCar;     /* 0x105BC810                    */
extern int32_t  g_brRaceBeginIntroSide;   /* 0x105CCB9C                    */
extern int32_t  g_brRaceBeginMovieDone;   /* 0x105BCAE0                    */
extern int32_t  g_brRaceBeginRecArmed;    /* 0x105BC7B8                    */
extern int32_t  g_brRaceBeginStage;       /* 0x105BC760                    */
extern int32_t  g_brRaceBeginFade;        /* 0x105CCB8C                    */
extern int32_t  g_brRaceBeginRamped;      /* 0x105CCB98                    */
extern int32_t  g_brRaceBeginActive;      /* 0x106ED684                    */
extern int32_t  g_brRaceBeginSeqIdx;      /* 0x105BC768                    */
extern float    g_brRaceBeginSeqT;        /* 0x105BC884                    */
extern int32_t  g_brRaceBeginSeq888;      /* 0x105BC888                    */
extern int32_t  g_brRaceBeginDrawn;       /* 0x106ED6D8                    */
extern int32_t  g_brRaceBeginLights;      /* 0x10AF2090                    */
extern int32_t  g_brRaceBeginB1CF10;      /* 0x10B1CF10                    */
extern int32_t  g_brRaceBeginAF6724;      /* 0x10AF6724                    */
extern int32_t  g_brRaceBeginDifficulty;  /* 0x106EA3F4                    */
extern int32_t  g_brRaceBegin226A4C;      /* 0x10226A4C                    */
extern int32_t  g_brRaceBegin6E86C8;      /* 0x106E86C8                    */
extern int32_t  g_brRaceBegin6E8720;      /* 0x106E8720                    */
extern float    g_brRaceBeginAF397C;      /* 0x10AF397C                    */
extern int32_t  g_brRaceBeginMirrorOff;   /* 0x11778848                    */
extern int32_t  g_aBrRaceBeginLink[4];    /* 0x10AF4C00..0x10AF4C0C        */

/* 0x11787850, stride 0xF000 -- the per-entrant recording buffers the mode-2
 * arm hands to each control block.  Each is BR_RACEBEGIN_REC_CAP bytes in the
 * original; this arm writes only the eight header bytes and then sets the
 * length to 8 and the capacity to 0xDE5C, so eight bytes of storage per
 * entrant is EXACT for everything in this range.  The rest of each buffer is
 * not modelled because nothing here touches it -- which is a statement about
 * this function, not about the buffer. */
extern uint8_t  g_aBrRaceBeginRecHdr[BR_RACEBEGIN_MAXREC][BR_RACEBEGIN_HDR_LEN];

/* 0x100A5EA8 -- the frame limiter's own arm flag; 0x10019A40 raises it and
 * the tail at 0x1001BC10 tests it.  Ships as 1. */
extern int32_t  g_brRaceBeginLimitOn;

/* 0x100A5EB0, sixteen bytes per record.  0x10019930 lays out a schedule over
 * these and 0x10019980 walks the same list decrementing +0x00.  What the
 * schedule IS is not established -- see BrRaceCueLayout. */
#define BR_RACEBEGIN_CUE_MAX  8
/* 0x106EEE3C, stride 0xC -- the track's "specials" list, walked at
 * 0x1001A31A.  The kind byte at +0x08 is read with `movsx`, has 3 subtracted
 * from it and indexes the five-entry table at 0x1001C664; anything outside
 * 3..7 is skipped by the `ja` at 0x1001A324. */
typedef struct BrRaceSpecial {
    int32_t f00;    /* +0x00, the value every arm stores  */
    int32_t f04;    /* +0x04, only kind 4 reads it        */
    int32_t kind;   /* +0x08, a SIGNED byte               */
} BrRaceSpecial;

#define BR_RACEBEGIN_SPECIAL_AIRPLANE   3   /* -> 0x1001A353 */
#define BR_RACEBEGIN_SPECIAL_PATHLEFT   4   /* -> 0x1001A32D */
#define BR_RACEBEGIN_SPECIAL_PATHRIGHT  5   /* -> 0x1001A344 */
#define BR_RACEBEGIN_SPECIAL_TRIGGER    6   /* -> 0x1001A362 */
#define BR_RACEBEGIN_SPECIAL_WATERFALL  7   /* -> 0x1001A371 */

typedef struct BrRaceCue {
    int32_t start;   /* +0x00, written by 0x10019930, decremented by
                      *        0x10019980                                 */
    int32_t len;     /* +0x04                                             */
    int32_t gap;     /* +0x08                                             */
    int32_t next;    /* +0x0C, non-zero == there is another record after
                      *        this one; the list terminator              */
} BrRaceCue;
extern BrRaceCue g_aBrRaceCue[BR_RACEBEGIN_CUE_MAX];  /* 0x100A5EB0       */
extern int32_t   g_brRaceCueArmed;                    /* 0x100A5EBC       */
extern int32_t   g_brRaceCueBase;                     /* 0x106E9A2C       */

/* ======================================================================
 * The frontier
 * ====================================================================== */
typedef enum BrRaceBeginOp {
    BR_RB_TICKMS = 0,     /* 0x1006E280   milliseconds                    */
    BR_RB_TRACE,          /* 0x10008D60   the folded `ret`; exact         */
    BR_RB_1000CB80,       /* 0x1000CB80                                   */
    BR_RB_SLOT_B7352C,    /* *0x10B7352C                                  */
    BR_RB_SLOT_18ED1E8,   /* *0x118ED1E8                                  */
    BR_RB_SLOT_0B849C,    /* *0x100B849C                                  */
    BR_RB_SLOT_B73528,    /* *0x10B73528                                  */
    BR_RB_TRACKHANDLING,  /* 0x10031140 == slice2_20.c BrTrackLoadHandling
                           * ALREADY PORTED -- wire it, do not re-write   */
    BR_RB_TRACKLOAD,      /* 0x100311C0   the .trk loader                 */
    BR_RB_1006E030,       /* 0x1006E030                                   */
    BR_RB_100189C0,       /* 0x100189C0                                   */
    BR_RB_1002BF24,       /* 0x1002BF24(p)                                */
    BR_RB_100353C0,       /* 0x100353C0(0x7B)                             */
    BR_RB_CARMODELSET,    /* 0x1006FD50(car, iModel)                      */
    BR_RB_100609D0,       /* 0x100609D0(p)                                */
    BR_RB_NETOPEN,        /* 0x10005CD0 + 0x10006400 + 0x10004E00         */
    BR_RB_NETSESSION,     /* 0x100060A0 -> handle                         */
    BR_RB_NETNAME,        /* 0x10006250(handle, name)                     */
    BR_RB_NETCOUNT,       /* 0x10009BA0 -> how many players               */
    BR_RB_NETNEXT,        /* 0x100060B0 -> next player index, <0 == none  */
    BR_RB_NETADD,         /* 0x1001C6A0(index)                            */
    BR_RB_PLAYMOVIE,      /* 0x10069A80(name, 0)                          */
    BR_RB_10063B60,       /* 0x10063B60                                   */
    BR_RB_10063A00,       /* 0x10063A00                                   */
    BR_RB_10063A40,       /* 0x10063A40                                   */
    BR_RB_10063DD0,       /* 0x10063DD0                                   */
    BR_RB_100627B0,       /* 0x100627B0(weather)                          */
    BR_RB_10062830,       /* 0x10062830(weather)                          */
    BR_RB_10061310,       /* 0x10061310                                   */
    BR_RB_VECXFORM,       /* 0x10034870(dst, src, m) -- dst == src here   */
    BR_RB_VECLEN,         /* 0x100347F0(v) -> float                       */
    BR_RB_1005C490,       /* 0x1005C490(car)                              */
    BR_RB_1006FCE0,       /* 0x1006FCE0(car, slot, arg)                   */
    BR_RB_1005E7B0,       /* 0x1005E7B0(car)                              */
    BR_RB_1006FCB0,       /* 0x1006FCB0(car, 0)                           */
    BR_RB_10001CF0,       /* 0x10001CF0(car)                              */
    BR_RB_CARIMAGE,       /* 0x100BCDD0 + 89992*slot -- br_cardata.h owns
                           * the image; this asks for the base            */
    BR_RB_TEXCREATE,      /* *0x118ED1C4, 15 arguments -> handle          */
    BR_RB_TEXDOWNLOAD,    /* *0x118ED1C0, 8 arguments  -> bytes written   */
    BR_RB_STRING,         /* 0x1006D280(id) -> text                       */
    BR_RB_FATAL,          /* 0x10008EF0(text)                             */
    BR_RB_BLOBLOAD,       /* 0x10030270(dst, name, buf)                   */
    BR_RB_1002ECAC,       /* 0x1002ECAC(p)                                */
    BR_RB_10033B50,       /* 0x10033B50                                   */
    BR_RB_1002E13B,       /* 0x1002E13B                                   */
    BR_RB_10060E30,       /* 0x10060E30                                   */
    BR_RB_RAMP_A,         /* 0x100181A0(a, b)                             */
    BR_RB_RAMP_B,         /* 0x10018230(a, b)                             */
    BR_RB_RAMP_C,         /* 0x10018290(a, b)                             */
    BR_RB_10002C00,       /* 0x10002C00 -> a track id                     */
    BR_RB_10002AF0,       /* 0x10002AF0(id)                               */
    BR_RB_10002D30,       /* 0x10002D30(v)                                */
    BR_RB_10013E80,       /* 0x10013E80                                   */
    BR_RB_EQUIPRECORD,    /* car+0xE8C's pointee, as raw bytes            */
    BR_RB_CTL,            /* car+0x29C0's pointee                         */
    BR_RB_DIFFICULTY,     /* 0x100BCAB0[track] -> the record byte +4      */
    BR_RB_10B71530,       /* 0x10B71530, read as a 1/2/3 ladder           */
    BR_RB_SPECIAL,        /* 0x106EEE3C[i], the specials list             */
    BR_RB_CTLHUMAN,       /* 0x1005D050, the body car+0xF08 gets for a
                           * slot inside the entrant count                */
    BR_RB_CTLAI,          /* 0x1005E690, the body every other slot gets   */
    /* the neighbours */
    BR_RB_10019840,       /* 0x10019840, inside 0x10019890                */
    BR_RB_10032E40,       /* 0x10032E40, inside 0x10019890                */
    BR_RB_1006E3F0,       /* 0x1006E3F0(this), inside 0x10019A40          */
    BR_RB_1006E360,       /* 0x1006E360,  tail call of 0x10019A40         */
    BR_RB_1005F530,       /* 0x1005F530(driver), inside 0x10019A10        */
    BR_RB_SQRT,           /* 0x10002570(x) -> sqrt, inside 0x100199A0     */
    BR_RB_1006F170,       /* 0x1006F170(car), inside 0x100199A0           */
    BR_RB_NOPS
} BrRaceBeginOp;

typedef struct BrRaceBeginOps {
    uint32_t (*pfnTickMs)(void);
    void     (*pfnTrace)(void);
    void     (*pfn1000CB80)(void);
    void     (*pfnSlotB7352C)(void);
    void     (*pfnSlot18ED1E8)(void);
    void     (*pfnSlot0B849C)(void);
    void     (*pfnSlotB73528)(void);
    void     (*pfnTrackHandling)(int32_t iTrack);
    void     (*pfnTrackLoad)(int32_t iTrack);
    void     (*pfn1006E030)(void);
    void     (*pfn100189C0)(void);
    void     (*pfn1002BF24)(void *p);
    void     (*pfn100353C0)(int32_t v);
    void     (*pfnCarModelSet)(BrDriverCar *pCar, int32_t iModel);
    void     (*pfn100609D0)(void *p);
    void     (*pfnNetOpen)(void);
    void    *(*pfnNetSession)(void);
    void     (*pfnNetName)(void *hSession, const char *pszName);
    uint32_t (*pfnNetCount)(void);
    int32_t  (*pfnNetNext)(void);
    void     (*pfnNetAdd)(int32_t iPlayer);
    void     (*pfnPlayMovie)(const char *pszName, int32_t a2);
    void     (*pfn10063B60)(void);
    void     (*pfn10063A00)(void);
    void     (*pfn10063A40)(void);
    void     (*pfn10063DD0)(void);
    void     (*pfn100627B0)(int32_t weather);
    void     (*pfn10062830)(int32_t weather);
    void     (*pfn10061310)(void);
    /* 0x10034870.  dst and src ARE the same pointer at the only call site --
     * see the banner.  The signature keeps both so a wiring can be checked
     * against the original rather than against this comment. */
    void     (*pfnVecXform)(float *pDst, const float *pSrc, const void *pMat);
    float    (*pfnVecLen)(const float *pV);
    void     (*pfn1005C490)(BrDriverCar *pCar);
    void     (*pfn1006FCE0)(BrDriverCar *pCar, int32_t slot, int32_t arg);
    void     (*pfn1005E7B0)(BrDriverCar *pCar);
    void     (*pfn1006FCB0)(BrDriverCar *pCar, int32_t arg);
    void     (*pfn10001CF0)(BrDriverCar *pCar);
    /* 0x100BCDD0 + 89992*slot.  NULL means the car images are not loaded,
     * which is the .bss state and the state this port is in. */
    uint8_t *(*pfnCarImage)(int32_t slot);
    int32_t  (*pfnTexCreate)(const uint8_t *pPixels, uint8_t *pHdr,
                             int32_t w, int32_t h, int32_t w2,
                             int32_t a5, int32_t a6, int32_t a7, int32_t a8,
                             int32_t a9, int32_t a10, int32_t a11,
                             int32_t a12, int32_t a13, int32_t a14);
    int32_t  (*pfnTexDownload)(const uint8_t *pSrc, uint8_t *pDst,
                               uint8_t *pHdr, int32_t w, int32_t h,
                               int32_t w2, int32_t a6, int32_t a7);
    const char *(*pfnString)(int32_t id);
    void     (*pfnFatal)(const char *pszText);
    void     (*pfnBlobLoad)(void *pDst, const char *pszName, void *pBuf);
    void     (*pfn1002ECAC)(void *p);
    void     (*pfn10033B50)(void);
    void     (*pfn1002E13B)(void);
    void     (*pfn10060E30)(void);
    void     (*pfnRampA)(float a, float b);
    void     (*pfnRampB)(float a, float b);
    void     (*pfnRampC)(float a, float b);
    int32_t  (*pfn10002C00)(void);
    void     (*pfn10002AF0)(int32_t id);
    void     (*pfn10002D30)(int32_t v);
    void     (*pfn10013E80)(void);
    /* car+0xE8C, as raw bytes.  Raw because br_racestart.h establishes that
     * this record is written to disc verbatim, and CONVENTIONS.md forbids
     * overlaying a struct on such an image. */
    uint8_t *(*pfnEquipRecord)(BrDriverCar *pCar);
    BrRaceCtl *(*pfnCtl)(BrDriverCar *pCar);
    int32_t  (*pfnDifficulty)(int32_t iTrack);
    int32_t  (*pfn10B71530)(void);
    /* 0x106EEE3C + 0xC*i.  Non-zero when the entry exists. */
    int32_t  (*pfnSpecial)(int32_t i, BrRaceSpecial *pOut);
    void     (*pfnCtlHuman)(BrDriverCar *pCar);
    void     (*pfnCtlAi)(BrDriverCar *pCar);
    void     (*pfn10019840)(void);
    void     (*pfn10032E40)(void);
    /* 0x1006E3F0.  __thiscall on 0x105BC858, an object with no host model,
     * so the `this` is not an argument here -- inventing a pointer for it
     * would be a claim about a layout nothing in this range establishes. */
    void     (*pfn1006E3F0)(void);
    void     (*pfn1006E360)(void);
    void     (*pfn1005F530)(BrDriver *pDrv);
    float    (*pfnSqrt)(float x);
    void     (*pfn1006F170)(BrDriverCar *pCar);
} BrRaceBeginOps;

extern BrRaceBeginOps g_brRaceBeginOps;

/* How many times each op was REACHED with no hook installed.  Zeroed by
 * BrRaceBeginReset. */
int32_t     BrRaceBeginSkipped(BrRaceBeginOp op);
const char *BrRaceBeginOpName(BrRaceBeginOp op);
void        BrRaceBeginReset(void);

/* The DEVIATION counter: how many times the per-entrant record loop was asked
 * for an index past BR_RACEBEGIN_MAXREC.  Non-zero means the original would
 * have overlapped three arrays and this port did not. */
int32_t     BrRaceBeginRecClamped(void);

/* How many times the replay path entered the paint loop with its counter
 * already at or past the bound -- i.e. how many times 0x1001A973's store made
 * the loop body a no-op.  It is a counter rather than a flag because the
 * alternative reading (that 0x1001A973 re-writes the BOUND) is the one this
 * pass got wrong first, and a counter is what makes the two distinguishable
 * from outside. */
int32_t     BrRaceBeginReplayPaintSkipped(void);

/* ======================================================================
 * The pieces
 * ====================================================================== */

/* 0x10019A70..0x10019AF8.  Stamps the clock, pushes the elapsed milliseconds
 * into the five-frame ring, and returns NON-ZERO when the caller should run
 * the one-time arm -- i.e. when the substate is still 0. */
int  BrRaceStepClock(void);

/* 0x10019AFE..0x1001AB6F, the one-time arm.  Runs the mode's own setup, the
 * common tail, the script seed (which it delegates to br_racestep.c's
 * BrRaceStepInit, since that is the same block of the same function) and the
 * asset work, and leaves the substate at 1. */
void BrRaceStepBegin(void);

/* ----------------------------------------------------------------------
 * The seven neighbours.  Each is a whole function and each is claimed.
 * ---------------------------------------------------------------------- */

/* 0x10019890, 98 B.  Three colour submissions around two calls, all skipped
 * while the game is paused. */
void BrRaceHudFrame(void);

/* 0x10019900, 34 B.  Puts the game into mode 4 stage 2 and installs the race
 * step.  The step it installs is 0x10019A70 itself, which cannot be named
 * from C, so the body it should install is an argument. */
void BrRaceEnterOutro(void (*pfnRaceStep)(void));

/* 0x10019930, 78 B.  Lays a schedule over g_aBrRaceCue. */
void BrRaceCueLayout(void);

/* 0x10019980, 31 B.  Decrements every cue's start. */
void BrRaceCueRewind(void);

/* 0x100199A0, 106 B.  A car controller: writes the car's speed from its
 * velocity and hands the car to 0x1006F170. */
void BrRaceCarCtlOutro(BrDriverCar *pCar);

/* 0x10019A10, 44 B.  0x1005F530 over every driver. */
void BrRaceDriverReset(void);

/* 0x10019A40, 35 B.  Resets the frame clock and arms the limiter. */
void BrRaceClockReset(void);

/* 0x100773A8 -- metres per second to miles per hour, 2.24f, the constant
 * 0x100199A0 scales the speed by.  (The exact factor is 2.2369; the game
 * uses 2.24.) */
#define BR_RACEBEGIN_MPS_TO_MPH   2.240000009536743f

/* 0x100773B0, subtracted from 0x10AF397C at 0x1001A694 -- so the store is an
 * ADD of one. */
#define BR_RACEBEGIN_AF397C_STEP  (-1.0f)

/* 0x1001AA1C / 0x1001AA33 / 0x1001AA4A -- the three ramps' arguments. */
#define BR_RACEBEGIN_RAMP_A       1.0f
#define BR_RACEBEGIN_RAMP_B       0.20000000298023224f   /* 0x3E4CCCCD */

/* 0x1001A5E8 -- the mode-2 arm's recovery level, and 0x1001A5C7's default. */
#define BR_RACEBEGIN_LINK_RECOVER 0.375f    /* 0x3EC00000 */
#define BR_RACEBEGIN_FULL         1.0f      /* 0x3F800000 */

/* 0x1001A0F7 / 0x1001A1CA -- the per-entrant record's capacity and length. */
#define BR_RACEBEGIN_REC_CAP      0xDE5C
#define BR_RACEBEGIN_REC_LEN0     8

/* 0x1001AB28 / 0x1001AB35 -- mode 4's two fixed track ids. */
#define BR_RACEBEGIN_OUTRO_TRACK  0x0C
#define BR_RACEBEGIN_CREDIT_TRACK 0x0D

/* 0x10019B3E / 0x1001A28C -- mode 5's fixed track. */
#define BR_RACEBEGIN_MODE5_TRACK  0x0C

/* 0x10019B86 -- the only argument 0x100353C0 is ever given here. */
#define BR_RACEBEGIN_100353C0_ARG 0x7B

/* 0x1001A6E1 -- the weather that selects the far header offset. */
#define BR_RACEBEGIN_WEATHER_FAR  4
#define BR_RACEBEGIN_HDR_OFF_FAR  0x300
#define BR_RACEBEGIN_HDR_OFF_NEAR 0x100

/* 0x1001A756 -- the mip chain the paint block builds. */
#define BR_RACEBEGIN_TEX_LEVELS   0xF

/* 0x1001A8D8 / 0x1001A90D -- the two overrun messages' string ids. */
#define BR_RACEBEGIN_MSG_OVERRUN  0x12C
#define BR_RACEBEGIN_MSG_TOOBIG   0x12D

/* 0x1001A905 -- the paint scratch area's size. */
#define BR_RACEBEGIN_PAINT_BYTES  0x8000

/* 89992, the per-car image stride.  br_racestart.h reads the same number out
 * of "sizeof(UltraCarHeader)=%d"; the `lea` chain at 0x10019D00 and the one
 * at 0x1001A6CC both compute it. */
#define BR_RACEBEGIN_CARIMAGE_STRIDE  89992

/* 0x1001A6B5's cursor, and the -1 it writes.  See the banner. */
#define BR_RACEBEGIN_TEXSLOT_NONE   (-1)

#endif /* BR_RACEBEGIN_H */
