/* br_entity.c -- the world's objects and where they sit.
 *
 * RESPONSIBILITY: what is in the world and where -- setting an entity up,
 * linking it to its record in the parallel table, and moving it.
 *
 * Moved here out of the address batches under src/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include "slice1_05.h"
#ifdef BR_MATCHING_BUILD
/* slice1_09.h declares this cdecl; the original is thiscall with no stack
 * args.  Hide that prototype so the matching body can use __fastcall --
 * the same split src/core/slice1_09.c made while this lived there. */
#define BrEntityBindAux      BrEntityBindAux_cdecl
#endif
#include "slice1_09.h"   /* BR_ENTITY_* offsets and strides, BrMat4 */
#ifdef BR_MATCHING_BUILD
#undef BrEntityBindAux
#endif

/* 0x10035FE0 */
/* WHAT IT DOES: prepares one entity for use -- clears its state, works out its
 * own number from where it sits in the array, and links it to the matching
 * record in the parallel table so the two can find each other later. */
BrEnt    g_aBrEnts[16];      /* 0x106ED708 */
BrEntRec g_aBrEntRecs[16];   /* 0x106ED630 */

/* @implements 0x10035FE0 d3d BrEntInit */
/* @n64 0x80255B54 located */
void __fastcall BrEntInit(BrEnt *pEnt)
{
    long idx;

    /* Written in this order by the original: +0x30, +0x2C, +0x44. */
    pEnt->f30 = 0;
    pEnt->f2C = 0;
    pEnt->f44 = 0;

    idx = (long)(pEnt - g_aBrEnts);
    pEnt->f154 = (int32_t)idx;
    pEnt->f158 = &g_aBrEntRecs[idx];
}

/* 0x100307D0 -- BrMat4Identity (br_mat.h). Reproduced as a static for the
 * same reason as BrBitStreamAlignRead above: 0x10076C90 calls it and this
 * translation unit must link on its own.
 * DEVIATION: duplicate of an existing symbol, kept private (static). */
/* WHAT IT DOES: resets a transform matrix to "no transform at all" -- ones
 * down the diagonal, zeroes everywhere else -- so whatever it is applied to
 * comes through unchanged. */
/* @implements 0x100307D0 d3d BrMat4IdentityLocal */
#ifdef BR_MATCHING_BUILD
/* The original is fully unrolled: sixteen sequential stores, 1.0f and 0.0f
 * hoisted into edx/ecx as integer patterns. */
static void BrMat4IdentityLocal(BrMat4 *pM)
{
    pM->m[0][0] = 1.0f; pM->m[0][1] = 0.0f; pM->m[0][2] = 0.0f; pM->m[0][3] = 0.0f;
    pM->m[1][0] = 0.0f; pM->m[1][1] = 1.0f; pM->m[1][2] = 0.0f; pM->m[1][3] = 0.0f;
    pM->m[2][0] = 0.0f; pM->m[2][1] = 0.0f; pM->m[2][2] = 1.0f; pM->m[2][3] = 0.0f;
    pM->m[3][0] = 0.0f; pM->m[3][1] = 0.0f; pM->m[3][2] = 0.0f; pM->m[3][3] = 1.0f;
}
#else
static void BrMat4IdentityLocal(BrMat4 *pM)
{
    int r, c;
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            pM->m[r][c] = (r == c) ? 1.0f : 0.0f;
}
#endif

/* 0x10076C90  __thiscall.
 *
 * The original is  idx = (this - 0x10ACDEA8) / 0x2B68  performed with the
 * magic multiply 0x5E5D422B followed by `sar edx,12` and the usual
 * shr/add sign fix -- i.e. plain signed division truncating toward zero.
 * The 348-byte scale is assembled as 348 = ((idx*8 - idx)*4 + idx)*3*4
 * through three LEAs.
 *
 * DEVIATION: the two array bases are parameters instead of the hardcoded
 * 0x10ACDEA8 / 0x106C6678, and the aux pointer is stored as a host pointer
 * (8 bytes on a 64-bit build) where the original stored a 32-bit value. The
 * two strides below are the ORIGINAL 32-bit strides and are not adjusted --
 * they are the sizes of the game's own structures, not of anything declared
 * here. */
/* WHAT IT DOES: links a world object to its matching record in a second,
 * parallel array, by working out how far along the main array the object
 * sits and stepping the same distance into the other one, and then resets
 * the object's transform matrix to no transform. */
/* @implements 0x10076C90 d3d BrEntityBindAux */
/* @n64 0x802207A4 located */
#ifdef BR_MATCHING_BUILD
/* thiscall, no stack args. Both array bases are pinned globals; the index
 * is a signed magic-divide by the 0x2B68 entity stride. */
extern char DAT_10af1208;   /* entity[0] */
extern char DAT_106ed708;   /* aux[0], stride 348 */

void __fastcall BrEntityBindAux(void *pThis, int _edx_unused)
{
    char *p  = (char *)pThis;
    int  idx = (int)((p - &DAT_10af1208) / BR_ENTITY_STRIDE);

    (void)_edx_unused;
    *(void **)(p + BR_ENTITY_OFF_AUX) =
        &DAT_106ed708 + idx * BR_ENTITY_AUX_STRIDE;
    BrMat4IdentityLocal((BrMat4 *)(void *)(p + BR_ENTITY_OFF_MATRIX));
}
#else
void BrEntityBindAux(void *pEntity, void *pEntityArrayBase,
                     void *pAuxArrayBase)
{
    unsigned char *p    = (unsigned char *)pEntity;
    unsigned char *pAux = (unsigned char *)pAuxArrayBase;
    ptrdiff_t      idx  = (p - (unsigned char *)pEntityArrayBase)
                          / BR_ENTITY_STRIDE;
    void **ppAux = (void **)(void *)(p + BR_ENTITY_OFF_AUX);

    *ppAux = pAux + idx * BR_ENTITY_AUX_STRIDE;
    BrMat4IdentityLocal((BrMat4 *)(void *)(p + BR_ENTITY_OFF_MATRIX));
}
#endif
