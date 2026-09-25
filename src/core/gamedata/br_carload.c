/* br_carload.c -- gamedata: loading one car's .rca record off the disc.
 *
 * Builds "cars/" + the car's name + ".rca", reads the whole file into the
 * caller's buffer, checks the "RCar" magic and hands the record to the
 * fixup pass. Filed out of slice4_53.c.
 *
 * The portable reader for the same record -- the same 340 bytes at glide
 * 0x10030DE0 -- is br_cardata.c; see br_cardata.h for the trail from the
 * disc to body+0x1DC.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice4_53.h"
#include "slice2_20.h"      /* BrFileReadInto, BrRcaLoadCar */

#include <stdio.h>
#include <string.h>

/* 0x10037740 */
/* WHAT IT DOES: loads one car's model and data out of the game's .rca
 * archive into the given buffer. */
/* @implements 0x10037740 d3d BrSub10037740 */
#ifdef BR_MATCHING_BUILD
/* The original is the full path-building loader the port folded into
 * BrRcaLoadCar: special-buffer gate, per-arm hook call (cross-jumped),
 * strcpy/strcat path assembly, magic check via the imported memcmp, the
 * (bug-for-bug) self-referential failure sprintf into BrLogPrint, and the
 * flag save/restore around it all. */
extern int   DAT_10ac67a4;
extern int   DAT_10ac67c0;
extern int   DAT_100b8498;
extern char  DAT_100bcdd0;          /* the special car buffer */
extern char  DAT_100b7900[];        /* base path   */
extern char *DAT_100b7d00[];        /* per-car names */
extern char  DAT_100aa310[];        /* extension   */
extern char  DAT_100aa308[];        /* magic bytes */
extern char  DAT_100aa2f4[];        /* failure format */
extern int   FUN_1005a080(int idx, int flag);
extern void  BrSub10030770(void *pCar);   /* glide 0x10030770 */
extern void  BrLogPrint(const void *p);
/* The original calls the /MD import (FF 15) -- go through the import
 * slot explicitly; string.h's decl is not dllimport for the intrinsics. */
extern int (__cdecl *_imp__memcmp)(const void *, const void *, unsigned int);
#define BR_MEMCMP_IMP (*_imp__memcmp)

void BrSub10037740(void *pCar, void *pArg)
{
    int  saved;
    char szMsg[0x100];
    char szPath[0x400];
    int  idx = (int)pArg;

    DAT_10ac67a4 = idx;
    if (pCar != (void *)&DAT_100bcdd0) {
        saved = DAT_100b8498;
        if (DAT_100b8498 == 0)
            DAT_100b8498 = 1;
        FUN_1005a080(idx, 0);
    } else {
        FUN_1005a080(idx, 1);
    }

    DAT_10ac67c0 = 0;
    strcpy(szPath, DAT_100b7900);
    strcat(szPath, DAT_100b7d00[idx]);
    strcat(szPath, DAT_100aa310);

    BrFileReadInto(pCar, szPath, -1);

    if (BR_MEMCMP_IMP(pCar, DAT_100aa308, 4) != 0) {
        sprintf(szMsg, DAT_100aa2f4, szMsg);
        BrLogPrint(szMsg);
    }

    BrSub10030770(pCar);

    if (pCar != (void *)&DAT_100bcdd0)
        DAT_100b8498 = saved;
}
#else
/* WHAT IT DOES: the port spelling of the same load -- fetch car iCar's .rca
 * record into the caller's buffer. The path building, magic check and fixup
 * are folded into the portable reader instead of being spelled out here. */
/* @implements 0x10037740 d3d BrSub10037740 */
void BrSub10037740(void *pCar, void *pArg)
{
    /* DEVIATION: pArg is declared void* by slice2_19 but is an integer index
     * in the original.  DEVIATION: cbDest is slice2_20's port-only bound;
     * the original has none.  0x15F88 is the stride its only caller
     * (0x10035520) uses to compute pCar, so it is the true extent. */
    BrRcaLoadCar(pCar, (size_t)BR_RCA_CAR_STRIDE, (int)(intptr_t)pArg);
}
#endif

/* ==========================================================================
 * The .rca texture-record fixup (filed out of drawing/br_fadewipe.c, which
 * took it from the address batch slice2_16.c)
 *
 * Once a car's .rca file is in memory, each texture record in it is turned
 * the right way round, rebased and has its pixels pulled in from the file
 * blob.  Placed after the loader, record fixup before mesh swap: that is the
 * layout whose /O2 compile of both bodies is byte-identical to the one they
 * were certified at (probed across every gamedata module, 2026-09-25).
 * ========================================================================== */
#include <stdint.h>
#include <string.h>
void BrSegPtrFixup(uint32_t *p);
extern int32_t g_br675540;                 /* 0x10675540 */
extern void    BrGbiCall10075330(void *pv);
void BrRcaSwapMesh(void *pv);
void BrRcaFixupRecord(void *pRec);

/* Byte-swap the u32 that starts at p, byte-wise so the host's own endianness
 * never enters into it. */
static void br16_swap_u32_at(uint8_t *p)
{
    uint8_t t;
    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void br16_swap_u16_at(uint8_t *p)
{
    uint8_t t = p[0];
    p[0] = p[1];
    p[1] = t;
}

static uint16_t br16_ld16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

int32_t  g_brSegN64Base;   /* 0x104B16E4 */
int32_t  g_brSegHostBase;  /* 0x104B16E0 */
uint8_t *g_brRcaBlob;      /* 0x106B7C7C */

/* WHAT IT DOES: prepares one texture record loaded from an .rca data file
 * for use: turns its fields the right way round, rebases the two addresses
 * it carries onto real memory, and then pulls in the actual pixel data --
 * either through an attached mesh header that says where in the file blob
 * the pixels live, or through a plain index into that blob. When the copying
 * is switched off it just does the byte order and lets the record go. */
/* @t4-pass 0x10018B60 1 2026-09-07 probes 77 bytes 492 insns 165 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018B60 2 2026-09-07 probes 77 bytes 492 insns 165 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018B60 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 492/493 insns 165/165 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind multiset
 * (rows 0+0), 1 byte of encoding shadow, 2 masked regions; two crank
 * census passes at these numbers.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10018B60 glide BrRcaFixupRecord */
void BrRcaFixupRecord(void *pRec)
{
    uint8_t *r = (uint8_t *)pRec;
    uint32_t tmp;
    uint32_t flags;
    uint8_t  a, b;

    /* +0x00 and +0x04: 4-byte reversal, then the in-place rebase.  The
     * three loads before any store are the original's order. */
    {
        uint8_t t0 = r[0], t3 = r[3], t1 = r[1];
        r[3] = t0; r[0] = t3;
        t3 = r[2];
        r[2] = t1; r[1] = t3;
    }
    BrSegPtrFixup((uint32_t *)(void *)r);

    {
        uint8_t t0 = r[4], t3 = r[7], t1 = r[5];
        r[7] = t0; r[4] = t3;
        t3 = r[6];
        r[6] = t1; r[5] = t3;
    }
    BrSegPtrFixup((uint32_t *)(void *)(r + 4));

    {
        uint8_t t0 = r[8], t3 = r[11], t1 = r[9];
        r[11] = t0; r[8] = t3;
        t3 = r[10];
        r[10] = t1; r[9] = t3;
    }

    *(uint16_t *)(r + 0x0C) = (uint16_t)(r[0x0D] | (r[0x0C] << 8));
    *(uint16_t *)(r + 0x0E) = (uint16_t)(r[0x0F] | (r[0x0E] << 8));
    *(uint16_t *)(r + 0x10) = (uint16_t)(r[0x11] | (r[0x10] << 8));
    *(uint16_t *)(r + 0x12) = (uint16_t)(r[0x13] | (r[0x12] << 8));
    *(uint16_t *)(r + 0x14) = (uint16_t)(r[0x15] | (r[0x14] << 8));
    *(uint16_t *)(r + 0x16) = (uint16_t)(r[0x17] | (r[0x16] << 8));
    /* 0x18..0x1F are deliberately left alone. */

    /* +0x20 is staged in a stack slot and reassembled from its bytes --
     * Horner form, matching the original's shl/or chain. */
    tmp = *(uint32_t *)(r + 0x20);
    flags = ((((((uint32_t)(uint8_t)tmp << 8)
             | ((const uint8_t *)&tmp)[1]) << 8)
             | ((const uint8_t *)&tmp)[2]) << 8)
             | ((const uint8_t *)&tmp)[3];
    *(uint32_t *)(r + 0x20) = flags;

    if (g_br675540 != 0) {
        if ((uint8_t)(flags >> 20) & 1) {
            uint8_t *mesh;
            uint32_t off0, off1;
            int      entry = 0;

            BrSegPtrFixup((uint32_t *)(void *)(r + 8));
            BrRcaSwapMesh(*(void **)(void *)(r + 8));
            mesh = *(uint8_t **)(void *)(r + 8);
            if (*(uint16_t *)(mesh + 2) == 2
                && *(uint32_t *)(mesh + 8) == 0xFFFFFFFFu)
                entry = 1;

            off1 = *(uint32_t *)(mesh + entry * 12 + 0x10);
            off0 = *(uint32_t *)(mesh + (entry + 1) * 12);

            if (entry == 0) {
                uint32_t n = *(uint32_t *)(r + 0x20) & 0x0003FFFFu;
                if (n != 0 && off0 != 0xFFFFFFFFu)
                    memcpy(*(void **)(void *)r, g_brRcaBlob + off0, n);
            }
            if (*(void **)(void *)(r + 4) != NULL
                && off1 != 0xFFFFFFFFu) {
                uint32_t len =
                    ((*(uint32_t *)(r + 0x20) & 0x0F000000u) == 0x01000000u)
                        ? 0x20u : 0x200u;
                memcpy(*(void **)(void *)(r + 4), g_brRcaBlob + off1, len);
            }
        } else {
            /* +0x08 is a 12-bit index scaled by 32 rather than a pointer. */
            uint32_t src = (*(uint32_t *)(r + 8) & 0xFFFu) << 5;
            if (*(void **)(void *)(r + 4) != NULL) {
                uint32_t len = ((flags & 0x0F000000u) == 0x01000000u)
                                   ? 0x20u : 0x200u;
                memcpy(*(void **)(void *)(r + 4), g_brRcaBlob + src, len);
            }
        }
    }
    BrGbiCall10075330(*(void **)(void *)(r + 4));
}


/* 0x1002BC90 */
/* WHAT IT DOES: turns a loaded mesh header the right way round: its entry
 * count, one following word, and then three words for each entry. Note the
 * entry count is re-read from the header on every pass of the loop, exactly
 * as the original does, so swapping it can change how many entries get
 * processed. */
/* @t4-pass 0x10018D50 1 2026-09-07 probes 58 bytes 180 insns 71 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018D50 2 2026-09-07 probes 58 bytes 180 insns 71 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018D50 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 180/180 insns 71/71 rows 0+0 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 5 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10018D50 glide BrRcaSwapMesh */
#ifdef BR_MATCHING_BUILD
void BrRcaSwapMesh(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;
    uint32_t v;
    uint8_t *e;

    if (p == NULL)
        return;

    *(uint16_t *)(p + 2) = (uint16_t)(p[3] | (p[2] << 8));
    v = (((((uint32_t)p[4] << 8) | p[5]) << 8) | p[6]) << 8 | p[7];
    *(uint32_t *)(p + 4) = v;
    if (*(unsigned short *)(p + 2) <= 0)
        return;

    e = p + 0xA;
    i = 0;
    do {
        v = (((((uint32_t)e[-2] << 8) | e[-1]) << 8) | e[0]) << 8 | e[1];
        *(uint32_t *)(e - 2) = v;
        v = (((((uint32_t)e[2] << 8) | e[3]) << 8) | e[4]) << 8 | e[5];
        *(uint32_t *)(e + 2) = v;
        v = (((((uint32_t)e[6] << 8) | e[7]) << 8) | e[8]) << 8 | e[9];
        *(uint32_t *)(e + 6) = v;
        e += 0xC;
        i += 1;
    } while (i < (int)*(unsigned short *)(p + 2));
}
#else
void BrRcaSwapMesh(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    uint32_t i;

    if (p == NULL)
        return;

    br16_swap_u16_at(p + 2);
    br16_swap_u32_at(p + 4);

    /* The count is re-read from +0x02 on every iteration, exactly as the
     * original does. */
    for (i = 0; i < (uint32_t)br16_ld16(p + 2); ++i) {
        uint8_t *e = p + 8 + i * 12;
        br16_swap_u32_at(e + 0);
        br16_swap_u32_at(e + 4);
        br16_swap_u32_at(e + 8);
    }
}
#endif
