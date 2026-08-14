/* slice2_23.c -- BRD3D.dll 0x1003DC10-0x10040330, agent 23. See slice2_23.h.
 *
 * Constants below were read out of orig/BRD3D.dll rather than guessed:
 *   0x1008F660 == 8.0f, 0x1008F664 == -8.0f, the immediate 0x43020000 stored
 *   into item[0].F414 by 0x1003FA00 == 130.0f, the table at 0x100AB334 is 21
 *   records of 8 bytes whose second dword of the last record is exactly the
 *   global 0x100AB3D8, and the four record tables 0x10040040 searches hold
 *   120 / 134 / 134 / 10 records of 0x24 bytes.
 *
 * Two CRT routines the original calls are used directly instead of ported,
 * per the contract's "at or above 0x1007CC40 is statically linked MSVC CRT"
 * rule: 0x1008C000 is _itoa (called as _itoa(v, buf, 10)) and 0x1008C320 is
 * strcmp. 0x1007C8A0 is __ftol and is reproduced locally as br23_ftol.
 *
 * Every string move in this range is an inlined `rep movsd` + `rep movsb`
 * over strlen+1 bytes, i.e. exactly strcpy, with no bound. strcpy is used.
 */

#include "slice2_23.h"

#include <stdio.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br23_assert_cfgrec[(sizeof(BrCfgRec) == 0x24) ? 1 : -1];
typedef char br23_assert_item[
    (BR_UI_ITEM_OFF_I420 < BR_UI_ITEM_STRIDE) ? 1 : -1];

/* ==========================================================================
 * Byte-offset accessors
 * ========================================================================== */

uint32_t BrUiLd32(const BrUiObj *pObj, size_t off)
{
    uint32_t v;
    memcpy(&v, pObj + off, sizeof(v));
    return v;
}

void BrUiSt32(BrUiObj *pObj, size_t off, uint32_t v)
{
    memcpy(pObj + off, &v, sizeof(v));
}

int16_t BrUiLd16(const BrUiObj *pObj, size_t off)
{
    int16_t v;
    memcpy(&v, pObj + off, sizeof(v));
    return v;
}

void BrUiSt16(BrUiObj *pObj, size_t off, int16_t v)
{
    memcpy(pObj + off, &v, sizeof(v));
}

float BrUiLdF(const BrUiObj *pObj, size_t off)
{
    float v;
    memcpy(&v, pObj + off, sizeof(v));
    return v;
}

void BrUiStF(BrUiObj *pObj, size_t off, float v)
{
    memcpy(pObj + off, &v, sizeof(v));
}

void *BrUiLdPtr(const BrUiObj *pObj, size_t off)
{
    void *p;
    memcpy(&p, pObj + off, sizeof(p));
    return p;
}

void BrUiStPtr(BrUiObj *pObj, size_t off, void *p)
{
    memcpy(pObj + off, &p, sizeof(p));
}

BrUiObj *BrUiItem(BrUiObj *pObj, int32_t i)
{
    /* Signed arithmetic throughout: the original's index is sign-extended
     * from 16 bits and a negative one addresses backwards. */
    return pObj + (ptrdiff_t)BR_UI_OFF_ITEM
                + (ptrdiff_t)BR_UI_ITEM_STRIDE * (ptrdiff_t)i;
}

char *BrUiItemText(BrUiObj *pObj, int32_t i)
{
    return (char *)(BrUiItem(pObj, i) + BR_UI_ITEM_OFF_TEXT);
}

const BrUiWidgetVtbl *BrUiItemVtblOf(BrUiObj *pObj, int32_t i)
{
    return (const BrUiWidgetVtbl *)BrUiLdPtr(BrUiItem(pObj, i),
                                             BR_UI_ITEM_OFF_VTBL);
}

/* ==========================================================================
 * Local reproductions of routines the contract already pins down
 * ========================================================================== */

/* 0x1007C8A0 __ftol: x87 `fistp` with the rounding control set to truncate,
 * low dword taken before any clamp. On overflow or NaN the x87 stores the
 * "integer indefinite" value 0x80000000, which is what this returns. */
static int32_t br23_ftol(float f)
{
    if (!(f >= -2147483648.0f && f < 2147483648.0f)) {
        return INT32_MIN;
    }
    return (int32_t)f;
}

/* The item-text side of every "set the caption" callback: strcpy, then the
 * item vtable's +0x04 slot on the item as `this`. */
static void br23_item0_set(BrUiObj *pObj, const char *pSrc)
{
    strcpy(BrUiItemText(pObj, 0), pSrc);
    BrUiItemVtblOf(pObj, 0)->f04(BrUiItem(pObj, 0));
}

/* ...and the same followed by BrUiItemApply(pObj, 0), which is the shape of
 * all but two members of the family (0x1003E840 and 0x1003F170 stop short). */
static int32_t br23_item0_set_apply(BrUiObj *pObj, BrUiGlobals *pG,
                                    const char *pSrc)
{
    br23_item0_set(pObj, pSrc);
    (void)BrUiItemApply(pObj, 0, pG);
    return 1;
}

static int32_t br23_text_id(BrUiObj *pObj, BrUiGlobals *pG, int32_t id)
{
    return br23_item0_set_apply(pObj, pG, BrStrGet(id));
}

/* `sel->f20(sel, v)` on the nested widget at +0x3838. */
static int32_t br23_sel_offer(BrUiObj *pObj, int32_t v)
{
    BrUiObj              *pSel = pObj + BR_UI_OFF_SEL;
    const BrUiWidgetVtbl *pVt  = (const BrUiWidgetVtbl *)
                                     BrUiLdPtr(pObj, BR_UI_OFF_SEL);
    return pVt->f20(pSel, v);
}

static void br23_sel_commit(BrUiObj *pObj, int32_t v)
{
    BrUiObj              *pSel = pObj + BR_UI_OFF_SEL;
    const BrUiWidgetVtbl *pVt  = (const BrUiWidgetVtbl *)
                                     BrUiLdPtr(pObj, BR_UI_OFF_SEL);
    pVt->f24(pSel, v);
}

/* The bare "offer the global, keep the answer if it is not negative" body
 * shared by 0x1003EAE0 / 0x1003EB60 / 0x1003EB90 / 0x1003EC80 / 0x1003ED10 /
 * 0x1003EDF0. Returns the value the original leaves in eax. */
static int32_t br23_poll_store(BrUiObj *pObj, int32_t *pVal)
{
    int32_t r = br23_sel_offer(pObj, *pVal);
    if (r >= 0) {
        *pVal = r;
    }
    return r;
}

/* Clears bit 4 of <obj>+0x1C, but only when the text is non-empty. The
 * original computes strlen with `repne scasb` and branches on `!= 0`. */
static void br23_clear_bit4_if_text(BrUiObj *pTarget, const char *pText)
{
    if (pText[0] != '\0') {
        BrUiSt32(pTarget, BR_UI_OFF_FLAGS,
                 BrUiLd32(pTarget, BR_UI_OFF_FLAGS) & ~(uint32_t)0x10);
    }
}

/* strcmp-then-copy, exactly as the original spells it. The comparison is
 * pure overhead -- copying unconditionally would be equivalent -- but it is
 * preserved because it is observable through a debugger and costs nothing. */
static void br23_copy_if_differs(char *pDst, const char *pSrc)
{
    if (strcmp(pDst, pSrc) != 0) {
        strcpy(pDst, pSrc);
    }
}

/* ==========================================================================
 * 0x1003DC10
 * ========================================================================== */

typedef struct BrDPlayErrEnt {
    int32_t     code;
    const char *pszName;
} BrDPlayErrEnt;

/* Transcribed from the compare immediates of the original's binary search,
 * in the same ascending-as-signed order the search assumes. */
static const BrDPlayErrEnt g_aDPlayErrs[] = {
    { (int32_t)0x8000000Au, "DPERR_PENDING"                 },
    { (int32_t)0x80004001u, "DPERR_UNSUPPORTED"             },
    { (int32_t)0x80004002u, "DPERR_NOINTERFACE"             },
    { (int32_t)0x80004005u, "DPERR_GENERIC"                 },
    { (int32_t)0x8007000Eu, "DPERR_OUTOFMEMORY"             },
    { (int32_t)0x80070057u, "DPERR_INVALIDPARAMS"           },
    { (int32_t)0x88770005u, "DPERR_ALREADYINITIALIZED"      },
    { (int32_t)0x8877000Au, "DPERR_ACCESSDENIED"            },
    { (int32_t)0x88770014u, "DPERR_ACTIVEPLAYERS"           },
    { (int32_t)0x8877001Eu, "DPERR_BUFFERTOOSMALL"          },
    { (int32_t)0x88770028u, "DPERR_CANTADDPLAYER"           },
    { (int32_t)0x88770032u, "DPERR_CANTCREATEGROUP"         },
    { (int32_t)0x8877003Cu, "DPERR_CANTCREATEPLAYER"        },
    { (int32_t)0x88770046u, "DPERR_CANTCREATESESSION"       },
    { (int32_t)0x88770050u, "DPERR_CAPSNOTAVAILABLEYET"     },
    { (int32_t)0x8877005Au, "DPERR_EXCEPTION"               },
    { (int32_t)0x88770078u, "DPERR_INVALIDFLAGS"            },
    { (int32_t)0x88770082u, "DPERR_INVALIDOBJECT"           },
    { (int32_t)0x88770096u, "DPERR_INVALIDPLAYER"           },
    { (int32_t)0x8877009Bu, "DPERR_INVALIDGROUP"            },
    { (int32_t)0x887700A0u, "DPERR_NOCAPS"                  },
    { (int32_t)0x887700AAu, "DPERR_NOCONNECTION"            },
    { (int32_t)0x887700BEu, "DPERR_NOMESSAGES"              },
    { (int32_t)0x887700C8u, "DPERR_NONAMESERVERFOUND"       },
    { (int32_t)0x887700D2u, "DPERR_NOPLAYERS"               },
    { (int32_t)0x887700DCu, "DPERR_NOSESSIONS"              },
    { (int32_t)0x887700E6u, "DPERR_SENDTOOBIG"              },
    { (int32_t)0x887700F0u, "DPERR_TIMEOUT"                 },
    { (int32_t)0x887700FAu, "DPERR_UNAVAILABLE"             },
    { (int32_t)0x8877010Eu, "DPERR_BUSY"                    },
    { (int32_t)0x88770118u, "DPERR_USERCANCEL"              },
    { (int32_t)0x88770122u, "DPERR_CANNOTCREATESERVER"      },
    { (int32_t)0x8877012Cu, "DPERR_PLAYERLOST"              },
    { (int32_t)0x88770136u, "DPERR_SESSIONLOST"             },
    { (int32_t)0x88770140u, "DPERR_UNINITIALIZED"           },
    { (int32_t)0x8877014Au, "DPERR_NONEWPLAYERS"            },
    { (int32_t)0x88770154u, "DPERR_INVALIDPASSWORD"         },
    { (int32_t)0x8877015Eu, "DPERR_CONNECTING"              },
    { (int32_t)0x887703E8u, "DPERR_BUFFERTOOLARGE"          },
    { (int32_t)0x887703F2u, "DPERR_CANTCREATEPROCESS"       },
    { (int32_t)0x887703FCu, "DPERR_APPNOTSTARTED"           },
    { (int32_t)0x88770406u, "DPERR_INVALIDINTERFACE"        },
    { (int32_t)0x88770410u, "DPERR_NOSERVICEPROVIDER"       },
    { (int32_t)0x8877041Au, "DPERR_UNKNOWNAPPLICATION"      },
    { (int32_t)0x8877042Eu, "DPERR_NOTLOBBIED"              },
    { (int32_t)0x88770438u, "DPERR_SERVICEPROVIDERLOADED"   },
    { (int32_t)0x88770442u, "DPERR_ALREADYREGISTERED"       },
    { (int32_t)0x8877044Cu, "DPERR_NOTREGISTERED"           },
    { (int32_t)0x887707D0u, "DPERR_AUTHENTICATIONFAILED"    },
    { (int32_t)0x887707DAu, "DPERR_CANTLOADSSPI"            },
    { (int32_t)0x887707E4u, "DPERR_ENCRYPTIONFAILED"        },
    { (int32_t)0x887707EEu, "DPERR_SIGNFAILED"              },
    { (int32_t)0x887707F8u, "DPERR_CANTLOADSECURITYPACKAGE" },
    { (int32_t)0x88770802u, "DPERR_ENCRYPTIONNOTSUPPORTED"  },
    { (int32_t)0x8877080Cu, "DPERR_CANTLOADCAPI"            },
    { (int32_t)0x88770816u, "DPERR_NOTLOGGEDIN"             },
    { (int32_t)0x88770820u, "DPERR_LOGONDENIED"             },
    { 0,                    "DP_OK"                         }
};

const char *BrDPlayErrName(int32_t hr)
{
    /* DEVIATION: the original renders the fallback into the fixed global
     * buffer at 0x10A9BFE0, whose size is not established. A file-scope
     * buffer of 16 bytes is used instead; "0x%08X" needs 11 including the
     * NUL. The single-buffer, non-re-entrant behaviour is preserved because
     * callers of the original depend on the returned pointer staying valid
     * only until the next call. */
    static char szUnknown[16];
    size_t      i;

    /* The original binary-searches; an exact-match scan gives the identical
     * answer for every input because the table holds no duplicates. */
    for (i = 0; i < sizeof(g_aDPlayErrs) / sizeof(g_aDPlayErrs[0]); i++) {
        if (g_aDPlayErrs[i].code == hr) {
            return g_aDPlayErrs[i].pszName;
        }
    }

    /* USER32!wsprintfA(g_A9BFE0, "0x%08X", hr) -- uppercase, zero-padded. */
    sprintf(szUnknown, "0x%08X", (unsigned int)(uint32_t)hr);
    return szUnknown;
}

/* ==========================================================================
 * 0x1003DFC0
 * ========================================================================== */

void BrUiFn1003DFC0(BrStartupState *pState, void *pB4DF30)
{
    pState->g0B380C = 0;
    pState->g22B350 = 0;
    pState->g22B34C = 0;
    pState->g094354 = 1;
    pState->g09435C = 2;
    pState->g094358 = 1;
    pState->gB4E1D0 = 0;
    pState->gB4E1D4 = pB4DF30;
    pState->g094350 = 1;
}

/* ==========================================================================
 * 0x1003E010 / 0x1003E040
 * ========================================================================== */

void BrUiFn1003E010(BrUiGlobals *pG)
{
    pG->gAA27E0 = (int16_t)0x0102;
    pG->gAA2598 = 0x102;
}

void BrUiFn1003E040(BrUiGlobals *pG)
{
    pG->gAA27E2 = (int16_t)0x0037;
    pG->gA9D010 = 0x37;
}

/* ==========================================================================
 * 0x1003E0E0
 * ========================================================================== */

int32_t BrUiFn1003E0E0(const BrActiveFlags *pFlags)
{
    if (BrFn1005FFD0() >= 0) {
        return 1;
    }
    if (BrIsAnyActive(pFlags) != 0) {
        return 1;
    }
    return 0;
}

/* ==========================================================================
 * 0x1003E7A0
 * ========================================================================== */

int32_t BrUiDraw1003E7A0(BrUiObj *pObj)
{
    const BrUiObjVtbl *pVt = (const BrUiObjVtbl *)BrUiLdPtr(pObj,
                                                           BR_UI_OFF_VTBL);
    BrUiObj *pItem0 = BrUiItem(pObj, 0);
    int32_t  x0     = br23_ftol(BrUiLdF(pItem0, BR_UI_ITEM_OFF_F410)) - 3;
    int32_t  y      = br23_ftol(BrUiLdF(pItem0, BR_UI_ITEM_OFF_F414)) - 0x0C;
    int32_t  w      = BrUiLd16(pItem0, BR_UI_ITEM_OFF_W40A);
    /* `cdq / and edx,0xF / add / sar 4` -- signed divide by 16 truncating
     * toward zero, then one more. */
    int32_t  nSigned = (w / 16) + 1;
    uint32_t n       = (uint32_t)nSigned;
    uint32_t left;
    int32_t  x       = x0;
    uint32_t nRun    = 0u;

    pVt->f14(pObj, 0x3D, x0 - 8, y);

    /* GOTCHA: `test ebx,ebx / jbe` -- the guard is UNSIGNED, so only n == 0
     * skips the loop. A negative nSigned runs it ~2^32 times, exactly as the
     * original does. Kept deliberately. */
    if (n != 0u) {
        left = n;
        do {
            pVt->f14(pObj, 0x3B, x, y);
            x += 0x10;
            left--;
        } while (left != 0u);
        nRun = n;
    }

    pVt->f14(pObj, 0x3C, (int32_t)(nRun << 4) + x0, y);
    return 1;
}

/* ==========================================================================
 * 0x1003E840
 * ========================================================================== */

int32_t BrUiText1003E840(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t id = 0x0C;

    if (pG->g0AA010 == 0 && pG->g220B20 == 0) {
        id = 0x51;
    }
    /* No BrUiItemApply here -- see the header. */
    br23_item0_set(pObj, BrStrGet(id));
    return 1;
}

/* ==========================================================================
 * 0x1003E8D0 / 0x1003EA90
 * ========================================================================== */

static int32_t br23_num_common(BrUiObj *pObj, int32_t value)
{
    BrUiObj              *pItem0 = BrUiItem(pObj, 0);
    const BrUiWidgetVtbl *pVt    = BrUiItemVtblOf(pObj, 0);

    /* _itoa(value, buf, 10) -- radix 10, so plain "%d". */
    sprintf(BrUiItemText(pObj, 0), "%d", (int)value);
    pVt->f08(pItem0);
    /* The original guards this with `test edi,edi` on the buffer's ADDRESS,
     * which is never null. The call therefore always happens. */
    pVt->f2C(pItem0);
    return 1;
}

int32_t BrUiNum1003E8D0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_num_common(pObj, (int32_t)pG->tAA26E8[pG->gAA28AC]);
}

int32_t BrUiNum1003EA90(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_num_common(pObj, (int32_t)pG->tA9D068[pG->gAA28AC]);
}

/* ==========================================================================
 * 0x1003E920 / 0x1003EA40
 * ========================================================================== */

int32_t BrUiFn1003E920(BrUiObj *pObj, BrUiGlobals *pG)
{
    /* lea ecx,[eax+eax*4] ; lea edx,[eax+ecx*2+0x3D]  ->  11*a + 61 */
    int32_t v = pG->g0AC65C * 11 + 0x3D;
    BrUiStF(pObj, BR_UI_OFF_F3C, (float)v);
    return 1;
}

int32_t BrUiFn1003EA40(BrUiObj *pObj, BrUiGlobals *pG)
{
    uint32_t n = (pG->g0AB3D8 != 0) ? pG->gB4E708 : pG->gB4E70C;
    /* lea ecx,[eax*8+0x4A] then `fild dword` -- the sum is formed in 32-bit
     * wraparound arithmetic and then read as SIGNED. */
    int32_t  v = (int32_t)(n * 8u + 0x4Au);
    BrUiStF(pObj, BR_UI_OFF_F3C, (float)v);
    return 1;
}

/* ==========================================================================
 * 0x1003E950
 * ========================================================================== */

int32_t BrUiCode1003E950(BrUiObj *pObj, BrUiGlobals *pG)
{
    int16_t c = (pG->g0AB3D8 != 0) ? (int16_t)0x68 : (int16_t)0x69;
    BrUiSt16(pObj, BR_UI_OFF_W2A40, c);
    BrUiSt16(pObj, BR_UI_OFF_W1E20C, c);
    return 1;
}

/* ==========================================================================
 * 0x1003E980 / 0x1003E9E0
 * ========================================================================== */

static int32_t br23_draw_row(BrUiObj *pObj, const uint32_t *pCount)
{
    const BrUiObjVtbl *pVt = (const BrUiObjVtbl *)BrUiLdPtr(pObj,
                                                           BR_UI_OFF_VTBL);
    int32_t  x = br23_ftol(BrUiLdF(pObj, BR_UI_OFF_F3C));
    int32_t  y = br23_ftol(BrUiLdF(pObj, BR_UI_OFF_F40)) + 0x13;
    uint32_t i;

    pVt->f14(pObj, 0x74, x, y);

    /* The bound is re-read from the global every iteration -- `mov eax,[g]`
     * sits inside the loop body in the original. */
    for (i = 0u; i < *pCount; i++) {
        pVt->f14(pObj, 0x75, x, y);
        x += 0x0C;
    }
    return 1;
}

int32_t BrUiDraw1003E980(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_draw_row(pObj, &pG->gB4E708);
}

int32_t BrUiDraw1003E9E0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_draw_row(pObj, &pG->gB4E70C);
}

/* ==========================================================================
 * The poll family
 * ========================================================================== */

int32_t BrUiPoll1003EAE0(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->g0AB3F4);
    return 1;
}

/* 0x1003EB10 and 0x1003EC30 are byte-for-byte the same routine emitted
 * twice; both are exported so the address map stays complete. */
static int32_t br23_poll_commit(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t r = br23_sel_offer(pObj, pG->g0AB3F4);

    if (r >= 0) {
        pG->g0AB3F4 = r;
    } else {
        r = pG->g0AB3F4;   /* re-read; can itself be negative */
    }
    if (pG->gAA28D8 != 0 && r >= 0) {
        br23_sel_commit(pObj, r);
    }
    return 1;
}

int32_t BrUiPoll1003EB10(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_poll_commit(pObj, pG);
}

int32_t BrUiPoll1003EC30(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_poll_commit(pObj, pG);
}

int32_t BrUiPoll1003EB60(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->gAA28AC);
    return 1;
}

int32_t BrUiPoll1003EB90(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->gAA2880);
    return 1;
}

int32_t BrUiPoll1003EBC0(BrUiObj *pObj, BrUiGlobals *pG)
{
    /* The answer is thrown away -- there is no store-back here. */
    (void)br23_sel_offer(pObj, pG->gAA2880);
    return 1;
}

int32_t BrUiPoll1003EBE0(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t r = br23_sel_offer(pObj, pG->gAA2880);

    if (r >= 0) {
        pG->gAA2880 = r;
    } else {
        r = pG->gAA2880;
    }
    /* obj + 0x3C98 + 0x438*r, read as a pointer. Same stride as the item
     * array but a different base -- see the header's modelling note. No
     * range check, and r can still be negative on the else path. */
    pG->g0AB3E0 = BrUiLdPtr(pObj + (ptrdiff_t)BR_UI_OFF_TBL3C98
                                 + (ptrdiff_t)BR_UI_ITEM_STRIDE * (ptrdiff_t)r,
                            0u);
    return 1;
}

int32_t BrUiPoll1003EC80(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->gAA2840);
    return 1;
}

int32_t BrUiPoll1003ED10(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->gAA2A2C);
    return 1;
}

int32_t BrUiPoll1003EDF0(BrUiObj *pObj, BrUiGlobals *pG)
{
    (void)br23_poll_store(pObj, &pG->gAA2A30);
    return 1;
}

int32_t BrUiPoll1003EE20(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t v = pG->gAA2A34;
    int32_t r;

    /* `test/jl` then `cmp 0xC/jl` -- anything outside [0, 12) is offered as
     * -1. The clamp does NOT touch the stored global. */
    if (v < 0 || v >= 0x0C) {
        v = -1;
    }
    r = br23_sel_offer(pObj, v);
    if (r >= 0) {
        pG->gAA2A34 = r;
    }
    return 1;
}

/* ==========================================================================
 * 0x1003EE50
 * ========================================================================== */

int32_t BrUiItemApply(BrUiObj *pObj, int16_t index, BrUiGlobals *pG)
{
    BrUiObj              *pItem = BrUiItem(pObj, index);
    const BrUiWidgetVtbl *pVt   = BrUiItemVtblOf(pObj, index);

    pVt->f04(pItem);

    if (BrUiLd32(pItem, BR_UI_ITEM_OFF_I420) == 0u) {
        /* The original guards the next call with `test esi,esi` where esi is
         * pItem + 0x09 -- an address, never null. Dead guard, no branch. */
        pVt->f10(pItem);
        return 0;
    }

    /* `test al,al / jle` -- the low byte of f14's result is read SIGNED.
     * When it is positive AND flag bit 1 is clear the whole confirm block is
     * skipped and only f10 runs. */
    if ((int8_t)(pVt->f14(pItem) & 0xFF) <= 0
        || (BrUiLd32(pObj, BR_UI_OFF_FLAGS) & 2u) != 0u) {

        if (pG->gAA285C == 0) {
            pG->gAA28D8 = 0;
            BrUiSt32(pItem, BR_UI_ITEM_OFF_I420, 0u);
            /* `and al,0xFD` then a full dword store of eax -- bit 1 of the
             * flag word is cleared, the upper 24 bits survive. */
            BrUiSt32(pObj, BR_UI_OFF_FLAGS,
                     BrUiLd32(pObj, BR_UI_OFF_FLAGS) & ~(uint32_t)2);
        }

        BrFn1003E070();

        {
            void *pFn = BrUiLdPtr(pObj, BR_UI_OFF_ONAPPLY);
            if (pFn != NULL) {
                ((void (*)(BrUiObj *))pFn)(pObj);
            }
        }
    }

    pVt->f10(pItem);
    return 1;
}

/* ==========================================================================
 * Text read-back callbacks
 * ========================================================================== */

int32_t BrUiFn1003EEF0(BrUiObj *pObj, BrUiGlobals *pG)
{
    char *pText;

    (void)BrUiItemApply(pObj, 0, pG);
    pText = BrUiItemText(pObj, 0);
    br23_clear_bit4_if_text(pG->pAA29A8, pText);
    br23_copy_if_differs(pG->szB4E2E8, pText);
    return 1;
}

int32_t BrUiFn1003EF60(BrUiObj *pObj, BrUiGlobals *pG)
{
    br23_clear_bit4_if_text(pG->pAA29A8, BrUiItemText(pObj, 0));
    return 1;
}

int32_t BrUiFn1003EF90(BrUiObj *pObj, BrUiGlobals *pG)
{
    char *pText;

    (void)BrUiItemApply(pObj, 0, pG);
    pText = BrUiItemText(pObj, 0);
    br23_clear_bit4_if_text(pG->pAA29E8, pText);
    if (strcmp(pG->szA9CDF0, pText) != 0) {
        strcpy(pG->szA9CDF0, pText);
        /* GOTCHA: the mirror into szB4E1E4 is INSIDE the differs-branch. It
         * goes stale whenever the caption is unchanged. */
        strcpy(pG->szB4E1E4, pG->szA9CDF0);
    }
    return 1;
}

int32_t BrUiFn1003F020(BrUiObj *pObj, BrUiGlobals *pG)
{
    br23_clear_bit4_if_text(pG->pAA29E8, BrUiItemText(pObj, 0));
    return 1;
}

static int32_t br23_readback(BrUiObj *pObj, BrUiGlobals *pG, char *pDst)
{
    (void)BrUiItemApply(pObj, 0, pG);
    br23_copy_if_differs(pDst, BrUiItemText(pObj, 0));
    return 1;
}

int32_t BrUiFn1003F050(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_readback(pObj, pG, pG->szB4E740);
}

int32_t BrUiFn1003F0B0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_readback(pObj, pG, pG->szB4E760);
}

int32_t BrUiFn1003F110(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_readback(pObj, pG, pG->szA9DD28);
}

int32_t BrUiFn1003F170(BrUiObj *pObj, BrUiGlobals *pG)
{
    char *pText = BrUiItemText(pObj, 0);

    strcpy(pG->szA9DD28, pText);
    /* The literal 0 is the residue of the string scan's `xor eax,eax`, not a
     * variable -- see the header. */
    BrFn1003D210(pG->g680584, pG->gA9D008, 0);
    /* Both copies read sz39B720 fresh; 0x1003D210 is expected to have
     * rewritten it. */
    strcpy(pG->szA9DD28, pG->sz39B720);
    strcpy(pText, pG->sz39B720);
    return 1;
}

int32_t BrUiFn1003F210(BrUiObj *pObj, BrUiGlobals *pG)
{
    char *pText;

    (void)BrUiItemApply(pObj, 0, pG);
    pText = BrUiItemText(pObj, 0);
    br23_clear_bit4_if_text(pG->pAA29BC, pText);
    br23_copy_if_differs(pG->szA9D018, pText);
    return 1;
}

int32_t BrUiFn1003F280(BrUiObj *pObj, BrUiGlobals *pG)
{
    br23_clear_bit4_if_text(pG->pAA29BC, BrUiItemText(pObj, 0));
    return 1;
}

/* ==========================================================================
 * The code family
 * ========================================================================== */

int32_t BrUiCode1003F440(BrUiObj *pObj, BrUiGlobals *pG)
{
    /* Stage 1. Skipped entirely when gAA26F0 <= 0, leaving W1E20C alone. */
    if (pG->gAA26F0 > 0) {
        int32_t k = pG->gAA26F0 - 1;
        int16_t v;

        switch (k) {
        case 0:  v = 0x73; break;
        case 1:  v = 0x72; break;
        case 2:  v = 0x71; break;
        case 3:  v = 0x70; break;
        case 4:  v = 0x6F; break;
        default: v = -1;   break;   /* `ja` -- unsigned, so k > 4 */
        }
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    }

    /* Stage 2 runs only when gAA26F0 is exactly 0. */
    if (pG->gAA26F0 == 0) {
        int32_t k = (pG->gAA26F4 & 0xFF) - 1;
        int16_t v;

        switch (k) {
        case 0:  v = 0x47; break;
        case 1:  v = 0x49; break;
        case 2:  v = 0x4B; break;
        /* Three inputs share one output; that is in the jump table. */
        case 3:
        case 4:
        case 5:  v = 0x4D; break;
        default: v = -1;   break;
        }
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    }
    return 1;
}

int32_t BrUiCode1003F540(BrUiObj *pObj, BrUiGlobals *pG)
{
    if (pG->gAA26F0 > 0) {
        int16_t v;

        switch (pG->gAA26F0) {
        case 2:  v = 0x6D; break;
        case 3:  v = 0x6E; break;
        case 4:  v = 0x6C; break;
        /* GOTCHA: 1 is NOT a case here -- the chain starts at `sub eax,2`,
         * so gAA26F0 == 1 falls into the -1 default alongside >= 5. */
        default: v = -1;   break;
        }
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    }

    if (pG->gAA26F0 == 0) {
        int16_t v;

        switch (pG->gAA26F4 & 0xFF) {
        case 1:  v = 0x48; break;
        case 2:  v = 0x4A; break;
        case 3:  v = 0x4C; break;
        default: v = -1;   break;
        }
        BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    }
    return 1;
}

int32_t BrUiCode1003F5E0(BrUiObj *pObj, BrUiGlobals *pG)
{
    int16_t v;

    switch (pG->gAA2A18) {
    case 0u: v = 0x56; break;
    case 1u: v = 0x57; break;
    case 2u: v = 0x59; break;
    case 3u: v = 0x5B; break;
    case 4u: v = 0x5D; break;
    default: v = 0x56; break;   /* same as index 0 */
    }
    BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    return 1;
}

int32_t BrUiCode1003F680(BrUiObj *pObj, BrUiGlobals *pG)
{
    int16_t v;

    switch (pG->gAA2A18) {
    case 0u: v = (int16_t)0xFFFF; break;   /* index 0 IS the sentinel here */
    case 1u: v = 0x58; break;
    case 2u: v = 0x5A; break;
    case 3u: v = 0x5C; break;
    case 4u: v = 0x5E; break;
    default: v = (int16_t)0xFFFF; break;
    }
    BrUiSt16(pObj, BR_UI_OFF_W1E20C, v);
    return 1;
}

int32_t BrUiCode1003F720(BrUiObj *pObj, BrUiGlobals *pG)
{
    if (pG->gAA2904 == pG->gAA2964 && pG->gAA28E8 == 0) {
        return -2;   /* 0xFFFFFFFE -- nothing is written */
    }
    /* Dword-strided table, 16-bit read. */
    BrUiSt16(pObj, BR_UI_OFF_W1E20C,
             (int16_t)(uint16_t)((uint32_t)pG->tAC5A8[pG->g0AC654] & 0xFFFFu));
    return 1;
}

/* ==========================================================================
 * The text family
 * ========================================================================== */

/* The "session is local" predicate all three of 0x1003F720 / 0x1003F760 /
 * 0x1003FA00 / 0x1003FE80 open with. */
static int br23_is_solo(const BrUiGlobals *pG)
{
    return (pG->gAA2904 == pG->gAA2964 && pG->gAA28E8 == 0);
}

int32_t BrUiText1003F760(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t id;

    if (br23_is_solo(pG)) {
        id = 0x14;
    } else {
        int32_t i = pG->g0AC654;
        /* `cmp 0xF / jle` then `sub 0x10` -- a wrap, not a clamp. */
        if (i > 0x0F) {
            i -= 0x10;
        }
        id = pG->tAC368[i];
    }
    return br23_text_id(pObj, pG, id);
}

int32_t BrUiText1003F7F0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC348[pG->g0AC64C]);
}

int32_t BrUiText1003F860(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC3A8[pG->gAA2A08]);
}

int32_t BrUiText1003F8D0(BrUiObj *pObj, BrUiGlobals *pG)
{
    if (pG->gAA2850 != 0) {
        strcpy(BrUiItemText(pObj, 0), BrStrGet(0xAF));
        /* aA9D5C0 is the conflict-flag array 0x10040330 maintains. */
        BrUiItem(pObj, 0)[BR_UI_ITEM_OFF_B08] =
            (pG->aA9D5C0[pG->gAA2840] != 0) ? (unsigned char)4
                                            : (unsigned char)1;
    } else {
        /* No B08 write on this path -- whatever was there survives. */
        strcpy(BrUiItemText(pObj, 0), pG->sz0AD300);
    }
    BrUiItemVtblOf(pObj, 0)->f04(BrUiItem(pObj, 0));
    (void)BrUiItemApply(pObj, 0, pG);
    return 1;
}

int32_t BrUiText1003F990(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC358[pG->g0AC650]);
}

int32_t BrUiText1003FA00(BrUiObj *pObj, BrUiGlobals *pG)
{
    /* DEVIATION: the original's scratch buffer is the 0x74 bytes at
     * esp+0x10 in a 0x84-byte frame; a copy longer than that smashes its
     * stack. 512 bytes are used here so the port cannot corrupt anything.
     * No behaviour that the original defines is changed. */
    char     szTmp[512];
    int32_t  a;
    void    *pEnt;

    if (br23_is_solo(pG)) {
        /* Straight to the final copy with the string-table pointer as the
         * source -- the scratch buffer is bypassed. */
        return br23_text_id(pObj, pG, 0x1B);
    }

    if (pG->g0AA010 == 0) {
        int32_t i    = pG->gAA28B8;
        int32_t base = (pG->gAA28A8 != 0u) ? pG->gAA28AC : pG->gAA28A4;
        int32_t k    = base + 12 * i;

        strcpy(szTmp, BrStrGet(pG->tAC308[pG->tB3820[2 * k]]));
        /* The original re-loads gAA28B8 and the base global here rather than
         * reusing the value above. Reproduced literally. */
        i    = pG->gAA28B8;
        base = (pG->gAA28A8 != 0u) ? pG->gAA28AC : pG->gAA28A4;
        a    = pG->tB3820[2 * (base + 12 * i)];
    } else {
        strcpy(szTmp, BrStrGet(pG->tAC308[pG->g0AC648]));
        a = pG->g0AC648;
    }

    pEnt = pG->tBD2A8[a];
    if ((((const unsigned char *)pEnt)[4] & 0x10u) != 0u) {
        /* GOTCHA: the value saved is the object's +0x40, but the value
         * RESTORED lands in item[0].F414 (obj+0x2F70). The original saves
         * one field and writes it back over a different one; +0x2F70's
         * previous contents are lost. Preserved as-is. */
        uint32_t saved = BrUiLd32(pObj, BR_UI_OFF_F40);

        BrUiStF(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F414, 130.0f);
        br23_item0_set(pObj, BrStrGet(0xB0));
        (void)BrUiItemApply(pObj, 0, pG);
        BrUiSt32(BrUiItem(pObj, 0), BR_UI_ITEM_OFF_F414, saved);
    }

    return br23_item0_set_apply(pObj, pG, szTmp);
}

int32_t BrUiText1003FC40(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC3F0[pG->gAA287C]);
}

int32_t BrUiText1003FCB0(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t id = (pG->g18ABDBC != 0) ? pG->tAC400[pG->gAA2A1C] : 0x74;
    return br23_text_id(pObj, pG, id);
}

int32_t BrUiText1003FD30(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC418[pG->gAA2A28]);
}

int32_t BrUiText1003FDA0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC408[pG->gAA2A20]);
}

int32_t BrUiText1003FE10(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC410[pG->gAA2A24]);
}

int32_t BrUiText1003FE80(BrUiObj *pObj, BrUiGlobals *pG)
{
    int32_t id;

    if (br23_is_solo(pG)) {
        BrUiObj *pItem0 = BrUiItem(pObj, 0);

        /* fsub 0x1008F660 (== 8.0f) before, fsub 0x1008F664 (== -8.0f)
         * after: the caption is drawn 8 units up and then put back. */
        BrUiStF(pItem0, BR_UI_ITEM_OFF_F414,
                BrUiLdF(pItem0, BR_UI_ITEM_OFF_F414) - 8.0f);
        (void)br23_text_id(pObj, pG, 0x1C);
        BrUiStF(pItem0, BR_UI_ITEM_OFF_F414,
                BrUiLdF(pItem0, BR_UI_ITEM_OFF_F414) - (-8.0f));
        return 1;
    }

    if (pG->g0AA010 == 0) {
        int32_t i    = pG->gAA28B8;
        int32_t base = (pG->gAA28A8 != 0u) ? pG->gAA28AC : pG->gAA28A4;
        /* Byte 1 of the same 2-byte record 0x1003FA00 reads byte 0 of. */
        id = pG->tAC3B0[pG->tB3820[2 * (base + 12 * i) + 1]];
    } else {
        id = pG->tAC3B0[pG->gAA2A00];
    }
    return br23_text_id(pObj, pG, id);
}

int32_t BrUiText1003FFD0(BrUiObj *pObj, BrUiGlobals *pG)
{
    return br23_text_id(pObj, pG, pG->tAC3E0[pG->gAA2A0C]);
}

/* ==========================================================================
 * 0x10040040
 * ========================================================================== */

int32_t BrCfgLookupIndex(const BrCfgTables *pT, int32_t type, uint32_t key)
{
    const BrCfgRec *aRecs;
    int32_t         n;
    int32_t         i;

    /* `cmp eax,3 / ja` -- unsigned, so negatives fall out here too. */
    if ((uint32_t)type > 3u) {
        return 0;
    }

    switch (type) {
    case 0:  aRecs = pT->aT0; n = BR_CFG_T0_COUNT; break;
    /* Types 1 and 2 share a table in the original's jump table. */
    case 1:  aRecs = pT->aT1; n = BR_CFG_T1_COUNT; break;
    case 2:  aRecs = pT->aT1; n = BR_CFG_T1_COUNT; break;
    default: aRecs = pT->aT3; n = BR_CFG_T3_COUNT; break;
    }

    for (i = 0; i < n; i++) {
        if (aRecs[i].key == key) {
            return i;
        }
    }
    return 0;   /* indistinguishable from a hit at index 0 */
}

/* ==========================================================================
 * 0x100400E0
 * ========================================================================== */

int32_t BrUiText100400E0(BrUiObj *pObj, BrUiGlobals *pG,
                         const BrCfgTables *pT, void *pB4DF30)
{
    const char *pSrc = NULL;   /* NULL means "no copy at all" */

    if (pG->gAA2844 != 0) {
        pSrc = BrStrGet(0xB2);
    } else if ((uint32_t)pG->gAA2A0C > 3u) {
        /* GOTCHA: no copy, no fallback string -- straight to f04 + apply on
         * whatever text the item already held. */
        pSrc = NULL;
    } else {
        int32_t  kind = pG->gAA2A0C;
        uint32_t rec  = pG->aAB334[2 * pG->gAA2840];

        if (kind == 0) {
            uint8_t k = BrFn10069C30(pB4DF30, 0, rec);
            pSrc = pT->aT0[BrCfgLookupIndex(pT, 0, k)].szText;
        } else {
            uint8_t k = BrFn10069C30(pB4DF30, kind, rec);

            /* The second query re-reads gAA2840 and the record. */
            rec = pG->aAB334[2 * pG->gAA2840];
            if (BrFn10069BC0(pB4DF30, kind, rec) != 0) {
                if (kind == 3) {
                    pSrc = pT->aT3[BrCfgLookupIndex(pT, 3, k)].szText;
                } else {
                    pSrc = pT->aT1[BrCfgLookupIndex(pT, kind, k)].szText;
                }
            } else if (k != 0u) {
                /* Falls back to the type-0 table with the SAME key. */
                pSrc = pT->aT0[BrCfgLookupIndex(pT, 0, k)].szText;
            } else {
                pSrc = BrStrGet(0xB1);
            }
        }
    }

    if (pSrc != NULL) {
        strcpy(BrUiItemText(pObj, 0), pSrc);
    }
    BrUiItemVtblOf(pObj, 0)->f04(BrUiItem(pObj, 0));
    (void)BrUiItemApply(pObj, 0, pG);
    return 1;
}

/* ==========================================================================
 * 0x10040330
 * ========================================================================== */

int32_t BrCfgFindConflicts(BrUiGlobals *pG, int32_t kind, void *pB4DF30)
{
    /* Records whose key is one of these are never taken as the `j` of a
     * pair while i is below the filter cut-off. */
    static const uint32_t aSkip[3] = { 0x0Cu, 0x0Du, 0x0Eu };
    const int32_t kFilterTo = 12;   /* the test is `ebx < 0x100AB394` */

    int32_t i;
    int32_t found = 0;

    for (i = 0; i < BR_UI_AB334_COUNT; i++) {
        uint32_t keyI = pG->aAB334[2 * i];
        int32_t  aI;
        uint8_t  bI;
        int32_t  j;

        /* GOTCHA: this zeroing is what makes flags set by earlier passes on
         * a HIGHER index disappear. It is in the original. */
        pG->aA9D5C0[i] = 0;

        aI = BrFn10069BC0(pB4DF30, kind, keyI);
        bI = BrFn10069C30(pB4DF30, kind, keyI);

        /* `inc esi / cmp esi,0x15 / jge` -- the counter is bumped before the
         * test, so the inner loop is skipped only on the final pass. */
        if (i + 1 >= BR_UI_AB334_COUNT) {
            continue;
        }

        for (j = i + 1; j < BR_UI_AB334_COUNT; j++) {
            uint32_t keyJ = pG->aAB334[2 * j];
            int32_t  aJ;
            uint8_t  bJ;

            if (i < kFilterTo
                && (keyJ == aSkip[0] || keyJ == aSkip[1]
                    || keyJ == aSkip[2])) {
                continue;
            }

            aJ = BrFn10069BC0(pB4DF30, kind, keyJ);
            bJ = BrFn10069C30(pB4DF30, kind, keyJ);

            /* Both answers zero means "unbound", which never conflicts. */
            if (aI == 0 && bI == 0u) {
                continue;
            }
            if (aI != aJ || bI != bJ) {
                continue;
            }

            found = 1;
            pG->aA9D5C0[j] = 1;
            pG->aA9D5C0[i] = 1;
        }
    }
    return found;
}
