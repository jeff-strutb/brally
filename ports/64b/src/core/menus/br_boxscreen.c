/* br_boxscreen.c -- menus: the twenty-control screen with three boxes.
 *
 * BrExt_10054B50 builds the largest of the menu screens: twenty controls,
 * three of them drawn rectangles sharing one left edge.  The Br73
 * construction kit (static __inline in the matching build) is carried here as
 * a copy of the one slice6_73.c's other builders use.
 *
 * Filed out of the address batch slice6_73.c, whose preamble is carried
 * verbatim below.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "slice6_73.h"

/* --- DUPLICATE OWNERSHIP (host link only) -------------------------------
 * slice6_73 and slice6_70 each independently ported 0x1003E680. Both bodies
 * are faithful; the duplication is a coordination artefact of parallel
 * passes, not a disagreement about behaviour.
 *
 * slice6_70 is the OWNER. Under BR_HOST_LINK this module's copies are renamed
 * so the full-program link has exactly one definition of each. The renaming
 * covers this file's internal calls too, so the module stays self-consistent
 * and its own test binary -- which links this .o alone -- is unaffected.
 *
 * This is a stopgap. The duplicate bodies should be deleted and the callers
 * pointed at slice6_70's, once someone has diffed the two transcriptions
 * line-by-line and confirmed they agree. Until that diff is done, deleting the
 * wrong one would silently discard the better transcription.
 * ------------------------------------------------------------------------ */
#ifdef BR_HOST_LINK
#define BrSub1003E680 BrSub1003E680_dup_73
#define BrExt_1003E680 BrExt_1003E680_dup_73
#endif

/* The eight row offsets, as magnitudes.  See the banner. */
#define BR73_ROW_19    19.0f    /* 0x1008F680 */
#define BR73_ROW_38    38.0f    /* 0x1008F684 */
#define BR73_ROW_76    76.0f    /* 0x1008F68C */
#define BR73_ROW_95    95.0f    /* 0x1008F690 */
#define BR73_ROW_114  114.0f    /* 0x1008F694 */
#define BR73_ROW_33    33.0f    /* 0x1008F69C */

/* 0x1008F6A4.  The float nearest -1/11; the original multiplies by it and
 * then SUBTRACTS the product from the low endpoint, so the net effect is a
 * forward interpolation by n/11. */
#define BR73_NEG_RECIP_11  (-0.09090909361839294f)

/* ==========================================================================
 * 0. Small shared helpers
 * ========================================================================== */

/* DEVIATION (memory safety): 0x1003E260 is reached through slice1_06.h's
 * injected host.  An unwired host means "no reporter", where the original
 * always has one; nothing else changes. */
/* Under the matching build the whole Br73 construction kit is __inline and
 * spelled to the GLIDE original's exact shape (see the Br72 twin in
 * slice6_72.c): the original inlines every helper, stores unchecked, calls
 * the one-argument fatal 0x100378C0 error routine directly, and falls through
 * after it. */
#define BR73_HELPER static __inline
#define BR73_NEW_RAW FUN_10074572
/* FUN_100378c0: prototype in br_funcs.h */
extern void *FUN_10074572(uint32_t);   /* the EH-aware operator new the original calls */ /* the fatal error routine, as the original calls it */
/* the same two ctors slice6_72 calls direct (matching-only externs, so the
 * two TUs never co-link these) */
extern BrUiPage_ * BR_THISCALL1 FUN_100418c0(BrUiPage_ *);
extern BrUiCtl_  * BR_THISCALL1 FUN_10040b10(BrUiCtl_ *);
/* (port-only Br73Err removed) */


/* The page prologue, identical in all six builders except for the flag value
 * (1 everywhere but 0x10050060's SECOND page) and the two extra hook stores
 * 0x10054B50 makes.
 *
 * GOTCHA reproduced: aFlags[] is written with the count as it stands on
 * entry, the allocation happens next, and apPages[] is then written with the
 * count RE-READ from the object.  Nothing in between can change it, but the
 * original does re-read, and a constructor that published itself could make
 * the two differ.
 *
 * DEVIATION (memory safety, twice): the two array writes are bounded.  The
 * original has the same implicit bounds -- aPages ends where aFlags begins
 * and aFlags ends at the object's 0xC8 bytes -- but does not check them.
 *
 * DEVIATION (memory safety): on allocation failure the original reports error
 * index 4 and then dereferences NULL.  Index 4 is FATAL in g_aBrErrTable, so
 * BrErrShow does not return there in practice; the port returns NULL. */
/* (port-only Br73PageNew removed) */


/* The control prologue, ~80 occurrences.
 *
 * GOTCHA reproduced: the slot is written BEFORE the NULL test, and cCtl is
 * NOT advanced here -- every block bumps it at its end, so a failed
 * allocation leaves a NULL in the array and still moves the cursor on. */
/* (port-only Br73CtlNew removed) */


/* Allocate, register and place one control.  a4/a5 are 2 and 5 at every call
 * site in this packet, so they are not parameters. */
/* (port-only Br73Ctl removed) */


/* The label tail: `BrStrGet(id)` then vtable +0x34. */
/* (port-only Br73Text removed) */


/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pPage / pPhase / pCtl, which every builder declares. */
/* fully macro-expanded: VC5's inline budget gave up on four of the twenty
 * __inline sites, and a call to our own static helper cannot place */
#define BR73_CTL(x, y, flags, a6, a7)                                        \
    do {                                                                     \
        pCtl = (BrUiCtl_ *)BR73_NEW_RAW(BR73_ALLOC(BrUiCtl_,                \
                                                    BR73_CTL_ORIG_SIZE));    \
        pCtl = (pCtl != NULL) ? FUN_10040b10(pCtl) : NULL;                   \
        pPage->apCtl[pPage->cCtl] = pCtl;                                    \
        if (pCtl == NULL) { FUN_100378c0(4); }                               \
        pCtl->pVtbl->f38(pCtl, pPhase, (x), (y), (flags), 2, 5,              \
                         (a6), (a7));                                        \
    } while (0)

/* ==========================================================================
 * 0x10054B50 -- 20 controls, three of them rectangles
 * ========================================================================== */

/* WHAT IT DOES: lays out the largest of the menu screens -- twenty controls,
 * three of which are drawn boxes rather than text. All three boxes end up
 * sharing a left edge because only the first works one out and the other two
 * reuse it, which is the original's doing and not a simplification here. */

/* ===== matching-build immediates for BrExt_10054B50 =====================
 * Same story and same verification as slice6_72's block: the original
 * stores hooks and pushes styles as immediates; the env/g_br73 routing is
 * port scaffolding the image never initialises.  Three hooks recur in BOTH
 * builders with identical glide values (p10043FA0, p10041300, p100413B0),
 * which pins the sequence mapping independently. */
extern void FUN_10039f30(void); extern void FUN_10039f60(void);
extern void FUN_100406e0(void); extern void FUN_10040530(void);
extern void FUN_1003ed30(void); extern void FUN_1003ed10(void);
extern void FUN_1003abd0(void); extern void FUN_1003ad10(void);
extern void FUN_1003ac70(void);
extern void FUN_100407b0(void); extern void FUN_1003d4f0(void);
extern void FUN_1003a860(void); extern void FUN_1003a910(void);
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define BR73H(f) ((BrUiCtlHookFn_)&f)
#define pH73_p100409F0  BR73H(FUN_10039f30)
#define pH73_p10040A20  BR73H(FUN_10039f60)
#define pH73_p10047360  BR73H(FUN_100407b0)
#define pH73_p10047290  BR73H(FUN_100406e0)
#define pH73_p100470E0  BR73H(FUN_10040530)
#define pH73_p100458A0  BR73H(FUN_1003ed30)
#define pH73_p10043FA0  BR73H(FUN_1003d4f0)
#define pH73_p10045880  BR73H(FUN_1003ed10)
#define pH73_p10041300  BR73H(FUN_1003a860)
#define pH73_p100413B0  BR73H(FUN_1003a910)
#define pH73_p10041670  BR73H(FUN_1003abd0)
#define pH73_p100417B0  BR73H(FUN_1003ad10)
#define pH73_p10041710  BR73H(FUN_1003ac70)
#define pS73_p0AB448    ((const void *)&(*(unsigned char *)&g_hot0))
#define pS73_p0AB458    ((const void *)&(*(unsigned char *)&DAT_100aabf8))
#define pS73_p0AB468    ((const void *)&(*(unsigned char *)&DAT_100aac08))
#define pS73_p0AB478    ((const void *)&(*(unsigned char *)&DAT_100aac18))
#define pS73_p0AB4F8    ((const void *)&(*(unsigned char *)&DAT_100aac98))
#define pS73_p0AB508    ((const void *)&(*(unsigned char *)&DAT_100aaca8))
#define g73_n0AB428     (*(int32_t *)&g_hot2)
#define g73_n0AB42C     (*(int32_t *)((char *)&g_hot2 + 0x4))

/* WHAT IT DOES: lays out the largest of the menu screens -- twenty controls,
 * three of them drawn boxes that share a left edge because only the first
 * works one out (the original's doing; the fuller description is with the
 * dossier at the head of this section). */
/* @implements 0x10054B50 d3d BrExt_10054B50 */
/* BrExt_10054B50: the placed body is BrExt_10054B50_1004DA00.cpp */
