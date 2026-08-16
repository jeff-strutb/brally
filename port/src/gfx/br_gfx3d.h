/* br_gfx3d.h -- the display-list machine's renderer seam, portable side.
 *
 * WHY THIS EXISTS AND WHY IT LOOKS LIKE THIS
 * ----------------------------------------------------------------------
 * br_dl.h ends at a BrDlSink of six function pointers, and says the state
 * model behind them is CLOSED: the combiner (0x1001E7A0) is a chain of exact
 * equality tests -- ten configurations plus a default -- and the render mode
 * (0x10021270) is the same shape.  That is not a stylistic observation, it
 * is what makes a FIXED ARRAY of pipeline states the right answer on Metal
 * instead of a runtime shader cache: every state the retail data can ask for
 * is known before the first frame.
 *
 * So the backend is:
 *
 *     aPipeline[BR_DL_CC__COUNT][BR_GFX3D_BLEND__COUNT]   -- 10 x 3, static
 *     aDepth   [BR_GFX3D_Z__COUNT][2]                     --  4 x 2, static
 *
 * built once in BrGfx3dInit, and a vertex batch that flushes whenever the
 * (combiner, blend, depth, texture, constant colour) tuple changes.
 *
 * WHAT THE TEN COMBINER ROWS ACTUALLY DO
 * ----------------------------------------------------------------------
 * br_dl.h names the rows but records no arguments.  Read off 0x1001E7A0,
 * every row ends in grColorCombine(func, factor, local, other, invert)
 * (glide2x!_grColorCombine@20 via the thunk at 0x10072990) and three rows
 * additionally call grConstantColorValue (0x10072996):
 *
 *   row  (w0,w1)               func factor local other  const
 *   ---  --------------------  ---- ------ ----- -----  --------------
 *   dflt (anything else)         3     1     0     1     -      tex * shade
 *   1    FCFFFFFF FFFCF87C       3     8     1     1     -      tex
 *   2    FCFFFFFF FFFE793C       1     0     0     2     -      shade
 *   3    FC567EAC FFFFF3F9       3     8     1     2    0x000000FF
 *   4    FCFF97FF FF2DFEFF       3     8     1     2     -      (last const)
 *   5    FCFFFFFF FFFDF2F9       3     8     1     2     -      (last const)
 *   6    FCFFFFFF FFFF73B9       3     8     1     2    0xFFFFFFFF
 *   7    FC127E08 F3FFF2F8       7     4     1     1     -      envmap blend
 *   8    FC317E02 5F/51FEF3FA    3     1     0     1     -      tex * shade + decal
 *   9    FC127FFF FFFFF838       3     8     1     2    0x00000000
 *
 * with GR_COMBINE_FUNCTION_LOCAL=1, SCALE_OTHER=3, BLEND=7;
 * GR_COMBINE_FACTOR_ZERO=0, LOCAL=1, TEXTURE_ALPHA=4, ONE=8;
 * GR_COMBINE_LOCAL_ITERATED=0, CONSTANT=1; GR_COMBINE_OTHER_ITERATED=0,
 * TEXTURE=1, CONSTANT=2.
 *
 * TWO ENUMERATOR NAMES IN br_dl.h ARE SWAPPED, and this is not a quibble:
 * BR_DL_CC_SHADE is attached to FCFFFFFF/FFFCF87C, which Glide renders as
 * `1.0 * texture` and whose N64 words decode to (a-b)*c+d == TEXEL0; and
 * BR_DL_CC_TEX is attached to FCFFFFFF/FFFE793C, which Glide renders as the
 * ITERATED (vertex) colour and whose N64 words decode to SHADE.  Both
 * decodes agree, and row 6 independently corroborates the reading:
 * grConstantColorValue(-1) is white and its N64 d-input is the literal `1`.
 * The names are left alone -- another pass owns br_dl.h -- and the truth is
 * recorded here and used by the shader table.
 *
 * WHAT THE RENDER MODE DOES
 * ----------------------------------------------------------------------
 * 0x10021270 is grDepthMask(1) followed by nine exact compares and a
 * bit-tested fallback, and every arm sets some subset of {alpha test
 * reference, alpha test function, grAlphaCombine, grAlphaBlendFunction,
 * grDepthBufferFunction, grDepthMask}.  Only three distinct blend functions
 * and four distinct depth functions appear in the whole routine, which is
 * where BR_GFX3D_BLEND__COUNT and BR_GFX3D_Z__COUNT come from.  The
 * transcription lives in br_gfx_metal.m beside the addresses.
 */
#ifndef BR_GFX3D_H
#define BR_GFX3D_H

#include <stdint.h>

#include "br_gfx.h"
#include "br_dl.h"

/* grAlphaBlendFunction(rgb_src, rgb_dst, a_src, a_dst); the three tuples
 * 0x10021270 ever passes.  GR_BLEND_ZERO=0, SRC_ALPHA=1, ONE=4,
 * ONE_MINUS_SRC_ALPHA=5. */
typedef enum BrGfx3dBlend {
    BR_GFX3D_BLEND_OPAQUE = 0,  /* (4,0,4,0) -- 0x10021538, the default arm */
    BR_GFX3D_BLEND_ALPHA,       /* (1,5,4,0) -- 0x100212DF and five others  */
    BR_GFX3D_BLEND_ADD,         /* (1,4,4,0) -- 0x10021335, 0x1002144E     */
    BR_GFX3D_BLEND__COUNT
} BrGfx3dBlend;

/* grDepthBufferFunction; GR_CMP_LESS=1, EQUAL=2, LEQUAL=3, ALWAYS=7. */
typedef enum BrGfx3dZ {
    BR_GFX3D_Z_LESS = 0,        /* mode == 0                                */
    BR_GFX3D_Z_EQUAL,           /* modes 0x00504F50 and 4 -- ZMODE_DEC      */
    BR_GFX3D_Z_LEQUAL,          /* mode == 1                                */
    BR_GFX3D_Z_ALWAYS,
    BR_GFX3D_Z__COUNT
} BrGfx3dZ;

/* Enough slots for the whole closed set twice over; a display list that
 * needed more would itself be the finding. */
#define BR_GFX3D_MAX_SEEN 24

typedef struct BrGfx3dStats {
    uint32_t cTri;                          /* triangles submitted          */
    uint32_t cVerts;                        /* vertices in the batch stream */
    uint32_t cDraws;                        /* batch flushes == draw calls  */
    uint32_t cStateChanges;                 /* flushes caused by a state    */
    uint32_t cRects;
    uint32_t cBinds;
    uint32_t cCombineCmds;                  /* G_SETCOMBINE seen            */
    uint32_t cModeCmds;                     /* SETOTHERMODE_L shift 3 seen  */
    uint32_t cModeUnrecognised;             /* fell to the bit-tested arm   */
    uint32_t aCombineUse[BR_DL_CC__COUNT];  /* triangles per combiner row   */
    uint32_t aBlendUse[BR_GFX3D_BLEND__COUNT];
    uint32_t aZUse[BR_GFX3D_Z__COUNT];

    /* Which words the DATA actually carries, as opposed to which words the
     * two routines can recognise.  The difference between those two sets is
     * the only honest answer to "how much of the state model is retail data
     * exercising", and it cannot be got from the disassembly alone. */
    uint32_t aSeenCombine[BR_GFX3D_MAX_SEEN][2];
    uint32_t cSeenCombine;
    uint32_t aSeenMode[BR_GFX3D_MAX_SEEN];
    uint32_t cSeenMode;
} BrGfx3dStats;

/* Build the pipeline and depth-state arrays and the depth target.  Safe to
 * call more than once; returns 0 on success.  Deliberately NOT folded into
 * BrGfxCreate, so a caller that only wants the 2D path (every existing one)
 * pays nothing and cannot be broken by a change here. */
int BrGfx3dInit(BrGfx *pGfx);

/* Install this backend as pDl's sink.  pDl is retained: the sink signatures
 * carry no fill colour or primitive colour, and the original reads both out
 * of interpreter state at use time, so the backend does the same. */
void BrGfx3dAttach(BrGfx *pGfx, BrDl *pDl);

/* Begin/end a 3D pass on the offscreen target.  This is its own render pass
 * with a depth attachment; BrGfxBeginFrame's 2D pass is untouched. */
void BrGfx3dBeginFrame(BrGfx *pGfx, float r, float g, float b, float a);
void BrGfx3dEndFrame(BrGfx *pGfx);

/* Point a display-list texture handle (0xDC's low 24 bits) at an uploaded
 * texture.  Nothing yet explains how a texture reaches the bind opcode, so
 * unmapped handles sample a 1x1 white texel and the combiner still runs. */
void BrGfx3dMapTexture(BrGfx *pGfx, uint32_t handle, BrTexture tex);

/* NOT FROM THE ORIGINAL: force every draw to BR_GFX3D_Z_ALWAYS with depth
 * writes off.  Exists so the Metal output can be compared against br_dl.c's
 * reference rasteriser, which has no z-buffer -- without it the two images
 * differ for a reason that has nothing to do with the port being wrong. */
void BrGfx3dSetDepthTest(BrGfx *pGfx, int fEnable);

const BrGfx3dStats *BrGfx3dGetStats(const BrGfx *pGfx);

#endif /* BR_GFX3D_H */
