/* slice8_87.c -- the slice2_23 family of control hooks over br_ui.h's
 * canonical BrUiCtl_.  See slice8_87.h for the pre-flight table, the four
 * places the brief and the tree disagree, the four conflicts this module
 * reports and the two slots it leaves NULL.
 *
 * Transcribed from orig/BRGlide.dll -- the project reference -- at the GLIDE
 * address of each pairing, with tools/dumpasm.py.  BRD3D.dll was read for
 * exactly two things: to settle what 0x1008C320 is (CONFLICT 1) and to
 * confirm that the seven .rdata tables are identical in both images.
 *
 * ==========================================================================
 * TWO PORT-WIDE DEVIATIONS, applied everywhere and stated once
 *
 * 1. NULL GUARDS.  The original dereferences a vtable pointer, a global
 *    object pointer and a global table pointer with no test, because in the
 *    original they are always there.  In this port the control vtable, the
 *    text-box vtable, g_pBr72Env, g_pBrUiNav and the two injected tables are
 *    all wired by the host and any of them can be NULL while the slot it
 *    would reach is unported.  Every one is guarded, and a guarded-out call
 *    is a MISSING EFFECT, not a no-op that has been argued to be equivalent.
 *    slice8_85.c and slice6_73.c's builders guard the same way.
 *
 * 2. BOUNDED COPIES.  Every string move in this range is an inlined
 *    `rep movsd` + `rep movsb` over strlen+1 bytes -- strcpy with no bound --
 *    into a fixed .data buffer or into the control's own 0x400-byte caption.
 *    Every one is bounded and NUL-terminated here.  0x1003FA00's scratch is
 *    a 0x74-byte stack buffer in the original (esp+0x10 inside a 0x84-byte
 *    frame) that a longer string smashes; it is BR87_TEXT_MAX here.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice8_87.h"

#include <string.h>

BrUiHook87Ctx g_brHook87;

void BrUiHook87Reset(void)
{
    memset(&g_brHook87, 0, sizeof(g_brHook87));
}

/* ==========================================================================
 * Cross-slice declarations
 *
 * slice2_25.h cannot be included here for the reason slice7_80.c, slice8_84.c
 * and slice8_85.c all state: it declares BrOptObjCtor over its five-field
 * partial view of the phase while slice6_73.h -- which this module needs --
 * declares the same symbol over the canonical BrPhase_.  That is
 * slice6_73.h's CONFLICT 1 and the two headers are never combined.  The two
 * scalars are therefore declared by hand, copied VERBATIM from slice2_25.h so
 * that a future diff of the two is a diff.
 * ========================================================================== */

/* XSLICE port/include/slice2_25.h:343, 352 */
extern int32_t g_brAA287C;      /* 0x10AA287C  0..3, a mode selector */
extern int32_t g_brAA28E8;      /* 0x10AA28E8 */

/* XSLICE 0x1003E070 (Glide 0x10037710) -- slice4_51.h:95 / slice2_23.h:269,
 * both `void (void)`.  A ten-byte thunk; 0x1003EE50 calls it. */
extern void BrFn1003E070(void);

/* ==========================================================================
 * The seven .rdata tables.
 *
 * Restated as `static const` rather than shared, exactly as slice8_85.c
 * restates 0x100AC62C and br_sprfont.c restates 0x100408C0: a `static const`
 * in another translation unit is not linkable.  Every one was extracted from
 * BOTH images and compared dword for dword -- they are identical -- and every
 * extent is fixed by the address of the NEXT table in the image, not guessed.
 *
 *   D3D 0x100AC308 .. 0x100AC348  = Glide 0x100ABAA8 .. 0x100ABAE8   16
 *   D3D 0x100AC3B0 .. 0x100AC3E0  = Glide 0x100ABB50 .. 0x100ABB80   12
 *   D3D 0x100AC3F0 .. 0x100AC400  = Glide 0x100ABB90 .. 0x100ABBA0    4
 *   D3D 0x100AC400 .. 0x100AC408  = Glide 0x100ABBA0 .. 0x100ABBA8    2
 *   D3D 0x100AC408 .. 0x100AC410  = Glide 0x100ABBA8 .. 0x100ABBB0    2
 *   D3D 0x100AC410 .. 0x100AC418  = Glide 0x100ABBB0 .. 0x100ABBB8    2
 *   D3D 0x100AC418 .. 0x100AC420  = Glide 0x100ABBB8 .. 0x100ABBC0    2
 *
 * NOTE on 0x100AC3B0: read in Glide alone it looks 16 entries long, because
 * 0x100ABB80 holds the FOUR ids {0x88, 0x89, 0x8A, 0x8B} that in D3D live at
 * their own symbol 0x100AC3E0 (0x1003FFD0's table, not this module's).  The
 * D3D symbol boundary is what fixes the extent at 12.
 * ========================================================================== */

/* 0x100AC308 -- byte-indexed by a 0x100B3820 record. */
static const int32_t k_AC308[16] = {
    0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C,
    0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C,
    0x00, 0x7D, 0x7D, 0x00
};

/* 0x100AC3B0 -- 0x1003FE80's table. */
static const int32_t k_AC3B0[12] = {
    0x83, 0x84, 0x85, 0x86, 0x87, 0x00,
    0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0x00
};

static const int32_t k_AC3F0[4] = { 0x55, 0x56, 0x57, 0x8C };  /* 0x1003FC40 */
static const int32_t k_AC400[2] = { 0x73, 0x74 };              /* 0x1003FCB0 */
static const int32_t k_AC408[2] = { 0x73, 0x74 };              /* 0x1003FDA0 */
static const int32_t k_AC410[2] = { 0x73, 0x74 };              /* 0x1003FE10 */
static const int32_t k_AC418[2] = { 0x73, 0x74 };              /* 0x1003FD30 */

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

/* CONFLICT 1.  MSVCRT `_stricmp`, which BOTH builds call here: Glide through
 * the import at 0x118F0554 and D3D through the statically linked 0x1008C320.
 * Only the C locale is reproduced -- the fold D3D 0x1008C33A..0x1008C357
 * performs on 'A'..'Z' -- because that is the arm the locale table at
 * 0x118AC358 selects in a stock run and the only one this port can observe.
 * DEVIATION: the original's locale-aware arm is not reproduced.
 * Only "is it zero" is ever tested by a caller in this module. */
static int Br87StriCmp(const char *pszA, const char *pszB)
{
    for (;;) {
        unsigned char a = (unsigned char)*pszA++;
        unsigned char b = (unsigned char)*pszB++;

        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if (a != b) {
            return (a < b) ? -1 : 1;
        }
        if (a == 0u) {
            return 0;
        }
    }
}

/* DEVIATION 2.  A NULL source is the original's fault path -- BrStrGet
 * answers NULL for an id the table does not hold and the original would
 * `repne scasb` off it -- so the copy is SKIPPED rather than turned into an
 * empty string: leaving the destination alone is the smaller lie. */
static void Br87CopyBounded(char *pszDst, size_t cbDst, const char *pszSrc)
{
    size_t cb;

    if (pszDst == NULL || cbDst == 0u || pszSrc == NULL) {
        return;
    }
    cb = strlen(pszSrc);
    if (cb > cbDst - 1u) {
        cb = cbDst - 1u;
    }
    memcpy(pszDst, pszSrc, cb);
    pszDst[cb] = '\0';
}

/* `mov edx,[eax+0x3838] / lea ecx,[eax+0x3838] / push v / call [edx+0x20]`.
 * Returns the original's eax.  DEVIATION: -1 when the vtable is unwired,
 * which is the "no answer" value every caller here already handles.
 * slice8_85.h's CONFLICT 1 is the type: slice3_39.h leaves +0x20 `void *`. */
typedef int32_t (*Br87ListSelFn)(BrTextList *pThis, int32_t v);

static int32_t Br87ListSel(BrTextList *pList, int32_t v)
{
    Br87ListSelFn pfn;

    if (pList->pVtbl == NULL || pList->pVtbl->f20 == NULL) {
        return -1;
    }
    pfn = (Br87ListSelFn)pList->pVtbl->f20;
    return pfn(pList, v);
}

/* slice8_85.h's CONFLICT 2: text-box vtable +0x14 returns an int whose LOW
 * BYTE is tested SIGNED, and slice3_39.h types the slot `void (*)(BrTextBox *)`.
 * Restated rather than silently re-derived. */
typedef int32_t (*Br87BoxAskFn)(BrTextBox *pThis);

/* Glide 0x10038380 == D3D 0x1003EE50 (153 B) -- "the item changed" path.
 *
 * ESP TRACE, because the two frame reads use DIFFERENT displacements for the
 * SAME two arguments and that is the bug this project has shipped twice:
 *     entry            esp = E,  arg0 @ E+4, arg1 @ E+8
 *     10038380  movsx eax,[esp+8]      esp = E      -> E+8  = arg1 (index)
 *     10038385  push ebx               esp = E-4
 *     10038386  mov  ebx,[esp+8]       esp = E-4    -> E+4  = arg0 (control)
 * The index is sign-extended from 16 bits, then scaled `*3 *5 *9 *8` == 0x438
 * and added to the control -- aText[index], not a separate array.
 *
 * Returns 0 on the early-out path and 1 otherwise, exactly as the original.
 * GOTCHA: the early-out's `test esi,esi` is on `pCtl + 0x2B65 + 0x438*i`, an
 * ADDRESS that can never be null.  The guard is dead; kept as this comment.
 *
 * Byte-for-byte the routine slice8_85.c holds as Br85ItemApply.  Repeated
 * rather than shared for the reason slice8_84.c gives for Br84LeavePrologue:
 * that one is static, and this module must not take a link dependency on
 * another module's internals. */
static int32_t Br87ItemApply(BrUiCtl_ *pCtl, int16_t index)
{
    BrTextBox           *pBox = &pCtl->aText[index];
    const BrTextBoxVtbl *pV   = pBox->pVtbl;
    int32_t              ask;

    if (pV != NULL && pV->pfn04 != NULL) {
        pV->pfn04(pBox);
    }

    if (pBox->f420 == 0u) {
        if (pV != NULL && pV->pfn10 != NULL) {
            pV->pfn10(pBox);
        }
        return 0;
    }

    /* `call [ebp+0x14] / test al,al / jle`.  DEVIATION: an unwired slot
     * answers 0, which takes the `jle` arm -- the same arm the original takes
     * when the widget declines. */
    ask = 0;
    if (pV != NULL && pV->pfn14 != NULL) {
        /* Re-read through a union rather than a cast: a direct cast between
         * two function-pointer types with different return types is what
         * -Wcast-function-type-mismatch is for, and silencing it would hide
         * exactly the conflict this is reporting. */
        union { void (*pfnVoid)(BrTextBox *); Br87BoxAskFn pfnAsk; } u;
        u.pfnVoid = pV->pfn14;
        ask = u.pfnAsk(pBox);
    }

    if ((int8_t)(ask & 0xFF) <= 0
        || ((uint32_t)pCtl->flags1C & 2u) != 0u) {

        /* 0x10AA285C (Glide 0x10AC5BB4), br_state.h's BrActiveFlags::override
         * and slice2_23.h's gAA285C.  ONE storage; this reads it. */
        BrActiveFlags *pA = (g_pBrUiNav != NULL) ? g_pBrUiNav->pActive : NULL;

        if (pA != NULL && pA->override == 0) {
            g_brAA28D8  = 0;                  /* 0x10AA28D8 */
            pBox->f420  = 0u;
            /* `and al,0xFD` on the LOW BYTE, then a FULL DWORD store of eax:
             * bit 1 is cleared and the upper 24 bits survive. */
            pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C & ~(uint32_t)2);
        }

        BrFn1003E070();

        /* `mov eax,[ebx+0x10] / test / push ebx / call eax` -- the CONTROL's
         * +0x10 hook, called with the control, result discarded. */
        if (pCtl->pfn10 != NULL) {
            (void)pCtl->pfn10(pCtl);
        }
    }

    if (pV != NULL && pV->pfn10 != NULL) {
        pV->pfn10(pBox);
    }
    return 1;
}

/* `lea edi,[<src>] / rep movs / lea ecx,[ebx+0x2B5C] / call [edx+4]` -- the
 * caption move plus the text box's "text changed" slot. */
static void Br87SetCaption(BrUiCtl_ *pCtl, const char *pszSrc)
{
    BrTextBox *pBox = &pCtl->aText[0];

    Br87CopyBounded(pBox->sz, sizeof(pBox->sz), pszSrc);
    if (pBox->pVtbl != NULL && pBox->pVtbl->pfn04 != NULL) {
        pBox->pVtbl->pfn04(pBox);
    }
}

static int32_t Br87SetCaptionApply(BrUiCtl_ *pCtl, const char *pszSrc)
{
    Br87SetCaption(pCtl, pszSrc);
    (void)Br87ItemApply(pCtl, 0);
    return 1;
}

static int32_t Br87CaptionId(BrUiCtl_ *pCtl, int32_t id)
{
    return Br87SetCaptionApply(pCtl, BrStrGet((int)id));
}

/* DEVIATION: every one of these table reads is unbounded in the original --
 * an out-of-range index reads whatever follows the table.  Bounded here; an
 * out-of-range index yields -1, which BrStrGet answers NULL for, which
 * Br87CopyBounded then skips.  The caption is left as it was. */
static int32_t Br87TableId(const int32_t *aIds, size_t cIds, int32_t i)
{
    if (i < 0 || (size_t)i >= cIds) {
        return -1;
    }
    return aIds[i];
}

/* `mov eax,[0x10AC5D40] / and dword ptr [eax+0x1C],0xFFFFFFEF`, but only when
 * the caption is NON-EMPTY -- the original computes strlen with `repne scasb`
 * and branches on `!= 0`. */
static void Br87ClearBit4IfText(BrUiCtl_ *pTarget, const char *pszText)
{
    if (pszText == NULL || pszText[0] == '\0' || pTarget == NULL) {
        return;
    }
    pTarget->flags1C = (int32_t)((uint32_t)pTarget->flags1C & ~(uint32_t)0x10);
}

/* CONFLICT 4.  `mov eax,[0x10AC5C5C] / mov ecx,[0x10AC5CBC] / cmp eax,ecx`
 * plus `mov eax,[0x10AC5C40] / test eax,eax` -- i.e.
 * 0x10AA2904 == 0x10AA2964 && 0x10AA28E8 == 0.  The original compares two raw
 * dwords, so the two pointers are compared as `const void *` here; see the
 * header for why the port holds them in two different views. */
static int Br87IsSolo(void)
{
    const void *pCur = NULL;
    const void *pRef = NULL;

    if (g_pBrUiNav != NULL) {
        pCur = (const void *)g_pBrUiNav->pAA2904;
        if (g_pBrUiNav->pG != NULL) {
            pRef = (const void *)g_pBrUiNav->pG->pAA2964;
        }
    }
    return (pCur == pRef) && (g_brAA28E8 == 0);
}

/* `mov al,[0x10AC5C00] / test al,al` picks which base the 12-stride index is
 * measured from.  0x10AA28A8 is a BYTE; 0x10AA28AC and 0x10AA28A4 are dwords. */
static int32_t Br87Base28A8(void)
{
    uint8_t b = 0u;

    if (g_pBrUiNav != NULL && g_pBrUiNav->pG != NULL) {
        b = g_pBrUiNav->pG->bAA28A8;
    }
    return (b != 0u) ? g_br73.nAA28AC : g_br73.nAA28A4;
}

/* `mov al, byte ptr [<k>*2 + 0x100B3028]` (byte 0) and
 * `mov cl, byte ptr [<k>*2 + 0x100B3029]` (byte 1) -- one 2-byte record.
 * DEVIATION: unbounded in the original; 0 when the table is unwired or the
 * record index is out of range. */
static int32_t Br87Rec(int32_t k, unsigned which)
{
    if (g_brHook87.pB3820 == NULL || k < 0
        || (size_t)k >= g_brHook87.cB3820) {
        return 0;
    }
    return (int32_t)g_brHook87.pB3820[2u * (size_t)k + which];
}

/* `mov ecx, dword ptr [eax*4 + 0x100BCAB0]` then `test byte ptr [ecx+4],0x10`.
 * DEVIATION: bounded, and NULL when unwired -- which takes the same arm as a
 * clear bit. */
static const void *Br87Ent(int32_t a)
{
    if (g_brHook87.apBD2A8 == NULL || a < 0
        || (size_t)a >= g_brHook87.cBD2A8) {
        return NULL;
    }
    return g_brHook87.apBD2A8[a];
}

/* ==========================================================================
 * The list-poll hook
 * ========================================================================== */

int32_t BrUiHook87_1003EAE0(BrUiCtl_ *pCtl)
{
    int32_t v = Br87ListSel(&pCtl->list, g_br0AB3F4);

    if (v >= 0) {
        g_br0AB3F4 = v;
    }
    return 1;
}

/* ==========================================================================
 * Text read-back hooks
 * ========================================================================== */

int32_t BrUiHook87_1003EF90(BrUiCtl_ *pCtl)
{
    char *pszText;

    (void)Br87ItemApply(pCtl, 0);
    pszText = pCtl->aText[0].sz;                       /* control +0x2B65 */

    Br87ClearBit4IfText((g_pBr72Env != NULL) ? g_pBr72Env->pAA29E8 : NULL,
                        pszText);

    if (g_pBr72Env != NULL
        && Br87StriCmp(g_pBr72Env->szA9CDF0, pszText) != 0) {
        Br87CopyBounded(g_pBr72Env->szA9CDF0,
                        sizeof(g_pBr72Env->szA9CDF0), pszText);
        /* GOTCHA (a real defect, preserved): this mirror is INSIDE the
         * differs-branch, so 0x10B4E1E4 goes stale whenever the caption is
         * unchanged.  CONFLICT 3 is where its storage lives. */
        Br87CopyBounded(g_brHook87.szB4E1E4, sizeof(g_brHook87.szB4E1E4),
                        g_pBr72Env->szA9CDF0);
    }
    return 1;
}

/* 0x1003F020. THE SECOND TRANSCRIPTION OF THIS ADDRESS -- slice2_23.c:715 has
 * the first, as BrUiFn1003F020 over that module's byte-image BrUiObj. Two
 * models of one object is the established pattern here, but two NAMES for one
 * address is not, and CONVENTIONS.md forbids it. Recorded rather than silently
 * left: they must not drift.
 *
 * The listing, and it is unguarded:
 *
 *   1003F030  repne scasb                  strlen of pCtl + 0x2B65
 *   1003F034  dec ecx
 *   1003F035  je  0x1003F040               empty string -> do nothing
 *   1003F037  mov eax, [0x10AA29E8]        NO NULL TEST
 *   1003F03C  and dword ptr [eax+0x1C], 0xFFFFFFEF
 *
 * DEVIATION, now stated instead of assumed: the NULL check on g_pBr72Env below
 * is OURS. The original dereferences 0x10AA29E8 unconditionally and faults if
 * it is null; the slice2_23 copy of this function correctly has no guard. The
 * equivalence audit flagged this one as an undocumented divergence, which is
 * the right call -- an undocumented guard is indistinguishable from a
 * misreading, and this project has produced both.
 *
 * It is kept rather than removed because 0x10AA29E8 has no owner in this tree
 * yet, so the pointer is genuinely NULL here and the original's fault would be
 * a harness crash rather than reproduced behaviour. When the owner lands, this
 * guard should go. */
int32_t BrUiHook87_1003F020(BrUiCtl_ *pCtl)
{
    /* No apply and no copy -- the bit-4 clear alone, and only when the string
     * is NON-empty (`dec ecx / je` at 0x1003F034). */
    Br87ClearBit4IfText((g_pBr72Env != NULL) ? g_pBr72Env->pAA29E8 : NULL,
                        pCtl->aText[0].sz);
    return 1;
}

int32_t BrUiHook87_1003F210(BrUiCtl_ *pCtl)
{
    char *pszText;

    (void)Br87ItemApply(pCtl, 0);
    pszText = pCtl->aText[0].sz;

    Br87ClearBit4IfText(g_brS71.pAA29BC, pszText);

    if (g_brS71.pA9D018 != NULL
        && Br87StriCmp(g_brS71.pA9D018, pszText) != 0) {
        Br87CopyBounded(g_brS71.pA9D018, (size_t)BR71_A9D018_SIZE, pszText);
    }
    return 1;
}

/* @n64 0x8021C848 located */
int32_t BrUiHook87_1003F280(BrUiCtl_ *pCtl)
{
    Br87ClearBit4IfText(g_brS71.pAA29BC, pCtl->aText[0].sz);
    return 1;
}

/* ==========================================================================
 * Caption hooks -- one table lookup each
 * ========================================================================== */

/* @n64 0x8026E820 located */
int32_t BrUiHook87_1003FC40(BrUiCtl_ *pCtl)
{
    return Br87CaptionId(pCtl, Br87TableId(k_AC3F0, 4u, g_brAA287C));
}

int32_t BrUiHook87_1003FCB0(BrUiCtl_ *pCtl)
{
    int32_t id;

    if (g_pBr72Env != NULL && g_pBr72Env->n18ABDBC != 0) {
        id = Br87TableId(k_AC400, 2u, g_pBr72Env->nAA2A1C);
    } else {
        /* `push 0x74` -- a literal id, not a table read. */
        id = 0x74;
    }
    return Br87CaptionId(pCtl, id);
}

int32_t BrUiHook87_1003FD30(BrUiCtl_ *pCtl)
{
    int32_t i = (g_pBr72Env != NULL) ? g_pBr72Env->nAA2A28 : 0;

    return Br87CaptionId(pCtl, Br87TableId(k_AC418, 2u, i));
}

int32_t BrUiHook87_1003FDA0(BrUiCtl_ *pCtl)
{
    int32_t i = (g_pBr72Env != NULL) ? g_pBr72Env->nAA2A20 : 0;

    return Br87CaptionId(pCtl, Br87TableId(k_AC408, 2u, i));
}

int32_t BrUiHook87_1003FE10(BrUiCtl_ *pCtl)
{
    int32_t i = (g_pBr72Env != NULL) ? g_pBr72Env->nAA2A24 : 0;

    return Br87CaptionId(pCtl, Br87TableId(k_AC410, 2u, i));
}

/* ==========================================================================
 * The two record-driven caption hooks
 * ========================================================================== */

int32_t BrUiHook87_1003FA00(BrUiCtl_ *pCtl)
{
    /* DEVIATION 2: the original stages this in the 0x74 bytes at esp+0x10 of
     * a 0x84-byte frame, which a longer string smashes. */
    char        szTmp[BR87_TEXT_MAX];
    int32_t     a;
    const void *pEnt;

    /* ESP TRACE.  The control is read at TWO different displacements because
     * of an intervening push, and they are the same argument:
     *     entry              esp = E,   arg0 @ E+4
     *     10038F4B sub 0x84  esp = E-0x84
     *     pushes ebx/esi/edi esp = E-0x90
     *     10038F61 push 0x1b esp = E-0x94
     *     10038F68 mov ebx,[esp+0x98]              -> E+4  = arg0
     *     100390B3 mov ebx,[esp+0x94] (esp=E-0x90) -> E+4  = arg0
     * and the saved dword and the scratch buffer are DIFFERENT slots:
     *     100390C8 mov [esp+0x10],edx (esp=E-0x94) -> E-0x84  the save slot
     *     10039091 lea edx,[esp+0x10] (esp=E-0x90) -> E-0x80  the scratch  */

    if (Br87IsSolo()) {
        /* The solo arm jumps straight into the common copy with the string
         * table's own pointer as the source -- the scratch is bypassed. */
        return Br87CaptionId(pCtl, 0x1B);
    }

    szTmp[0] = '\0';

    if (g_br73.n0AA010 == 0) {
        /* CONFLICT 2: `movsx eax,byte ptr [0x10AC5C10]` -- SIGNED. */
        int32_t i    = (int32_t)(int8_t)g_br73.bAA28B8;
        int32_t base = Br87Base28A8();

        Br87CopyBounded(szTmp, sizeof(szTmp),
                        BrStrGet((int)Br87TableId(k_AC308, 16u,
                                                  Br87Rec(base + 12 * i, 0u))));
        /* The original RE-LOADS both the index byte and the base global here
         * rather than reusing the values above (0x10038FD8 / 0x10038FE5 on
         * one arm, 0x10039042 / 0x1003904F on the other).  Reproduced
         * literally: nothing in between can change them, but a debugger sees
         * the second pair of loads. */
        i    = (int32_t)(int8_t)g_br73.bAA28B8;
        base = Br87Base28A8();
        a    = Br87Rec(base + 12 * i, 0u);
    } else {
        Br87CopyBounded(szTmp, sizeof(szTmp),
                        BrStrGet((int)Br87TableId(k_AC308, 16u,
                                                  g_br73.n0AC648)));
        a = g_br73.n0AC648;      /* re-loaded at 0x100390A2 */
    }

    pEnt = Br87Ent(a);
    if (pEnt != NULL && (((const unsigned char *)pEnt)[4] & 0x10u) != 0u) {
        /* GOTCHA: the field SAVED is the CONTROL's +0x40, and the field the
         * saved dword is RESTORED into is aText[0].y (control +0x2F70).  The
         * original saves one and writes it back over the other; +0x2F70's
         * previous contents are lost.  Preserved.
         *
         * Both moves are raw dwords in the original (`mov`, not `fld/fstp`),
         * so they are raw dwords here -- a float assignment could quieten a
         * signalling NaN and change the bits. */
        uint32_t saved;

        memcpy(&saved, &pCtl->y, sizeof(saved));            /* +0x40   */
        pCtl->aText[0].y = 130.0f;                          /* +0x2F70 */
        Br87SetCaption(pCtl, BrStrGet(0xB0));
        (void)Br87ItemApply(pCtl, 0);
        memcpy(&pCtl->aText[0].y, &saved, sizeof(saved));
    }

    return Br87SetCaptionApply(pCtl, szTmp);
}

int32_t BrUiHook87_1003FE80(BrUiCtl_ *pCtl)
{
    int32_t id;

    if (Br87IsSolo()) {
        BrTextBox *pBox = &pCtl->aText[0];

        /* `fsub [0x10077628]` where that dword is 8.0f, then after the
         * caption `fsub [0x1007762C]` where THAT dword is -8.0f.  The second
         * is a subtract of a negative, not an add, and is kept in the
         * original's negated form. */
        pBox->y = pBox->y - 8.0f;
        (void)Br87CaptionId(pCtl, 0x1C);
        pBox->y = pBox->y - (-8.0f);
        return 1;
    }

    if (g_br73.n0AA010 == 0) {
        int32_t i    = (int32_t)(int8_t)g_br73.bAA28B8;   /* CONFLICT 2 */
        int32_t base = Br87Base28A8();

        /* Byte 1 of the same 2-byte record 0x1003FA00 reads byte 0 of --
         * the original addresses 0x100B3029, one past the table base. */
        id = Br87TableId(k_AC3B0, 12u, Br87Rec(base + 12 * i, 1u));
    } else {
        id = Br87TableId(k_AC3B0, 12u, g_br73.nAA2A00);
    }
    return Br87CaptionId(pCtl, id);
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook87Install71(BrS71Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    pHooks->p1003EAE0 = BrUiHook87_1003EAE0;   /* pfn04, slice6_71.c:451 */
    pHooks->p1003F210 = BrUiHook87_1003F210;   /* pfn04, slice6_71.c:635 */
    pHooks->p1003F280 = BrUiHook87_1003F280;   /* pfn10, slice6_71.c:636 */

    /* p1003F720 is the fourth slice2_23 slot on this table and NO builder in
     * slice6_71.c installs it -- slice8_84.c says the same.  It is also the
     * one member of the family that does not always return 1, so it is left
     * for the pass that has a caller to test it against. */
}

void BrUiHook87Install72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    /* PRE-FLIGHT (1): 0x1003EC30 is byte-for-byte 0x1003EB10, which
     * slice8_85.c already holds over the canonical control.  Wired, not
     * re-transcribed, and slice8_85.c is not touched. */
    pHooks->p1003EC30 = BrUiHook85_1003EB10;   /* pfn04, slice6_72.c:1289 */

    pHooks->p1003EF90 = BrUiHook87_1003EF90;   /* pfn04, slice6_72.c:686  */
    pHooks->p1003F020 = BrUiHook87_1003F020;   /* pfn10, slice6_72.c:687  */
    pHooks->p1003F5E0 = BrUiHook87_1003F5E0;   /* pfn04, slice6_72.c:918  */
    pHooks->p1003F680 = BrUiHook87_1003F680;   /* pfn04, slice6_72.c:924  */
    pHooks->p1003FA00 = BrUiHook87_1003FA00;   /* pfn04, :894 and :1078   */
    pHooks->p1003FCB0 = BrUiHook87_1003FCB0;   /* pfn04, slice6_72.c:1520 */
    pHooks->p1003FD30 = BrUiHook87_1003FD30;   /* pfn04, slice6_72.c:1536 */
    pHooks->p1003FDA0 = BrUiHook87_1003FDA0;   /* pfn04, slice6_72.c:1568 */
    pHooks->p1003FE10 = BrUiHook87_1003FE10;   /* pfn04, slice6_72.c:1552 */
    pHooks->p1003FE80 = BrUiHook87_1003FE80;   /* pfn04, :878 and :1062   */

    /* p1003E7A0 is the twelfth slice2_23 slot on this table and belongs to
     * slice8_85.c (BrUiHook85_1003E7A0); this installer must not race it. */
}

void BrUiHook87Install73(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    pHooks->p1003FC40 = BrUiHook87_1003FC40;   /* pfn04, slice6_73.c:394 */

    /* NOT INSTALLED, deliberately, and a visible hole rather than a silent
     * one: p1003ECB0 (control pfn08, slice6_73.c:496).  See PRE-FLIGHT (2)
     * in slice8_87.h -- it has no body in orig/BRGlide.dll at all and its one
     * callee 0x1007A7D0 is d3d_only and unported. */
}
