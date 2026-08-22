/* slice5_62.c -- a later pass, slice 5.  See port/include/slice5_62.h for what
 * this module is, which caller declares each name, and every GOTCHA.
 *
 * Each function below is annotated with the original address it came from.
 */
#include "slice5_62.h"

#include <stdio.h>
#include <string.h>

#include "br_crt.h"     /* BrFtolTrunc (0x1007C8A0) */
#include "slice1_02.h"  /* BrNetState, BrNetMutexLock/Unlock,
                         * BrNetSlotGetF02C (0x10004A10) */
#include "slice4_50.h"  /* BrNetSend4760 (0x10004760) and the four globals
                         * that call already needs */

/* ==================================================================== */
/* 1. 0x100419D0                                                        */
/* ==================================================================== */

static BrX419D0State s_x419D0;

BrX419D0State *BrX419D0GetState(void)
{
    return &s_x419D0;
}

/* WHAT IT DOES: PURPOSE UNKNOWN. Observably it picks one object out of a
 * table -- which one is decided by a separate global index -- and calls one
 * particular routine on it, passing the caller's argument along with three
 * fixed values. Neither the table nor the object has a recoverable type, so
 * what the call is FOR cannot be stated. If the chosen slot is empty it does
 * nothing. */
/* @implements 0x100419D0 d3d BrExt_100419D0 */
void BrExt_100419D0(void *p)
{
    BrX419D0State  *pSt = &s_x419D0;
    BrX419D0Table  *pTable;
    BrX419D0Target *pTarget;

    /* DEVIATION: the original dereferences 0x10AA2904 and its +0x14 with no
     * check at all.  Two NULL guards added; the third (the selected slot) is
     * the original's own and is the only reason this function can be a
     * no-op. */
    if (pSt->pOwner == NULL) {
        return;
    }
    pTable = pSt->pOwner->pTable;
    if (pTable == NULL) {
        return;
    }

    pTarget = pTable->apObj[pSt->index];
    if (pTarget != NULL) {
        pTarget->apfn[BR_X419D0_VTBL_SLOT](pTarget, p, 1, 1, pSt->pvAB558);
    }
}

/* ==================================================================== */
/* 2. 0x1005FCF0                                                        */
/* ==================================================================== */

/* Verbatim from the .data image at 0x100B3820. */
const uint8_t g_brB3820[BR_SESS_TABLE_BYTES] = {
    0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x07, 0x50, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x04, 0x02, 0x00, 0x00, 0xE1, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x04, 0x06, 0x0C, 0x00,
    0x04, 0x01, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, 0xE2, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x02, 0x20, 0x00,
    0x04, 0x03, 0x01, 0x01, 0x02, 0x04, 0x00, 0x02, 0xE3, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x05, 0x20, 0x00,
    0x02, 0x03, 0x00, 0x04, 0x0A, 0x03, 0x01, 0x02, 0xE4, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x05, 0x05, 0x20, 0x00,
    0x06, 0x01, 0x02, 0x02, 0x01, 0x04, 0x0A, 0x02, 0x17, 0xD9, 0x7E, 0x3F,
    0xEE, 0x7C, 0x7F, 0x3F, 0x17, 0xD9, 0x7E, 0x3F, 0xEE, 0x7C, 0x7F, 0x3F,
    0x17, 0xD9, 0x7E, 0x3F, 0xEE, 0x7C, 0x7F, 0x3F, 0x17, 0xD9, 0x7E, 0x3F,
    0xEE, 0x7C, 0x7F, 0x3F, 0x49, 0x2E, 0x7F, 0x3F, 0x84, 0x0D, 0x7F, 0x3F,
    0xA0, 0x1A, 0x7F, 0x3F, 0x49, 0x2E, 0x7F, 0x3F, 0xA0, 0x1A, 0x7F, 0x3F,
    0x17, 0xD9, 0x7E, 0x3F, 0x17, 0xD9, 0x7E, 0x3F, 0xB2, 0x9D, 0x7F, 0x3F,
    0x52, 0xB8, 0x7E, 0x3F, 0x9B, 0x55, 0x7F, 0x3F, 0xE0, 0xBE, 0x7E, 0x3F,
    0x44, 0x69, 0x7F, 0x3F, 0x04, 0x56, 0x7E, 0x3F, 0x64, 0x3B, 0x7F, 0x3F,
    0x04, 0x56, 0x7E, 0x3F, 0xD7, 0x34, 0x7F, 0x3F, 0x3F, 0x35, 0x7E, 0x3F,
    0x89, 0xD2, 0x7E, 0x3F, 0xC9, 0x76, 0x7E, 0x3F, 0xBB, 0x27, 0x7F, 0x3F,
    0x52, 0xB8, 0x7E, 0x3F, 0xD2, 0x6F, 0x7F, 0x3F, 0xAD, 0x69, 0x7E, 0x3F,
    0x64, 0x3B, 0x7F, 0x3F
};

static BrSessLatch s_sess;

BrSessLatch *BrSessLatchGetState(void)
{
    return &s_sess;
}

void BrSub1005FCF0(void)
{
    BrSessLatch *p = &s_sess;

    p->fAA27EC = p->f094354;
    p->fAA27F0 = p->f09435C;
    p->fAA26F0 = p->fAA28A0;
    p->fAA27F4 = p->f094358;
    p->fAA26F4 = (uint8_t)p->fAA28B8;
    p->fAA27F8 = p->fB4E1D0;
    p->fAA26F5 = (uint8_t)p->fAA28A4;

    if (p->f0AA010 == 0) {
        uint32_t a = (uint32_t)p->fAA28B8 & 0xFFu;
        uint32_t c = (uint32_t)p->fAA28A4 & 0xFFu;
        uint32_t i = 2u * (12u * a + c);

        /* DEVIATION: the original indexes 0x100B3820 with no bound at all.
         * The snapshot here is 256 bytes; past that the two destinations are
         * left unchanged rather than reading whatever the host happens to
         * have there.  In range the bytes are the image's own. */
        if (i + 1u < (uint32_t)BR_SESS_TABLE_BYTES) {
            p->f0B380C = g_brB3820[i];
            p->f22B350 = g_brB3820[i + 1u];
        }
    }

    /* Both halves of the SAME dword -- see the header. */
    p->fAA2A10 |= p->fAA27E0 & 0xFFFFu;
    p->fAA2A14 |= (p->fAA27E0 >> 16) & 0xFFFFu;
}

/* ==================================================================== */
/* 3. 0x1001E7C0 and 0x1001C820                                         */
/* ==================================================================== */

static BrRasterSel s_raster;

BrRasterSel *BrRasterSelGetState(void)
{
    return &s_raster;
}

/* aPending[i] = value; then dirty bit (1 << i) is CLEARED when the shadow
 * already matches and SET when it does not.  Factored out because 0x1001E7C0
 * open-codes it four times with different constants. */
static void BrRasterSetPending(BrRasterSel *p, int i, uint32_t value)
{
    uint32_t bit = 1u << i;

    if (p->aShadow[i] == value) {
        p->dirty &= ~bit;
    } else {
        p->dirty |= bit;
    }
    p->aPending[i] = value;
}

void BrGbiGeoModeChanged(void)   /* 0x1001E7C0 */
{
    BrRasterSel *p     = &s_raster;
    uint32_t     delta = p->geoPrev ^ p->geoCur;
    uint32_t     cur   = p->geoCur;

    /* The original writes aPending[4] BEFORE comparing the shadow; the order
     * cannot matter because they are different words, but it is preserved in
     * BrRasterSetPending's comment above rather than in its body. */
    if ((delta & (BR_GEO_BIT12 | BR_GEO_BIT13)) != 0) {
        if ((cur & BR_GEO_BIT12) != 0) {
            BrRasterSetPending(p, 4, 2);
        } else if ((cur & BR_GEO_BIT13) != 0) {
            BrRasterSetPending(p, 4, 3);
        } else {
            BrRasterSetPending(p, 4, 1);
        }
    }

    if ((delta & BR_GEO_BIT0) != 0) {
        uint32_t v = ((cur & BR_GEO_BIT0) != 0) ? 2u : 8u;

        p->f4C16A0 = v;
        BrRasterSetPending(p, 5, v);
    }

    if ((cur & BR_GEO_BIT0) != 0) {
        if ((cur & BR_GEO_BIT17) != 0) {
            p->pfn0A7A00 = (p->f4C0DC0 != 0) ? BR_FN_A00_10022480
                                             : BR_FN_A00_10021E80;
        } else {
            p->pfn0A7A00 = BR_FN_A00_10021BD0;
        }
        /* Overrides whatever the 0x20000 test just chose. */
        if ((cur & BR_GEO_BIT18) != 0) {
            p->pfn0A7A00 = BR_FN_A00_100231D0;
            if ((cur & BR_GEO_BIT19) == 0) {
                p->pfn0A7A00 = BR_FN_A00_100228F0;
            }
        }

        if ((cur & BR_GEO_BIT9) != 0) {
            p->pfn0A7CEC = BR_FN_CEC_1001CFF0;
            p->pfn0A7CB4 = BR_FN_CB4_1001E170;
        } else {
            p->pfn0A7CEC = BR_FN_CEC_1001E980;
            p->pfn0A7CB4 = BR_FN_CB4_10020380;
        }
    } else {
        p->pfn0A7A00 = BR_FN_A00_10023CC0;
        if ((cur & BR_GEO_BIT17) == 0) {
            p->pfn0A7A00 = BR_FN_A00_10023A10;
        }

        if ((cur & BR_GEO_BIT9) != 0) {
            p->pfn0A7CEC = BR_FN_CEC_1001FBE0;
            p->pfn0A7CB4 = BR_FN_CB4_10020860;
        } else {
            p->pfn0A7CEC = BR_FN_CEC_1001F2B0;
            p->pfn0A7CB4 = BR_FN_CB4_100205F0;
        }
    }
}

void BrSub_1001C820(uint32_t w0, uint32_t w1)   /* 0x1001C820 */
{
    BrRasterSel *p    = &s_raster;
    uint32_t     flag = 0;   /* edx: the value that ends up in f4C0DC0 */

    p->f4C0DC0 = 0;

    if (w0 == 0xFCFFFFFFu && w1 == 0xFFFCF87Cu) {
        p->f4C0BB4 = 0xFFFFFFFFu;
        goto tail_9EC_CA;
    }
    if (w0 == 0xFCFFFFFFu && w1 == 0xFFFE793Cu) {
        goto tail_9EC_C690_or_BC90;
    }
    if (w0 == 0xFC567EACu && w1 == 0xFFFFF3F9u) {
        goto tail_9EC_CA_via_FF000000;
    }
    if (w0 == 0xFCFF97FFu && w1 == 0xFF2DFEFFu) {
        goto tail_9EC_C690_or_BC90;
    }
    if (w0 == 0xFCFFFFFFu) {
        if (w1 == 0xFFFDF2F9u) {
            goto tail_9EC_C690_or_BC90;
        }
        if (w1 == 0xFFFF73B9u) {
            p->f4C0BB4 = 0xFFFFFFFFu;
            goto tail_9EC_CA;
        }
    }
    if (w0 == 0xFC127E08u && w1 == 0xF3FFF2F8u) {
        if (p->f6C661C == 0 && p->f6C6624 == 0) {
            /* pfn0A79EC is deliberately left ALONE on this path. */
            goto tail_a7a00;
        }
        goto tail_9EC_C690_or_BC90;
    }
    if (w0 == 0xFC317E02u && (w1 == 0x5FFEF3FAu || w1 == 0x51FEF3FAu)) {
        p->pfn0A79EC = (p->f6C6618 != 0) ? BR_FN_9EC_1001C690
                                         : BR_FN_9EC_1001BC90;
        flag = 1;
        p->f4C0DC0 = 1;
        goto tail_a7a00;
    }
    if (w0 == 0xFC127FFFu && w1 == 0xFFFFF838u) {
        goto tail_9EC_CA_via_FF000000;
    }

    /* No recognised pair. */
    goto tail_9EC_C690_or_BC90;

tail_9EC_CA_via_FF000000:
    p->f4C0BB4 = 0xFF000000u;
tail_9EC_CA:
    p->pfn0A79EC = (p->f6C6618 != 0) ? BR_FN_9EC_1001CA90 : BR_FN_9EC_1001CA10;
    goto tail_a7a00;

tail_9EC_C690_or_BC90:
    p->pfn0A79EC = (p->f6C6618 != 0) ? BR_FN_9EC_1001C690 : BR_FN_9EC_1001BC90;

tail_a7a00:
    if (flag != 0) {
        if (p->pfn0A7A00 == BR_FN_A00_10021E80) {
            p->pfn0A7A00 = BR_FN_A00_10022480;
        }
    } else {
        if (p->pfn0A7A00 == BR_FN_A00_10022480) {
            p->pfn0A7A00 = BR_FN_A00_10021E80;
        }
    }
}

/* ==================================================================== */
/* 4. 0x100020D0                                                        */
/* ==================================================================== */

/* WHAT IT DOES: turns a number of seconds into a lap or race time the player
 * can read -- minutes, seconds and hundredths, as in "1:23.45". It does not
 * cope with a negative or nonsensical time; those come out as garbled text
 * rather than being clamped. */
void BrSub_100020D0(char *pszOut, float v)
{
    /* 0x1008F098 = 100.0f, read from the image. */
    int32_t total = BrFtolTrunc(v * 100.0f);
    int32_t cs;         /* whole centiseconds -> whole seconds */
    int32_t hundredths;
    int32_t minutes;
    int32_t seconds;
    char    szTmp[64];

    /* All four are the original's magic-multiply divisions, which round
     * toward zero exactly as C99's `/` does. */
    cs         = total / 100;
    hundredths = total - cs * 100;
    minutes    = cs / 60;
    seconds    = cs - minutes * 60;

    /* DEVIATION: the original sprintf()s straight into the caller's buffer.
     * The format is the image's own literal at 0x10094094 and the widest
     * possible result is 18 bytes, so the intermediate below is bounded and
     * the bytes handed to the caller are identical. */
    (void)snprintf(szTmp, sizeof szTmp, "%d:%02d.%02d",
                   (int)minutes, (int)seconds, (int)hundredths);
    memcpy(pszOut, szTmp, strlen(szTmp) + 1u);
}

/* ==================================================================== */
/* 5. 0x1003289F                                                        */
/* ==================================================================== */

static BrScissorClamp s_scissor;

BrScissorClamp *BrScissorClampGetState(void)
{
    return &s_scissor;
}

/* fild -> fstp dword -> fld dword -> fmul 1.0f -> __ftol.
 * 0x1008F4EC is 1.0f in the shipped DLL, so the only effect is the trip
 * through float32.  Kept because that trip is lossy above 2^24. */
static int32_t BrScissorPack(int32_t v)
{
    return BrFtolTrunc((float)v * 1.0f);
}

/* WHAT IT DOES: restricts drawing to a rectangle of the screen, so anything
 * drawn afterwards is trimmed to that region -- how the game keeps split
 * screen halves and mirror insets from spilling over each other. The
 * rectangle is trimmed to the screen first, but only its size is trimmed at
 * the far edges, so a rectangle entirely off-screen still emits a zero-size
 * one rather than being dropped, and an optional doubling is applied
 * afterwards and can push it back outside. */
/* @implements 0x1003289F d3d BrSub_1003289F */
void BrSub_1003289F(int a, int b, int c, int d)
{
    BrScissorClamp *p = &s_scissor;
    uint32_t       *pCmd;

    if (a < p->minX) {
        c -= (p->minX - a);
        a  = p->minX;
    }
    if (a + c > p->maxX) {
        c = p->maxX - a;
    }
    if (c < 0) {
        c = 0;
    }

    if (b < p->minY) {
        d -= (p->minY - b);
        b  = p->minY;
    }
    if (b + d > p->maxY) {
        d = p->maxY - b;
    }
    if (d < 0) {
        d = 0;
    }

    if (p->doubled != 0) {
        a <<= 1;
        b <<= 1;
        c <<= 1;
        d <<= 1;
    }

    pCmd = p->pDl;
    p->pDl += 2;
    pCmd[0] = BR_SCISSOR_TAG_SYNC;
    pCmd[1] = 0;

    pCmd = p->pDl;
    p->pDl += 2;
    pCmd[0] = BR_SCISSOR_TAG_RECT
            | (((uint32_t)BrScissorPack(a) & 0xFFFu) << 12)
            | ((uint32_t)BrScissorPack(b) & 0xFFFu);
    /* No tag on the second word. */
    pCmd[1] = (((uint32_t)BrScissorPack(a + c) & 0xFFFu) << 12)
            | ((uint32_t)BrScissorPack(b + d) & 0xFFFu);
}

/* ==================================================================== */
/* 6. 0x10069490 -- adapter over br_pool.c                              */
/* ==================================================================== */

BrPool g_brPool10069490;

/* WHAT IT DOES: hands out one scratch transform from the frame pool, for a
 * caller that needs somewhere to build a position-and-facing that only has to
 * survive the rest of this frame. */
/* @implements 0x10069490 d3d BrSub_10069490 */
/* Third instance of the frame-bank template that slice3_41.c's BrPool16Alloc
 * and BrPool32Alloc carry -- the original hand-inlines the allocator into each
 * bank with that bank's constants folded, rather than calling a shared helper.
 * This one is the 64-byte bank: 256 usable, 257 slots per frame, and its own
 * counter and two bases.  The `= ++c` on the counter is required for the same
 * reason as there; `c + 1` costs a byte and moves a register. */
#ifdef BR_MATCHING_BUILD
extern int32_t BrG_6C65EC;      /* 0x106C65EC  frame parity               */
extern int32_t BrG_B01C40;      /* 0x10B01C40  64-byte bank counter       */
extern uint8_t BrG_AF9BC0[];    /* 0x10AF9BC0  64-byte bank base          */
extern uint8_t BrG_AFDBC0[];    /* 0x10AFDBC0  64-byte bank overflow slot */
BrMat4 *BrSub_10069490(void)
{
    int32_t c = BrG_B01C40;
    if (c < 256) {
        uint8_t *p = &BrG_AF9BC0[(BrG_6C65EC * 257 + c) * 64];
        BrG_B01C40 = ++c;
        return (BrMat4 *)p;
    }
    BrG_B01C40 = ++c;
    return (BrMat4 *)&BrG_AFDBC0[BrG_6C65EC * 257 * 64];
}
#else
BrMat4 *BrSub_10069490(void)
{
    return (BrMat4 *)BrPoolAlloc(&g_brPool10069490);
}
#endif

/* ==================================================================== */
/* 7. 0x10004FC0                                                        */
/* ==================================================================== */

static BrNetKeepAlive s_keep;

BrNetKeepAlive *BrNetKeepAliveGetState(void)
{
    return &s_keep;
}

void BrNetKeepAliveTick(void)
{
    BrNetKeepAlive *p    = &s_keep;
    BrNetState     *pNet = p->pNet;
    int32_t         live;   /* esi: non-zero only when the counter advanced
                             * to something below the wrap point */
    int32_t         f02C;
    int32_t         a8;

    BrNetMutexLock(pNet->h1022AF04);

    live = pNet->f1022AAF4;
    if (live != 0) {
        live = live + 1;
        pNet->f1022AAF4 = live;
        if (live >= BR_NET_KEEPALIVE_PERIOD) {
            live = 0;
            pNet->f1022AF20 = 1;
            pNet->f1022AAF4 = 0;
        }
    }

    BrNetMutexUnlock(pNet->h1022AF04);

    if (live == 0) {
        return;
    }
    if (p->f22AF18 == 0 || p->f22AF14 == 0 || p->f6909E0 != 0) {
        return;
    }
    if (p->fACEE50 >= p->f0BD3E0) {
        return;
    }

    /* One argument in the original; slice1_02.h adds the state pointer. */
    f02C = BrNetSlotGetF02C(pNet, g_br094294);

    /* `and al, 0xBF / or al, 0x80` -- only the LOW BYTE is rewritten, and the
     * argument is an int32_t in slice4_50.h's model, so the upper 24 bits of
     * the return value ride along. */
    a8 = (int32_t)(((uint32_t)f02C & 0xFFFFFF00u)
                   | (((uint32_t)f02C & 0xBFu) | 0x80u));

    /* The tenth argument is the stack slot the 0x10004A10 call left behind --
     * a literal 0.  See the header. */
    BrNetSend4760((BrDPlay **)&p->p277B40,
                  g_br094294,
                  p->f22B34C,
                  g_brAD0854[0], g_brAD0854[1], g_brAD0854[2],
                  g_br277B48,
                  g_brPB4E2E8,
                  a8,
                  0);
}

/* ==================================================================== */
/* 8. 0x100765E0                                                        */
/* ==================================================================== */

void BrSub100765E0(const BrMat4 *pSrc, BrVec4 *pDst)
{
    const float m00 = pSrc->m[0][0], m01 = pSrc->m[0][1], m02 = pSrc->m[0][2];
    const float m10 = pSrc->m[1][0], m11 = pSrc->m[1][1], m12 = pSrc->m[1][2];
    const float m20 = pSrc->m[2][0], m21 = pSrc->m[2][1], m22 = pSrc->m[2][2];

    if (!(m00 < 0.0f)) {
        const float t = m11 + m22;

        if (!(t < 0.0f)) {
            pDst->f00 = (m00 + t) - (-1.0f);
            pDst->f04 = m12 - m21;
            pDst->f08 = m20 - m02;
            pDst->f0C = m01 - m10;
        } else {
            pDst->f00 = m12 - m21;
            pDst->f04 = ((m00 - (-1.0f)) - m11) - m22;
            pDst->f08 = m01 + m10;
            pDst->f0C = m20 + m02;
        }
    } else {
        if (!((m11 - m22) < 0.0f)) {
            pDst->f00 = m20 - m02;
            pDst->f04 = m01 + m10;
            pDst->f08 = (m11 - (m00 - 1.0f)) - m22;
            pDst->f0C = m21 + m12;
        } else {
            pDst->f00 = m01 - m10;
            pDst->f04 = m20 + m02;
            pDst->f08 = m21 + m12;
            pDst->f0C = m22 - ((m00 + m11) - 1.0f);
        }
    }

    BrVec4Normalise(pDst);
}
