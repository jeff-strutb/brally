/* br_entity.c -- the world's objects and where they sit.
 *
 * RESPONSIBILITY: what is in the world and where -- setting an entity up,
 * linking it to its record in the parallel table, counting the ones in use,
 * and rebinding their graphics handles.
 *
 * Moved here out of the address batches under src/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include <string.h>

#include "slice1_05.h"
#ifdef BR_MATCHING_BUILD
/* Orig takes no args: it walks DAT_10af2110 / DAT_100b2f04 directly. */
#define BrEntityCountActive BrEntityCountActive_cdecl_hdr
/* slice1_09.h declares this cdecl; the original is thiscall with no stack
 * args.  Hide that prototype so the matching body can use __fastcall --
 * the same split src/core/slice1_09.c made while this lived there. */
#define BrEntityBindAux      BrEntityBindAux_cdecl
#endif
#include "slice1_09.h"   /* BR_ENTITY_* offsets and strides, BrMat4 */
#include "slice2_12.h"   /* the BrEntityCountActive prototype */
#ifdef BR_MATCHING_BUILD
#undef BrEntityBindAux
#undef BrEntityCountActive
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

/* 0x10005470.  BR_ENTITY_STRIDE (0x2B68) comes from slice1_09.h.
 *
 * NOTE: the base here is 0x10ACEDB0, which is NOT the 0x10ACDEA8 that pass
 * 09's entity helpers use -- the two differ by 0xF08, not by a whole number of
 * records. Either this walks a different array or it starts 0xF08 into the
 * record; the stride and the "first dword non-zero" test are all this code
 * establishes, so the base stays a parameter. */
/* WHAT IT DOES: counts how many entries in a table of cars or other world
 * objects are in use, by checking each record's first word for a non-zero
 * value. */
/* @implements 0x10005470 d3d BrEntityCountActive */
#ifdef BR_MATCHING_BUILD
/* Orig: `mov edx,[DAT_100b2f04]; mov ecx, offset DAT_10af2110` then a
 * countdown do-while.  Parameters are a port convenience. */
extern int32_t DAT_100b2f04;
extern unsigned char DAT_10af2110[];
uint32_t BrEntityCountActive(void)
{
    int32_t n = DAT_100b2f04;
    uint32_t c = 0;
    unsigned char *p;

    /* Orig `test edx,edx; jle ret` — skip the countdown, do not early-return
     * (that duplicates `ret`). */
    if (n > 0) {
        p = DAT_10af2110;
        do {
            if (*(int32_t *)p != 0)
                ++c;
            p += BR_ENTITY_STRIDE;
            --n;
        } while (n != 0);
    }
    return c;
}
#else
uint32_t BrEntityCountActive(const void *pvRecords, int32_t cRecords)
{
    const unsigned char *p = (const unsigned char *)pvRecords;
    uint32_t             n = 0;
    int32_t              i;

    for (i = 0; i < cRecords; ++i) {
        uint32_t first;
        memcpy(&first, p, sizeof first);        /* byte order is irrelevant */
        if (first != 0)
            ++n;
        p += BR_ENTITY_STRIDE;
    }
    return n;
}
#endif

/* ---- moved out of src/core/slice2_19.c's ghidra-matched tail ---------- */
#ifdef BR_MATCHING_BUILD
extern int DAT_106ed6fc;
extern int DAT_100b2f04;
extern unsigned char DAT_10af3bb7;
extern char DAT_10af3bcc;
extern int DAT_100aa128;
extern int DAT_100aa1e8;
extern int DAT_100aa068;
#ifndef BR_FUNCPTR_DEFINED
#define BR_FUNCPTR_DEFINED
typedef int (*funcptr)();
#endif
extern funcptr DAT_10b73534;
int FUN_1002d864();

/* WHAT IT DOES: for every entity slot (stride 0x2B68), for each of its 3 banks, rebind
 * the 10 primary and 3 secondary handles at +0x8018/+0x80BC of the entity's data block
 * through 0x1002D864 -- against one of two tables picked by the type byte at +0x3BB7 --
 * then fire the hook at 0x10B73534. The bank counter is DECLARED INSIDE the outer loop
 * (that block scoping is what gives it the last /Od stack slot). */
/* @implements 0x1002DB88 glide BrEntGfxRebindAll */

void BrEntGfxRebindAll(void)

{
  int local_8;
  int local_c;
  
  DAT_106ed6fc = 0;
  for (local_8 = 0; local_8 < DAT_100b2f04; local_8 = local_8 + 1) {
    int local_10;
    for (local_10 = 0; local_10 < 3; local_10 = local_10 + 1) {
      if ((&DAT_10af3bb7)[local_8 * 0x2b68] == 2) {
        for (local_c = 0; local_c < 10; local_c = local_c + 1) {
          FUN_1002d864(*(int *)
                        (*(int *)(&DAT_10af3bcc + local_8 * 0x2b68) + 0x8018 + local_10 * 0x28 +
                        local_c * 4),&DAT_100aa128);
        }
        for (local_c = 0; local_c < 3; local_c = local_c + 1) {
          FUN_1002d864(*(int *)
                        (*(int *)(&DAT_10af3bcc + local_8 * 0x2b68) + 0x80bc + local_10 * 0xc +
                        local_c * 4),&DAT_100aa1e8);
        }
      }
      else {
        for (local_c = 0; local_c < 10; local_c = local_c + 1) {
          FUN_1002d864(*(int *)
                        (*(int *)(&DAT_10af3bcc + local_8 * 0x2b68) + 0x8018 + local_10 * 0x28 +
                        local_c * 4),&DAT_100aa068);
        }
        for (local_c = 0; local_c < 3; local_c = local_c + 1) {
          FUN_1002d864(*(int *)
                        (*(int *)(&DAT_10af3bcc + local_8 * 0x2b68) + 0x80bc + local_10 * 0xc +
                        local_c * 4),&DAT_100aa068);
        }
      }
    }
  }
  (*DAT_10b73534)();
  return;
}

#endif /* BR_MATCHING_BUILD */
