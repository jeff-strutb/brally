/* slice2_15.h -- decompiled from BRD3D.dll, agent-15 packet
 * (address range 0x10016A60 - 0x1001CCA0).
 *
 * The packet is four loosely related clusters:
 *
 *   1. HUD / on-screen instrument drawing, 0x10016A60-0x100180B0.
 *      Everything here emits into ONE global display-list write cursor
 *      (0x106C0680) as 8-byte {w0,w1} commands.
 *
 *   2. Weather: wind, lightning and a screen-space particle field,
 *      0x10019490-0x100196D0.
 *
 *   3. Display-list COMMAND HANDLERS, 0x1001BE10-0x1001CCA0. These are the
 *      other end of the pipe: each takes a pointer to one 8-byte command,
 *      decodes it into renderer state, and returns the address of the next
 *      command (arg + 8).
 *
 *   4. Five one-line "recache a screen dimension" thunks, 0x1001BB80-0x1001BC50.
 *
 * Everything the original reached through a fixed address is modelled as a
 * file-static state block reachable through a Get...() accessor, so the ported
 * functions keep the original's argument lists exactly.
 *
 * ------------------------------------------------------------------------
 * WHAT THE COMMAND STREAM IS
 * ------------------------------------------------------------------------
 * This is N64-GBI-SHAPED but it is NOT stock F3D: 0x10016A60 emits a rect with
 * opcode byte 0xE3 (stock G_TEXRECT is 0xE4), and 0xDE/0xDF carry IEEE floats
 * (+1.0f / -1.0f) rather than G_DL/G_ENDDL payloads. Several opcodes do line
 * up with the RDP (0xF2 G_SETTILESIZE, 0xFB G_SETENVCOLOR, 0xB9/0xBA
 * G_SETOTHERMODE_L/H, 0x04 G_VTX, 0xB1 G_TRI2), so treat it as the PC
 * backend's own dialect of F3DEX rather than assuming stock semantics.
 *
 * TWO rect encodings coexist and they are NOT interchangeable:
 *   0x1001BE30  reads the four 12-bit coordinate fields as 10.2 FIXED POINT
 *               (shl 20 / sar 22, i.e. sign-extend then >>2) and masks to 10 bits
 *   0x1001C7A0  reads the SAME fields as plain signed integers (shl 20 / sar 20)
 *               with no mask
 * 0x10016A60 emits coordinates in the second (integer) form.
 *
 * ------------------------------------------------------------------------
 * CONSTANTS
 * ------------------------------------------------------------------------
 * Every float constant used here was read out of the DLL's .rdata rather than
 * guessed; see slice2_15.c. The ones that pin down meaning:
 *   0x1008F338 = 1/2.24        (m/s -> mph)
 *   0x1008F348 = 0.6213712     (km -> miles)
 *   0x1008F378 = 0.2777778     (km/h -> m/s)   0x1008F37C = 3.6 (the inverse)
 *   0x1008F36C = -343.0        (speed of sound; the lightning timer integrates
 *                               distance, and thunder ends past 2048 units)
 *   0x1008F350 = 1/32768, 0x1008F354 = 1.0     (rand()&0xFFFF -> [-1,+1])
 *   0x1008F358 = 2*pi, 0x1008F360 = -2*pi      (wind-angle wrap)
 */
#ifndef SLICE2_15_H
#define SLICE2_15_H

#include <stddef.h>
#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_bits.h"

/* =====================================================================
 * Display list
 * ===================================================================== */

/* One command. The original allocates by bumping the global cursor by 8 and
 * writing two dwords, so this struct IS the allocation unit. */
typedef struct BrGfxCmd { uint32_t w0, w1; } BrGfxCmd;

/* 0x106C0680 -- the write cursor. Nothing here ever bounds-checks it. */
typedef struct BrGfxOut { BrGfxCmd *pCur; } BrGfxOut;
BrGfxOut *BrGfxGetOut(void);

/* =====================================================================
 * Screen / view
 * ===================================================================== */

typedef struct BrScreenInfo {
    int32_t cx;      /* 0x100A81C0 -- framebuffer width  */
    int32_t cy;      /* 0x100A81C4 -- framebuffer height */
    int32_t cViews;  /* 0x100AA8B4 -- 1 = full screen, 2 = split, ...  */
    int32_t iView;   /* 0x106C5708 -- view being drawn right now */
} BrScreenInfo;
BrScreenInfo *BrScreenGet(void);

/* The record every HUD function indexes with `11 * iView` scaled by 8, i.e. a
 * stride of 0x58 bytes.
 *
 * GOTCHA: only the RECTANGLE (+0x00..+0x10) is read per-view. The dial frame
 * table at +0x14..+0x50 and the overlay list at +0x54 are read from RECORD 0
 * unconditionally, because 0x10016B40 keeps a second, un-indexed copy of the
 * base pointer for them. That asymmetry is in the original; do not "fix" it.
 *
 * aDial is indexed BACKWARDS: the original computes `[base + (0x14 - v)*4]`
 * for a clamped v in [0,15], i.e. aDial[15 - v]. */
typedef struct BrHudView {
    int32_t  x, y, w, h;    /* +0x00 +0x04 +0x08 +0x0C */
    int32_t  iSprite;       /* +0x10 -- index into the sprite descriptor table */
    uint32_t aDial[16];     /* +0x14 .. +0x50 */
    uint32_t dlOverlay;     /* +0x54 */
} BrHudView;                /* 0x58 */

/* =====================================================================
 * Sprite descriptor table (0x100C12A0)
 * ===================================================================== */

/* The original indexes it as `(11*i*64 - i)*16 + i` scaled by 8, which is
 * 89992 bytes per record. Only the prefix below and a byte area starting at
 * +0x500 are ever touched, so the record is declared as that prefix and the
 * true stride is kept separately. */
#define BR_HUDSPRITE_STRIDE  89992u
#define BR_HUDSPRITE_DATAOFF 0x500u

typedef struct BrHudSprite {
    uint8_t  a0000[0xDB];
    uint8_t  mode;              /* +0x00DB -- 0, 1 or 2; see BrHudDrawDial */
    uint8_t  a00DC[0xE4-0xDC];
    int8_t   e4, e5;            /* +0x00E4 +0x00E5 -- read with movsx: SIGNED */
    int8_t   e6, e7;            /* +0x00E6 +0x00E7 */
    int8_t   e8, e9;            /* +0x00E8 +0x00E9 */
    int8_t   ea, eb;            /* +0x00EA +0x00EB */
    float    fEC;               /* +0x00EC */
    float    fF0;               /* +0x00F0 */
    uint32_t fF4, fF8;
    uint32_t fFC;               /* +0x00FC -- byte stride of the +0x500 data */
} BrHudSprite;                  /* 0x100 (a PREFIX of the 89992-byte record) */

/* =====================================================================
 * Car records (stride 0x2B68 -- a project-wide known constant)
 * ===================================================================== */

#define BR_CAR_STRIDE 0x2B68

typedef struct BrCar {
    uint8_t  a0000[0x0FF4];
    float    f0FF4;             /* +0x0FF4 */
    int32_t  f0FF8;             /* +0x0FF8 -- ranking key, compared against 0xFF */
    uint8_t  a0FFC[0x1030-0x0FFC];
    float    f1030;             /* +0x1030 -- speed; scaled by 1/2.24 */
    uint8_t  a1034[BR_CAR_STRIDE-0x1034];
} BrCar;                        /* exactly BR_CAR_STRIDE, no pointers inside */

/* =====================================================================
 * Race / session block (0x106C2CF8 points at it)
 * ===================================================================== */

/* Logical, not byte-exact: the original offsets are in the comments. The gaps
 * between them are large and their contents unknown, so padding them out would
 * be noise. */
typedef struct BrRace {
    float       f0E24;      /* +0x0E24 -- added to a 0..0x7F jitter */
    float       f0E68;      /* +0x0E68 */
    int32_t     f0E70;      /* +0x0E70 */
    int32_t     cSplits;    /* +0x0FA8 */
    const float *aSplits;   /* +0x0FB4, stride 4 (in the original, inline) */
    const char  *psz0FFC;   /* +0x0FFC */
    const char  *psz1004;   /* +0x1004 */
    float       f1030;      /* +0x1030 -- the speed shown by BrHudDraw */
} BrRace;

/* =====================================================================
 * HUD environment -- every other global the HUD half reads
 * ===================================================================== */

/* The 4-vertex quad 0x10016B40 builds at 0x104AFD20, one per
 * (iView + 2*f6C65EC). Stride 0x80; per-vertex stride 0x20. */
typedef struct BrHudVert {
    float x, y, z;          /* +0x00 +0x04 +0x08 */
    float f0C, f10;         /* +0x0C +0x10 -- never written here */
    float f14, f18, f1C;    /* +0x14 +0x18 +0x1C -- written 0, 255.0f, 0 */
} BrHudVert;
typedef struct BrHudQuad { BrHudVert v[4]; } BrHudQuad;   /* 0x80 */

#define BR_HUD_QUADS 4      /* 0x104AFD20 is indexed by iView + 2*f6C65EC */
#define BR_HUD_VIEWS 4      /* 0x100A73A8 is indexed by iView */

typedef struct BrHudEnv {
    /* gates */
    int32_t  f0BD3F4;       /* 0x100BD3F4 -- BrHudDrawDial returns unless set  */
    int32_t  f0BD3F0;       /* 0x100BD3F0 -- BrHudDrawSplitList gate           */
    int32_t  f22AF1C;       /* 0x1022AF1C -- non-zero suppresses dial + speed  */
    int32_t  f6909B4;       /* 0x106909B4 -- non-zero replaces rand() with 0x40 */
    int32_t  f0ADF60;       /* 0x100ADF60 -- non-zero selects miles over km    */

    /* inputs */
    int32_t  f6C0684;       /* 0x106C0684 } fed to BrSub_1003407D as floats    */
    int32_t  f6C299C;       /* 0x106C299C }                                    */
    int32_t  f6C65EC;       /* 0x106C65EC -- quad bank selector                */
    int32_t  cCars;         /* 0x100B36FC                                      */
    BrRace  *pRace;         /* 0x106C2CF8                                      */

    /* tables */
    const uint8_t *pSprites;        /* 0x100C12A0, BR_HUDSPRITE_STRIDE apart   */
    void *const   *apStrings;       /* 0x11829370, for BrHandleLookup          */
    BrHudQuad      aQuads[BR_HUD_QUADS];         /* 0x104AFD20                 */
    int32_t        aLastSeq[BR_HUD_VIEWS];       /* 0x100A73A8, init -1        */

    /* text */
    const char *pszCentre;  /* 0x104B0338 -- BrHudDrawViewCentreText draws it  */
    char        szText[64]; /* 0x104B0320 -- sprintf scratch                   */
    char        szGap[32];  /* 0x104AFF20 -- BrHudFormatGapString returns it   */
    const char *pszSplitPrefix; /* 0x100A73C0 -- the literal "%ww"             */
} BrHudEnv;
BrHudEnv *BrHudGetEnv(void);

/* =====================================================================
 * Scene setup environment (0x10018070 / 0x100180B0)
 * ===================================================================== */

typedef struct BrCamObj {
    uint8_t  a00[0x30];
    int32_t  f30, f34;      /* +0x30 +0x34 -- passed through untouched */
    float    f38;           /* +0x38 -- scaled by 0.99 */
} BrCamObj;

typedef struct BrSceneEnv {
    int32_t  f6C6608;       /* 0x106C6608 -- non-zero skips everything but the
                             *               four opening 0xBC commands       */
    int32_t  f6C661C;       /* 0x106C661C } any non-zero, or f6C7C98 == 0, or  */
    int32_t  f6C6620;       /* 0x106C6620 } f0B4050 == 2, makes                */
    int32_t  f6C6624;       /* 0x106C6624 } BrSceneUsePlainClear return 1      */
    int32_t  f6C7C98;       /* 0x106C7C98 */
    int32_t  f0B4050;       /* 0x100B4050 */
    int32_t  f6C6618;       /* 0x106C6618 -- gates the 0xFB env-colour command */
    int32_t  f0A79CC;       /* 0x100A79CC -- shared with the lightning state   */
    int32_t  f6C32D0;       /* 0x106C32D0 */
    int32_t  f6C0258;       /* 0x106C0258 */
    int32_t  f6C3364;       /* 0x106C3364 } compared for EQUALITY only         */
    int32_t  f6C1174;       /* 0x106C1174 }                                    */
    uint8_t  c6C0200;       /* 0x106C0200 } four byte colour components        */
    uint8_t  c6C1614;       /* 0x106C1614 }                                    */
    uint8_t  c6C0260;       /* 0x106C0260 }                                    */
    uint8_t  c690BE8;       /* 0x10690BE8 }                                    */
    BrCamObj *pCam;         /* 0x106C6490 */
    BrMat4    mtx;          /* 0x106C0860 */
} BrSceneEnv;
BrSceneEnv *BrSceneGetEnv(void);

/* =====================================================================
 * Weather
 * ===================================================================== */

/* The 32-byte header at 0x104B2550 followed by the particle field at
 * 0x104B2570. 0x10019490 writes 512 records of three int16 per layer and then
 * advances by 0xC3C bytes = 522 records, so ten records per layer are spare;
 * the limit 0x104B3DEC yields exactly two layers. */
#define BR_PARTICLES_PER_LAYER 512
#define BR_PARTICLE_STRIDE     522     /* 0xC3C / 6 */
#define BR_PARTICLE_LAYERS     2

/* The per-view drift block that BrWeatherStepParticles integrates. */
typedef struct BrCamBlock {
    BrVec3  v00;                /* +0x00 */
    uint8_t a0C[0x30-0x0C];
    BrVec3  v30;                /* +0x30 */
} BrCamBlock;

typedef struct BrWeather {
    int32_t cParticles;         /* 0x104B2550 -- 0x200 / cViews               */
    int32_t f2554, f2558;       /* 0x104B2554 0x104B2558 -- BrForward1001A4B0 */
    float   thunderDist;        /* 0x104B255C -- integrates 343 * dt          */
    float   windX, windY, windZ;/* 0x104B2560 0x104B2564 0x104B2568           */
    int32_t f256C;              /* 0x104B256C -- untouched here               */
    int16_t aParticles[BR_PARTICLE_LAYERS][BR_PARTICLE_STRIDE][3];  /* 0x104B2570 */

    float   windAngle;          /* 0x104BBE0C -- wrapped into [0, 2*pi)       */
    float   windGain;           /* 0x100A79C8 -- clamped to [0.5, 1.0]        */
    float   dt;                 /* 0x106C2CFC                                 */
    int32_t lightning;          /* 0x100A79CC -- -1 idle, 3..0 counting down  */
    float   flashX, flashY;     /* 0x104B0378 0x104B037C                      */
    int32_t flashZ;             /* 0x104B0380 <- 0x106C7C80                   */
    int32_t f6C7C80;            /* 0x106C7C80                                 */

    int32_t fInit;              /* 0x104BBE14 -- one-shot seed flag           */
    float   speed;              /* 0x104BBDE8                                 */
    float   k;                  /* 0x104B1F00                                 */
    BrVec3  aPrev[BR_HUD_VIEWS];/* 0x104B0688, stride 0xC                     */
    BrVec3  aDrift[BR_HUD_VIEWS];/* 0x104BBDF0, stride 0xC                    */

    int32_t rain;               /* 0x106C6620 */
    int32_t storm;              /* 0x106C6624 */

    /* 0x10AD05DC holds a POINTER, restrided by BR_CAR_STRIDE. A pointer array
     * cannot be laid out at that stride on a 64-bit host, so the port asks for
     * the block instead. Returning NULL is not handled -- neither is it in the
     * original. */
    const BrCamBlock *(*pfnGetBlock)(int iView);

    /* 0x11829100, indexed by BrForward1001A4B0's argument. */
    void *const *apTable;
} BrWeather;
BrWeather *BrWeatherGet(void);

/* =====================================================================
 * Command-handler destination state
 * ===================================================================== */

/* Every global written by the 0x1001BB80-0x1001CCA0 handlers. Grouped by the
 * handler that writes it; their MEANING is not established, so they keep
 * address-derived names. */
typedef struct BrRdpRegs {
    int32_t f4C5164;        /* 0x104C5164 <- screen cx        (0x1001BB80) */
    int32_t f4C01A0;        /* 0x104C01A0 <- screen cy        (0x1001BBA0) */
    float   f4BBF08;        /* 0x104BBF08 <- (float)(cx / 2)  (0x1001BBC0) */
    float   f4C0BB0;        /* 0x104C0BB0 <- (float)(cx / 2)  (0x1001BC20) */
    float   f4C0BB8;        /* 0x104C0BB8 <- (float)(cy / 2)  (0x1001BC50) */

    uint32_t f4C5158, f4C515C;  /* 0x104C5158 0x104C515C      (0x1001C7F0) */

    float   f4BBF04, f4C0BAC, f4BBEB8, f4BBE2C;  /* 0x1001CB40, a..d */
    float   f4C5154, f4C5160, f4C1690, f4C0BA8;  /* 0x1001CCA0, a..d */

    uint8_t c4BBF00;        /* 0x104BBF00 } (0x1001CC00) */
    uint8_t c4BC194;        /* 0x104BC194 }              */
    uint8_t c4C5150;        /* 0x104C5150 }              */
    uint8_t c4C15CC;        /* 0x104C15CC }              */

    /* 0x118AA0B8 -- an indirect call target, not a data global. */
    void (*pfn18AA0B8)(uint32_t w0lo24, uint32_t w1);
} BrRdpRegs;
BrRdpRegs *BrRdpGetRegs(void);

/* =====================================================================
 * Functions
 * ===================================================================== */

/* 0x10016A60  emit THREE commands describing one textured rectangle:
 *     0xDC000000 | (dlAddr & 0xFFFFFF)   w1 = 1
 *     0xF2002002                         w1 = tile size, uls=ult=0.5 (10.2),
 *                                             lrs = w*4-2, lrt = h*4-2
 *     0xE3000000 | (x+w)<<12 | (y+h)     w1 = x<<12 | y
 * Coordinates are PLAIN INTEGERS in 12-bit fields (the *4 / >>2 round trip in
 * the original cancels exactly), so this pairs with 0x1001C7A0, not 0x1001BE30. */
void BrGfxDrawTexRect(uint32_t dlAddr, int x, int y, int w, int h);

/* 0x10016B40  build the instrument quad and its two sprites for the current
 * view. Mechanism: four vertices swept about (x, cy - y) at angles
 * A-0.05, A+0.05 (radius 15 or 20) and A+0.3, A-0.3 (radius 5 or 7) -- a
 * needle. A is derived from the sprite's fF0/fEC pair and a jittered clock.
 *
 * Returns immediately unless env->f0BD3F4 is set and env->f22AF1C is clear,
 * and the needle half additionally requires sprite->mode == 0. */
void BrHudDrawDial(BrHudView *aViews);

/* 0x10017690  draw env->pszCentre at (x + w/2, y + h/3 + 0x18 + (3*s)/16)
 * where s is 0x1E for a full-screen view and 0x14 otherwise. */
void BrHudDrawViewCentreText(const BrHudView *aViews);

/* 0x10017790  draw pRace->psz0FFC at (x + w/2, y + h/3 + s/4) if it is set,
 * else pRace->psz1004 at (..., y + h/3 + (3*s)/16).
 *
 * GOTCHA: the two branches use DIFFERENT text sizes -- the psz0FFC branch
 * passes s (0x1E / 0x14) and the psz1004 branch passes the other size
 * (0x14 / 0x0F). */
void BrHudDrawViewMessage(const BrHudView *aViews);

/* 0x10017CD0  seconds of gap between the best-ranked car and aCars[iCar].
 *
 *   f = aCars[best].f0FF4 - aCars[iCar].f0FF4, where `best` is the car with
 *       the smallest f0FF8, seeded at 0xFF (so f0FF8 >= 0xFF never wins)
 *   if f == 0 (or unordered) return f
 *   v = aCars[iCar].f1030 / 2.24
 *   if v == 0 (or unordered) return 1000.0f      <- the "no time" sentinel
 *   return f / max(v, 25.0f)                     <- ASYMMETRIC: v is clamped
 *                                                   up only, never down */
float BrHudGapSeconds(const BrCar *aCars, int iCar);

/* 0x10017C80  "+<gap>" when BrHudGapSeconds is strictly positive, else "".
 * Returns env->szGap either way -- never NULL, never a fresh buffer. */
const char *BrHudFormatGapString(const BrCar *aCars, int iCar);

/* 0x10017D90  the per-frame HUD pass. In order: viewport, dial, 0x10017290,
 * splits, 0x100173F0, message, then the speed readout in km/h or mph.
 * The speed is clamped at zero BEFORE any of the sub-passes run. */
void BrHudDraw(BrHudView *aViews, int a2);

/* 0x10017F30  draw one line per pRace->aSplits[i], i in [0, cSplits). */
void BrHudDrawSplitList(const BrHudView *aViews);

/* 0x10017FE0  format and draw "<prefix><rank>. <m>:<ss>.<cc>".
 *
 * total = (int)(fSeconds * 100.0f) truncated toward zero, then
 * hundredths = total % 100; total /= 100; m = total / 60; ss = total % 60.
 * All divisions are SIGNED and truncate, so a negative fSeconds yields
 * negative fields. Same arithmetic as slice1_03's BrFormatTime. */
void BrHudDrawSplitLine(const char *pszPrefix, int rank, float fSeconds,
                        int x, int y);

/* 0x10018070  1 when the frame should take the plain-clear path.
 * Note the polarity: it returns 1 if ANY of f6C661C/f6C6620/f6C6624 is set,
 * OR f6C7C98 is zero, OR f0B4050 == 2. Zero only when all five agree. */
int BrSceneUsePlainClear(void);

/* 0x100180B0  emit the frame's fixed setup command block. */
void BrSceneSetupFrame(const BrHudView *aViews);

/* 0x10019490  refill both particle layers with random int16 triples.
 * Also resets cParticles to 0x200 -- which BrWeatherStepParticles then
 * overwrites with 0x200 / cViews. */
void BrWeatherRandomiseParticles(void);

/* 0x100194E0  integrate windAngle (wrapped to [0, 2*pi)) and windGain
 * (clamped to [0.5, 1.0]), then windX/windY = cos/sin * gain * dt, windZ = 0. */
void BrWeatherStepWind(void);

/* 0x10019620  lightning state machine.
 *   lightning < 0 : with probability 0x80/0x10000, start a strike --
 *                   thunderDist = 0, lightning = 3, flashX/Y/Z picked
 *   lightning > 0 : thunderDist += 343 * dt, lightning--
 *   lightning ==0 : thunderDist += 343 * dt; when it passes 2048, go idle (-1) */
void BrWeatherStepLightning(void);

/* 0x100196D0  advance the particle field for every view. */
void BrWeatherStepParticles(void);

/* 0x1001A4B0  forward apTable[i] plus &f2554 / &f2558 to 0x100290A0. */
void BrForward1001A4B0(int i);

/* The five recache thunks. Each is preceded by an incremental-link jmp/nop
 * pad in the original; the pad has no effect. */
void BrRdpCacheScreenWidth(void);   /* 0x1001BB80 */
void BrRdpCacheScreenHeight(void);  /* 0x1001BBA0 */
void BrRdpCacheHalfWidthA(void);    /* 0x1001BBC0 */
void BrRdpCacheHalfWidthB(void);    /* 0x1001BC20 */
void BrRdpCacheHalfHeight(void);    /* 0x1001BC50 */

/* ---- command handlers: all take one command and return the next ---- */

/* 0x1001BE10  call regs->pfn18AA0B8(w0 & 0xFFFFFF, w1). */
const BrGfxCmd *BrCmdDispatchIndirect(const BrGfxCmd *pCmd);

/* 0x1001BE30  rect with 10.2 fixed-point coordinates, masked to 10 bits.
 * 0x1001C7A0  the same rect with plain 12-bit signed integer coordinates.
 *
 * Both call 0x1001BE90 as (x1, cy - y2 - 1, x2 + 1, cy - y1): Y is FLIPPED
 * against the screen height and the far edges are adjusted by one, so the
 * result is a half-open [left,right) x [top,bottom) box. */
const BrGfxCmd *BrCmdRectFixed(const BrGfxCmd *pCmd);
const BrGfxCmd *BrCmdRectInt(const BrGfxCmd *pCmd);

/* 0x1001C7F0  latch w0/w1 into f4C5158/f4C515C, then apply via 0x1001C820. */
const BrGfxCmd *BrCmdLatchPair(const BrGfxCmd *pCmd);

/* 0x1001CB40 / 0x1001CCA0  unpack w1 as four bytes scaled by 1/255 into two
 * different sets of float globals. Byte order is high-to-low: bits 31..24 land
 * in the FIRST slot. */
const BrGfxCmd *BrCmdSetColorA(const BrGfxCmd *pCmd);
const BrGfxCmd *BrCmdSetColorB(const BrGfxCmd *pCmd);

/* 0x1001CC00  unpack four bit fields of w1 into byte globals:
 *   c4BBF00 = ((w1>>8) & ~7) | ((w1>>13) & 7)
 *   c4BC194 = ((w1>>3) & ~7) | ((w1>>8)  & 7)
 *   c4C5150 = (((w1 & 0xFE) << 2) & 0xFF) | ((w1>>3) & 7)
 *   c4C15CC = (w1 & 1) ? 0xFF : 0x00
 * The first two are the classic "splice three bits" xor idiom; the third
 * truncates to eight bits BEFORE the or, which drops w1 bits 6 and 7. */
const BrGfxCmd *BrCmdUnpackModeBits(const BrGfxCmd *pCmd);

/* =====================================================================
 * Cross-slice callees (definitions live outside this packet)
 * ===================================================================== */

/* XSLICE 0x10019300 */
extern void BrTextDraw(const char *psz, int x, int y);

/* XSLICE 0x1003BD50 */
extern int BrRandom(void);

/* XSLICE 0x1002B2A0 */
extern int BrSub_1002B2A0(void);
/* XSLICE 0x1003407D */
extern void BrSub_1003407D(float a, float b);
/* XSLICE 0x100020D0 */
extern void BrSub_100020D0(char *pszOut, float v);
/* XSLICE 0x1003289F */
extern void BrSub_1003289F(int a, int b, int c, int d);
/* XSLICE 0x10017290 */
extern void BrSub_10017290(BrHudView *aViews);
/* XSLICE 0x100173F0 */
extern void BrSub_100173F0(BrHudView *aViews, int a2);
/* XSLICE 0x10019260 */
extern void BrSub_10019260(void);
/* XSLICE 0x10019270 */
extern void BrSub_10019270(void);
/* XSLICE 0x10019280 */
extern void BrSub_10019280(void);
/* XSLICE 0x10019290 */
extern void BrSub_10019290(void);
/* XSLICE 0x100192F0 */
extern void BrSub_100192F0(int size);
/* XSLICE 0x1002F900 -- seventeen arguments: the command slot then four
 * {0,0,0,tag} groups. */
extern void BrSub_1002F900(BrGfxCmd *pCmd,
                           int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                           int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                           int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                           int32_t a13, int32_t a14, int32_t a15, int32_t a16);
/* XSLICE 0x10031140 */
extern void BrSub_10031140(BrMat4 *pM, int32_t a, int32_t b, float c);
/* XSLICE 0x10031688 */
extern void BrSub_10031688(int32_t x, int32_t y, int32_t w, int32_t h,
                           int32_t c0, int32_t c1, int32_t c2);
/* XSLICE 0x10069490 */
extern BrMat4 *BrSub_10069490(void);
/* XSLICE 0x1001BE90 */
extern void BrSub_1001BE90(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
/* XSLICE 0x1001C820 */
extern void BrSub_1001C820(uint32_t w0, uint32_t w1);
/* XSLICE 0x100290A0 */
extern void BrSub_100290A0(void *pv1, void *pv2, void *pv3);

#endif /* SLICE2_15_H */
