/* slice2_17.h -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * Address range 0x1002BF40 .. 0x10031688.
 *
 * Clusters in this packet:
 *
 *   1. camera / basis matrices     0x100309A0 0x10030B50 0x10030E20 0x10030EE0
 *   2. RDP fill / scissor emitters 0x10031481 0x100314E8 0x10031688
 *   3. the prop display list       0x1002FB20
 *   4. the car table + save/restore 0x1002F130 0x1002F230 0x1002F2A0 0x1002F320
 *   5. small global glue           the 0x1002Cxxx run and the 0x100312xx run
 *
 * Already covered elsewhere and therefore NOT repeated here:
 *   br_mat.h    0x100307A0 0x100307D0 0x10030810 0x10030930 0x100310F0
 *   br_vecd.h   0x100305B0 0x100305F0 0x10030600 0x10030640 0x10030670 0x10030DE0
 *   slice1_05.h 0x1002C150..0x1002C1B0, 0x1002F900/0x1002FAC0/0x1002FAF0,
 *               0x100306C0, 0x10031140
 *
 * SKIPPED: 0x10030210 -- see the .c for why.
 */
#ifndef SLICE2_17_H
#define SLICE2_17_H

#include <stddef.h>
#include <stdint.h>

#include "br_mat.h"      /* BrMat4, BrMat4Copy                    */
#include "br_vecd.h"     /* BrVec3d, BrPackNormalByte             */
#include "slice1_05.h"   /* BrGfxWords, BrMat4Mul, BrMat4Translate,
                          * BrRdpSetCombineLERP, BrPtrList        */

/* ===================================================================== */
/* Module state                                                          */
/* ===================================================================== */

/* Strides read straight off the address arithmetic in the original. */
#define BR_CAR_STRIDE     0x2B68   /* 0x10ACDEA8[i]  (matches the contract) */
#define BR_SLOT_STRIDE    0x0080   /* 0x10ACD498[i]                         */
#define BR_SCRATCH_STRIDE 0x0018   /* 0x106C29F0[i]                         */
#define BR_SCRATCH_SLOTS  32       /* the (i+1) % 32 in 0x10031190          */
#define BR_SCRATCH_DEPTH  0x20     /* the 0x20 compared in 0x10031190       */

/* Byte offsets inside one 0x2B68 car record. Every one of these is a
 * *difference between two absolute addresses* in the original, so they are
 * facts, not guesses:
 *
 *   0x10ACDFEC - 0x10ACDEA8 = 0x0144      owner pointer
 *   0x10ACDFF0 - 0x10ACDEA8 = 0x0148      name string (strcpy target)
 *   0x10ACED34 - 0x10ACDEA8 = 0x0E8C      pointer to a command block
 *   0x10ACEDB0 - 0x10ACDEA8 = 0x0F08      zeroed on removal
 *   0x10ACEE50 - 0x10ACDEA8 = 0x0FA8      save/restore window base
 *   0x10AD0850 - 0x10ACDEA8 = 0x29A8
 *   0x10AD0854 - 0x10ACDEA8 = 0x29AC
 */
#define BR_CAR_OFF_OWNER   0x0144
#define BR_CAR_OFF_NAME    0x0148
#define BR_CAR_OFF_CMDPTR  0x0E8C
#define BR_CAR_OFF_ACTIVE  0x0F08
#define BR_CAR_OFF_SAVE0   0x0FA8   /* <-> 0x106805B0[i]                 */
#define BR_CAR_OFF_SAVEVEC 0x0FB4   /* <-> 0x10690950[i] (nSaveDwords)   */
#define BR_CAR_OFF_SAVE1   0x0FE4   /* <-> 0x106909C0[i]  (save only!)   */
#define BR_CAR_OFF_SAVE2   0x0FE8   /* <-> 0x10680748[i]                 */
#define BR_CAR_OFF_SAVE3   0x0FEC   /* <-> 0x10680728[i]                 */
#define BR_CAR_OFF_SAVE4   0x0FF8   /* <-> 0x106805C8[i]                 */
#define BR_CAR_OFF_TAG     0x29A8
#define BR_CAR_OFF_RGB     0x29AC   /* three bytes, +0x29AC/AD/AE        */

/* Byte offset inside one 0x80 slot record (0x10ACD4F8 - 0x10ACD498). */
#define BR_SLOT_OFF_CARPTR 0x60

/* Everything in this packet that lives in .data. Gathered into one struct
 * the same way slice1_03.h gathers BrTextState, so the code stays testable.
 * The arrays are POINTERS: the original's capacities are not established by
 * any code in this packet, so the caller supplies the storage.
 *
 * NOTE FOR INTEGRATION: pGfx is 0x106C0680, the same display-list cursor
 * BrTextState::pGfx in slice1_03.h names. They must end up as one variable.
 */
typedef struct BrS17State {
    /* --- display list ------------------------------------------------ */
    uint32_t *pGfx;              /* 0x106C0680  write cursor, 2 dwords/cmd */

    /* --- counts ------------------------------------------------------ */
    int nCars;                   /* 0x100B4050                            */
    int nEntA;                   /* 0x100B36F8                            */
    int nEntB;                   /* 0x100B36FC                            */
    int nSaveDwords;             /* 0x100BD3E0  dword count for the copies */

    /* --- car table --------------------------------------------------- */
    unsigned char *pCars;        /* 0x10ACDEA8, BR_CAR_STRIDE apart        */
    unsigned char *pSlots;       /* 0x10ACD498, BR_SLOT_STRIDE apart       */
    uint32_t *pSave5B0;          /* 0x106805B0[i]                          */
    uint32_t *pSave9C0;          /* 0x106909C0[i]                          */
    uint32_t *pSave748;          /* 0x10680748[i]                          */
    uint32_t *pSave728;          /* 0x10680728[i]                          */
    uint32_t *pSave5C8;          /* 0x106805C8[i]                          */
    uint32_t *pSave950;          /* 0x10690950[i], 0x30 apart              */

    /* --- read-only tables in .data ----------------------------------- */
    const signed char *pTblAA210;/* 0x100AA210, indexed by pSave5C8[i]     */
    const uint32_t    *pColAA5D0;/* 0x100AA5D0, 4 entries                  */

    /* --- scratch ring (0x10031190) ----------------------------------- */
    unsigned char *pScratch;     /* 0x106C29F0, BR_SCRATCH_STRIDE apart    */
    void          *pScratchWait; /* 0x106C64E0, handed to 0x10042AF0       */
    int  nScratchDepth;          /* 0x106C65DC                             */
    int  iScratch;               /* 0x106C65D8                             */

    /* --- screen ------------------------------------------------------ */
    int  screenW;                /* 0x106C0684  (default 0x100A81C0 = 640) */
    int  screenH;                /* 0x106C299C  (default 0x100A81C4 = 480) */
    int  defaultW;               /* 0x100A81C0                             */
    int  defaultH;               /* 0x100A81C4                             */
    int  scaleShift;             /* 0x106C65E4                             */

    /* --- 0x1002Cxxx glue --------------------------------------------- */
    BrPtrList *pPtrList;         /* 0x1067B548 / 0x1067B550                */
    int  bank;                   /* 0x1067D570                            */
    int  bank578;                /* 0x1067D578                            */
    int  bank57C;                /* 0x1067D57C                            */
    uint32_t *pBankHdr;          /* 0x1067D558, 3 dwords per bank          */
    unsigned char *pBankBuf;     /* 0x1067D584, 3 x 0x800 per bank         */
    int  f6909B0;                /* 0x106909B0                             */
    int  f6C2CFC;                /* 0x106C2CFC                             */
    int  f680944;                /* 0x10680944                             */
    int  f6909B4;                /* 0x106909B4                             */
    int  f0AA010;                /* 0x100AA010                             */
    int  f6805B8;                /* 0x106805B8                             */
    int  f6909B8;                /* 0x106909B8                             */
    void *pThis6806B0;           /* 0x106806B0                             */

    /* --- scene / render mode bits ------------------------------------ */
    int  f690A1C;                /* 0x10690A1C                             */
    uint32_t f6C0258;            /* 0x106C0258                             */
    uint32_t f6C0688;            /* 0x106C0688                             */
    uint32_t f6C0920;            /* 0x106C0920                             */
    uint32_t f6C3364;            /* 0x106C3364                             */
    uint32_t f6C1174;            /* 0x106C1174                             */
    int  f0AA880;                /* 0x100AA880                             */
    BrMat4 *pLightMtx;           /* 0x106C08A0                             */
    BrMat4 *pTransMtx;           /* 0x106C0860                             */

    /* --- 0x10031227 counters ----------------------------------------- */
    int f6C32CC, f6C56DC, f6C1178;
    int f6C161C, f6C1610;
    int f6C33B8, f6C06A4, f6C069C;
} BrS17State;

BrS17State *BrS17GetState(void);

/* ===================================================================== */
/* 1. camera / basis matrices                                            */
/* ===================================================================== */

/* 0x100309A0 -- guLookAtF, as reworked by Boss Game Studios.
 *
 * Ten arguments, exactly libultra's guLookAtF order:
 *      (mtx, eye.x,eye.y,eye.z, at.x,at.y,at.z, up.x,up.y,up.z)
 *
 * DIFFERENT FROM STOCK libultra. Stock builds the basis as
 *      z = norm(eye - at);  x = norm(up x z);  y = z x x
 * This build instead Gram-Schmidts the up vector:
 *      z = norm(eye - at)
 *      y = norm(up - dot(up, z) * z)
 *      x = y x z
 * Do not substitute a stock guLookAtF; the two disagree whenever `up` is
 * not already perpendicular to the view direction.
 *
 * The basis is computed in DOUBLE precision (the routine calls the
 * br_vecd.h double vector library), and only the sixteen stores are float.
 *
 * Layout written (row-major, row-vector convention, matching BrMat4Mul):
 *      m[i][0] = x[i]   m[i][1] = y[i]   m[i][2] = z[i]   m[i][3] = 0
 *      m[3][*] = -dot(eye, x), -dot(eye, y), -dot(eye, z), 1
 *
 * GOTCHA: there is NO guard on a degenerate basis. If eye == at the
 * normalise leaves the zero vector alone (br_vecd.h) and the whole 3x3 comes
 * out zero -- no error, no identity fallback. */
void BrMat4LookAt(BrMat4 *pM,
                  float xEye, float yEye, float zEye,
                  float xAt,  float yAt,  float zAt,
                  float xUp,  float yUp,  float zUp);

/* The four packed direction bytes 0x10030B50/0x10030E20 write. Two 16-byte
 * records; the direction triple sits at +8 of each, which is the N64
 * Light_t layout (col[3], pad, colc[3], pad, dir[3], pad). Only `dir` is
 * touched -- colours are left exactly as the caller had them. */
typedef struct BrLightPair {
    unsigned char f00[8];
    signed char   dir0[3];       /* +0x08  packed column 0 (the x axis) */
    unsigned char f0B[5];
    unsigned char f10[8];
    signed char   dir1[3];       /* +0x18  packed column 1 (the y axis) */
    unsigned char f1B[5];
} BrLightPair;

/* The four integers 0x10030B50 writes. See BrLightDirsAndAngles. */
typedef struct BrSkyAngles {
    int32_t s0;   /* +0x00 */
    int32_t t0;   /* +0x04 */
    int32_t s1;   /* +0x08 */
    int32_t t1;   /* +0x0C */
} BrSkyAngles;

/* 0x10030E20 -- eleven arguments.
 *
 *   BrMat4LookAt(pM, eye, at, up)  then
 *   pLights->dir0 = pack(column 0 of pM)      (x axis)
 *   pLights->dir1 = pack(column 1 of pM)      (y axis)
 *
 * `pack` is BrPackNormalByte (br_vecd.h, 0x10030DE0), so 1.0 saturates to
 * 127 and the encoding is very slightly asymmetric.
 *
 * GOTCHA: the argument taken by the caller as "light struct" is the SECOND
 * argument, but it is skipped when the ten lookAt arguments are forwarded.
 * The lookAt arguments are a0 and a2..a10, never a1. */
void BrLightDirsFromLookAt(BrMat4 *pM, BrLightPair *pLights,
                           float xEye, float yEye, float zEye,
                           float xAt,  float yAt,  float zAt,
                           float xUp,  float yUp,  float zUp);

/* 0x10030B50 -- twenty arguments. Everything BrLightDirsFromLookAt does,
 * plus two angle pairs written into pAngles.
 *
 * For a direction D and a half-revolution count N the original computes
 *
 *      s = N - (int)( atan2(dot(x,D), dot(z,D)) * N * -1/pi )
 *      t = N - (int)( asin (dot(y,D))           * N * -1/pi )
 *
 * with x/y/z the columns of the lookAt matrix and D normalised in place.
 * (int) is __ftol, i.e. truncation toward zero. Because the constant is
 * NEGATIVE this is really N + theta*N/pi, so theta = 0 lands on N, a half
 * turn on 0 or 2N -- a 2N-wide wrap-around index, almost certainly an
 * environment-map s/t.
 *
 * GOTCHA: the first pair hardcodes N = 0x100. The second takes N from
 * arguments, and takes TWO of them: `nS1 * 4` scales the atan2 pair and
 * `nT1 * 4` scales the asin pair. They are separate arguments even though
 * every other symmetry in the routine says they should be one.
 *
 * GOTCHA: the two constants used are 0xC0545F30B4E4E30A (-81.487...) and
 * 0xBFD45F30B4E4E30A (-0.31831...). They are exactly 256x apart, which is
 * why the first pair's literal 0x100 and the second pair's `n * 4` behave
 * the same way. Neither is the correctly rounded -256/pi or -1/pi.
 *
 * GOTCHA: pDirA and pDirB are normalised IN PLACE by the original -- the
 * caller's floats are read but the doubles it builds are local, so nothing
 * the caller passed is modified. Reproduced. */
void BrLightDirsAndAngles(BrMat4 *pM, BrLightPair *pLights,
                          BrSkyAngles *pAngles,
                          float xEye, float yEye, float zEye,
                          float xAt,  float yAt,  float zAt,
                          float xUp,  float yUp,  float zUp,
                          float xA, float yA, float zA,
                          float xB, float yB, float zB,
                          int nS1, int nT1);

/* 0x10030EE0 -- guRotateF: rotate `degrees` about the axis (x, y, z).
 *
 * Implemented, unusually, out of BrMat4LookAt: it builds a basis whose third
 * axis is the rotation axis, rotates about that basis' z, and maps back.
 *      out = L * Rz(degrees) * transpose(L)
 * where L = lookAt(eye = axis, at = origin, up = (y, z, x)).
 *
 * GOTCHA: the `up` handed to lookAt is a CYCLIC SHIFT of the axis, (y,z,x).
 * That is deliberate (it can never be parallel to the axis unless the axis
 * has all three components equal) and must not be "corrected".
 *
 * GOTCHA: the degenerate test is three separate x87 `fcomp` against 0.0
 * checking C3 only, so a NaN component counts as zero and yields the
 * IDENTITY rather than a NaN matrix. Reproduced with !(v<0) && !(v>0).
 *
 * GOTCHA: the rotation is applied with the ROW-vector convention, so
 *      [ c  s 0 0 ]
 *      [-s  c 0 0 ]
 * not its transpose. */
void BrMat4RotateAxis(BrMat4 *pM, float degrees, float x, float y, float z);

/* 0x100312A7 (the packet lists the tail, 0x100312BB) -- largest absolute
 * value among TWELVE consecutive floats.
 *
 * The original tracks the most-negative and the most-positive separately,
 * both seeded at 0.0, negates the former and returns the larger. The result
 * is therefore max(|v|) but never below 0.0 even for an all-negative or
 * all-positive input.
 *
 * GOTCHA: the sign test is `fcomp v, 0.0` testing C0, which is also set for
 * an unordered compare, so a NaN element takes the negative branch. Both
 * subsequent compares then fail, so a NaN is silently ignored rather than
 * poisoning the result. Reproduced. */
float BrFloat12MaxAbs(const float *pv);

/* ===================================================================== */
/* 2. RDP fill / scissor emitters                                        */
/* ===================================================================== */

/* Command byte 0xE1 in this build's stream is a FILL RECTANGLE with plain
 * 12-bit integer corners (not the 10.2 fixed point of the N64's 0xF6):
 *      w0 = 0xE1000000 | (lrx & 0xFFF) << 12 | (lry & 0xFFF)
 *      w1 =              (ulx & 0xFFF) << 12 | (uly & 0xFFF)          */
#define BR_GFX_FILLRECT 0xE1000000u

/* 0x100314E8 -- clear the whole screen to an RGB colour.
 *
 * The colour is packed RGBA5551 from the top five bits of each channel with
 * alpha forced to 1, then duplicated into both halves of the fill colour:
 *      c = (r<<8 & 0xF800) | (g<<3 & 0x7C0) | (b>>2 & 0x3E) | 1
 *      w1 = c << 16 | c
 * `b >> 2` is an ARITHMETIC shift in the original, so a negative b still
 * lands in 0x3E after the mask; reproduced with a signed shift.
 *
 * The rectangle covers (0,0) .. (W<<s - 1, H<<s - 1) with W/H the screen
 * size globals and s the 0x106C65E4 scale shift. */
void BrGfxClearScreen(int r, int g, int b);

/* 0x10031688 -- fill one rectangle in the same colour encoding.
 *
 * Arguments are (ulx, uly, w, h, r, g, b): the lower-right corner is
 * computed as ulx + w and uly + h.
 *
 * GOTCHA -- ASYMMETRIC SCALING, almost certainly an original bug, faithfully
 * reproduced: when the scale shift is non-zero all four coordinates are
 * first DOUBLED, and then the lower-right corner is shifted left by the
 * shift AGAIN while the upper-left corner is not shifted at all. With a
 * shift of 1 the rectangle therefore spans (2*ulx, 2*uly) to
 * (4*(ulx+w) - 1, 4*(uly+h) - 1). */
void BrGfxFillRect(int ulx, int uly, int w, int h, int r, int g, int b);

/* 0x10031481 -- emit one 0xDC command out of an array of 0x24-byte records.
 *
 *      if ((rec[i].f20 >> 20) & 1) return;      // gated, no command
 *      w0 = (rec[i].f00 & 0x00FFFFFF) | 0xDC000000
 *      w1 = 1
 *
 * The record is only ever read at +0x00 and +0x20, so it stays a raw stride
 * rather than a struct. */
#define BR_TEXREC_STRIDE 0x24
void BrGfxEmitTexCmd(int i, const void *pRecords);

/* ===================================================================== */
/* 3. the prop display list                                              */
/* ===================================================================== */

/* One entry of the list 0x1002FB20 walks. The stride 0x14 and every field
 * offset come from the pointer arithmetic (esi starts at pList + 0xC, i.e.
 * at +4 of the first record, and the display list is read at esi - 4). */
typedef struct BrPropItem {
    uint32_t dl;        /* +0x00  segment address, emitted as a 0x06 branch */
    unsigned char f04;  /* +0x04  bit0..1 colour index, bit2, bit3 pass,
                         *        bit7 gated on 0x100AA880                  */
    unsigned char f05;  /* +0x05  bit2 selects the two BC00xx0A blocks      */
    unsigned char f06, f07;
    float x, y, z;      /* +0x08 +0x0C +0x10, fed to BrMat4Translate        */
} BrPropItem;

typedef struct BrPropList {
    uint16_t f00;
    uint16_t count;         /* +0x02, compared UNSIGNED */
    uint32_t f04;
    BrPropItem items[1];    /* +0x08 */
} BrPropList;

/* 0x1002FB20 -- emit the whole prop pass.
 *
 * Two passes over the list: pass 0 takes the items whose f04 bit 3 is CLEAR,
 * pass 1 the ones where it is set. Items with dl == 0 are skipped in both.
 *
 * GOTCHA: the pass test is written as `((~f04 >> 3) & 1) == (pass == 0)`,
 * which is the same thing, but note it is a comparison against `pass == 0`
 * and not against the pass number -- with more than two passes it would
 * behave differently.
 *
 * GOTCHA: the count is a u16 compared with `jb`, i.e. unsigned, against a
 * 32-bit counter. */
void BrScenePropsDraw(const BrPropList *pList, const BrMat4 *pViewMtx);

/* ===================================================================== */
/* 4. the car table                                                      */
/* ===================================================================== */

/* 0x1002F130  append one car: fill in the RGB triple, hand the record to
 * two callees, strcpy a name in and bump BOTH counters.
 *
 * GOTCHA: the two counters are bumped by DIFFERENT amounts of code -- nEntB
 * (0x100B36FC) indexes the record being written and is incremented first,
 * nEntA (0x100B36F8) is incremented afterwards. They are only ever equal
 * because nothing else writes them here. */
void BrCarTableAdd(void *pOwner);

/* 0x1002F230  clear every car whose owner matches, and unhook it from the
 * 0x80-stride slot array.
 *
 * GOTCHA: the inner unhook loop runs over nEntA slots but is entered fresh
 * for each matching car, and it does NOT break on the first match. */
void BrCarTableRemove(const void *pOwner);

/* 0x1002F2A0  copy five dwords and one nSaveDwords-long run out of each car
 * record into the parallel arrays, then raise 0x106909B8. */
void BrCarStateSave(void);

/* 0x1002F320  the reverse, plus a per-car fixup of the command block at
 * +0x0E8C which only runs when 0x100AA010 is zero and 0x106909B8 is set.
 *
 * GOTCHA: BrCarStateSave writes pSave9C0 (0x106909C0) but BrCarStateRestore
 * never reads it. The round trip is deliberately not symmetric. */
void BrCarStateRestore(void);

/* ===================================================================== */
/* 5. small global glue                                                  */
/* ===================================================================== */

/* 0x1002BF40  linear search of the BrPtrList from slice1_05.h.
 * GOTCHA: a NULL needle returns 1 (present) WITHOUT looking at the list. */
int BrPtrListContains(const BrPtrList *pList, const void *pv);

/* 0x1002C210  toggle the double-buffered debug bank and clear it.
 * Three 0x800-byte sub-buffers and a three-dword header per bank. */
void BrS17BankFlip(void);

/* 0x1002C2A0  tail-jump to the 0x100751D0 method on the 0x106806B0 object. */
void BrS17Release(void);
/* 0x1002C2B0  atexit(0x1002C2C0). Returns atexit's result, which the
 * original discards. */
int  BrS17RegisterAtExit(void);

/* 0x1002C2D0  call 0x1003563A(0x10680944), saving and restoring 0x106C2CFC
 * around it -- but only when 0x106909B0 is -1.
 *
 * GOTCHA: the save is conditional on the value BEFORE the call and the
 * restore on the value AFTER it. If the callee changes 0x106909B0 to -1 the
 * restore writes an UNINITIALISED stack slot into 0x106C2CFC. Reproduced,
 * with the local seeded from the current value so the port is at least
 * deterministic -- see the DEVIATION in the .c. */
void BrS17DrawGated(void);

/* 0x1002C320  three stub calls plus BrS17DrawGated and 0x100397C0, all
 * gated on 0x106909B4 being zero. */
void BrS17DrawFrame(void);

/* 0x1002C390  set 0x100AA010 = 4, 0x106805B8 = 2, then 0x10034C66(0x1002C500). */
void BrS17SetMode4(void);

/* 0x1002C410  walk 16-byte records from 0x100A66F0 decrementing field +0
 * while field +0x0C is non-zero.
 * GOTCHA: the guard is read from the FIRST record before the loop and then
 * from each record after it is decremented, so a table whose first +0x0C is
 * zero is left completely alone. */
#define BR_TICKREC_STRIDE 0x10
void BrS17TimerTick(void *pRecords);

/* 0x1002C430  car[+0x1030] = sqrt(x*x + y*y + z*z) * 2.24, from the three
 * floats at +0x1E8/+0x1EC/+0x1F0, gated on +0x730. Then 0x10075F10(car).
 *
 * 2.24 is the m/s -> mph factor (0x1008F448 = 0x400F5C29), so +0x1030 is a
 * speed in mph. The x87 order is (x*x + z*z) + y*y with x = +0x1E8,
 * y = +0x1EC, z = +0x1F0 -- preserved, addition is not associative.
 *
 * GOTCHA: when +0x730 is zero nothing is computed and +0x1030 keeps its old
 * value, but 0x10075F10 is still called. */
void BrCarUpdateSpeedMph(void *pCar);

/* 0x1002C4A0  0x100664C0 on each of nEntA records of the 0x80 slot array. */
void BrS17SlotsRelease(void);

/* 0x10031190  hand out the next of 32 0x18-byte scratch records.
 *
 * GOTCHA: when the depth counter has already reached 0x20 the original calls
 * the wait/assert helper and then does NOT increment -- so the counter
 * sticks at 0x20 and every further call waits again.
 *
 * GOTCHA: the index advance is MSVC's signed `(i + 1) % 32`, i.e. it keeps
 * the sign, so a negative index stays negative and indexes backwards. */
void *BrScratchRingAlloc(void);
/* 0x100311E4  drain: wait once per outstanding depth, decrementing. */
void  BrScratchRingDrain(void);
/* 0x10031212  zeroes its own two argument slots and returns 0. */
int   BrScratchRingNull(int a0, int a1);

/* 0x10031227  zero eight render counters in three copy chains. */
void BrRenderCountersReset(void);

/* 0x10031282 / 0x1003128C  screenW/screenH <- 0x100A81C0/0x100A81C4
 * (640 and 480 in the shipped DLL). 0x10031282 is a bare wrapper. */
void BrScreenSizeInit(void);
void BrScreenSizeApply(void);

/* 0x10031342  empty function (push ebp / mov ebp,esp / pop ebp / ret). */
void BrTexNoOp(void);

/* 0x10031347  ceil(log2(size)) for RDP tile setup.
 *
 * Works on size - 1 and walks a binary search of masks, so
 *      size <= 1 -> 0, 2 -> 1, 3..4 -> 2, 5..8 -> 3, ... 513..1024 -> 10.
 *
 * *pOut1 is unconditionally set to 0xFFFF.
 *
 * GOTCHA: sizes above 1024 print "ERROR: unhandled texture size: %d" (with
 * size - 1, not size) and leave *pOut2 COMPLETELY UNTOUCHED -- the caller
 * gets whatever was there.
 *
 * GOTCHA: the arguments are (size, pOut1, pOut2) but pOut2 is the useful
 * one; pOut1 is the constant. Preserved. */
void BrTexSizeShift(int size, int *pOut1, int *pOut2);

#endif /* SLICE2_17_H */
