/* slice8_88.c -- seven control hooks the slice6_7x builders install.
 *
 * See slice8_88.h for what this module is, which builder line proves each
 * hook-to-slot pairing, the pre-flight result (five of the seven already had
 * bodies elsewhere, and why none of them can be delegated to), the three
 * conflicts it reports, and the one slot it deliberately leaves alone.
 *
 * Transcribed from orig/BRGlide.dll -- the project reference -- at the Glide
 * addresses config/shared.csv pairs with the D3D addresses the builders name,
 * and cross-checked instruction for instruction against orig/BRD3D.dll.
 *
 * ==========================================================================
 * ONE PORT-WIDE DEVIATION, applied everywhere and stated once
 *
 * The original dereferences the text-box vtable pointer with no NULL test,
 * because in the original there is always a vtable.  In this port that vtable
 * is wired by the host and can be NULL while the slot it would reach is
 * unported.  Every virtual call below is therefore guarded, and a guarded-out
 * call is a MISSING EFFECT, not a no-op that has been argued to be
 * equivalent.  slice8_85.c and slice6_73.c guard the same calls the same way.
 * ========================================================================== */
#include "slice8_88.h"

#include <stdio.h>
#include <string.h>

BrUiHook88Ctx g_brHook88;

void BrUiHook88Reset(void)
{
    memset(&g_brHook88, 0, sizeof(g_brHook88));
}

/* ==========================================================================
 * Cross-slice declarations.
 *
 * slice2_25.h and slice3_45.h cannot be included here for the reason
 * slice7_80.c and slice8_85.c both state at length: slice2_25.h declares
 * BrOptObjCtor over its five-field partial view of the phase while
 * slice6_73.h -- which this module needs for BrUi73Hooks -- declares the same
 * symbol over the canonical BrPhase_.  That is slice6_73.h's CONFLICT 1 and
 * the two headers are never combined.
 *
 * So the four foreign globals are declared by hand, copied verbatim from the
 * headers that define them so a future diff of the two is a diff.
 * ========================================================================== */

/* XSLICE port/include/slice2_25.h:308, 311, 352 */
extern int32_t g_brAA2A1C;      /* 0x10AA2A1C  force feedback, 0..1        */
extern int32_t g_brAA2A28;      /* 0x10AA2A28  skid marks, 0..1            */
extern int32_t g_brAA28E8;      /* 0x10AA28E8                              */

/* XSLICE port/include/slice3_45.h:562 -- "a force-feedback device is present
 * and exclusive".  0x118ABDBC. */
extern int32_t g_br18ABDBC;

/* ==========================================================================
 * The tables, read out of orig/BRGlide.dll and confirmed byte for byte
 * against orig/BRD3D.dll.
 *
 * slice2_24.c holds the same four as its own file-static k_AC550 / k_AC590 /
 * k_AC630 / k_AC640, and they agree byte for byte.  They are RESTATED rather
 * than shared because a `static const` in another translation unit is not
 * linkable; br_sprfont.c set that precedent when it restated 0x100408C0's
 * 51-byte table, and slice8_85.c followed it for 0x100AC62C.
 * ========================================================================== */

/* 0x100AC550 (D3D) / 0x100ABCF0 (Glide) -- 16 words.  The extent is the
 * bound slice2_24.c applies; the original's read is unbounded. */
static const uint16_t k_AC550[16] = {
    0x0010, 0x0012, 0x0011, 0x001B, 0x001C, 0x0076, 0x0010, 0x0012,
    0x0011, 0x001B, 0x001C, 0x0076, 0x0000, 0x008F, 0x008F, 0x0000
};

/* 0x100AC590 (D3D) / 0x100ABD30 (Glide) -- 8 signed bytes. */
static const int8_t k_AC590[8] = { 0x17, 0x13, 0x15, 0x16, 0x14, 0, 0, 0 };

/* 0x100AC630 (D3D) / 0x100ABDD0 (Glide) -- 4 signed bytes. */
static const int8_t k_AC630[4] = { 0x61, 0x66, 0, 0 };

/* 0x100AC640 (D3D) / 0x100ABDE0 (Glide) -- 2 dwords, of which the code takes
 * only the low word. */
static const uint32_t k_AC640[2] = { 0x0000008Cu, 0x0000008Du };

/* 0x100AD278 (D3D) / 0x100ACA50 (Glide).  CONFLICT 2 in the header: the image
 * holds DWORDS there --
 *
 *     30 00 00 00  2f 00 00 00  2e 00 00 00  2d 00 00 00  ...
 *
 * -- a descending run of ids, and 0x100414B0 `strcpy`s from that address.
 * What a strcpy actually copies is the byte 0x30 and then the first dword's
 * zero padding: the one-character string "0".  PRESERVED BUG -- the original
 * mistook a record array for a string, and the clear arm of 0x100414B0
 * therefore always displays "0". */
static const char k_AD278[] = "0";

/* The two bounded lookups.  The originals index without any bound at all; the
 * bounds here are slice2_24.c's, restated so the two modules cannot diverge
 * on the answer for an out-of-range index. */
static int16_t Br88TabS8(const int8_t *pTab, size_t cTab, uint32_t i)
{
    return (i < cTab) ? (int16_t)pTab[i] : (int16_t)0;   /* DEVIATION: bound */
}

static int16_t Br88TabU16(const uint16_t *pTab, size_t cTab, uint32_t i)
{
    return (i < cTab) ? (int16_t)pTab[i] : (int16_t)0;   /* DEVIATION: bound */
}

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

/* CONFLICT 1: `movsx eax, byte ptr [0x10AA28B8]`.  The index is SIGNED, so a
 * byte >= 0x80 walks BACKWARDS off the front of the stage array -- exactly
 * what slice5_63.c reproduces on purpose for the same global.  slice6_73.h
 * types the field `uint8_t`; when it adopts `int8_t` this cast deletes. */
static int32_t Br88StageIndex(void)
{
    return (int32_t)(int8_t)g_br73.bAA28B8;
}

/* The stage byte 0x10040730 and 0x100407E0 share.
 *
 *     movsx eax, byte [0x10AA28B8]     ; e
 *     lea   eax, [eax + eax*2]         ; 3e
 *     mov   ecx, [0x10AA28AC | 0x10AA28A4]   ; k
 *     lea   edx, [ecx + eax*4]         ; k + 12e
 *     mov   al, byte [edx*2 + 0x100B3820 (+1 for the high half)]
 *
 * so the byte offset from the array base 0x100B3810 is
 * 0x10 + 24*e + 2*k + hi -- stride 0x18, the f10[] member at +0x10.  Written
 * as byte arithmetic, not `pStages[e].f10[k]`, because neither e nor k is
 * bounded by the original and the C form would be undefined rather than
 * merely wrong. */
static uint32_t Br88StageByte(int32_t e, uint32_t k, int hi)
{
    const uint8_t *p;
    ptrdiff_t      off;

    if (g_brHook88.pStages == NULL) {   /* DEVIATION: the original would fault */
        return 0;
    }
    p   = (const uint8_t *)g_brHook88.pStages;
    off = (ptrdiff_t)0x10 + (ptrdiff_t)24 * e + 2 * (ptrdiff_t)k + hi;
    return p[off];
}

/* The column selector 0x10040730 and 0x100407E0 share.  `mov al, byte
 * [0x10AA28A8] / test al,al`: a BYTE test, not a dword one. */
static uint32_t Br88StageColumn(void)
{
    return (g_brHook88.bAA28A8 != 0) ? (uint32_t)g_br73.nAA28AC
                                     : (uint32_t)g_br73.nAA28A4;
}

/* The two tails a text assignment ends with.  Which pair of vtable slots runs
 * is what separates a CAPTION (+0x04 then +0x10) from a VALUE (+0x08 then
 * +0x2C); slice2_24.h states the same rule.
 *
 * GOTCHA, and it is the original's: the second call is guarded by
 * `test ebx,ebx` where ebx is the text buffer's ADDRESS (control + 0x2B65),
 * which is never null -- so the guard never fires and the call always runs. */
static void Br88NotifyCaption(BrTextBox *pBox)
{
    const BrTextBoxVtbl *pV = pBox->pVtbl;

    if (pV != NULL && pV->pfn04 != NULL) {
        pV->pfn04(pBox);
    }
    if (pV != NULL && pV->pfn10 != NULL) {
        pV->pfn10(pBox);
    }
}

static void Br88NotifyValue(BrTextBox *pBox)
{
    const BrTextBoxVtbl *pV = pBox->pVtbl;

    if (pV != NULL && pV->pfn08 != NULL) {
        pV->pfn08(pBox);
    }
    if (pV != NULL && pV->pfn2C != NULL) {
        pV->pfn2C(pBox);
    }
}

/* `repne scasb` for the length, then `rep movsd`/`rep movsb` of length+1.
 * DEVIATION: the original's copy is unbounded into a 0x400-byte field. */
static void Br88StoreText(BrTextBox *pBox, const char *psz)
{
    size_t cb = strlen(psz);

    if (cb > (size_t)BR_TEXTBOX_MAX - 1u) {
        cb = (size_t)BR_TEXTBOX_MAX - 1u;
    }
    memcpy(pBox->sz, psz, cb);
    pBox->sz[cb] = '\0';
}

/* ==========================================================================
 * Caption setters
 * ========================================================================== */

/* WHAT IT DOES: sets the caption on one menu row by looking the current
 * selection up in a small table. Which selection it uses depends on the game
 * mode. Note that unlike its neighbours this one takes the table entry
 * unsigned, which for the shipped tables makes no difference. */
/* @d3donly 0x10040730 BrUiHook88_10040730 -- glide twin 0x10039C70 claimed by slice2_24.c:BrMenuCap0730 */
int32_t BrUiHook88_10040730(BrUiCtl_ *pCtl)
{
    uint32_t i;

    if (g_br73.n0AA010 != 0) {
        i = (uint32_t)g_br73.n0AC648;
    } else {
        i = Br88StageByte(Br88StageIndex(), Br88StageColumn(), 0);
    }

    /* GOTCHA: `mov cx, word` -- no sign extension, unlike every neighbour. */
    pCtl->w1E20C = (uint16_t)Br88TabU16(k_AC550, 16, i);
    return 1;
}

/* The guard 0x100407A0, 0x100407E0 and 0x100408D0 share.  Non-zero means
 * "bail out".
 *
 * DEVIATION: g_pBrUiNav may be NULL in this port while the host has not built
 * a nav yet.  Reading the current phase as NULL then makes the comparison
 * against an unwired pAA2964 true and the hook answers -2, "leave this item
 * alone" -- which is the safe answer for a screen that does not exist. */
static int Br88IsIdle(void)
{
    const BrPhase_ *pCur = (g_pBrUiNav != NULL) ? g_pBrUiNav->pAA2904 : NULL;

    return (pCur == g_brHook88.pAA2964) && (g_brAA28E8 == 0);
}

/* WHAT IT DOES: the same kind of table-driven caption setter for a
 * neighbouring menu row, but guarded: while the screen it belongs to is not
 * the one the player is on, it answers "leave this item alone" and changes
 * nothing. That answer is unique to this hook. */
/* @d3donly 0x100407E0 BrUiHook88_100407E0 -- glide twin 0x10039D20 claimed by slice2_24.c:BrMenuCap07E0 */
int32_t BrUiHook88_100407E0(BrUiCtl_ *pCtl)
{
    uint32_t i;

    if (Br88IsIdle()) {
        return -2;              /* the reserved "leave this item alone" */
    }

    if (g_br73.n0AA010 != 0) {
        i = (uint32_t)g_br73.nAA2A00;
    } else {
        /* the HIGH byte of the same stage word -- 0x100B3821, not 0x100B3820 */
        i = Br88StageByte(Br88StageIndex(), Br88StageColumn(), 1);
    }

    pCtl->w1E20C = (uint16_t)Br88TabS8(k_AC590, 8, i);
    return 1;
}

/* WHAT IT DOES: sets a menu row's caption from a four-entry table, or --
 * when one particular flag is clear -- from a fixed entry. The fixed one is
 * element one of that same table, not element zero, which is easy to
 * misread. */
/* @d3donly 0x10040950 BrUiHook88_10040950 -- glide twin 0x10039E90 claimed by slice2_24.c:BrMenuCap0950 */
int32_t BrUiHook88_10040950(BrUiCtl_ *pCtl)
{
    /* The original's arm order is kept: the SET arm is the table lookup and
     * the CLEAR arm is the hard-wired one. */
    if (g_br18ABDBC != 0) {
        pCtl->w1E20C = (uint16_t)Br88TabS8(k_AC630, 4, (uint32_t)g_brAA2A1C);
    } else {
        /* 0x100AC631 -- element ONE of the same table, not element zero. */
        pCtl->w1E20C = (uint16_t)(int16_t)k_AC630[1];
    }
    return 1;
}

int32_t BrUiHook88_10040990(BrUiCtl_ *pCtl)
{
    uint32_t i = (uint32_t)g_brAA2A28;
    /* dword table, LOW WORD taken, NOT sign-extended.  DEVIATION: bounded. */
    uint16_t v = (i < 2u) ? (uint16_t)(k_AC640[i] & 0xFFFFu) : (uint16_t)0;

    pCtl->w1E20C = v;
    return 1;
}

/* ==========================================================================
 * 0x100413B0 -- text setter, caption shape
 * ========================================================================== */

int32_t BrUiHook88_100413B0(BrUiCtl_ *pCtl)
{
    /* The original's local really is 0x80 bytes (`sub esp, 0x80`), and its
     * sprintf into it is unbounded.  DEVIATION: bounded here, same extent. */
    char        sz[0x80];
    int32_t     n;
    int         id;
    const char *pszHead;
    const char *pszTail;

    /* Step 1: sprintf(0x10AA2518, "%d", 0x10AA28A0 + 1).  The format string is
     * the literal at 0x100A73C4 (D3D) / 0x100A6B84 (Glide), read out of the
     * image: "%d".  DEVIATION: the original's destination is an unbounded
     * .data buffer whose extent this port declares, so snprintf is used
     * against that extent, and a host that has not wired it is skipped rather
     * than faulted -- exactly what slice8_85.c's Br85TextNumber does. */
    if (g_br73.szAA2518 != NULL && g_br73.cbScratch != 0u) {
        snprintf(g_br73.szAA2518, g_br73.cbScratch, "%d",
                 (int)(g_br73.nAA28A0 + 1));
    }

    /* Step 2: the id comes from a SECOND read of the same global.  `lea
     * eax,[ecx+1] / dec eax / je` three times tests ecx itself against 0, 1
     * and 2; anything else -- including negatives -- falls through to 0xB6. */
    n = g_br73.nAA28A0;
    if (n == 0) {
        id = 0xB3;
    } else if (n == 1) {
        id = 0xB4;
    } else if (n == 2) {
        id = 0xB5;
    } else {
        id = 0xB6;
    }

    /* Step 3: sprintf(local, "%s%s", 0x10AA2518, BrStrGet(id)).  BrStrGet runs
     * BEFORE the sprintf in the original -- its result is pushed as the last
     * argument -- so it is sequenced explicitly here rather than left to C's
     * unspecified argument order.
     *
     * DEVIATION: BrStrGet answers NULL for an id outside the table and the
     * original would hand that straight to sprintf. */
    pszTail = BrStrGet(id);
    pszHead = (g_br73.szAA2518 != NULL) ? g_br73.szAA2518 : "";
    snprintf(sz, sizeof sz, "%s%s", pszHead,
             (pszTail != NULL) ? pszTail : "");

    Br88StoreText(&pCtl->aText[0], sz);
    Br88NotifyCaption(&pCtl->aText[0]);
    return 1;
}

/* ==========================================================================
 * 0x100414B0 -- text setter, value shape
 * ========================================================================== */

/* The four halfwords start 0x1E bytes into the 0x10AA26F0 block (i.e. at
 * 0x10AA270E) and step by 8 per index.  slice5_63.c's BrExt_1005FBC0 folds
 * the SAME four into 0x10AA28C4 with the same three constants; they are
 * restated rather than shared because that module's are file-static. */
#define BR88_AA270E_OFF     0x1E
#define BR88_AA270E_STRIDE  8
#define BR88_AA270E_TERMS   4

static uint32_t Br88SumAA270E(void)
{
    int32_t  base;
    uint32_t sum = 0;
    int      i;

    if (g_br73.aAA26F0 == NULL) {   /* DEVIATION: the original would fault */
        return 0;
    }

    /* CONFLICT 1: the index byte is SIGNED, so a byte >= 0x80 indexes
     * BACKWARDS off the front of the block.  Faithfully reproduced, as in
     * slice5_63.c. */
    base = Br88StageIndex() * BR88_AA270E_STRIDE + BR88_AA270E_OFF;

    /* DEVIATION: the halfwords are read with memcpy from a byte view because
     * 0x10AA270E is not 4-byte aligned, so a uint16_t * into an int32_t array
     * would be misaligned.  `xor esi,esi` runs ONCE, before the loop, and each
     * iteration writes only `si` -- so every term is a ZERO-extended 16-bit
     * value however the record is signed. */
    for (i = 0; i < BR88_AA270E_TERMS; ++i) {
        uint16_t hw;
        memcpy(&hw,
               (const unsigned char *)g_br73.aAA26F0
                   + base + i * (int)sizeof hw,
               sizeof hw);
        sum += hw;
    }
    return sum;
}

/* `_strupr` (0x1007F240 in D3D, MSVCRT's own in Glide), in place, ASCII. */
static void Br88StrUpr(char *psz)
{
    size_t i;

    for (i = 0; psz[i] != '\0'; ++i) {
        if (psz[i] >= 'a' && psz[i] <= 'z') {
            psz[i] = (char)(psz[i] - ('a' - 'A'));
        }
    }
}

int32_t BrUiHook88_100414B0(BrUiCtl_ *pCtl)
{
    /* `sub esp,0x20` then `rep stosd` with ecx = 8: a 0x20-byte local, zeroed
     * before either arm runs. */
    char sz[0x20];

    memset(sz, 0, sizeof sz);

    if (g_br73.nAA289C == 0) {
        /* CONFLICT 2: this copies "0", not the table.  See k_AD278. */
        size_t cb = strlen(k_AD278);
        if (cb > sizeof sz - 1u) {              /* DEVIATION: bounded */
            cb = sizeof sz - 1u;
        }
        memcpy(sz, k_AD278, cb);
        sz[cb] = '\0';
    } else {
        /* `_itoa(sum, buf, 10)`.  The sum is at most 4 * 0xFFFF, so the
         * signed/unsigned question _itoa would raise cannot arise. */
        snprintf(sz, sizeof sz, "%d", (int)Br88SumAA270E());
    }

    /* CONFLICT 3: `dec ecx / jne` on the length, with eax already zero from
     * the scan set-up -- so an EMPTY string returns 0, not 1, and skips both
     * the store and both notifications.  Unreachable in the original: the
     * clear arm yields "0" and _itoa never yields "".  Transcribed, not
     * claimed as tested. */
    if (strlen(sz) == 0u) {
        return 0;
    }

    Br88StrUpr(sz);
    Br88StoreText(&pCtl->aText[0], sz);
    Br88NotifyValue(&pCtl->aText[0]);
    return 1;
}

/* ==========================================================================
 * 0x10042B00 -- the one-shot toggle
 * ========================================================================== */

int32_t BrUiHook88_10042B00(BrUiCtl_ *pCtl)
{
    if (g_brAA28D8 == 0) {
        g_brAA28D8 = 1;
        /* `xor ecx,ecx / test edx,edx / sete cl / store` -- the result is
         * exactly 0 or 1, never the complement of a wider value. */
        pCtl->aText[0].f420 = (pCtl->aText[0].f420 == 0u) ? 1u : 0u;
    }
    return 1;
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook88Install71(BrS71Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }
    pHooks->p10042B00 = BrUiHook88_10042B00;   /* slice6_71.c:634, pfn08 */
}

void BrUiHook88Install72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    pHooks->p10040730 = BrUiHook88_10040730;   /* slice6_72.c:886, :1070 */
    pHooks->p100407E0 = BrUiHook88_100407E0;   /* slice6_72.c:870, :1054 */
    pHooks->p10040950 = BrUiHook88_10040950;   /* slice6_72.c:1510       */
    pHooks->p10040990 = BrUiHook88_10040990;   /* slice6_72.c:1528       */
    pHooks->p100413B0 = BrUiHook88_100413B0;   /* slice6_72.c:1153       */
    pHooks->p100414B0 = BrUiHook88_100414B0;   /* slice6_72.c:1123       */

    /* NOT INSTALLED, deliberately: p100436B0 is slice7_80.c's
     * BrUiOptInstall72's (slice7_80.c:127).  Two installers writing one slot
     * is the fight test_slice8_84.c:340 exists to catch. */
}

void BrUiHook88Install73(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }
    pHooks->p100413B0 = BrUiHook88_100413B0;   /* slice6_73.c:890, pfn04 */
}
