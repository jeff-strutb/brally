/* br_texhalf.c -- 0x10023D70, half-res BMP substitute for a texture request.
 *
 * Called from FUN_10027710 (the Glide make path) when a filled BrTexReq272
 * wants a half-resolution stand-in.  Matching build only.
 *
 * RESIDUE 2026-09-12: 1328/1377 B.  The orig inlines dword/byte copy
 * loops where this file calls import memcpy; malloc is the /MD import.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern int DAT_118ed1a0;
extern int DAT_10ac67c0;
extern int DAT_10ac67a4;
extern int DAT_100b8498;
extern int DAT_1186c988;
extern int DAT_105e1828;

int  FUN_10059fe0(int, int, int);
int  FUN_1005a020(int, int, int *, int *, int *, int *);
int  FUN_1005a500(int, int, int, int, int, void *, int, int);
int  FUN_1005a630(int, int, int, int, int);
int  FUN_1005a070(void);
int  FUN_100242e0(void *, int, int);
int  FUN_10023cb0(void *, void *, int);
int  FUN_10024490(void *, int, int, void *, int, int, int);

/* WHAT IT DOES: when the Glide texture path asks for a half-resolution
 * stand-in of a just-closed tile run, fetch the BMP from the texture cache,
 * optionally double the request's w/h (and the two caller-supplied scale
 * outs), convert into the 0x1186C988 scratch, and copy the pixels into a
 * malloc'd buffer hung off the request.  Four more mip scratch slots at
 * request+0x27C..+0x288 are filled the same way when a cache slot is free.
 * Returns 1 on success (including a missing BMP, which just leaves the
 * request unmarked), 0 if the backend is not the BMP path. */
/* RESIDUE (2026-09-20): 1345/1377 B, 411/415 insns, 12 regions, regnorm 7+11.
 * Routing the two early return-0 guards through a shared `goto fail` (the
 * original shares one epilogue rather than inlining each early exit) cut the
 * gap from 13+10 to 7+11.  What is left is codegen only: the zero constant
 * lives in ebp in the original and esi here (cascades to the cmp-against-0 and
 * store forms), the shared fail block is placed at the function tail in the
 * original and near entry here (je-vs-jne guard polarity), and the request
 * doublings are `shl` in the original vs `add r,r` here (`*2` and `<<1` both
 * fold to add, unmoved).  A5 oracle EQUIVALENT. */
/* @t4-pass 0x10023D70 1 2026-09-20 probes 14 bytes 1345 insns 411 regions 12 rows 18 census yes  (goto-fail shared exit landed 13+10->7+11; then *2-vs-<<1 doubling moved nothing) */
/* @t4-pass 0x10023D70 2 2026-09-20 probes 10 bytes 1345 insns 411 regions 12 rows 18 census no   (baseline reconfirm; zero-register ebp/esi + fail-block placement are codegen, not source-driven) */
/* @t3 0x10023D70 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1345/1377 insns 411/415 rows 11+7 regions 12 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation + block placement only (see RESIDUE above).
 * A5 oracle EQUIVALENT is the completeness proof (rule 12).  Do not reopen
 * before the end-grind. */
/* @implements 0x10023D70 glide FUN_10023d70 */
int FUN_10023d70(int *pOutA, int *pOutB, int *pReq)
{
    int x0, y0, x1, x2;
    int hBmp;
    int wPow, hPow;
    int i;
    int *pSlot;
    void *pBuf;
    uint32_t cb;

    pReq[0x268 / 4] = 0;
    if (DAT_118ed1a0 != 2)
        goto fail;

    DAT_10ac67c0 = DAT_10ac67c0 + 1;
    hBmp = FUN_10059fe0(DAT_10ac67a4, DAT_10ac67c0, 0);
    if (hBmp == 0)
        goto fail;

    FUN_1005a020(DAT_10ac67a4, DAT_10ac67c0, &x0, &y0, &x1, &x2);
    if (x0 == 0 && y0 == 0 && x1 == 0 && x2 == 0) {
        x1 = 0;
        y0 = pReq[0x2a0 / 4] << 1;
        x2 = pReq[0x2a4 / 4] << 1;
        x0 = 0;
    }

    wPow = pReq[0x2a0 / 4];
    hPow = pReq[0x2a4 / 4];
    if (wPow == pReq[8 / 4] && hPow == pReq[0xc / 4] && DAT_100b8498 <= 0) {
        if (FUN_1005a500(hBmp, x0, y0, x1, x2, &DAT_1186c988,
                         wPow * 2, hPow * 2) == 0)
            return 1;
        hPow = pReq[0xc / 4] << 1;
        wPow = pReq[8 / 4] << 1;
        pReq[8 / 4] = wPow;
        pReq[0xc / 4] = hPow;
        pReq[0x3c / 4] = pReq[0x3c / 4] << 2;
        FUN_100242e0((void *)((uint8_t *)pReq + 0x1c), wPow, hPow);
        pReq[0x18 / 4] = pReq[0x1c / 4];
        *pOutA = *pOutA << 1;
        *pOutB = *pOutB << 1;
        pReq[0x268 / 4] = 1;
        pReq[0x26c / 4] = DAT_10ac67a4;
        pReq[0x270 / 4] = DAT_10ac67c0;
        pReq[0x274 / 4] = 0;
        pReq[0x278 / 4] = FUN_1005a630(hBmp, x0, y0, x1, x2);
        FUN_10023cb0(&DAT_1186c988, &DAT_1186c988,
                     pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 0x10);
        pReq[0x28c / 4] = pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 8;
        if (FUN_1005a070() >= 0) {
            pSlot = pReq + 0x288 / 4;
            i = 3;
            do {
                int h = FUN_10059fe0(DAT_10ac67a4, DAT_10ac67c0, i);
                if (h == 0) {
                    *pSlot = 0;
                } else {
                    if (FUN_1005a500(h, x0, y0, x1, x2, &DAT_1186c988,
                                     pReq[0x2a0 / 4] << 1,
                                     pReq[0x2a4 / 4] << 1) != 0) {
                        FUN_10023cb0(&DAT_1186c988, &DAT_1186c988,
                                     pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 0x10);
                        pReq[0x28c / 4] =
                            pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 8;
                    }
                    pBuf = malloc((uint32_t)pReq[0x28c / 4]);
                    *pSlot = (int)pBuf;
                    memcpy(pBuf, &DAT_1186c988, (uint32_t)pReq[0x28c / 4]);
                }
                i = i - 1;
                pSlot = pSlot - 1;
            } while (i >= 0);
            return 1;
        }
        pSlot = pReq + 0x288 / 4;
        i = 3;
        do {
            *pSlot = 0;
            pSlot = pSlot - 1;
            i = i - 1;
        } while (i != 0);
    } else {
        if (FUN_1005a500(hBmp, x0, y0, x1, x2, &DAT_105e1828,
                         wPow << 1, pReq[0x2a4 / 4] << 1) == 0)
            return 1;
        pReq[0x268 / 4] = 1;
        pReq[0x26c / 4] = DAT_10ac67a4;
        pReq[0x270 / 4] = DAT_10ac67c0;
        pReq[0x274 / 4] = 0;
        pReq[0x278 / 4] = FUN_1005a630(hBmp, x0, y0, x1, x2);
        FUN_10023cb0(&DAT_105e1828, &DAT_105e1828,
                     pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 0x10);
        pReq[0x28c / 4] = pReq[0xc / 4] * pReq[8 / 4] * 2;
        FUN_10024490(&DAT_1186c988, pReq[8 / 4], pReq[0xc / 4], &DAT_105e1828,
                     pReq[0x2a0 / 4] << 1, pReq[0x2a4 / 4] << 1, pReq[0x10 / 4]);
        if (FUN_1005a070() >= 0) {
            pSlot = pReq + 0x288 / 4;
            i = 3;
            do {
                int h = FUN_10059fe0(DAT_10ac67a4, DAT_10ac67c0, i);
                if (h == 0) {
                    *pSlot = 0;
                } else {
                    if (FUN_1005a500(h, x0, y0, x1, x2, &DAT_105e1828,
                                     pReq[0x2a0 / 4] << 1,
                                     pReq[0x2a4 / 4] << 1) != 0) {
                        FUN_10023cb0(&DAT_105e1828, &DAT_105e1828,
                                     pReq[0x2a4 / 4] * pReq[0x2a0 / 4] * 0x10);
                        pReq[0x28c / 4] = pReq[0xc / 4] * pReq[8 / 4] * 2;
                        FUN_10024490(&DAT_1186c988, pReq[8 / 4], pReq[0xc / 4],
                                     &DAT_105e1828, pReq[0x2a0 / 4] << 1,
                                     pReq[0x2a4 / 4] << 1, pReq[0x10 / 4]);
                    }
                    pBuf = malloc((uint32_t)pReq[0x28c / 4]);
                    *pSlot = (int)pBuf;
                    memcpy(pBuf, &DAT_1186c988, (uint32_t)pReq[0x28c / 4]);
                }
                i = i - 1;
                pSlot = pSlot - 1;
            } while (i >= 0);
            return 1;
        }
        pSlot = pReq + 0x288 / 4;
        i = 3;
        do {
            *pSlot = 0;
            pSlot = pSlot - 1;
            i = i - 1;
        } while (i != 0);
    }

    pBuf = malloc((uint32_t)pReq[0x28c / 4]);
    pReq[0x27c / 4] = (int)pBuf;
    memcpy(pBuf, &DAT_1186c988, (uint32_t)pReq[0x28c / 4]);
    return 1;
fail:
    return 0;
}

#endif /* BR_MATCHING_BUILD */
