/* slice2_19.h -- Boss Rally (BRD3D.dll) decompilation, a later pass.
 *
 * Packet range 0x10033CB1 .. 0x10036C00 (work/slice2/agent19.asm).
 *
 * Clusters found in this range:
 *
 *   1. camera / matrix set-up      0x10033CB1 0x10033E83 0x10033F7E 0x1003407D
 *   2. display-list segment fixup  0x1003445A 0x10035060 0x10035089
 *   3. per-car RDP mode words      0x100350EE 0x10035452 0x10035CA0
 *   4. keyframe vertex animation   0x10035585 0x100355FB 0x10035610 0x10035625
 *                                  0x1003563A
 *   5. controller translation      0x10035CE0 0x10035FC0
 *   6. big-endian model fixup      0x10036BD0 0x10036C00
 *   7. odds and ends               0x100347BA 0x10035041 0x10035059 0x10035520
 *                                  0x1003557B 0x10035B87 0x10035BA7 0x10035BBA
 *                                  0x10035C70
 *
 * GENERAL DEVIATIONS that apply to the whole file (not repeated per function):
 *
 *   D1. Structs below are SOURCE-faithful, not ABI-faithful. Where a struct
 *       holds a host pointer the offsets after it shift on a 64-bit host.
 *       The offsets in the comments are the original's.
 *   D2. Every global the original reaches by absolute address is a module
 *       global here, named g_Br<something> with the original address in the
 *       comment. Function-pointer globals are likewise real function
 *       pointers the caller installs.
 *   D3. Where the original reads a float constant out of .rdata, the constant
 *       is a module global named for its address (g_BrK08FnnnN). This entry
 *       used to say each was DERIVED or ASSUMED and that the assumed ones
 *       were not facts -- which is true, but the packet's not containing
 *       .rdata was never a reason to guess, because the IMAGE contains it.
 *       All eleven have now been READ OUT OF BRD3D.dll and each carries its
 *       byte pattern below. Two of the eight guesses were wrong, one of them
 *       (0x1008F548) by 12.5% on every analog axis of every frame.
 *   D4. Two indexed global tables have no determinable extent in this packet
 *       (0x106C5468, 0x100C12A0/0x106C6558). Following the precedent set by
 *       BrHandleLookup in br_bits.h, they are passed in as arguments so the
 *       port has no hardcoded absolute address and no invented array size.
 *
 * SKIPPED, with reasons, so the information is not lost:
 *
 *   0x100341B3  The packet starts at 0x100341E2, i.e. 47 bytes INTO the
 *               function -- the prologue is missing. It reads [ebp-0x18] and
 *               returns [ebp-0x14], neither of which is initialised in the
 *               bytes we have. Reconstructing them would be a guess.
 *               (It is a display-list walker that rewrites G_VTX/G_SETTIMG/
 *               G_TEXTURE-class words from a 6 x 32-byte substitution table
 *               and from three 16-byte / 8-byte tables at 0x100AA8B8 and
 *               0x100AA8C8, with a jump table at 0x100343FD indexed by
 *               opcode-0xB8. Declared XSLICE below.)
 *   0x100344D7  Consists entirely of nested loops that call 0x100341B3 over
 *               an entity array (stride 0x2B68, sub-array at +0x086C, tag
 *               byte at +0x0857) that this packet does not otherwise touch.
 *               With the callee skipped there is nothing left to verify.
 *   0x10034C51  Covered by slice1_05.h's hook family (0x10034C32..0x10034CA8).
 *               Declared XSLICE below because 0x10035CE0 calls it.
 *   0x10034F37  Same problem as 0x100341B3: the packet begins mid-function.
 *               It is a plane-interleaved RLE decoder; the run-length BIAS
 *               lives in [ebp-8], which is only ever written by the missing
 *               prologue. Everything else about it is recoverable (see the
 *               note at the bottom of slice2_19.c) but the bias is not, and
 *               it changes every output length.
 *   0x10035B91  Calls the unknown 0x10042AF0 with the address of an unknown
 *               global (0x106C6540) and two literals. There is no way to
 *               establish either type, so there is nothing to port.
 */
#ifndef SLICE2_19_H
#define SLICE2_19_H

#include "br_match.h"   /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

#include <stddef.h>
#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_pool.h"
#include "br_seg.h"

/* ================================================================== */
/* Float constants the original loads from .rdata                     */
/* ================================================================== */
/* DERIVED. 0x1003407D builds a pixel->NDC matrix as
 *   [ K/w 0 0 0 ][ 0 K/h 0 0 ][ 0 0 0 0 ][ -1 -1 0 1 ]
 * which is only a screen matrix for K == 2. 0x1003596E independently uses it
 * as `K*lo - t`, a reflection about `lo`, which also needs K == 2. */
extern float g_BrK08F514;   /* 0x1008F514 == 40000000 == 2.0f -- MEASURED */

/* MEASURED. 0x10033E83 computes the guPerspective fovy as
 *   a2 * K08F518 * (a5/a4) * K08F51C.
 * This entry used to read "ASSUMED == 1.0f ... neither value is in this
 * packet". Both values were always in the IMAGE, which is the only packet
 * that counts, and the assumption was wrong: with 1.0f the field of view came
 * out around 0.75 degrees. The .c had been corrected and this header had not,
 * so the two disagreed about whether the numbers were even known. */
extern float g_BrK08F518;   /* 0x1008F518 == 3FAAAAAB == 1.33333337f (4/3)    */
extern float g_BrK08F51C;   /* 0x1008F51C == 42652EE0 == 57.2957764f (180/pi) */

/* MEASURED, and the guess was right. 0x100347BA is a saturating clamp: it
 * compares against these and assigns 2.5f / 5.0f respectively. Threshold ==
 * target is the standard clamp idiom -- and the bytes agree. */
extern float g_BrK08F520;   /* 0x1008F520 == 40200000 == 2.5f */
extern float g_BrK08F524;   /* 0x1008F524 == 40A00000 == 5.0f */

/* DERIVED == 4096.0f. 0x1003563A multiplies the normalised keyframe
 * parameter (which is (t-lo)/(hi-lo), so in [0,1]) by this, truncates to int,
 * and then uses the result as a Q12 fraction (`imul` followed by `sar 12`).
 * Only 4096 makes that a unit interval. */
extern float g_BrK08F52C;   /* 0x1008F52C == 45800000 == 4096.0f -- MEASURED */

/* MEASURED, and the guess was right -- 1/128, not the equally plausible
 * 1/127. Scales an interpolated int8 into the animated vertex's +0x14..+0x1C
 * slot (a normal, by position in a 0x20-byte vertex). */
extern float g_BrK08F530;   /* 0x1008F530 == 3C000000 == 0.0078125f (1/128) */

/* MEASURED, and the guess was right. 0x1003563A scales the ping-pong wrap
 * span by it exactly once. */
extern float g_BrK08F534;   /* 0x1008F534 == 3F000000 == 0.5f */

/* MEASURED, and the guess was WRONG BY 12.5%. This entry used to say
 * "ASSUMED == 1/80 ... but it is still a reading". The image says 1/70, and
 * this constant scales every analog axis on every frame.
 *
 * The reasoning that produced 1/80 is worth keeping, because it is a sound
 * inference from a true observation. The observation -- 0x10035CE0
 * synthesises +/-0x50 (+/-80) for digital left/right -- is correct. The
 * inference, that the constant must therefore map 80 onto exactly 1.0, is
 * not: 80 * (1/70) == 1.14285719f, and the +/-1 clamp at 0x10035F66 /
 * 0x10035F7D cuts it straight back to +/-1. The digital arm SATURATES by
 * design, so it lands on +/-1 under either constant and is evidence for
 * neither. Verified by enumeration in test_slice2_19.c.
 *
 * The general shape: a value that a later clamp makes insensitive to the
 * constant cannot pin the constant. Before leaning on a piece of evidence,
 * check that it could have come out differently. */
extern float g_BrK08F548;   /* 0x1008F548 == 3C6A0EA1 == 0.0142857144f (1/70) */

/* MEASURED, and the guess was right. Upper / lower clamp thresholds in
 * 0x10035CE0, which assigns +1.0f / -1.0f on the far side of each. */
extern float g_BrK08F54C;   /* 0x1008F54C == 3F800000 ==  1.0f */
extern float g_BrK08F550;   /* 0x1008F550 == BF800000 == -1.0f */

/* ================================================================== */
/* 1. Camera / matrix set-up                                          */
/* ================================================================== */

/* The camera basis, pinned by the guLookAtF call in 0x10033E83 which passes
 * (+0x30) as the EYE, (+0x30 componentwise + +0x00) as the AT and (+0x20) as
 * the UP. 0x10033CB1 then uses +0x10 and +0x20 as the two frustum extent
 * axes, so +0x10 is the remaining (right) axis. The 4-byte gaps are not read
 * by anything in this packet. */
typedef struct BrCamBasis {
    BrVec3 fwd;      /* +0x00  direction; eye+fwd is the lookat target */
    float  pad0C;
    BrVec3 right;    /* +0x10  scaled by `a` below                     */
    float  pad1C;
    BrVec3 up;       /* +0x20  scaled by `b` below, and the lookat up  */
    float  pad2C;
    BrVec3 eye;      /* +0x30                                          */
} BrCamBasis;

/* Camera globals. All four corner vectors are plain BrVec3 -- established by
 * 0x10035C70 (a 3-dword copy) writing two of them and by br_vec.h's routines
 * writing the rest. */
extern BrVec3 g_BrCamEye;        /* 0x106C3310  copy of pCam->eye        */
extern BrVec3 g_BrCamCentre;     /* 0x106C2990  eye + fwd*dist           */
extern BrVec3 g_BrCamExtentR;    /* 0x106C56F8  right * a                */
extern BrVec3 g_BrCamExtentU;    /* 0x106C0690  up    * b                */
extern BrVec3 g_BrCamCentreCopy; /* 0x106C334C  copy of g_BrCamCentre    */
extern BrVec3 g_BrCamCorner0;    /* 0x106C331C  centre + R + U           */
extern BrVec3 g_BrCamCorner1;    /* 0x106C3328  centre - R + U           */
extern BrVec3 g_BrCamCorner2;    /* 0x106C3334  centre - R - U           */
extern BrVec3 g_BrCamCorner3;    /* 0x106C3340  centre + R - U           */
extern float  g_BrCamDist;       /* 0x106C0210  = a3 of BrCamFrustumBuild */
extern float  g_BrCamFovIn;      /* 0x106C53C0  = a2 of BrCamFrustumBuild */

/* 0x100AA8B4 -- ALIAS RESOLVED. slice2_11.h calls the same address
 * g_brMode0AA8B4 ("== 1 selects -11.0f over -19.8f"); this packet calls it
 * g_BrCamMode ("== 2 halves the U extent"). One original dword, two host
 * objects -- link-clean and wrong on the first write. The storage now lives
 * once, in port/src/br_data.c, where the image's initial value (1) is also
 * recorded; g_BrCamMode stays as a spelling of it so no call site changes. */
extern int g_brMode0AA8B4;
#define g_BrCamMode g_brMode0AA8B4

extern BrMat4   g_BrViewMat;     /* 0x106C58C0  guLookAtF output          */
extern BrMat4   g_BrProjMat;     /* 0x106C0218  guPerspective output      */
extern BrMat4   g_BrProjMatFixed;/* 0x106C08A0  guPerspective output (fixed) */
extern BrMat4   g_BrCurMat;      /* 0x106C29A8  view*proj, or the screen matrix */
extern uint16_t g_BrPerspNorm;   /* 0x106C067C  guPerspective's perspNorm */
extern float    g_BrCamFar;      /* 0x106C5AB4                            */
extern float    g_BrCamNear;     /* 0x106C3360  hardcoded to 0.8f         */
extern void    *g_BrMtxSlot;     /* 0x106C32D0 (glide 0x106EA360) the 64-byte
                                  *             pool slot the matrix was copied
                                  *             into -- i.e. a pointer to the
                                  *             current frame's projection
                                  *             matrix. G_MTX 0x01030040 payload
                                  *             re-emitted by render fns such as
                                  *             the glide frontier 0x10011D20. */
extern uint32_t *g_BrGfxPtr;     /* 0x106C0680  display-list write cursor */
extern BrPool   *g_BrPool;       /* the original's BrPoolAlloc globals    */

/* 0x10033CB1  Build the four frustum corners and the frustum centre.
 *
 *   a = Br_10002240(a2) * a3
 *   b = a * a5 / a4;   if (g_BrCamMode == 2)  b /= 2.0f
 *
 * a2..a5 keep the original's positional names because only their ROLE is
 * established, not their meaning: a2 feeds the unknown 0x10002240 (almost
 * certainly a tangent, given how the result is used), a3 is a distance, and
 * a4/a5 form the aspect ratio -- note the division is a5/a4, i.e. the
 * SECOND over the FIRST, which is the reciprocal of the aspect ratio that
 * 0x10033E83 hands to guPerspective from the same two values.
 *
 * GOTCHA: the four corners are then pulled 75% of the way from the EYE
 * (BrVec3Lerp(c, c, eye, 0.75f) == (c - eye)*0.75 + eye), so they are not the
 * far-plane corners even though they are built at distance a3. */
void BrCamFrustumBuild(const BrCamBasis *pCam, float a2, float a3,
                       float a4, float a5);

/* 0x10033E83  guLookAtF + guPerspective + multiply + copy to a pool slot.
 *
 * fovy  = a2 * g_BrK08F518 * (a5/a4) * g_BrK08F51C
 * aspect= a4 / a5
 * near  = 0.8f (hardcoded into 0x106C3360 immediately before the call)
 * far   = a3
 * scale = 1.0f
 *
 * IMPORTANT FOR INTEGRATION -- br_mat.h's BrMat4Perspective (0x10030930)
 * is declared with SIX parameters and its own comment says the order is
 * unverified. This packet contains two call sites that settle it: this one
 * and 0x10033F7E, which passes the literals (45.0f, 1.3333334f, 10.0f,
 * 2000.0f, 1.0f). Both push SEVEN arguments (`add esp,0x1c`), in stock
 * libultra guPerspectiveF order:
 *     (mf, perspNorm, fovy, aspect, near, far, scale)
 * br_mat.h is missing the trailing `scale`. Declared here as
 * BrMat4Perspective7 so the two declarations cannot collide. */
void BrCamMatrixSetup(const BrCamBasis *pCam, float a2, float a3,
                      float a4, float a5);

/* 0x10033F7E  The fixed-camera variant: eye (512,384,1000) looking at
 * (512,384,0) with up (0,1,0), 45 degree fovy, 4:3, near 10, far 2000.
 *
 * GOTCHA: BOTH PARAMETERS ARE DEAD. The original opens with
 * `mov eax,[ebp+8] / mov [ebp+8],eax` twice and never reads them again. They
 * are kept so call sites transcribe unchanged. */
void BrCamMatrixSetupFixed(float a1, float a2);

/* 0x1003407D  Replace g_BrCurMat with the pixel->NDC screen matrix
 *   [ 2/w  0  0 0 ][ 0 2/h 0 0 ][ 0 0 0 0 ][ -1 -1 0 1 ]
 * and emit the same two display-list commands as 0x10033F7E.
 *
 * GOTCHA: row 2 is left entirely zero -- z is discarded, not passed through.
 * That is an explicit pair of zero stores in the original, not an omission.
 *
 * GOTCHA: the original does the same dead `mov [ebp+8],eax` self-assignment
 * as 0x10033F7E, but here the parameters ARE used, as divisors. */
void BrCamMatrixSetupOrtho(float w, float h);

/* ================================================================== */
/* 2. Display-list segment fixup                                      */
/* ================================================================== */

/* 0x10035060  Rebase one 32-bit address if it falls in [lo, hi).
 *
 * GOTCHA: the comparisons are UNSIGNED (jb / jae) and the range is
 * half-open at the top. Out-of-range values are left untouched, not zeroed
 * -- which is the opposite of br_seg.h's BrSegFixup, whose below-base case
 * writes 0. Do not merge the two. */
void BrDlRebaseWord(uint32_t *pWord, uint32_t lo, uint32_t hi, uint32_t base);

/* 0x10035089  Walk an F3D display list and rebase the address word of every
 * command whose opcode is G_VTX (0x04) or G_SETTIMG (0xFD). Stops at
 * G_ENDDL (0xB8).
 *
 * GOTCHA: a NULL list returns immediately, but a list with no G_ENDDL runs
 * off the end forever -- there is no length limit and no other terminator.
 * GOTCHA: G_DL (0x06) is NOT followed, so nested lists are missed. */
void BrDlRebase(uint32_t *pDl, uint32_t lo, uint32_t hi, uint32_t base);

/* The owner record 0x1003445A works on. Only three fields are touched. */
typedef struct BrDlOwner {
    unsigned char pad00[0x44];
    uint32_t     *pDl;        /* +0x44 */
    unsigned char pad48[4];
    uint16_t      flags;      /* +0x4C  bit 2 = suppress, bit 3 = done */
} BrDlOwner;

extern int32_t     g_Br0B380C;      /* 0x100B380C  a mode selector        */
extern int32_t     g_Br6C666C;      /* 0x106C666C                         */
extern const void *g_BrDlTableA;    /* 0x100AA8D8  arg 2 of 0x100341B3    */

/* 0x1003445A
 *   g_Br6C666C = 0
 *   want = !(g_Br0B380C == 2 || g_Br0B380C == 8)
 *   if (!(flags & 4))  g_Br6C666C = want
 *   if (Br_100341B3(pDl, g_BrDlTableA))  flags |= 8
 *
 * GOTCHA: g_Br6C666C is cleared unconditionally and only then conditionally
 * re-set, so a caller with flags bit 2 set leaves it at 0 rather than at its
 * previous value. */
void BrDlOwnerFixup(BrDlOwner *pOwner);

/* ================================================================== */
/* 3. Per-car RDP mode words                                          */
/* ================================================================== */

/* A 0x24-byte slot record. Only +0x04 and +0x20 are read here. */
typedef struct BrGfxSlot {
    uint32_t  f00;
    uint16_t *pWords;          /* +0x04 */
    unsigned char pad08[0x18];
    uint32_t  f20;             /* +0x20  bits[27:24] must be 1 */
} BrGfxSlot;

/* The per-car graphics record. Offsets are the original's; see D1. */
typedef struct BrCarGfx {
    uint32_t   f00;
    uint32_t   aDl[30];                        /* +0x04 .. +0x7B */
    int32_t    cDl;                            /* +0x7C          */
    uint32_t   f80;                            /* +0x80          */
    uint32_t   aDlExtra[4];                    /* +0x84 +0x88 +0x8C +0x90 */
    unsigned char pad94[0x8014 - 0x94];
    BrGfxSlot *pSlots;                         /* +0x8014        */
    unsigned char pad8018[0x8110 - 0x8018];
    unsigned char aSlotIdx[12];                /* +0x8110 .. +0x811B */
} BrCarGfx;

extern int32_t g_BrCarCount;    /* 0x100B36FC */
/* 0x100AC300 / 0x106C661C / 0x106C6624 -- ALIASES RESOLVED. slice2_20.c
 * calls these g_i0AC300, g_i6C661C and g_i6C6624. Storage in
 * port/src/br_data.c, which is also where the recovered initial value of
 * 0x100AC300 lives: it is 1, not 0. That matters here -- "non-zero suppresses
 * part 2" means the shipped build suppresses it and this port did not. */
extern int g_i0AC300;
extern int g_i6C661C;
extern int g_i6C6624;
#define g_Br0AC300 g_i0AC300
#define g_Br6C661C g_i6C661C
#define g_Br6C6624 g_i6C6624
extern void  (*g_BrGfxSubmit)(uint32_t dl);   /* 0x118AA0C0 */
/* 0x118AA0C4 -- ALIAS FOUND, NOT RESOLVED. slice2_20.c declares the same
 * address as `void (*g_pfn18AA0C4)(void *pv)`. The two disagree about the
 * ARGUMENT: this packet passes a 32-bit display-list address (BrLd32), that
 * one passes a host pointer. Both are .bss NULL at boot, so nothing has
 * drifted yet -- but whichever module installs the hook, the other will not
 * see it. Merging needs the argument adjudicated (the original takes one
 * 32-bit DL address, which is the shape here), not a cast, so it is left
 * declared twice and flagged rather than silently unified. */
extern void  (*g_BrGfxSubmitB)(uint32_t p);   /* 0x118AA0C4 */

/* 0x100350EE  Stamp an RGBA5551 colour into the first two halfwords of
 * twelve word blocks, submit the car's display lists, then rewrite ten RDP
 * mode halfwords in ONE further block and submit up to four extra lists.
 *
 * r/g/b are 5-bit; the two halfwords are packed differently:
 *   [0] = (r<<11) | (g<<6) | (b<<1) | alpha        -- full 5-bit fields
 *   [1] = ((r&0x1E)<<10) | ((g&0x1E)<<5) | (b&0x1E) | alpha
 * and BOTH are stored byte-swapped (big-endian, for the RDP).
 *
 * GOTCHA (looks like an original bug, preserved): the alpha bit is taken
 * from `pWords[i]` -- the loop index -- but the results are always written to
 * `pWords[0]` and `pWords[1]`. Only i == 0 and i == 1 read what they write.
 *
 * GOTCHA: the four "extra list" blocks are NOT symmetric. Blocks 1 and 3
 * copy +0x1E into +0x1A and +0x14 into +0x10; blocks 2 and 4 copy +0x1C into
 * +0x1A and +0x12 into +0x10. That asymmetry is in the original.
 *
 * GOTCHA: the whole second half is skipped when g_BrCarCount == 0 or when
 * the slot named by aSlotIdx[11] has a null pWords -- but the FIRST loop and
 * the display-list submissions have already run by then, except for the very
 * first guard which returns before anything happens. */
void BrCarGfxSetColour(BrCarGfx *pCar, int r, int g, int b);

/* The object 0x10035CA0 writes into (thiscall in the original). */
typedef struct BrRgbSink {
    unsigned char pad0000[0x29AC];
    unsigned char r, g, b;     /* +0x29AC +0x29AD +0x29AE */
} BrRgbSink;

/* 0x10035CA0  __thiscall, `ret 0xC`. Stores the LOW BYTE of each argument. */
void BrRgbSinkSet(BrRgbSink *pSink, int r, int g, int b);

/* 0x10035452  Read the RGBA5551 halfword back out of the slot named by
 * aSlotIdx[2] and hand the expanded 8-bit RGB to BrRgbSinkSet.
 *
 * Expansion is the usual 5->8 bit replicate: (c>>8)&0xF8 | (c>>13)&7 etc.
 *
 * GOTCHA: it reads the halfword NATIVELY, while 0x100350EE wrote it
 * BYTE-SWAPPED. Either one of the two is wrong in the original or the block
 * is rewritten in between; nothing in this packet resolves it, so both are
 * transcribed exactly as found. */
void BrCarGfxReadColour(BrRgbSink *pSink, const BrCarGfx *pCar);

/* ================================================================== */
/* 4. Keyframe vertex animation                                       */
/* ================================================================== */

/* An animated vertex. 0x20 bytes; +0x0C and +0x10 are never written here. */
typedef struct BrAnimVtx {
    float x, y, z;         /* +0x00 +0x04 +0x08 */
    float f0C, f10;        /* +0x0C +0x10       */
    float nx, ny, nz;      /* +0x14 +0x18 +0x1C */
} BrAnimVtx;

/* One keyframe: a time, then cVerts*3 int16 positions followed by cVerts*3
 * int8 values, both starting at +0x04. */
typedef struct BrAnimKey {
    float t;               /* +0x00 */
    /* +0x04: int16_t pos[cVerts*3]; int8_t  nrm[cVerts*3]; */
} BrAnimKey;

/* One animated mesh. flags bit 0 = loop, bit 1 = ping-pong, bit 2 = playing
 * in reverse (set and cleared by BrAnimUpdate itself). */
typedef struct BrAnimTrack {
    uint32_t    cVerts;    /* +0x00 */
    BrAnimVtx  *pOut;      /* +0x04 */
    uint32_t    f08;       /* +0x08 */
    int32_t     cKeys;     /* +0x0C */
    uint16_t    flags;     /* +0x10 */
    uint16_t    iKey;      /* +0x12 */
    float       tLo;       /* +0x14 */
    float       tHi;       /* +0x18 */
    float       t;         /* +0x1C */
    BrAnimKey  *aKeys[1];  /* +0x20, cKeys of them */
} BrAnimTrack;

typedef struct BrAnimList {
    int32_t      n;
    BrAnimTrack *a[1];
} BrAnimList;

typedef struct BrAnimSet {
    uint32_t     f00;
    BrAnimList  *pList;    /* +0x04 */
} BrAnimSet;

/* 0x106C2CFC -- ALIAS RESOLVED. slice2_20.c calls this g_f6C2CFC. Storage in
 * port/src/br_data.c; .bss in the original, so it really does start at 0. */
extern float g_f6C2CFC;
#define g_BrAnimDt g_f6C2CFC   /* 0x106C2CFC  seconds advanced per call */

/* 0x10035585  For every track in the set: flags = (flags | orBits) & ~andArg.
 *
 * GOTCHA: the THIRD argument is complemented before use (`not eax`), and the
 * complement is 32-bit while the AND that consumes it is 16-bit. So passing 0
 * clears nothing (~0 == 0xFFFF after truncation), which is exactly what
 * BrAnimSetPingPong relies on. Read the parameter as "bits to CLEAR". */
void BrAnimFlagsApply(BrAnimSet *pSet, uint16_t orBits, uint32_t clearBits);

void BrAnimSetOnce(BrAnimSet *pSet);      /* 0x100355FB  (p, 0, 3) play once */
void BrAnimSetLoop(BrAnimSet *pSet);      /* 0x10035610  (p, 1, 2) loop      */
void BrAnimSetPingPong(BrAnimSet *pSet);  /* 0x10035625  (p, 3, 0) loop+pong */

/* 0x1003563A  Advance every track by g_BrAnimDt and rewrite its vertex block
 * by interpolating between the two bracketing keyframes.
 *
 * Interpolation is FIXED POINT: frac = (int)(u * 4096) with u in [0,1], then
 * lo + (((hi - lo) * frac) >> 12), truncated back to int16 (positions) or
 * int8 (normals) BEFORE the conversion to float. That truncation is a
 * `movsx ax`/`movsx al` in the original and does wrap.
 *
 * GOTCHA -- DIVISION BY ZERO, three ways, all in the original:
 *   * forward and t < tLo          -> both brackets become aKeys[0]
 *   * forward, t >= tHi, not looping -> both become aKeys[cKeys-1]
 *   In both cases (hi->t - lo->t) is 0 and the parameter is computed as
 *   (0 - key->t) / 0. Preserved.
 *
 * GOTCHA: the bracket search advances while `key->t <= t` and then indexes
 * aKeys[k] WITHOUT re-testing k against cKeys, so a track whose last key
 * time is <= t reads one element past the array. Preserved.
 *
 * GOTCHA: in ping-pong mode, when the wrapped time is still past the scaled
 * limit, control falls into the PLAIN loop's wrap code (a shared tail in the
 * original) and the reverse bit is NOT set -- it just subtracts (tHi - tLo)
 * once more and restarts forward. That is a real cross-branch fallthrough,
 * not a transcription slip. */
void BrAnimUpdate(BrAnimSet *pSet);

/* ================================================================== */
/* 5. Controller translation                                          */
/* ================================================================== */

/* The raw pad record at +0x158. Layout matches libultra's OSContPad. */
typedef struct BrPadRaw {
    uint8_t b0;        /* +0x00  button bits  0..7 */
    uint8_t b1;        /* +0x01  button bits  8..15 */
    int8_t  stickX;    /* +0x02 */
    int8_t  stickY;    /* +0x03 */
    uint8_t status;    /* +0x04 */
} BrPadRaw;

typedef struct BrPad {
    uint32_t  buttons;              /* +0x00  translated bitfield */
    unsigned char pad04[0x14];
    float     axisX;                /* +0x18 */
    float     axisY;                /* +0x1C */
    float     axisSteer;            /* +0x20 */
    int8_t    steer;                /* +0x24 */
    unsigned char pad25[3];
    int32_t   f28;                  /* +0x28 */
    int32_t   f2C, f30;             /* +0x2C +0x30  enables */
    int32_t   f34, f38;             /* +0x34 +0x38  counters */
    int32_t   f3C, f40;             /* +0x3C +0x40  limits   */
    int32_t   f44;                  /* +0x44 */
    unsigned char pad48[0x158 - 0x48];
    BrPadRaw *pRaw;                 /* +0x158 */
} BrPad;

/* Translated button bits, read straight off the fourteen tests in the
 * original. The source column is the N64 CONT_* mask. */
enum {
    BR_PAD_DRIGHT = 0x00000001,   /* 0x0100 */
    BR_PAD_DDOWN  = 0x00000002,   /* 0x0400 */
    BR_PAD_DLEFT  = 0x00000004,   /* 0x0200 */
    BR_PAD_DUP    = 0x00000008,   /* 0x0800 */
    BR_PAD_A      = 0x00000010,   /* 0x8000 */
    BR_PAD_B      = 0x00000020,   /* 0x4000 */
    BR_PAD_CUP    = 0x00000100,   /* 0x0008 */
    BR_PAD_CRIGHT = 0x00000200,   /* 0x0001 */
    BR_PAD_CDOWN  = 0x00000400,   /* 0x0004 */
    BR_PAD_CLEFT  = 0x00000800,   /* 0x0002 */
    BR_PAD_L      = 0x00001000,   /* 0x0020 */
    BR_PAD_R      = 0x00002000,   /* 0x0010 */
    BR_PAD_START  = 0x00004000,   /* 0x1000 */
    BR_PAD_Z      = 0x00008000,   /* 0x2000 */
    /* Derived bits, only set when the hook check below passes. */
    BR_PAD_A_D    = 0x00010000,   /* A held                      */
    BR_PAD_A_BACK = 0x00020000,   /* A held and stickY < -64     */
    BR_PAD_B_ALT  = 0x00040000,   /* B held, A not held          */
    BR_PAD_B_A    = 0x00080000,   /* B held, A held              */
    BR_PAD_R_ALT  = 0x00100000,   /* R  */
    BR_PAD_L_ALT  = 0x00200000,   /* L  */
    BR_PAD_CUP2   = 0x01000000,
    BR_PAD_CDOWN2 = 0x04000000,
    BR_PAD_CLEFT2 = 0x08000000
};

extern const unsigned char *g_BrPadModeBytes; /* 0x10B4E1D4 (deref'd once)  */
extern int32_t g_Br6909B4;                    /* 0x106909B4  gates the ramp */
extern const void *g_BrPadHookFn;             /* the literal 0x1002C500     */

/* 0x10035CE0  __thiscall. Translate the raw pad into pPad->buttons, derive
 * the digital steering value, run two two-step ramps, and produce three
 * clamped float axes.
 *
 * GOTCHA: when pRaw->status != 0 the routine ZEROES the raw record's
 * buttons, stickX and stickY before reading them, so an errored pad reads as
 * fully neutral, and pPad->f28 becomes (status == 8).
 *
 * GOTCHA: digital left and right BOTH held cancel to 0, as does neither.
 * Left alone gives -80 (0xB0), right alone +80 (0x50).
 *
 * GOTCHA: `if (f2C == 0 && f30 == 0)` loads f44 into a register and then
 * discards it -- a dead load in the original, reproduced as a comment only.
 *
 * GOTCHA: the ramp compares `cur < limit` and then adds 2, so the counter can
 * finish one past the limit. It only advances while g_Br6909B4 == 0.
 *
 * GOTCHA: the second byte of the two probed at g_BrPadModeBytes is +7, not
 * +2 -- they are not adjacent. When either has bit 7 set, steering comes
 * straight from pRaw->stickX instead of from the D-pad. */
void BR_THISCALL1 BrPadTranslate(BrPad *pPad);

/* 0x10035FC0  __thiscall. Split `a` by `b`:
 *     a' = a & ~b        (bits present in a but not in b)
 *     b' = a &  b        (bits present in both)
 *
 * NOT the same routine as br_bits.h's BrBitLatchTake (0x10035FA0): that one
 * takes the mask as an argument and ORs into the second field; this one takes
 * the mask FROM the second field and OVERWRITES it. Both fields are rewritten
 * from the pre-call values, so it is a single atomic edge split. */
typedef struct BrBitPair { uint32_t a, b; } BrBitPair;
void BR_THISCALL1 BrBitEdgeSplit(BrBitPair *pPair);

/* ================================================================== */
/* 6. Big-endian model fixup                                          */
/* ================================================================== */

/* The 32-bit slots that hold N64 addresses inside a loaded model image
 * cannot hold a host pointer on a 64-bit build, so the port keeps them
 * 32-bit and goes through these two hooks.
 *
 *   g_BrModelFixup   is 0x1002B970 (br_seg.h's BrSegFixup with the map
 *                    supplied from g_BrSegMap).
 *   g_BrModelDeref   has no counterpart in the original -- there the slot IS
 *                    the pointer after the fixup. DEVIATION, see D1. */
extern void  (*g_BrModelFixup)(uint32_t *pSlot);
extern void  *(*g_BrModelDeref)(uint32_t slot);
extern BrSegMap *g_BrSegMap;

/* 0x10036C00  Convert a freshly loaded model image from big-endian N64 form
 * to host form, in place, and resolve every embedded address.
 *
 * The layout it walks, entirely determined by which bytes get reversed:
 *
 *   header  +0x00 u16      +0x02 u16 nRec     +0x04 ptr -> block
 *           +0x08 aRec[nRec], stride 0x14:
 *                  +0x00 ptr  +0x04 u16  +0x06 u16
 *                  +0x08 u32  +0x0C u32  +0x10 u32
 *   block   +0x00 u32 n     +0x04 ptr a[n] -> item
 *   item    +0x00 u32 m     +0x04 ptr  +0x08 ptr  +0x0C u32 k
 *           +0x10 u16  +0x12 u16  +0x14 u32  +0x18 u32  +0x1C u32
 *           +0x20 ptr b[k] -> leaf
 *   leaf    +0x00 u32      +0x04 u16 * (3 * item->m)
 *
 * GOTCHA: the leaf's halfword count comes from ITEM->m (times 3), not from
 * anything in the leaf itself, even though the leaf's own first dword is
 * byte-reversed as if it were a count.
 *
 * GOTCHA: if header +0x04 is zero the whole first half is skipped -- and the
 * check is made on the still-big-endian bytes, before the reversal, which
 * only works because zero is a palindrome.
 *
 * GOTCHA: the two halves are independent. The record loop at the end runs
 * even when the block pointer was null. */
void BrModelSwap(void *pImage);

/* 0x10036BD0  Load, install the segment bases, then byte-swap.
 *
 * GOTCHA: THE ARGUMENTS ARE PASSED TO 0x100088B0 IN REVERSE. The original
 * does `push arg1 / push arg2` so arg2 becomes the callee's first stack
 * argument. Preserved.
 *
 * GOTCHA: BrSegSetBases is called with n64Base == 0 and hostBase == the
 * loaded image, so every embedded address is rebased by the image address
 * with nothing excluded. */
void *BrModelLoad(void *pMgr, void *a1, void *a2);

/* ================================================================== */
/* 7. Odds and ends                                                   */
/* ================================================================== */

/* 0x100347BA  aTable[i] += amt, with a saturating clamp at both ends of the
 * pipeline.
 *
 * GOTCHA: the clamp is ASYMMETRIC in two ways. `amt` is clamped only from
 * ABOVE (amt > K520 -> 2.5f), never from below, so a large negative
 * contribution passes straight through; and the accumulator is likewise
 * clamped only from above (> K524 -> 5.0f) and can run arbitrarily negative.
 *
 * DEVIATION (D4): the table is an argument. The original hardcodes
 * 0x106C5468 and does not bounds-check `i`. */
void BrAccumAddClamp(float *aTable, int i, float amt);

/* 0x10035041  f04 = 0; f08 = v. The struct is not otherwise identified. */
typedef struct BrPairSlot { uint32_t f00, f04, f08; } BrPairSlot;
void BrPairSlotReset(BrPairSlot *p, uint32_t v);

/* 0x10035059, 0x1003557B, 0x10035B87  Three separate constant-returning
 * stubs, kept apart because they are three distinct call targets. */
int BrRet0_10035059(void);
int BrRet1_1003557B(void);
int BrRet1_10035B87(void);

/* 0x10035520  Install a car.
 *
 * GOTCHA: THE FLAG INVERTS THE MEANING. When `flag` is zero the car really
 * is loaded (0x10037740); when it is non-zero the routine only logs the
 * string "LoadCar()" and loads NOTHING -- but it still runs 0x1003551B and
 * still records pArg in the pointer table. Read `flag` as "already loaded".
 *
 * DEVIATION (D4): both indexed tables are arguments. In the original they
 * are 0x100C12A0 (stride 0x15F88) and 0x106C6558 (stride 4), and `i` is not
 * bounds-checked. */
void BrCarSlotLoad(unsigned char *aCars, void **aCarPtr, int i,
                   void *pArg, int flag);

extern void *g_BrLogArg;    /* 0x106C2CF0 */

/* 0x10035BA7  Pass g_BrLogArg to the logger.
 * GOTCHA: it takes a parameter and never reads it; 0x10035BBA calls it with
 * a literal 0. Both are preserved so call sites transcribe unchanged. */
void BrLogEmit(void *ignored);

/* 0x10035BBA  g_BrLogArg = p, then BrLogEmit(NULL). */
void BrLogSet(void *p);

/* 0x10035C70  Copy three consecutive floats, DESTINATION FIRST.
 *
 * Note the order relative to br_mat.h's BrMat4Copy (0x100307A0), which takes
 * the SOURCE first. These two sit six hundred bytes apart in the same binary
 * and disagree. Verified from the disassembly: here [ebp+8] is written and
 * [ebp+0xC] is read. */
void BrVec3Copy(BrVec3 *pDst, const BrVec3 *pSrc);

/* ================================================================== */
/* Cross-slice declarations                                           */
/* ================================================================== */

/* XSLICE 0x10002240 */
extern float BrSub10002240(float x);

/* XSLICE 0x100309A0 -- guLookAtF: (mf, eye, at, up), ten arguments. */
extern void BrMat4LookAt(BrMat4 *pM,
                         float ex, float ey, float ez,
                         float ax, float ay, float az,
                         float ux, float uy, float uz);

/* XSLICE 0x10030930 -- guPerspectiveF. See the note on BrCamMatrixSetup:
 * this is br_mat.h's BrMat4Perspective plus the `scale` argument it omits. */
extern int BrMat4Perspective7(BrMat4 *pM, uint16_t *pPerspNorm,
                              float fovyDegrees, float aspect,
                              float n, float f, float scale);

/* XSLICE 0x100306C0 -- slice1_05.h's BrMat4Mul, destination LAST. */
extern void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

/* XSLICE 0x100341B3  See the skip note at the top of this header. */
extern int BrSub100341B3(uint32_t *pDl, const void *pTable);

/* XSLICE 0x10034C51  slice1_05.h's hook family: returns 1 when the global at
 * 0x106C0964 equals pfn. */
extern int BrHookIsCurrent(const void *pfn);

/* XSLICE 0x10008CF0  the logger 0x10035520 hands "LoadCar()" to. */
extern void BrLogPrint(const void *p);

/* XSLICE 0x10037740, 0x1003551B  the two halves of a car load. */
extern void BrSub10037740(void *pCar, void *pArg);
extern void BrSub1003551B(void *pCar);

/* XSLICE 0x100088B0  __thiscall on the object at 0x10A99780. */
extern void *BrSub100088B0(void *pThis, void *a, void *b);

/* XSLICE 0x1002BD50  slice1_05.h's BrVtxCacheResolve, whose cache is a
 * global in the original. Renamed so the declarations cannot collide. */
extern void BrModelVtxResolve(uint32_t *pSlot, int count);

/* XSLICE 0x1002BF80, 0x10074DC0 -- not identified. */
extern void BrSub1002BF80(uint32_t v);
extern void BrSub10074DC0(int n);

#endif /* SLICE2_19_H */
