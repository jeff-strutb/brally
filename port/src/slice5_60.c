/* slice5_60.c -- Boss Rally (BRD3D.dll) decompilation, a later pass, slice 5.
 *
 * See port/include/slice5_60.h for what is here, what is not, why, and the
 * signature/name conflicts this packet turned up.  Addresses in comments are
 * the original's and were checked against work/slice5/agent60.asm and
 * the asm/ banks; every table and literal was read out of orig/BRD3D.dll with
 * tools/pe.py rather than guessed.
 */
#include <stdio.h>
#include <string.h>

#include "slice5_60.h"

/* ====================================================================== */
/* Globals this file owns.  Every one carries its original address; see the
 * header for the ones another slice models as a struct field.            */
/* ====================================================================== */

uint32_t g_Br0A79D8;        /* 0x100A79D8 */
int32_t  g_Br0AA720;        /* 0x100AA720 */
uint32_t g_Br4BBE28;        /* 0x104BBE28 */
uint32_t g_Br4C16A0;        /* 0x104C16A0 */
int32_t  g_Br4C5184;        /* 0x104C5184 */

int32_t  g_Br6C6618;        /* 0x106C6618 */
int32_t  g_Br6C6620;        /* 0x106C6620 */

BrPtrList *g_pBrDlPtrList;  /* 0x1067B548 / 0x1067B550 */

int32_t  g_BrAA2844;        /* 0x10AA2844 */
int32_t  g_BrAA2BDC;        /* 0x10AA2BDC */
int32_t  g_BrAA2BE0;        /* 0x10AA2BE0 */
int32_t  g_BrAA2DAC;        /* 0x10AA2DAC */
int32_t  g_BrAA2DB4;        /* 0x10AA2DB4 */
uint16_t g_Br0AB3DC;        /* 0x100AB3DC */
uint16_t g_BrAA286C;        /* 0x10AA286C */
int32_t  g_BrAA33E8;        /* 0x10AA33E8 */
void    *g_pBrAA2A78;       /* 0x10AA2A78 */

uint8_t  g_aBr1782E28[BR_CFG_BUF_SIZE];   /* 0x11782E28 */
uint32_t g_Br0ADF58;        /* 0x100ADF58 */
uint32_t g_Br0ADF5C;        /* 0x100ADF5C */
uint32_t g_Br0ADF60;        /* 0x100ADF60 */

void *(*g_BrCarEquipTarget)(int32_t index);   /* see the header */

/* 0x100AA8B8 -- one record.  Bytes straight out of .rdata. */
const BrDlSubst4 g_aBrAA8B8[BR_DLSUB_AA8B8_COUNT] = {
    { 0xFC127E08u, 0xF3FFF2F8u, 0xFC26A004u, 0x1FFC93F8u }
};

/* 0x100AA8C8 -- two records, match-only. */
const BrGfxWords g_aBrAA8C8[BR_DLSUB_AA8C8_COUNT] = {
    { 0xFCFFA004u, 0xFFFD93F8u },
    { 0xFC50FE04u, 0x3FFDF3F8u }
};

/* ====================================================================== */
/* 1. 0x100243D0                                                          */
/* ====================================================================== */

BrGfxWords *BrGbiCall100243D0(BrGfxWords *pCmd)
{
    /* `add eax, 8` -- one 8-byte command. */
    return pCmd + 1;
}

/* ====================================================================== */
/* 2. 0x10020FA0                                                          */
/* ====================================================================== */

/* The one primitive every write in 0x10020FA0 goes through.  The original
 * inlines it eighteen times, always as
 *
 *      aPending[i] = v;  if (aShadow[i] == v) clear bit i else set bit i
 *
 * with the clear/set spelled as a byte-wide AND/OR of the running `dirty`
 * word held in eax.  `pDirty` is that register. */
static void BrRsSet(BrGbiRectState *pSt, uint32_t *pDirty,
                    unsigned i, uint32_t v)
{
    pSt->aPending[i] = v;
    if (pSt->aShadow[i] == v)
        *pDirty &= ~(1u << i);
    else
        *pDirty |= (1u << i);
}

void BrGbiCall10020FA0(uint32_t w1)
{
    BrGbiRectState *pSt = BrGbiRectGetState();
    uint32_t d;

    /* The prologue runs before `w1` is looked at.  Note that the shadow of
     * index 0 is read HERE and the value read is reused much later, on the
     * `w1 == 1` and `w1 == 0x0D1849D8` arms, which do their own compare
     * against this stale copy rather than re-reading. */
    uint32_t shadow0 = pSt->aShadow[0];

    d = pSt->dirty;
    pSt->aPending[0] = 1u;
    if (shadow0 == 1u)
        d &= ~(1u << 0);
    else
        d |= (1u << 0);

    if (w1 == 0x00504F50u || w1 == 4u) {
        /* 0x10020FD8 and 0x10021066 -- byte-identical but for the value
         * index 3 receives. */
        BrRsSet(pSt, &d, 6, 8u);
        g_Br4BBE28 = 7u;
        BrRsSet(pSt, &d, 7, 7u);

        if (g_Br4C5184 == 0) {
            BrRsSet(pSt, &d, 2, 5u);
            BrRsSet(pSt, &d, 3, (w1 == 4u) ? g_Br0A79D8 : 6u);
        }

        /* tail A (0x100210E3) */
        BrRsSet(pSt, &d, 1, 1u);
        BrRsSet(pSt, &d, 10, 1u);
        g_Br4C16A0 = 3u;
        BrRsSet(pSt, &d, 5, 3u);
        pSt->dirty = d;
        return;
    }

    if (w1 == 0x0C184240u || w1 == 0x00504240u) {
        /* 0x10021156 */
        BrRsSet(pSt, &d, 6, 8u);
        g_Br4BBE28 = 7u;
        BrRsSet(pSt, &d, 7, 7u);

        if (g_Br4C5184 == 0) {
            BrRsSet(pSt, &d, 2, 5u);
            BrRsSet(pSt, &d, 3, 6u);
        }

        /* 0x100211D2 -- the SHORT tail: index 5 and 0x104C16A0 are not
         * touched here, unlike tail A. */
        BrRsSet(pSt, &d, 1, 1u);
        BrRsSet(pSt, &d, 10, 1u);
        pSt->dirty = d;
        return;
    }

    if (w1 == 3u) {
        /* 0x10021208.  GOTCHA: returns WITHOUT storing `dirty`, so the bit
         * the prologue just recomputed for index 0 is thrown away while
         * aPending[0] keeps the 1 it was given. */
        g_Br4C5184 = 0;
        return;
    }

    if (w1 == 1u) {
        /* 0x1002121F */
        g_Br4C16A0 = 4u;
        BrRsSet(pSt, &d, 5, 4u);

        /* The compare is against the shadow copy taken in the prologue. */
        pSt->aPending[0] = 0u;
        if (shadow0 == 0u)
            d &= ~(1u << 0);
        else
            d |= (1u << 0);

        pSt->dirty = d;
        return;
    }

    if (w1 == 0u) {
        /* 0x1002126A */
        g_Br4C16A0 = 2u;
        BrRsSet(pSt, &d, 5, 2u);
        BrRsSet(pSt, &d, 1, 1u);
        BrRsSet(pSt, &d, 10, 0u);
        pSt->dirty = d;
        return;
    }

    if (w1 == 0x0D1849D8u) {
        /* 0x100212C7.  The 3 index 2 receives is `edx`, left over from the
         * `mov edx, 3` at 0x10021208 that the w1 == 3 test used. */
        BrRsSet(pSt, &d, 2, 3u);
        BrRsSet(pSt, &d, 3, 2u);

        pSt->aPending[0] = 0u;
        if (shadow0 == 0u)
            d &= ~(1u << 0);
        else
            d |= (1u << 0);

        BrRsSet(pSt, &d, 10, 1u);
        g_Br4C5184 = 1;
        pSt->dirty = d;
        return;
    }

    /* 0x10021344 -- `test ch, 0x18`, i.e. bits 0x1800 of the word. */
    if ((w1 & 0x00001800u) != 0u) {
        if ((w1 & 0x00010000u) != 0u) {
            /* 0x10021355.  GOTCHA: the second silent early exit -- `dirty`
             * is not written. */
            if (g_Br0AA720 == 0)
                return;

            BrRsSet(pSt, &d, 6, 0x80u);

            g_Br4BBE28 = 7u;
            pSt->aPending[7] = 7u;
            if (pSt->aShadow[7] == 7u) {
                d &= ~(1u << 7);
            } else {
                d |= (1u << 7);
            }
            /* Either way control reaches the same tail; the original just
             * takes two different routes into it. */
        } else {
            /* 0x100213BB */
            BrRsSet(pSt, &d, 6, 1u);
            g_Br4BBE28 = 7u;
            BrRsSet(pSt, &d, 7, 7u);

            if (g_Br4C5184 == 0) {
                BrRsSet(pSt, &d, 2, 5u);
                BrRsSet(pSt, &d, 3, 6u);
            }

            /* 0x10021430 -- present only on THIS branch; the 0x10000 branch
             * above jumps straight past it into the tail. */
            pSt->aPending[0] = 0u;
            if (shadow0 == 0u)
                d &= ~(1u << 0);
            else
                d |= (1u << 0);
        }

        /* tail T1/T2 (0x10021440, 0x10021456) */
        BrRsSet(pSt, &d, 1, 1u);
        BrRsSet(pSt, &d, 10, 1u);
        pSt->dirty = d;
        return;
    }

    /* 0x10021476 -- (w1 & 0x1800) == 0 */
    if (g_Br4C5184 == 0) {
        BrRsSet(pSt, &d, 2, 2u);
        BrRsSet(pSt, &d, 3, 1u);
    }
    g_Br4BBE28 = 8u;            /* 8, not 7, on this arm alone */
    BrRsSet(pSt, &d, 7, 8u);
    BrRsSet(pSt, &d, 1, 0u);
    BrRsSet(pSt, &d, 10, 0u);
    pSt->dirty = d;
}

/* ====================================================================== */
/* 3. 0x100341B3                                                          */
/* ====================================================================== */

int BrSub100341B3(uint32_t *pDl, const void *pTable)
{
    const unsigned char *pRecs = (const unsigned char *)pTable;
    int ret  = 0;   /* [ebp-0x14] */
    int sel;        /* [ebp-0x18], copied to [ebp-0x0C] */
    int col;        /* [ebp-0x04] */
    int flag = 0;   /* [ebp-0x08] */
    int i;          /* [ebp-0x10] */

    sel = (g_Br6C661C == 0 && g_Br6C6624 == 0) ? 1 : 0;

    /* `lea eax, [edx + ecx + 1]` with ecx = (sel == 0). */
    col = (int)g_Br6C6618 + ((sel == 0) ? 1 : 0) + 1;

    if (pDl == NULL)
        goto done;

    for (;;) {
        uint32_t op = (pDl[0] >> 24) & 0xFFu;

        /* `sub ecx, 0xB8` then an UNSIGNED `ja 0x44`, so 0xB8..0xFC only. */
        op -= 0xB8u;
        if (op > 0x44u)
            goto next;

        switch (op) {
        case 0x00u:                     /* 0xB8 G_ENDDL */
            goto done;

        case 0x01u:                     /* 0xB9 G_SETOTHERMODE_L */
            for (i = 0; i < 6; i++) {
                const unsigned char *pRec = pRecs + (size_t)i * 32;
                uint32_t m0, m1, n0, n1;

                memcpy(&m0, pRec + 0, sizeof m0);
                if (pDl[0] != m0)
                    continue;
                memcpy(&m1, pRec + 4, sizeof m1);
                if (pDl[1] != m1)
                    continue;

                memcpy(&n0, pRec + (size_t)col * 8 + 0, sizeof n0);
                memcpy(&n1, pRec + (size_t)col * 8 + 4, sizeof n1);
                pDl[0] = n0;
                pDl[1] = n1;

                if (i >= 3)
                    ret = 1;
                break;
            }
            goto next;

        case 0x44u:                     /* 0xFC G_SETCOMBINE */
            if (sel != 0) {
                for (i = 0; i < BR_DLSUB_AA8B8_COUNT; i++) {
                    if (pDl[0] == g_aBrAA8B8[i].matchW0 &&
                        pDl[1] == g_aBrAA8B8[i].matchW1) {
                        pDl[0] = g_aBrAA8B8[i].newW0;
                        pDl[1] = g_aBrAA8B8[i].newW1;
                        break;
                    }
                }
            }
            /* 0x10034342 */
            if (g_Br6C6620 != 0 && g_Br6C666C != 0) {
                for (i = 0; i < BR_DLSUB_AA8C8_COUNT; i++) {
                    if (pDl[0] == g_aBrAA8C8[i].w0 &&
                        pDl[1] == g_aBrAA8C8[i].w1)
                        break;
                }
                flag = (i < BR_DLSUB_AA8C8_COUNT) ? 1 : 0;
            }
            goto next;

        case 0x42u:                     /* 0xFA G_SETPRIMCOLOR */
            if (flag != 0 && g_Br6C6620 != 0)
                pDl[1] = BR_DL_PRIMCOLOR_FORCED;
            goto next;

        case 0x43u:                     /* 0xFB G_SETENVCOLOR */
            if (flag != 0 && g_Br6C6620 != 0)
                pDl[1] = BR_DL_ENVCOLOR_FORCED;
            goto next;

        default:
            goto next;
        }

    next:
        pDl += 2;
    }

done:
    g_Br6C666C = 0;
    return ret;
}

/* ====================================================================== */
/* 4. 0x1002BF80                                                          */
/* ====================================================================== */

/* Big-endian 32-bit load.  The original builds each word a byte at a time
 * through ah/al/cl/ch, never with a struct overlay -- see CONTRACT. */
static uint32_t BrLdBe32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void BrStHost32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

void BrDlRegister(void *pv)
{
    uint8_t *p = (uint8_t *)pv;

    if (p == NULL)
        return;

    /* 0x1002C1F0 -- append to the flat pointer list.  DEVIATION: guarded,
     * see the header. */
    if (g_pBrDlPtrList != NULL)
        BrPtrListAdd(g_pBrDlPtrList, pv);

    for (;;) {
        uint32_t w0 = BrLdBe32(p);
        uint32_t w1 = BrLdBe32(p + 4);
        uint32_t op;

        /* Both words are rewritten in host order BEFORE the dispatch, and
         * unconditionally -- G_ENDDL is byte-swapped too. */
        BrStHost32(p + 0, w0);
        BrStHost32(p + 4, w1);

        /* `sar eax, 0x18` then `and eax, 0xFF`, i.e. the top byte of the
         * SWAPPED w0, which is the original first byte.  Then `add eax,-4`
         * and an unsigned `ja 0xF9`: opcodes 0x04..0xFD. */
        op = (w0 >> 24) & 0xFFu;
        if ((uint32_t)(op - 4u) > 0xF9u)
            goto next;

        switch (op) {
        case 0x04u:                 /* G_VTX -> 0x1002C150 */
            /* DEVIATION: slice1_05.h's BrF3DVtxFixup is 0x1002C150 MINUS its
             * tail, which is
             *      BrVtxCacheResolve(cache, (void **)&w1, n)
             * i.e. the original stuffs a host vertex pointer back into the
             * 32-bit w1 slot.  slice1_05.h already declined to reproduce that
             * ("A host pointer does not fit in 32 bits, so that final step is
             * left to the caller").  This caller cannot do it either -- the
             * slot it would have to write is the display list's own 32-bit
             * word.  The fixup runs; the cache resolve does not. */
            (void)BrF3DVtxFixup(g_BrSegMap, (BrGfxWords *)p);
            break;

        case 0xB1u:                 /* G_TRI2 -> 0x1002C1B0 */
            BrF3DTri2Fixup(p);
            break;

        case 0xBFu:                 /* G_TRI1 -> 0x1002C190 */
            BrF3DTri1Fixup(p);
            break;

        case 0xB8u:                 /* G_ENDDL -- the only exit */
            return;

        case 0xFDu:                 /* G_SETTIMG -> 0x1002B970 on &w1 */
            BrSegFixup(g_BrSegMap, (uint32_t *)(p + 4));
            break;

        default:
            break;
        }

    next:
        p += 8;
    }
}

void BrSub1002BF80(uint32_t v)
{
    /* DEVIATION: the original's argument IS the pointer.  slice2_19.h keeps
     * these display-list addresses 32-bit and resolves them through
     * g_BrModelDeref; slice2_19.c:894 hands this function the very word it
     * has just run g_BrModelFixup over, so the same hook is the right one.
     * A NULL hook is treated as "not wired" and does nothing, rather than
     * crashing. */
    if (g_BrModelDeref == NULL)
        return;

    BrDlRegister(g_BrModelDeref(v));
}

/* ====================================================================== */
/* 5. 0x100603A0                                                          */
/* ====================================================================== */

/* One button's down/release edge.  The original writes this out four times
 * over consecutive field pairs; only the last copy has the tail. */
static void BrMouseButtonEdge(BrMouseState *pMs, int i, int32_t *pRaw)
{
    if (pMs->aBtn[i] != 0) {
        if (pMs->aDown[i] == 0) {
            /* GOTCHA: the press edge does NOT touch aRelease[i]. */
            pMs->aDown[i] = 1;
        } else {
            pMs->aRelease[i] = 0;
        }
    } else {
        if (pMs->aDown[i] != 0) {
            pMs->aDown[i]    = 0;
            pMs->aRelease[i] = 1;
            pMs->f4C         = 0;
            *pRaw            = 1;
        } else {
            pMs->aRelease[i] = 0;
        }
    }
}

void BrSub100603A0(void *pThis, void *pArg)
{
    BrMouseState *pMs = (BrMouseState *)pThis;
    BrMouseSample st;
    int32_t now, held;
    int32_t armed;
    int i;
    int fAnyBtn;

    /* The original never reads its stack argument.  Kept so the declared
     * shape (and the original's `ret 4`) survives. */
    (void)pArg;

    if (g_BrAA2844 != 0)
        return;

    now  = BrSub10075020();
    held = g_BrAA3398[BR_CURSOR_HELD_MS] +
           (now - g_BrAA3398[BR_CURSOR_LAST_MS]);
    g_BrAA3398[BR_CURSOR_LAST_MS] = now;
    g_BrAA3398[BR_CURSOR_HELD_MS] = held;
    if (held > BR_CURSOR_ARM_MS)
        g_BrAA3398[BR_CURSOR_ARMED] = 1;

    if (pMs->pDev == NULL)
        return;

    memset(&st, 0, sizeof st);

    /* GOTCHA: Acquire is called once here and then TWICE per retry. */
    (void)pMs->pDev->pVtbl->Acquire(pMs->pDev);
    if ((uint32_t)pMs->pDev->pVtbl->GetDeviceState(pMs->pDev,
                                                  BR_MOUSE_SAMPLE_SIZE, &st)
        == BR_DIERR_INPUTLOST) {
        for (;;) {
            if (pMs->pDev->pVtbl->Acquire(pMs->pDev) < 0)
                break;
            (void)pMs->pDev->pVtbl->Acquire(pMs->pDev);
            if ((uint32_t)pMs->pDev->pVtbl->GetDeviceState(
                    pMs->pDev, BR_MOUSE_SAMPLE_SIZE, &st)
                != BR_DIERR_INPUTLOST)
                break;
        }
    }

    /* Accumulate. The three adds happen before either clamp. */
    pMs->x += st.dx;
    pMs->y += st.dy;
    pMs->z += st.dz;             /* never clamped */

    if (pMs->x < 0)
        pMs->x = 0;
    else if (pMs->x >= g_BrAA33B8)
        pMs->x = g_BrAA33B8;

    if (pMs->y < 0)
        pMs->y = 0;
    else if (pMs->y >= g_BrAA33B4)
        pMs->y = g_BrAA33B4;

    fAnyBtn = 0;
    for (i = 0; i < BR_MOUSE_BTN_COUNT; i++) {
        pMs->aBtn[i] = (uint8_t)(st.aBtn[i] & 0x80u);
        if (pMs->aBtn[i] != 0)
            fAnyBtn = 1;
    }
    if (fAnyBtn)
        pMs->f4C = 1;

    if (g_BrAA2BDC != 0)
        (void)BrCdTrackPrev();       /* 0x10002930 */
    if (g_BrAA2BE0 != 0)
        (void)BrCdTrackNext();       /* 0x10002970 */

    /* `armed` is the snapshot the original keeps in eax; the two blocks
     * below can zero it, and the LAST block tests the snapshot, not the
     * global. */
    armed = g_BrAA3398[BR_CURSOR_ARMED];

    if (armed != 0) {
        /* bit 7 of the DIK byte = key down.  0xC8 = DIK_UP, 0xD0 = DIK_DOWN. */
        if ((g_BrDikState[BR_DIK_UP] & 0x80u) != 0) {
            g_BrAA3398[BR_CURSOR_UP_LATCH] = 1;
            g_Br0AB3DC = (uint16_t)0xFFFFu;          /* the -1 step */
            g_BrAA3398[BR_CURSOR_HELD_MS]  = 0;
        }
        if ((g_BrDikState[BR_DIK_DOWN] & 0x80u) != 0) {
            g_BrAA3398[BR_CURSOR_DOWN_LATCH] = 1;
            g_Br0AB3DC = (uint16_t)1u;
            g_BrAA3398[BR_CURSOR_HELD_MS]    = 0;
        }
    }

    /* 0x10060537.  `req` is 1 when 0x10AA2DAC fired this frame, otherwise
     * whatever 0x10AA33A0 holds. */
    {
        int32_t req;

        if (g_BrAA2DAC != 0) {
            req = 1;
            g_Br0AB3DC = (uint16_t)0xFFFFu;
            g_BrAA3398[BR_CURSOR_HELD_MS] = 0;
        } else {
            req = g_BrAA3398[BR_CURSOR_UP_REQ];
        }

        if (g_BrAA2DB4 != 0) {
            g_BrAA3398[BR_CURSOR_DOWN_REQ] = 1;
            g_Br0AB3DC = (uint16_t)1u;
            g_BrAA3398[BR_CURSOR_HELD_MS]  = 0;
        }

        if (req != 0) {
            armed = 0;
            pMs->aBtn[0] = 1;        /* `mov [esi+0x24], bl` with ebx == 1 */
            pMs->f4C     = 0;
            g_BrAA3398[BR_CURSOR_ARMED] = 0;
        }
    }

    if (g_BrAA3398[BR_CURSOR_DOWN_REQ] != 0) {
        armed = 0;
        pMs->aBtn[1] = 1;
        pMs->f4C     = 0;
        g_BrAA3398[BR_CURSOR_ARMED] = 0;
    }

    if (armed != 0) {
        if (g_BrAA3398[BR_CURSOR_UP_LATCH] != 0) {
            g_BrAA286C = (uint16_t)(g_BrAA286C - 1u);
            g_BrAA3398[BR_CURSOR_ARMED] = 0;
        }
        if (g_BrAA3398[BR_CURSOR_DOWN_LATCH] != 0) {
            g_BrAA286C = (uint16_t)(g_BrAA286C + 1u);
            g_BrAA3398[BR_CURSOR_ARMED] = 0;
        }
    }

    g_BrAA3398[BR_CURSOR_DOWN_REQ]   = 0;
    g_BrAA3398[BR_CURSOR_UP_REQ]     = 0;
    g_BrAA3398[BR_CURSOR_DOWN_LATCH] = 0;
    g_BrAA3398[BR_CURSOR_UP_LATCH]   = 0;

    /* Anything moved since last frame? */
    {
        int fChanged = 0;

        if (pMs->x != pMs->xPrev || pMs->y != pMs->yPrev)
            fChanged = 1;
        else
            for (i = 0; i < BR_MOUSE_BTN_COUNT; i++)
                if (pMs->aBtn[i] != pMs->aBtnPrev[i]) {
                    fChanged = 1;
                    break;
                }

        if (fChanged)
            g_BrAA33E8 = BrSub10075020();
    }

    pMs->xPrev = pMs->x;
    pMs->yPrev = pMs->y;
    for (i = 0; i < BR_MOUSE_BTN_COUNT; i++)
        pMs->aBtnPrev[i] = pMs->aBtn[i];

    for (i = 0; i < BR_MOUSE_BTN_COUNT; i++)
        g_BrBtnRaw[i] = 0;                    /* 0x10AA33C0..0x10AA33CC */

    for (i = 0; i < BR_MOUSE_BTN_COUNT; i++)
        BrMouseButtonEdge(pMs, i, &g_BrBtnRaw[i]);

    g_pBrAA2A78 = pMs;
    BrMenuSub1005FFF0();                      /* 0x1005FFF0 */
}

/* ====================================================================== */
/* 6. 0x10071130                                                          */
/* ====================================================================== */

void BrSub10071130(int a, int b)
{
    FILE *fp;
    const char *pszPath;
    size_t cbWant;
    size_t cbGot;

    if (a == 4) {
        (void)BrSub10070610(4, b);          /* return discarded: see header */
        return;
    }
    if (a == 0) {
        (void)BrSub10070610(0, b);
        return;
    }
    if (a == 1) {
        (void)BrSub10070E60(b);
        return;
    }

    if (a == 2 || a == 3) {
        pszPath = BR_CFG_PATH;              /* 0x100B5DD0 */
        cbWant  = BR_CFG_FULL_SIZE;         /* 0x100 */
    } else {
        /* DEVIATION: the original loads the SAME stack slot into both eax
         * (the fopen path) and esi (the byte count):
         *      mov eax,[esp+0x28] ; mov esi,[esp+0x28]
         * so one 32-bit value is used as a `const char *` and as a length at
         * once.  That is not expressible with the declared `int b`, and it
         * cannot be anything but a bug -- a valid path pointer used as a read
         * length would be ~0x10000000 bytes.  The port takes the path the
         * original takes when the open fails.  Recorded rather than guessed. */
        pszPath = NULL;
        cbWant  = 0;
    }

    fp = (pszPath != NULL) ? fopen(pszPath, BR_CFG_MODE) : NULL;  /* 0x1007CE90 */
    if (fp == NULL) {
        /* return ((unsigned)b & 0xFF) != 0 -- the LOW BYTE only.  Discarded
         * by the declared `void` signature. */
        return;
    }

    if (a == 2)
        cbWant = BR_CFG_REC_SIZE;           /* 0x80 */

    /* DEVIATION (memory safety): the original's default arm reads an
     * unbounded count into a fixed global.  Clamped here; with the arm above
     * unreachable this is a no-op in practice. */
    if (cbWant > sizeof g_aBr1782E28)
        cbWant = sizeof g_aBr1782E28;

    cbGot = fread(g_aBr1782E28, 1, cbWant, fp);   /* 0x1007CF10 */
    if (cbGot != cbWant) {
        fclose(fp);                               /* 0x1007CD50 */
        return;                                   /* returned 0 */
    }

    if (a != 2) {
        fclose(fp);
        return;                                   /* returned 1 */
    }

    /* --- mode 2 only ------------------------------------------------- */
    memcpy(&g_Br0ADF58, g_aBr1782E28 + 0, sizeof g_Br0ADF58);
    memcpy(&g_Br0ADF5C, g_aBr1782E28 + 4, sizeof g_Br0ADF5C);
    memcpy(&g_Br0ADF60, g_aBr1782E28 + 8, sizeof g_Br0ADF60);

    BrStub8B80_1p("Loading car equipment se");   /* 0x100B5DAC, a bare ret */

    cbGot = fread(g_aBr1782E28, 1, (size_t)BR_CFG_REC_SIZE, fp);
    if (cbGot != (size_t)BR_CFG_REC_SIZE) {
        fclose(fp);
        return;                                   /* returned 0 */
    }

    {
        uint32_t aRec[BR_CAR_EQUIP_COUNT];        /* the `rep movsd` of 5 */
        void *pDst;

        memcpy(aRec, g_aBr1782E28, sizeof aRec);

        pDst = (g_BrCarEquipTarget != NULL) ? g_BrCarEquipTarget(g_br690A18)
                                            : NULL;
        if (pDst != NULL) {
            int k;
            for (k = 0; k < BR_CAR_EQUIP_COUNT; k++)
                memcpy((unsigned char *)pDst + BR_CAR_EQUIP_OFF + 4 * k,
                       &aRec[k], sizeof aRec[k]);
        }
        /* GOTCHA: the original re-reads 0x10690A18 and redoes the whole
         * 0x2B68 multiply before EACH of the five stores.  Hoisted, because
         * nothing between them can change it. */
    }

    BrStub8B80_1p("Done!\n");                     /* 0x100B5DA4, a bare ret */
    fclose(fp);
    /* returned 1 */
}

/* ======================================================================
 * NOT DONE, AND WHY -- kept here so the analysis is not lost.
 * ======================================================================
 *
 * 0x10044970  BrOptFn10044970 (wanted by slice2_25.h:474)
 *     ALREADY IMPLEMENTED.  port/src/slice2_26.c has this exact body under
 *     the name BrPhaseLeave_10044970(BrPhaseCtx *, void *), and
 *     slice2_26.h:362 documents it down to the "re-reads nAA287C after
 *     BrExt_1003C020" detail.  Duplicating it is forbidden by the contract.
 *     For the record, the body traced from this packet's listing agrees with
 *     that description exactly:
 *         if (nA9D000) { entity->pSub->vtbl[+0x18](sub, 0); BrExt_10038F30(0); }
 *         entity->pSub->vtbl[+0x1C](sub);
 *         if (pAA2904) pAA2904->vtbl[0](pAA2904, 1);
 *         nAA2950 = 0; pAA2904 = pAA2948;
 *         if (pAA29D8) pAA29D8->f1C &= ~0x10;
 *         BrExt_1003BF60(); nAA2898 = 1;
 *         if ((nAA287C == 0 || nAA287C == 1) && nA9D000 == 0) {
 *             BrExt_1003C020(); nAA287C = <re-read>;
 *         }
 *         if (nAA287C == 2 || nAA287C == 3)
 *             if (pAA29D8) pAA29D8->f1C &= ~0x10;
 *         return 0;
 *     NOTE the argument is an ENTITY record (+0x2AE8 is its sub-object), not
 *     a BrOptObj -- slice2_25.h's `BrOptObjFn` typing of the +0x08 slot is
 *     the wrong shape, and slice2_26.h's `void (*)(void *pEntity)` is right.
 *
 * 0x1003C020  BrSub1003C020 (slice2_25/4_50/4_53) and BrExt_1003C020
 *             (slice2_26).  ONE ADDRESS, TWO WANTED NAMES; both listings in
 *             the packet are byte-identical, so this is a naming duplicate
 *             and not a generator mispairing.
 *     Not portable.  294 bytes of which the load-bearing half is Win32 and
 *     COM: USER32 KillTimer / SetTimer (1 s, id 1) around a KERNEL32
 *     CreateEventA, and an IDirectPlay4 call through vtable byte offset
 *     +0x98 with (this, pv, 0).  It also depends on 0x1003C550, 0x1003D480
 *     (two out-parameters), 0x1003C520 and 0x1003CC70, none of which is
 *     modelled.  Its failure path formats "Could not select service provider
 *     because of error 0x%08X" and its success sentinel is the DirectPlay
 *     HRESULT 0x88770118, which is compared against for EQUALITY -- so that
 *     one error is silently swallowed.  Recommend integration stub it.
 *
 * 0x10004E50  BrNetSendDelta (slice2_11.h:180)
 *     Not portable, and not recoverable either.  It takes both the global
 *     mutex 0x1022AF34 and the per-player one at the head of a 0x978-stride
 *     record (base 0x10221328, index 0x10094294) with a single
 *     WaitForMultipleObjects(2, h, TRUE, INFINITE), pushes a 160-byte state
 *     snapshot into an 8-deep ring inside that record (`rep movsd` of 0x28
 *     dwords to +0x58 + 160*i, with the tag 0x80 at +0x38 + 4*i and a
 *     timestamp from 0x10003460 at +0x0C + 4*i), releases both, and then
 *     builds a message on the stack through a class this port does not model
 *     (ctor 0x10073B40, append-byte 0x10073D60 with `playerId | 0x80`,
 *     encode 0x10006830 taking (msg, pState, pRef), send 0x10004DD0 taking
 *     (pDPlay, msg)) whose result is compared against -1.  Returns 0 on -1,
 *     1 otherwise.  Four unmodelled callees plus two Win32 sync primitives is
 *     past the point where a port would be invention.
 *
 * 0x10056FF0, 0x10049F40, 0x1004F700, 0x10053CF0, 0x1004D640
 *     The five menu-screen constructors.  See the long note in slice5_60.h:
 *     the bodies are transcribable (slice3_33.c did five of the same family)
 *     but the WANTED parameter types -- slice2_26.h's BrPhase and
 *     slice2_25.h's BrOptObj -- model a different, incompatible view of the
 *     same 0xC8-byte object and have none of the fields these bodies touch.
 *     slice3_32.c, slice4_51.c and slice5_61.h declined the same family, and
 *     slice3_33.h flags the layout clash itself.  One merged phase layout
 *     unblocks all five; guessing one here would be undetectable at link
 *     time and wrong at every field access.
 */
