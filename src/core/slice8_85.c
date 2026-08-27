/* slice8_85.c -- the control hooks the six slice6_73.c builders install.
 *
 * See slice8_85.h for what this module is, which builder line proves each
 * hook-to-slot pairing, which addresses are transcribed a second time and
 * why delegation is not available, the five conflicts it reports, and the
 * slots it deliberately leaves NULL.
 *
 * Transcribed from orig/BRGlide.dll where the pairing in config/shared.csv is
 * a body match, and from orig/BRD3D.dll for the six 0x100440xx one-liners and
 * for 0x1003ECB0 / 0x1003F050 / 0x1003F0B0 / 0x10040A50 / 0x10040AC0, which
 * have no Glide partner recorded.  Every body was read at the D3D address the
 * builders name, because that is the address slice6_73.c's transcription is
 * keyed on.
 *
 * ==========================================================================
 * ONE PORT-WIDE DEVIATION, applied everywhere and stated once
 *
 * The original dereferences a vtable pointer with no NULL test, because in the
 * original there is always a vtable.  In this port the control vtable, the
 * text-box vtable and the embedded list's vtable are all wired by the host and
 * any of them can be NULL while the slot it would reach is unported -- see
 * br_uinav.c's BrUiNavInstallCtlVtbl, which deliberately leaves +0x14 alone.
 * Every virtual call below is therefore guarded, and a guarded-out call is a
 * MISSING EFFECT, not a no-op that has been argued to be equivalent.
 * slice6_73.c's builders guard their own f34 / f14 / f10 calls the same way.
 * ========================================================================== */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice8_85.h"

#include "br_sprfont.h"   /* BrSprFontKindHook_10047360 -- 0x10047360 over
                           * br_ui.h's control, br_sprfont.c                */

#include <stdio.h>
#include <string.h>

BrUiHook85Ctx g_brHook85;

void BrUiHook85Reset(void)
{
    memset(&g_brHook85, 0, sizeof(g_brHook85));
}

/* ==========================================================================
 * Cross-slice declarations.
 *
 * slice2_25.h CANNOT be included here, for the reason slice7_80.c states at
 * length: it declares BrOptObjCtor over its five-field partial view of the
 * phase while slice6_73.h -- which this module needs for BrUi73Hooks --
 * declares the same symbol over the canonical BrPhase_.  That is
 * slice6_73.h's CONFLICT 1 and the two headers are never combined.
 *
 * So the four slice2_25 symbols are declared by hand.  Three are copied
 * verbatim from slice2_25.h so a future diff of the two is a diff.
 * ========================================================================== */

/* XSLICE port/include/slice2_25.h:312-313, 333, 343 */
extern int32_t g_brB4E708;      /* 0x10B4E708  SFX volume index            */
extern int32_t g_brB4E70C;      /* 0x10B4E70C  CD volume index             */
extern int32_t g_br0AB3D8;      /* 0x100AB3D8  which volume row ran last   */
extern int32_t g_brAA287C;      /* 0x10AA287C  0..3, a mode selector       */

/* XSLICE 0x10043E70 -- slice2_25.h:618 spells the parameter `BrGameObj *`.
 * It is declared `void *` here because this module has no BrGameObj and MUST
 * NOT acquire one, and because the parameter is never read: the body (Glide
 * 0x1003D3C0, 216 bytes) touches only globals and its own SEH frame.  That is
 * exactly why the three callers below can pass NULL.  Same workaround, same
 * reason, as slice7_80.c's six hand-declared cyclers. */
extern int BrOptOpen2948(void *pUnused);

/* XSLICE 0x1003E070 -- slice4_51.h:95 / slice2_23.h:269, both `void (void)`.
 * A ten-byte thunk; 0x1003EE50 calls it. */
extern void BrFn1003E070(void);

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

/* CONFLICT 1: the embedded list's vtable slots +0x20 and +0x24.  slice3_39.h
 * leaves both `void *`; slice2_23.h types the same two slots on its own view
 * of this object.  The types below are slice2_23.h's, restated rather than
 * silently re-derived, and the cast is confined to these two helpers so there
 * is exactly one place to delete from when slice3_39.h adopts them. */
typedef int32_t (*Br85ListSelFn)(BrTextList *pThis, int32_t v);
typedef void    (*Br85ListAckFn)(BrTextList *pThis, int32_t v);

/* `mov edx,[eax+0x3838] / lea ecx,[eax+0x3838] / push v / call [edx+0x20]`.
 * Returns the original's eax.  DEVIATION: -1 when the vtable is unwired,
 * which is the "no answer" value every caller already handles. */
static int32_t Br85ListSel(BrTextList *pList, int32_t v)
{
    Br85ListSelFn pfn;

    if (pList->pVtbl == NULL || pList->pVtbl->f20 == NULL) {
        return -1;
    }
    pfn = (Br85ListSelFn)pList->pVtbl->f20;
    return pfn(pList, v);
}

static void Br85ListAck(BrTextList *pList, int32_t v)
{
    Br85ListAckFn pfn;

    if (pList->pVtbl == NULL || pList->pVtbl->f24 == NULL) {
        return;
    }
    pfn = (Br85ListAckFn)pList->pVtbl->f24;
    pfn(pList, v);
}

/* The three-argument message send every draw hook uses: control vtable +0x14,
 * `__thiscall` on the control with (msg, x, y).
 *
 * THE SLOT IS READ ONCE PER HOOK, NOT ONCE PER MESSAGE.  Both draw hooks load
 * `[[pCtl] + 0x14]` a single time, spill the pointer into the incoming
 * argument slot, and reach every later message through the spill:
 *
 *   0x1003E7A0  0x1003E7E2 mov eax,[esi]   / 0x1003E7E7 mov eax,[eax+0x14]
 *               0x1003E7F0 mov [esp+0x28],eax   (esp = E-36, so E+4 -- the
 *                                                argument slot, reused)
 *               0x1003E7F4 call eax             (the 0x3D message)
 *               0x1003E806 call [esp+0x28]      (each 0x3B)
 *               0x1003E823 call [esp+0x28]      (the 0x3C)
 *
 *   0x1003E980  0x1003E99C mov eax,[ebx]   / 0x1003E9A3 mov eax,[eax+0x14]
 *               0x1003E9AA mov [esp+0x20],eax   (esp = E-28, so E+4 again)
 *               0x1003E9AE call eax             (the 0x74 message)
 *               0x1003E9C1 call [esp+0x20]      (every 0x75)
 *
 * Every one of those displacements resolves to E+4 once the pushes between
 * them are counted, so it is one slot in both functions.  It matters because a
 * handler that swaps the control's vtable mid-draw does NOT change where the
 * rest of the row is sent -- so the send takes the pointer, and each hook
 * fetches it exactly once.
 *
 * DEVIATION, unchanged from before: the original calls the slot with no null
 * check at all.  The guard here answers an unwired vtable by doing nothing,
 * which is the only safe reading on a host where a null call is not a trap. */
typedef void (*Br85MsgFn)(BrUiCtl_ *pThis, int32_t msg, int32_t a, int32_t b);

static Br85MsgFn Br85MsgSlot(const BrUiCtl_ *pCtl)
{
    return (pCtl->pVtbl != NULL) ? pCtl->pVtbl->f14 : NULL;
}

static void Br85Msg(Br85MsgFn pfn, BrUiCtl_ *pCtl, int32_t msg,
                    int32_t x, int32_t y)
{
    if (pfn != NULL) {
        pfn(pCtl, msg, x, y);
    }
}

/* CONFLICT 2: text-box vtable +0x14 returns an int whose LOW BYTE is tested
 * SIGNED.  slice3_39.h types the slot `void (*)(BrTextBox *)`. */
typedef int32_t (*Br85BoxAskFn)(BrTextBox *pThis);

/* 0x1003EE50 (153 bytes) -- "the item changed" path, shared by 0x1003F050 and
 * 0x1003F0B0.  slice2_23.c has this body as BrUiItemApply over its byte image;
 * this is the same routine over br_ui.h's control, and the two were derived
 * from the same listing.
 *
 * Returns 0 on the early-out path and 1 otherwise, exactly as the original.
 *
 * GOTCHA: the early-out's `test esi,esi` is on `pCtl + 0x2B65 + stride*i`, an
 * ADDRESS that can never be null.  The guard is dead; kept as this comment.
 * GOTCHA: the index is sign-extended from 16 bits (`movsx eax,[esp+8]`). */
/* WHAT IT DOES: finishes an edit the player has made in a menu box -- it asks
 * the box whether the new value is acceptable and, if it is not (or if the
 * control is already flagged as needing to close), takes the box out of edit
 * mode and runs the control's own "something changed" handler. A box that was
 * not being edited in the first place is simply refreshed and left alone. */
/* @implements 0x1003EE50 d3d Br85ItemApply */
static int32_t Br85ItemApply(BrUiCtl_ *pCtl, int16_t index)
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

    /* `call [ebp+0x14] / test al,al / jle` -- CONFLICT 2.  DEVIATION: an
     * unwired slot answers 0, which takes the `jle` arm, i.e. the same arm the
     * original takes when the widget declines. */
    ask = 0;
    if (pV != NULL && pV->pfn14 != NULL) {
        /* Re-read through a union rather than a cast: a direct cast between
         * two function-pointer types with different return types is what
         * -Wcast-function-type-mismatch is for, and silencing the diagnostic
         * would hide exactly the conflict this is reporting.  The union makes
         * the reinterpretation explicit and local. */
        union { void (*pfnVoid)(BrTextBox *); Br85BoxAskFn pfnAsk; } u;
        u.pfnVoid = pV->pfn14;
        ask = u.pfnAsk(pBox);
    }

    if ((int8_t)(ask & 0xFF) <= 0
        || ((uint32_t)pCtl->flags1C & 2u) != 0u) {

        /* 0x10AA285C, which br_state.h models as BrActiveFlags::override and
         * slice2_23.h names gAA285C.  ONE storage; this reads it. */
        BrActiveFlags *pA = g_pBrUiNav->pActive;
        if (pA != NULL && pA->override == 0) {
            g_brAA28D8   = 0;
            pBox->f420   = 0u;
            /* `and al,0xFD` on the low byte, then a FULL DWORD store of eax:
             * bit 1 is cleared and the upper 24 bits survive. */
            pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C & ~(uint32_t)2);
        }

        BrFn1003E070();

        /* `mov eax,[ebx+0x10] / test / push ebx / call eax` -- the CONTROL's
         * +0x10 hook, called with the control. */
        if (pCtl->pfn10 != NULL) {
            (void)pCtl->pfn10(pCtl);
        }
    }

    if (pV != NULL && pV->pfn10 != NULL) {
        pV->pfn10(pBox);
    }
    return 1;
}

/* ==========================================================================
 * Draw hooks -- three messages through control vtable +0x14
 * ========================================================================== */

/* WHAT IT DOES: draws the frame round a menu box, stretched to fit the text
 * inside it -- a left end piece, as many middle pieces as the width needs,
 * then a right end piece. A box whose width comes out negative would draw
 * middle pieces essentially for ever, because the count is treated as
 * unsigned; that is the original's behaviour and is preserved. */
/* @implements 0x1003E7A0 d3d BrUiHook85_1003E7A0 */
int32_t BrUiHook85_1003E7A0(BrUiCtl_ *pCtl)
{
    const BrTextBox *pBox = &pCtl->aText[0];
    int32_t  x0 = BrFtolTrunc(pBox->x) - 3;       /* +0x2F6C, then -3        */
    int32_t  y  = BrFtolTrunc(pBox->y) - 0x0C;    /* +0x2F70, then -0x0C     */
    int32_t  w  = pBox->width;                    /* +0x2F66, movsx          */
    /* `cdq / and edx,0xF / add / sar 4` -- a signed divide by 16 truncating
     * toward zero, then one more. */
    int32_t  nSigned = (w / 16) + 1;
    uint32_t n       = (uint32_t)nSigned;
    uint32_t nRun    = 0u;
    int32_t  x       = x0;
    /* 0x1003E7E2/E7: read AFTER both __ftol calls, and only once. */
    Br85MsgFn pfn    = Br85MsgSlot(pCtl);

    Br85Msg(pfn, pCtl, 0x3D, x0 - 8, y);

    /* GOTCHA: `test ebx,ebx / jbe` -- the guard is UNSIGNED, so only n == 0
     * skips the loop.  A negative nSigned runs it ~2^32 times, exactly as the
     * original does.  Kept deliberately; slice2_23.c's twin says the same. */
    if (n != 0u) {
        uint32_t left = n;
        do {
            Br85Msg(pfn, pCtl, 0x3B, x, y);
            x += 0x10;
            left--;
        } while (left != 0u);
        nRun = n;
    }

    /* The third message's x is recomputed from the SAVED count and the SAVED
     * start, not from the walking x. */
    Br85Msg(pfn, pCtl, 0x3C, (int32_t)(nRun << 4) + x0, y);
    return 1;
}

int32_t BrUiHook85_1003E980(BrUiCtl_ *pCtl)
{
    /* Written out rather than routed through a shared helper: the original
     * has this body inline in each of the two row hooks, which is why the
     * factored form comes out 32 bytes against the original's 92. */
    /* Plain casts, not BrFtolTrunc: the original leaves the value on the x87
     * stack and calls MSVC's own __ftol helper, where the wrapper pushes it
     * as an integer argument instead. */
    int32_t  x = (int32_t)pCtl->x;
    int32_t  y = (int32_t)pCtl->y + 0x13;
    uint32_t i;
    Br85MsgFn pfn = Br85MsgSlot(pCtl);

    Br85Msg(pfn, pCtl, 0x74, x, y);

    for (i = 0u; i < (uint32_t)g_brB4E708; i++) {
        Br85Msg(pfn, pCtl, 0x75, x, y);
        x += 0x0C;
    }
    return 1;
}

/* WHAT IT DOES: draws the music volume bar. */
/* @implements 0x1003E9E0 d3d BrUiHook85_1003E9E0 */
int32_t BrUiHook85_1003E9E0(BrUiCtl_ *pCtl)
{
    /* Plain casts, not BrFtolTrunc: the original leaves the value on the x87
     * stack and calls MSVC's own __ftol helper, where the wrapper pushes it
     * as an integer argument instead. */
    int32_t  x = (int32_t)pCtl->x;
    int32_t  y = (int32_t)pCtl->y + 0x13;
    uint32_t i;
    Br85MsgFn pfn = Br85MsgSlot(pCtl);

    Br85Msg(pfn, pCtl, 0x74, x, y);

    for (i = 0u; i < (uint32_t)g_brB4E70C; i++) {
        Br85Msg(pfn, pCtl, 0x75, x, y);
        x += 0x0C;
    }
    return 1;
}

/* ==========================================================================
 * Code and geometry hooks
 * ========================================================================== */

/* WHAT IT DOES: picks which of two pictures a control is drawn with,
 * depending on which of the two volume rows the player last touched -- the
 * highlight that shows which row the cursor is on. */
/* @implements 0x1003E950 d3d BrUiHook85_1003E950 */
int32_t BrUiHook85_1003E950(BrUiCtl_ *pCtl)
{
    /* Note the inversion relative to the usual "flag set -> higher value". */
    uint16_t c = (g_br0AB3D8 != 0) ? (uint16_t)0x68 : (uint16_t)0x69;

    pCtl->aStepId[0] = c;      /* +0x2A40, a WORD store */
    pCtl->w1E20C     = c;      /* +0x1E20C, a WORD store */
    return 1;
}

/* WHAT IT DOES: slides a control sideways to sit at the end of whichever
 * volume bar is currently selected, so the marker follows the level the
 * player is setting. */
/* @implements 0x1003EA40 d3d BrUiHook85_1003EA40 */
int32_t BrUiHook85_1003EA40(BrUiCtl_ *pCtl)
{
    /* Both arms are written out: the original duplicates the lea/fild/fstp
     * rather than joining after the load.  `lea ecx,[eax*8+0x4A]` then
     * `fild dword` -- the sum is 32-bit wraparound, read back SIGNED. */
    int32_t v;

    if (g_br0AB3D8 != 0) {
        v = g_brB4E708 * 8 + 0x4A;
        pCtl->x = (float)v;    /* +0x3C */
        return 1;
    }
    v = g_brB4E70C * 8 + 0x4A;
    pCtl->x = (float)v;
    return 1;
}

int32_t BrUiHook85_10040930(BrUiCtl_ *pCtl)
{
    /* 0x100AC62C, four bytes, read out of orig/BRD3D.dll.  slice2_24.c holds
     * the same four as its file-static k_AC62C; they agree byte for byte.
     * They are restated rather than shared because a `static const` in another
     * translation unit is not linkable, and br_sprfont.c set the precedent
     * when it restated 0x100408C0's 51-byte table for the same reason. */
    static const int8_t k_AC62C[4] = { 0x45, 0x44, 0x43, 0x46 };
    uint32_t i = (uint32_t)g_brAA287C;

    /* DEVIATION: the original's `movsx cx,[eax+0x100AC62C]` is unbounded and a
     * mode outside 0..3 reads whatever follows the table.  Bounded here, the
     * same bound slice2_24.c applies. */
    pCtl->w1E20C = (uint16_t)(int16_t)((i < 4u) ? (int16_t)k_AC62C[i]
                                                : (int16_t)0);
    return 1;
}

/* ==========================================================================
 * List-poll hooks -- ask the embedded list at +0x3838 for a new value
 * ========================================================================== */

/* WHAT IT DOES: asks a scrolling list which row the player has moved to and
 * remembers the answer as the current selection. If the list declines to
 * answer, the previous selection stands. When a name is being edited it also
 * tells the list that the selection has been taken. */
/* @implements 0x1003EB10 d3d BrUiHook85_1003EB10 */
int32_t BrUiHook85_1003EB10(BrUiCtl_ *pCtl)
{
    BrTextList *pList = &pCtl->list;
    int32_t     v     = Br85ListSel(pList, g_br0AB3F4);

    if (v >= 0) {
        g_br0AB3F4 = v;
    } else {
        /* The original RELOADS the global on this arm, so `v` below is the
         * old value and not the negative answer. */
        v = g_br0AB3F4;
    }

    if (g_brAA28D8 != 0 && v >= 0) {
        Br85ListAck(pList, v);
    }
    return 1;
}

int32_t BrUiHook85_1003ED10(BrUiCtl_ *pCtl)
{
    int32_t v = Br85ListSel(&pCtl->list, g_brHook85.nAA2A2C);

    if (v >= 0) {
        g_brHook85.nAA2A2C = v;
    }
    return 1;
}

/* WHAT IT DOES: the same, for the twelve-entry car list -- it tells the list
 * where the cursor is, treating anything outside the twelve as "nowhere", and
 * takes back whatever row the list reports. Note the range check is applied
 * only to what it sends, never to what it stores. */
/* @implements 0x1003EE20 d3d BrUiHook85_1003EE20 */
int32_t BrUiHook85_1003EE20(BrUiCtl_ *pCtl)
{
    int32_t n = g_br73.nAA2A34;
    int32_t v;

    /* `jl -> -1`, `cmp 0xC / jl -> keep`, else -1.  The clamp is applied to
     * what is SENT; whatever comes back is stored unfiltered. */
    if (n < 0 || n >= 0x0C) {
        n = -1;
    }
    v = Br85ListSel(&pCtl->list, n);
    if (v >= 0) {
        g_br73.nAA2A34 = v;
    }
    return 1;
}

/* ==========================================================================
 * Text hooks
 * ========================================================================== */

/* 0x1008C320 IS `_stricmp`, NOT `strcmp` -- CASE-INSENSITIVE.
 *
 * BRGlide settles this with no inference at all, because it imports the CRT
 * instead of linking it statically:
 *
 *     100384F4  ff1554058f11   call dword ptr [0x118f0554]  ; MSVCRT!_stricmp
 *
 * and BRD3D's statically linked 0x1008C320 confirms it from the other side
 * with the classic ASCII fold (`sub 0x41 / cmp 0x1A / sbb / and 0x20`).
 *
 * Written out rather than calling strcasecmp: strcasecmp folds per the current
 * LOCALE, while the original folds A-Z and nothing else. Under a Turkish
 * locale strcasecmp maps 'I' differently and the two would disagree on real
 * captions. This is a decompilation, so the original's exact fold is the
 * specification.
 *
 * CONSEQUENCE, and it is observable: a caption differing from the stored one
 * ONLY in case compares EQUAL, so the copy is skipped and the destination
 * keeps its old capitalisation. */
/* WHAT IT DOES: compares two pieces of text while ignoring capitals, folding
 * only the plain English A to Z and nothing else. Callers use it to decide
 * whether the player has actually changed a name -- which means a name
 * retyped in different capitals counts as unchanged and the new
 * capitalisation is discarded. */
/* @d3donly 0x1008C320 br_stricmp_1008C320 -- absent from BRGlide (D3D-only / dynamically-imported CRT); no Glide twin exists */
static int br_stricmp_1008C320(const char *pA, const char *pB)
{
    for (;;) {
        unsigned char a = (unsigned char)*pA++;
        unsigned char b = (unsigned char)*pB++;
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 0x20);
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 0x20);
        if (a != b)  return (int)a - (int)b;
        if (a == 0)  return 0;
    }
}

/* 0x1003F050 and 0x1003F0B0 are the same 81 bytes over two buffers. */
/* WHAT IT DOES: takes what the player has typed into a menu box and copies it
 * into the game's own store of that name -- but only if it differs by more
 * than capitalisation. This is the shared body; the two hooks that use it
 * differ only in which name they write. */
/* @implements 0x1003F050 d3d Br85TextReadBack */
static int32_t Br85TextReadBack(BrUiCtl_ *pCtl, char *pszDst, size_t cbDst)
{
    const char *pszSrc;

    (void)Br85ItemApply(pCtl, 0);



    pszSrc = pCtl->aText[0].sz;        /* +0x2B65 */
    /* `call 0x1008C320` is _stricmp -- see br_stricmp_1008C320 above. The
     * copy runs only when it answers non-zero, i.e. when the two differ by
     * more than capitalisation. */
    if (br_stricmp_1008C320(pszDst, pszSrc) != 0) {
        /* DEVIATION: the original is an unbounded `rep movsd`/`rep movsb`. */
        size_t cb = strlen(pszSrc);
        if (cb > cbDst - 1u) {
            cb = cbDst - 1u;
        }
        memcpy(pszDst, pszSrc, cb);
        pszDst[cb] = '\0';
    }
    return 1;
}

int32_t BrUiHook85_1003F050(BrUiCtl_ *pCtl)
{
    return Br85TextReadBack(pCtl, g_brHook85.szB4E740,
                            sizeof(g_brHook85.szB4E740));
}

/* WHAT IT DOES: reads back what the player typed, into the second of the two
 * name stores. */
/* @implements 0x1003F0B0 d3d BrUiHook85_1003F0B0 */
int32_t BrUiHook85_1003F0B0(BrUiCtl_ *pCtl)
{
    return Br85TextReadBack(pCtl, g_brHook85.szB4E760,
                            sizeof(g_brHook85.szB4E760));
}

/* 0x10040A50 and 0x10040AC0: sprintf("%d", g + 1) into a scratch buffer, copy
 * it over aText[0]'s text, then the box's +0x08 and +0x2C.  The format string
 * is the literal at 0x100A73C4, read out of the image: "%d". */
static int32_t Br85TextNumber(BrUiCtl_ *pCtl, int32_t n, char *pszScratch)
{
    BrTextBox           *pBox = &pCtl->aText[0];
    const BrTextBoxVtbl *pV   = pBox->pVtbl;

    /* DEVIATION: the original's sprintf is unbounded into a .data buffer whose
     * extent this port declares, so snprintf is used against that extent.  A
     * host that has not wired the buffer is skipped rather than faulted -- the
     * builder-side code in slice6_73.c guards the same two pointers. */
    if (pszScratch != NULL && g_br73.cbScratch != 0u) {
        snprintf(pszScratch, g_br73.cbScratch, "%d", (int)(n + 1));

        /* DEVIATION: unbounded `rep movs` in the original. */
        {
            size_t cb = strlen(pszScratch);
            if (cb > (size_t)BR_TEXTBOX_MAX - 1u) {
                cb = (size_t)BR_TEXTBOX_MAX - 1u;
            }
            memcpy(pBox->sz, pszScratch, cb);
            pBox->sz[cb] = '\0';
        }
    }

    if (pV != NULL && pV->pfn08 != NULL) {
        pV->pfn08(pBox);
    }
    /* GOTCHA: the original guards this with `test ebx,ebx` where ebx is the
     * text buffer's ADDRESS -- never null -- so it always runs. */
    if (pV != NULL && pV->pfn2C != NULL) {
        pV->pfn2C(pBox);
    }
    return 1;
}

int32_t BrUiHook85_10040A50(BrUiCtl_ *pCtl)
{
    return Br85TextNumber(pCtl, g_br73.nAA28A0, g_br73.szAA2518);
}

int32_t BrUiHook85_10040AC0(BrUiCtl_ *pCtl)
{
    return Br85TextNumber(pCtl, g_br73.nAA28A4, g_br73.szA9D618);
}

/* ==========================================================================
 * Flag hooks
 * ========================================================================== */

int32_t BrUiHook85_100418D0(BrUiCtl_ *pCtl)
{
    /* When the flag is CLEAR the routine does nothing at all -- it does not
     * even load the control. */
    if (g_brHook81.nAA28E4 != 0) {
        pCtl->flags1C = (int32_t)((uint32_t)pCtl->flags1C & 0xFFFFEFEFu);
    }
    return 1;
}

/* WHAT IT DOES: puts a menu box into edit mode -- or takes it out again,
 * since it toggles -- when the player picks it, and only the first time round:
 * once the "an edit is in progress" flag is set it does nothing further. */
/* @implements 0x10042AC0 d3d BrUiHook85_10042AC0 */
int32_t BrUiHook85_10042AC0(BrUiCtl_ *pCtl)
{
    if (g_brAA28D8 == 0) {
        g_brAA28D8 = 1;
        /* `xor ecx,ecx / test edx,edx / sete cl / store` -- the result is
         * exactly 0 or 1, never the complement of a wider value. */
        pCtl->aText[0].f420 = (pCtl->aText[0].f420 == 0u) ? 1u : 0u;
    }
    return 1;
}

/* WHAT IT DOES: nothing except say "yes". It is installed where a menu
 * control needs a handler that accepts and does no work -- the original does
 * not even look at the control it is passed. */
/* @d3donly 0x10042AF0 BrUiHook85_10042AF0 -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
int32_t BrUiHook85_10042AF0(BrUiCtl_ *pCtl)
{
    (void)pCtl;                 /* the original never even loads it */
    return 1;
}

/* ==========================================================================
 * Phase-changing hooks
 * ========================================================================== */

int32_t BrUiHook85_10043FA0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner = pCtl->pOwner;        /* +0x2AE8 */

    /* CONFLICT 4: `push 1` into a slot br_phase.h types `void *`. */
    if (pOwner != NULL && pOwner->pVtbl != NULL && pOwner->pVtbl->f18 != NULL) {
        pOwner->pVtbl->f18(pOwner, (void *)(size_t)1);
    }
    g_pBrUiNav->pAA2904 = g_pBrUiNav->pAA2908;
    return 0;                   /* 0, which stops the frame */
}

int32_t BrUiHook85_100466C0(BrUiCtl_ *pCtl)
{
    BrUiNav  *pNav   = g_pBrUiNav;
    BrPhase_ *pOwner = pCtl->pOwner;
    BrPhase_ *pCur;
    BrPhase_ *pNext;

    /* The LEAVE prologue slice7_81.c's whole family shares:
     *     mov ecx,[eax+0x2AE8] / mov edx,[ecx] / call [edx+0x1C]
     * +0x2AE8 is the control's OWNING PHASE and phase vtable slot +0x1C is
     * 0x10048AA0, "release every page".  Written out here rather than shared
     * with slice7_81.c because that module's copy is file-static and this one
     * is a different translation unit -- the duplication is three lines and is
     * the reason this comment exists. */
    if (pOwner != NULL && pOwner->pVtbl != NULL && pOwner->pVtbl->f1C != NULL) {
        pOwner->pVtbl->f1C(pOwner);
    }

    /* The CURRENT phase is read AFTER +0x1C has run.  Its NULL test is the
     * original's own. */
    pCur = pNav->pAA2904;
    if (pCur != NULL && pCur->pVtbl != NULL && pCur->pVtbl->f00 != NULL) {
        (void)pCur->pVtbl->f00(pCur, 1);
    }

    /* GOTCHA, and it is the original's order: 0x10AA2918 is loaded BEFORE
     * 0x10AA2984 is cleared, and only then stored into 0x10AA2904. */
    pNext = g_brHook81.pAA2918;
    g_brHook85.nAA2984  = 0;
    pNav->pAA2904       = pNext;

    /* The same two-call teardown br_uinav.c's BrNavPhaseBail runs, with the
     * same two operands reached the same way. */
    BrSub1003E310();
    BrSub1006A4A0(pNav->pG->pB4DF30, pNav->pG->pB4FBE8);
    return 0;
}

/* ==========================================================================
 * 0x10044010 .. 0x100440B0 -- six one-liners
 *
 * Each is `mov [0x10AA287C], k / push arg / call <one of two> / mov eax,1`.
 * The two callees are 0x10043E70 (open a screen; ignores the argument) and
 * 0x10047360 (choose the kind byte; reads the control).  CONFLICT 5: a body
 * match in config/shared.csv pairs each +0x08 member with its +0x0C sibling
 * because the differing call target normalises away.  They are not the same
 * function and the two arms below are read from the D3D bodies.
 * ========================================================================== */

/* Written out rather than routed through Br85ModeOpen: each hook stores the
 * mode itself and passes its own pCtl to 0x10043E70, where the shared helper
 * loses the argument (it passes NULL) and hoists the store.  The callee never
 * reads the argument, so this is the same behaviour either way. */
int32_t BrUiHook85_10044010(BrUiCtl_ *pCtl)
{
    g_brAA287C = 0;
    (void)BrOptOpen2948(pCtl);
    return 1;
}
/* WHAT IT DOES: chooses the second play mode and opens the next screen. */
/* @implements 0x10044050 d3d BrUiHook85_10044050 */
int32_t BrUiHook85_10044050(BrUiCtl_ *pCtl)
{
    g_brAA287C = 1;
    (void)BrOptOpen2948(pCtl);
    return 1;
}
/* WHAT IT DOES: chooses the third play mode and opens the next screen. */
/* @implements 0x10044090 d3d BrUiHook85_10044090 */
int32_t BrUiHook85_10044090(BrUiCtl_ *pCtl)
{
    g_brAA287C = 2;
    (void)BrOptOpen2948(pCtl);
    return 1;
}

/* WHAT IT DOES: records the first play mode as the one under the cursor and
 * refreshes how that menu entry is drawn -- the highlight, not the choice.
 * These three are the drawing twins of the three above. */
/* @implements 0x10044030 d3d BrUiHook85_10044030 */
/* Inlined for the same reason as the 0x10044010 family above: the original
 * stores the mode itself and makes a one-argument tail call, where routing
 * through Br85ModeKind pushes the mode as a second argument. */
int32_t BrUiHook85_10044030(BrUiCtl_ *pCtl)
{
    g_brAA287C = 0;
    (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}
/* WHAT IT DOES: the same for the second play mode's menu entry. */
/* @implements 0x10044070 d3d BrUiHook85_10044070 */
int32_t BrUiHook85_10044070(BrUiCtl_ *pCtl)
{
    g_brAA287C = 1;
    (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}
/* WHAT IT DOES: the same for the third play mode's menu entry. */
/* @implements 0x100440B0 d3d BrUiHook85_100440B0 */
int32_t BrUiHook85_100440B0(BrUiCtl_ *pCtl)
{
    g_brAA287C = 2;
    (void)BrSprFontKindHook_10047360(pCtl);
    return 1;
}

/* ==========================================================================
 * 0x1004E810 -- the car list's row callback
 * ========================================================================== */

/* WHAT IT DOES: plays a preview of the music that goes with the row the
 * player has moved to in a list -- row zero corresponds to the disc's third
 * track, since the first two are not music the player picks. */
/* @implements 0x1004E810 d3d BrUiHook85_1004E810 */
int32_t BrUiHook85_1004E810(void *pUnused, const int32_t *pRow)
{
    (void)pUnused;              /* `[esp+4]` is pushed and never read */
    if (pRow != NULL) {         /* DEVIATION: the original dereferences it */
        BrCdTrackPlay((int)(*pRow + 2));
    }
    return 1;
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook85Install(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    pHooks->p1003E7A0 = BrUiHook85_1003E7A0;
    pHooks->p1003E950 = BrUiHook85_1003E950;
    pHooks->p1003E980 = BrUiHook85_1003E980;
    pHooks->p1003E9E0 = BrUiHook85_1003E9E0;
    pHooks->p1003EA40 = BrUiHook85_1003EA40;
    pHooks->p1003EB10 = BrUiHook85_1003EB10;
    pHooks->p1003ED10 = BrUiHook85_1003ED10;
    pHooks->p1003EE20 = BrUiHook85_1003EE20;
    pHooks->p1003F050 = BrUiHook85_1003F050;
    pHooks->p1003F0B0 = BrUiHook85_1003F0B0;

    pHooks->p10040930 = BrUiHook85_10040930;
    pHooks->p10040A50 = BrUiHook85_10040A50;
    pHooks->p10040AC0 = BrUiHook85_10040AC0;
    pHooks->p100418D0 = BrUiHook85_100418D0;

    pHooks->p10042AC0 = BrUiHook85_10042AC0;
    pHooks->p10042AF0 = BrUiHook85_10042AF0;

    pHooks->p10043FA0 = BrUiHook85_10043FA0;
    pHooks->p10044010 = BrUiHook85_10044010;
    pHooks->p10044030 = BrUiHook85_10044030;
    pHooks->p10044050 = BrUiHook85_10044050;
    pHooks->p10044070 = BrUiHook85_10044070;
    pHooks->p10044090 = BrUiHook85_10044090;
    pHooks->p100440B0 = BrUiHook85_100440B0;

    pHooks->p100466C0 = BrUiHook85_100466C0;

    /* CONFLICT 3: the slot's declared type is `void (*)(void)` and the body
     * takes two cdecl arguments.  The cast is here, at the one place that
     * installs it, so retyping the slot deletes exactly this line's cast. */
    pHooks->p1004E810 = (BrTextListCbFn)BrUiHook85_1004E810;

    /* NOT INSTALLED, deliberately, and each is a visible hole rather than a
     * silent one -- see the NOT PORTED section of slice8_85.h:
     *   p1003ECB0  p1003FC40  p10041300  p100413B0  p10041670  p10041710
     *   p100417B0  p10041DF0  p10042020  p10047210  p10047290
     *   p100409F0  p10040A20   (the two PAGE hooks)
     * and the eleven slots slice7_80.c / slice7_81.c / the host already own. */
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int g_brAA28D8;

/* WHAT IT DOES: stub that always returns 1. */
/* @implements 0x100385E0 glide BrStubTrue */

int BrStubTrue(void)

{
  return 1;
}

/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops. */
/* @implements 0x1003BFF0 glide BrToggleOnce_BFF0 */

int BrToggleOnce_BFF0(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}

/* WHAT IT DOES: on first call, toggle a boolean field at +0x2F7C; subsequent calls are no-ops (second instance). */
/* @implements 0x1003C050 glide BrToggleOnce_C050 */

int BrToggleOnce_C050(int param_1)

{
  if (g_brAA28D8 == 0) {
    g_brAA28D8 = 1;
    *(unsigned int *)(param_1 + 0x2f7c) = (unsigned int)(*(int *)(param_1 + 0x2f7c) == 0);
  }
  return 1;
}


#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
int FUN_100377a0(int);
extern char DAT_100acacc[];
_CRTIMP int __cdecl _getdrive(void);
_CRTIMP char *__cdecl _getcwd(char *, int);
_CRTIMP int __cdecl _chdrive(int);
_CRTIMP int __cdecl _chdir(const char *);

/* @implements 0x1003EE90 glide FUN_1003ee90 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1003ee90(void)

{
  int saved;
  int d;
  int result;
  char cwd[260];
  
  result = 0;
  saved = _getdrive();
  _getcwd(cwd, 0x104);
  for (d = 3; d <= 0x1a; d++) {
    if (_chdrive(d) != 0) {
      continue;
    }
    if (GetDriveTypeA((LPCSTR)0) != 5) {
      continue;
    }
    if (_chdir(DAT_100acacc) != 0) {
      continue;
    }
    if (FUN_100377a0(d) == 0) {
      continue;
    }
    result = 1;
    break;
  }
  _chdrive(saved);
  _chdir(cwd);
  return result;
}


int FUN_10059e00();
extern int DAT_10ac6730;
extern int DAT_10ac6734;
extern int g_br0AB3D8;
extern int g_brB4E708;

/* @implements 0x1003C240 glide FUN_1003c240 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1003c240(void)

{
  int dir;
  
  dir = DAT_10ac6734;
  g_br0AB3D8 = 1;
  if (dir != 0) {
    g_brB4E708 = g_brB4E708 + 1;
    if (g_brB4E708 > 9) {
      g_brB4E708 = 0;
      FUN_10059e00();
      return 1;
    }
  }
  else {
    if (DAT_10ac6730 != 0) {
      g_brB4E708 = g_brB4E708 - 1;
      if (g_brB4E708 < 0) {
        g_brB4E708 = 9;
      }
    }
  }
  FUN_10059e00();
  return 1;
}


int FUN_10059e00();
extern int DAT_10ac6730;
extern int DAT_10ac6734;
extern int g_br0AB3D8;
extern int g_brB4E70C;

/* @implements 0x1003C2B0 glide FUN_1003c2b0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1003c2b0(void)

{
  int dir;

  dir = DAT_10ac6734;
  g_br0AB3D8 = 0;
  if (dir != 0) {
    g_brB4E70C = g_brB4E70C + 1;
    if (g_brB4E70C > 9) {
      g_brB4E70C = 0;
      FUN_10059e00();
      return 1;
    }
  }
  else {
    if (DAT_10ac6730 != 0) {
      g_brB4E70C = g_brB4E70C - 1;
      if (g_brB4E70C < 0) {
        g_brB4E70C = 9;
      }
    }
  }
  FUN_10059e00();
  return 1;
}

#endif /* BR_MATCHING_BUILD */
