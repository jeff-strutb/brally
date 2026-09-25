/* br_modelswap.c -- gamedata: big-endian model image fixup.
 *
 * BrModelSwap byte-reverses every field of a loaded model image (authored
 * big-endian), relocates its internal pointers through the segment fixup and
 * hands each finished display list to the renderer.
 *
 * Filed out of the address batch slice2_19.c; the preamble is slice2_19.c's,
 * carried whole, with the byte-reversal helpers BrModelSwap uses.
 */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl (this, r, g, b).  Original is thiscall with
 * ret 0xC; hide that prototype so the definition can take the struct-arg
 * __fastcall shape that reproduces it. */
#define BrRgbSinkSet BrRgbSinkSet_hdr
#endif
#ifdef BR_MATCHING_BUILD
/* slice2_19.h / br_seg.h declare these cdecl with a leading state pointer the
 * originals do not have.  Hide those prototypes so BrModelLoad can call them
 * with the shapes the bytes show. */
#define BrSub100088B0 BrSub100088B0_cdecl
#define BrSegSetBases BrSegSetBases_cdecl
#endif
#include "slice2_19.h"
#ifdef BR_MATCHING_BUILD
#undef BrSub100088B0
#undef BrSegSetBases
typedef struct { void *p; } BrModelLoadArg;
extern int g_brModelMgr;                        /* 0x10AC0810 */
void * __fastcall BrSub100088B0(void *pThis, BrModelLoadArg a,
                                BrModelLoadArg b);
void BrSegSetBases(uint32_t n64Base, uint32_t hostBase);
#endif
#ifdef BR_MATCHING_BUILD
#undef BrRgbSinkSet
#endif

#include <string.h>

/* ================================================================== */
/* 6. Big-endian model fixup                                          */
/* ================================================================== */

/* `swap byte n with byte n+3, byte n+1 with byte n+2` -- what the original
 * spells out for every 32-bit slot it is about to hand to the fixup. */
/* 2026-09-13, BrModelSwap (6 unpaired rows): the original composes each
 * halfword LOW byte first (`mov al,[+3]; mov ah,[+2]`, and in the leaf loop
 * `xor ecx,ecx; mov cl,[+1]; mov ch,[+0]`); ours loads the shifted byte
 * first. DEAD: `p[1] | (p[0] << 8)` (byte-identical), a `+` sum (961),
 * `uint16_t v = p[1]; v |= ...` (710), a byte temp for the low byte (710),
 * an `unsigned int` accumulator for the zero-extension (796), and BrRev4
 * with two temps storing p[3] first (923). */
static void BrRev4(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t;

    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void BrRev2(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t = p[0];

    p[0] = p[1];
    p[1] = t;
}

/* The other form the original uses for 32-bit fields: compose the value
 * byte-wise MSB-first and store it natively. Identical to BrRev4 on the
 * little-endian host the original ran on; kept distinct because the two are
 * genuinely different instruction sequences. */
static void BrRdBe32(void *pv)
{
    const unsigned char *p = (const unsigned char *)pv;
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] <<  8) | (uint32_t)p[3];

    memcpy(pv, &v, 4);
}

static uint32_t BrLd32(const void *pv)
{
    uint32_t v;

    memcpy(&v, pv, 4);
    return v;
}

static uint16_t BrLd16(const void *pv)
{
    uint16_t v;

    memcpy(&v, pv, 2);
    return v;
}

/* 0x10036C00 */
/* WHAT IT DOES: makes a model file usable after loading. The game's art was
 * authored for a machine that stores numbers the other way round, so every
 * number in the file has to be turned back to front, and every address in it
 * corrected to where the data now sits -- header, geometry, animation frames
 * and all. Each finished piece is then handed to the renderer. */
/* @t4-pass 0x100302A0 1 2026-09-10 probes 60 bytes 1056 insns 370 regions 10 rows 15 census yes  (tools/crank.py) */
/* @t4-pass 0x100302A0 2 2026-09-20 probes 14 bytes 1056 insns 370 regions 10 rows 15 census yes  (tools/crank.py) */
/* @t3 0x100302A0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1056/1062 insns 370/371 rows 8+7 regions 10 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is the byte-reversal COMPOSE spelling: the original materialises a few
 * of the swapped words with shl/or composes in a different byte/register order
 * than this build emits, a lowering/allocation choice with no source handle (see
 * the dossier and dead list above).  The A5 behavioural oracle RUNS the full
 * serialized-model walk -- header swaps, block/item pointer fixups through
 * BrSegPtrFixup, the vertex resolve -- on a seeded well-formed model and returns
 * EQUIVALENT over 48 seeds, with a negative control on the reversed fields
 * proving teeth; that supersedes the byte gates (CLAUDE.md rule 12).  Do not
 * reopen before the end-grind. */
/* @implements 0x10036C00 d3d BrModelSwap */
#ifdef BR_MATCHING_BUILD
/* RESIDUE 1062 vs 1053 bytes, 371 vs 368 instructions, register-blind 8+11
 * (from 149+285 when this was first opened, and 13+23 before the leaf-loop
 * step below -- see the git log).  The 2-byte reversal being a halfword
 * COMPOSE AND ONE 16-BIT STORE, rather than two byte stores, was one big one:
 * the original has seven `mov word ptr` stores, exactly one per BrRev2 site.
 *
 * The other was the LEAF LOOP's bound.  Hoisting `3 * item->m` into an
 * `nHalf` local turned the loop into a count-DOWN (`dec`/`jne`) walking a
 * negative displacement, and freed a register so `j` never spilled.  The
 * original RE-READS the bound every pass -- it reloads PITEM from the slot,
 * loads item->m, `lea edx,[edx+edx*2]` and compares -- which is what puts `j`
 * in the THIRD stack slot and makes the frame `sub esp,0xc` rather than 8.
 * Spelling the bound in the for-condition closed the frame, the loop rotation
 * and the whole body: the leaf loop is now instruction-for-instruction exact.
 *
 * WHAT IS LEFT, all measured against this baseline:
 *  - 2 insns in the RECORD loop's guard.  The original walks that loop on a
 *    pointer biased +2 (`lea esi,[ebp+0xa]`) and rematerialises pRec each
 *    pass (`mov eax,[esi-2]` / `lea edi,[esi-2]` / `test eax,eax`), where we
 *    fold to `cmp dword ptr [esi],0`.  It then uses edi for offsets 0..3 and
 *    the two tail reloads, esi for 4..0x13.
 *  - 1 insn: the fixup argument, orig `mov ecx,edi` + `add ecx,edx` against
 *    our `mov ecx,[esi]` + `add ecx,edi`.  A register copy.
 *  - the `off = 0x20` init: orig emits `mov ebx,0x20` in the leaf loop's
 *    PREHEADER (after the `k <= 0` guard), we emit it before.  Same count.
 *  - BrRev4's two stores per pair come out in the opposite order at 2 of the
 *    sites -- and the sites disagree with each other, so it is scheduling.
 *  - ~30 instructions differ only as `[edi+eax]` vs `[eax+edi]` (SIB base and
 *    index exchanged).  Register-blind-invisible, byte-visible.
 * PROBED AND DEAD, do not re-run: single-temp and load-both-first spellings
 * of the byte swap (two byte stores never merge, however the temps are
 * arranged); the same through a `p_` pointer temp (better RAW, worse size and
 * instruction count); giving the leaf loop's doubled subscript its own local
 * stepped by 2 (register-blind 36 -> 49); the high-byte-down spelling of
 * BrRev4 (`t=p[3]; p[3]=p[0]; p[0]=t;` -- fixes the head site, 8+11 -> 16+21
 * overall); flipping BrRev2's `|` operands, `off` moved into the for-init,
 * writing PSLOT offset-first as `4 + 4*iItem + PBLOCK`, and giving the record
 * loop its own `unsigned char *p = pRec` with the guard through a local --
 * all four are INERT, VC5 canonicalises them to the identical bytes.
 *
 * MACROS, not statics -- MSVC5 will not inline a static with more than one
 * caller, so every BrRev/BrLd here was a `call` the original does not have.
 * Scoped to BrModelSwap with #undef below so the other users of these
 * helpers keep whatever shape they already match with. */
#define BrRev4(pv) do { unsigned char t_; \
    t_ = ((unsigned char *)(pv))[0]; ((unsigned char *)(pv))[0] = ((unsigned char *)(pv))[3]; ((unsigned char *)(pv))[3] = t_; \
    t_ = ((unsigned char *)(pv))[1]; ((unsigned char *)(pv))[1] = ((unsigned char *)(pv))[2]; ((unsigned char *)(pv))[2] = t_; } while (0)
#define BrRev2(pv) (*(uint16_t *)(void *)(pv) = (uint16_t)( \
    ((uint16_t)((unsigned char *)(pv))[0] << 8) | (uint16_t)((unsigned char *)(pv))[1] ))
#define BrRdBe32(pv) do { uint32_t v_ = \
      ((uint32_t)((unsigned char *)(pv))[0] << 24) | ((uint32_t)((unsigned char *)(pv))[1] << 16) \
    | ((uint32_t)((unsigned char *)(pv))[2] << 8)  | (uint32_t)((unsigned char *)(pv))[3]; \
    *(uint32_t *)(void *)(pv) = v_; } while (0)
#define BrLd32(pv) (*(const uint32_t *)(const void *)(pv))
#define BrLd16(pv) (*(const uint16_t *)(const void *)(pv))
/* DIRECT calls, not indirect: the original has nine `call rel32` and one
 * `call [mem]`; the two fixup/deref hooks are ordinary functions here. */
void  BrModelFixupDirect(uint32_t *pSlot);            /* 0x100189E0 */
void *BrModelDerefDirect(uint32_t slot);
/* A5-oracle symbol->VA hints (matched by t3b_env's declared-VA regexes; the
 * real declarations are elsewhere).  BrModelFixupDirect == BrSegPtrFixup,
 * BrModelVtxResolve == BrVtxCacheResolve, g_BrGfxSubmitB is a fn-ptr global:
 *   BrModelVtxResolve(uint32_t *pSlot, int n);  0x10018E10
 *   extern void g_BrGfxSubmitB;                  0x118ED1DC  */
#define g_BrModelFixup BrModelFixupDirect
#define g_BrModelDeref BrModelDerefDirect
#endif
void BrModelSwap(void *pImage)
{
    unsigned char *pHdr = (unsigned char *)pImage;
    unsigned char *pRec;
    uint32_t iRec;

    /* Header +0x00 and +0x02: two independent big-endian halfwords, and the
     * original does the SECOND one first -- its word store to +2 precedes the
     * one to +0. */
    BrRev2(pHdr + 2);
    BrRev2(pHdr + 0);

    /* GOTCHA: tested BEFORE the byte reversal. Only works because zero is a
     * palindrome. */
    if (BrLd32(pHdr + 4) != 0) {
        int32_t iItem;
#define PBLOCK (*(unsigned char **)(void *)(pHdr + 4))
#define PSLOT  (PBLOCK + 4 + 4 * (size_t)iItem)
#define PITEM  (*(unsigned char **)(void *)PSLOT)

        BrRev4(pHdr + 4);
        g_BrModelFixup((uint32_t *)(pHdr + 4));

        BrRdBe32(PBLOCK);                  /* block->n */

        /* The original re-reads the count from the block on every pass. */
        for (iItem = 0; iItem < (int32_t)BrLd32(PBLOCK); iItem++) {
            int32_t iLeaf;
            size_t off;

            BrRev4(PSLOT);
            g_BrModelFixup((uint32_t *)PSLOT);

            BrRdBe32(PITEM + 0x00);        /* item->m */
            BrRev4  (PITEM + 0x04);
            g_BrModelFixup((uint32_t *)(PITEM + 0x04));

            /* The vertex-cache resolve is handed the SLOT, not the value. */
            BrModelVtxResolve((uint32_t *)(PITEM + 0x04),
                              (int)BrLd32(PITEM + 0x00));

            BrRev4(PITEM + 0x08);
            g_BrModelFixup((uint32_t *)(PITEM + 0x08));

            BrRdBe32(PITEM + 0x0C);        /* item->k */
            BrRev2  (PITEM + 0x10);
            BrRev2  (PITEM + 0x12);
            /* These three are plain in-place byte reversals, not the
             * compose-and-store form: the original has exactly three
             * shl/or composes (block->n, item->m at +0x00 and item->k at
             * +0x0C) and six shl total, where the compose spelling here
             * gave twelve. */
            BrRev4(PITEM + 0x14);
            BrRev4(PITEM + 0x18);
            BrRev4(PITEM + 0x1C);

            /* The leaf count is likewise re-read from the item every pass;
             * the original's `if (k <= 0) skip` guard is the same test. */
            off = 0x20;
            for (iLeaf = 0;
                 iLeaf < (int32_t)BrLd32(PITEM + 0x0C);
                 iLeaf++, off += 4) {
                int32_t j;
#define PLEAF (*(unsigned char **)(void *)(PITEM + off))

                BrRev4(PITEM + off);
                g_BrModelFixup((uint32_t *)(PITEM + off));
                BrRev4(PLEAF + 0);

                /* GOTCHA: the halfword count comes from the ITEM's first
                 * dword, not the leaf's -- and it is re-read on every pass,
                 * like every other count in this function. */
                for (j = 0; j < 3 * (int32_t)BrLd32(PITEM + 0x00); j++)
                    BrRev2(PLEAF + 4 + 2 * (size_t)j);
#undef PLEAF
            }
        }
#undef PITEM
#undef PSLOT
#undef PBLOCK
    }

    /* ---- the record array at +0x08, stride 0x14 ----
     * The count is re-read from the header on every pass, and the compare
     * is unsigned. */
    /* The cursor is biased +2 into the record, not parked on its first
     * field: the original's `lea esi,[ebp+0xa]` and its `[esi-2]` reads of
     * the slot are that bias, and spelling it here is what puts the whole
     * record's field displacements on the original's numbers (the +4 and
     * +0xa biases were measured too, and are both worse). */
    pRec = pHdr + 10;

    for (iRec = 0; iRec < (uint32_t)BrLd16(pHdr + 2); iRec++, pRec += 0x14) {
        uint32_t v;

        if (BrLd32(pRec - 0x02) == 0)
            continue;

        BrRev4(pRec - 0x02);
        g_BrModelFixup((uint32_t *)(pRec - 0x02));
        BrRev2(pRec + 0x02);
        BrRev2(pRec + 0x04);
        BrRev4(pRec + 0x06);
        BrRev4(pRec + 0x0A);
        BrRev4(pRec + 0x0E);

        v = BrLd32(pRec - 0x02);
        BrF3DListFixup(v);
        BrSub10074DC0(8);
        g_BrGfxSubmitB(BrLd32(pRec - 0x02));
    }
}
#ifdef BR_MATCHING_BUILD
#undef BrRev4
#undef BrRev2
#undef BrRdBe32
#undef BrLd32
#undef BrLd16
#undef g_BrModelFixup
#undef g_BrModelDeref
#endif
