/* NOTE: these COM types were renamed Br*Com* -> Br*DPlay* during integration.
 * slice1_03.h defines a DIFFERENT BrComVtbl/BrComObj (a generic holder, other
 * vtable slots). Both are real and distinct -- this one is DirectPlay, keyed by
 * DPERR_BUFFERTOOSMALL 0x8877001E. Renamed rather than merged.
 *
 * slice1_06.h -- BRD3D.dll 0x10037030-0x1005D440, a later pass.
 *
 * Twelve of the sixteen functions in this range are recovered here. The four
 * that are not, and the reason for each, are listed at the bottom of this
 * file and in slice1_06.c.
 *
 * Every routine in this range reads its state out of file-scope globals; the
 * original takes almost no arguments. Following the precedent set by
 * br_pool.h / br_span.h / br_seg.h, the globals are gathered into structs and
 * passed in. Where that invents an argument order, destination-first is used
 * (br_vec.h convention) and the choice is called out at the declaration.
 *
 * Field names are positional (`f04`, `f20`) wherever the meaning of a field
 * could not be established. Names that do appear describe the MECHANISM the
 * original implements, never a guessed purpose.
 */
#ifndef SLICE1_06_H
#define SLICE1_06_H

#include <stddef.h>
#include <stdint.h>

#include "br_vec.h"

/* ==========================================================================
 * 0x10037030 / 0x10037070 -- the context at *(void**)0x106C7C3C
 * ========================================================================== */

/* 0x10037030. A 30-slot append-only list living at ctx+0x04..ctx+0x78 with
 * its counter at ctx+0x7C (4 + 30*4 == 0x7C exactly, which is what fixes the
 * capacity at 30).
 *
 * GOTCHA: `count` is incremented on BOTH paths. Past 30 items the original
 * stores nothing, bumps a separate global drop counter (0x106C7C40), and
 * still increments `count` -- so `count` is a count of REQUESTS, not of
 * stored items, and it grows without bound. Callers must clamp it to
 * BR_PENDLIST_MAX before using it as an index.
 *
 * There is no lower-bound check and no failure return. */
#define BR_PENDLIST_MAX 30

typedef struct BrPendList {
    void    *apItems[BR_PENDLIST_MAX];  /* ctx+0x04 .. ctx+0x78 */
    int32_t  count;                     /* ctx+0x7C */
} BrPendList;

/* pcDropped is the global at 0x106C7C40; may be NULL (see the .c). */
void BrPendListAdd(BrPendList *pList, void *pItem, uint32_t *pcDropped);

/* 0x10037070. Record array at ctx+0x8014, indexed indirectly through the
 * 12-byte index table at ctx+0x8110. Stride is 36 bytes: the original does
 * `lea eax,[eax+eax*8]` (index*9) and then scales by 4. */
#define BR_DEVREC_SLOTS  12
#define BR_DEVREC_STRIDE 36

typedef struct BrDevRec {
    uint32_t f00;
    uint32_t f04;               /* +0x04 -- the value compared against */
    uint32_t f08, f0C, f10, f14, f18, f1C;
    uint32_t f20;               /* +0x20 -- flags */
} BrDevRec;

/* The original masks +0x20 with 0x0F000000 and requires exactly 0x01000000,
 * i.e. a 4-bit type field in bits 24..27 whose only accepted value is 1. */
#define BR_DEVREC_TYPE_MASK  0x0F000000u
#define BR_DEVREC_TYPE_MATCH 0x01000000u

/* Returns 1 if any of the 12 indexed records has f04 == value, a non-zero
 * f04, and bits 24..27 of f20 equal to 1; else 0. Scans forward, stops at
 * the first hit. */
int BrDevRecMatch(const BrDevRec *aRecs, const uint8_t *abIndex,
                  uint32_t value);

/* ==========================================================================
 * 0x10037930 -- keyed lookup table
 * ========================================================================== */

typedef struct BrKeyEnt {
    uint32_t key;    /* +0x00 */
    uint32_t a;      /* +0x04 -- written to the first out-param  */
    uint32_t b;      /* +0x08 -- written to the second out-param */
    uint32_t f0C;    /* +0x0C -- never read by this routine      */
} BrKeyEnt;

typedef struct BrKeyTable {
    BrKeyEnt *aEnts;   /* 0x106C7E7C */
    int32_t   count;   /* 0x10A99778 */
    uint32_t  bias;    /* 0x10A9977C */
} BrKeyTable;

/* 0x10037930. Looks for `bias + key` among the first `count` entries.
 *
 * GOTCHA 1: the query is BIASED -- the original adds the global at
 * 0x10A9977C to the caller's key before comparing. Passing a raw table key
 * finds nothing unless bias is 0.
 * GOTCHA 2: the scan runs BACKWARDS from count-1 to 0, so with duplicate
 * keys the LAST matching entry wins.
 * Returns 1 and fills the two out-params on a hit, 0 otherwise (leaving them
 * untouched).
 * count <= 0 returns 0 without touching the table. */
int BrKeyTableFind(const BrKeyTable *pTable, uint32_t key,
                   uint32_t *pA, uint32_t *pB);

/* ==========================================================================
 * 0x1003B940 -- point-in-triangle
 * ========================================================================== */

/* Returns 1 if pPt is on the inside of all three edges of triangle
 * (pA, pB, pC) as judged against the reference direction pRef, else 0.
 *
 * For each edge it forms cross(edge, pPt - <a point on the edge>) and
 * requires dot(that, pRef) >= 0. The threshold is the constant at
 * 0x1008F62C, which is 0.0f, and the comparison is `fcomp` + `test ah,1`,
 * i.e. C0 -- so the boundary (== 0) counts as INSIDE and any NaN counts as
 * OUTSIDE.
 *
 * GOTCHA: the original is inconsistent about which endpoint it subtracts.
 * Edge A->B is paired with (pPt - pB) and edge C->A with (pPt - pA), i.e.
 * the edge's far end, while edge B->C uses (pPt - pB), its near end. This
 * makes no difference -- the two choices differ by a multiple of the edge
 * vector, which the cross product annihilates -- but do not "fix" it and do
 * not read it as evidence of a different predicate. Reproduced verbatim. */
int BrTriContainsPoint(const BrVec3 *pPt, const BrVec3 *pA, const BrVec3 *pB,
                       const BrVec3 *pC, const BrVec3 *pRef);

/* ==========================================================================
 * 0x1003D180 -- "ask for the size, allocate, ask again" COM helper
 * ========================================================================== */

/* The error the original tests for is 0x8877001E == DPERR_BUFFERTOOSMALL
 * (MAKE_HRESULT(1, 0x877, 30)), which is what identifies this as DirectPlay.
 * The method is vtable slot 21 (`call [eax+0x54]`). */
#define BR_COM_VTBL_SLOT      21
#define BR_COM_E_BUFFERTOOSMALL ((int32_t)0x8877001E)
#define BR_COM_E_OUTOFMEMORY    ((int32_t)0x8007000E)

typedef int32_t (*BrComGetFn)(void *pThis, void *pParam,
                              void *pvBuf, uint32_t *pcb);

typedef struct BrDPlayVtbl {
    void      *aUnused[BR_COM_VTBL_SLOT];   /* slots 0..20, never touched */
    BrComGetFn pfnGet;                      /* slot 21, at byte +0x54 */
} BrDPlayVtbl;

typedef struct BrDPlayObj {
    const BrDPlayVtbl *pVtbl;
} BrDPlayObj;

/* 0x1003D180.
 * Calls pfnGet(pObj, pParam, NULL, &cb) to size the result; only if that
 * returns exactly DPERR_BUFFERTOOSMALL does it allocate cb bytes and call
 * pfnGet again to fill them. On success *ppvOut receives the buffer and the
 * function returns 0.
 *
 * GOTCHA 1: any first-call result OTHER than DPERR_BUFFERTOOSMALL -- S_OK
 * included -- is treated as a failure and returned to the caller unchanged.
 * GOTCHA 2: on success it returns a hardcoded 0, discarding the (possibly
 * non-zero success) HRESULT the second call produced. */
int32_t BrComGetAlloc(BrDPlayObj *pObj, void *pParam, void **ppvOut);

/* ==========================================================================
 * 0x1003E1D0 -- paired scratch buffers
 * ========================================================================== */

/* Two 0x53-dword (332-byte) buffers. The original keeps a pointer to each
 * (0x10ACED34, 0x10AD189C); if a pointer is null it is first pointed at a
 * fixed static buffer (0x10AF9890 and 0x10AF99DC). Those two statics are
 * ADJACENT -- 0x10AF9890 + 0x53*4 == 0x10AF99DC exactly -- which is why they
 * are laid out adjacently below. */
#define BR_PAIRBUF_DWORDS 0x53

typedef struct BrPairBuf {
    uint32_t *pA;                         /* 0x10ACED34 */
    uint32_t *pB;                         /* 0x10AD189C */
    uint32_t  aStaticA[BR_PAIRBUF_DWORDS];  /* 0x10AF9890 */
    uint32_t  aStaticB[BR_PAIRBUF_DWORDS];  /* 0x10AF99DC */
} BrPairBuf;

/* Bind any null pointer to its static buffer, then zero both buffers.
 * Always returns 1; the original has no failure path. */
int BrPairBufReset(BrPairBuf *pBuf);

/* ==========================================================================
 * 0x1003E260 -- indexed error report
 * ========================================================================== */

/* The table at 0x100AC660 is 9 records of 8 bytes: { int fFatal; u32 idText }.
 * Record 9 onward is unrelated data, which corroborates the original's bound.
 *
 * GOTCHA: the bound is `cmp esi,8 / jg return`, a SIGNED greater-than against
 * the upper limit only. There is no lower bound, so a negative index reads
 * before the table in the original. See the DEVIATION in the .c. */
#define BR_ERR_COUNT       9
#define BR_ERR_CAPTION_ID  0xAAu   /* the string id used for the caption */

typedef struct BrErrEnt {
    int32_t  fFatal;   /* +0x00 -- non-zero: terminate with code 1 */
    uint32_t idText;   /* +0x04 -- string id for the body text     */
} BrErrEnt;

extern const BrErrEnt g_aBrErrTable[BR_ERR_COUNT];

/* The original calls a string-handle lookup (0x10074030, BrHandleLookup in
 * br_bits.h), USER32!MessageBoxA with a window handle out of 0x10680584, and
 * a terminate helper. Those three are injected here rather than hardcoded. */
typedef struct BrErrHost {
    const char *(*pfnLookup)(void *pUser, uint32_t id);
    void        (*pfnMessage)(void *pUser, const char *pszText,
                              const char *pszCaption, uint32_t uType);
    void        (*pfnExit)(void *pUser, int code);
    void        *pUser;
} BrErrHost;

/* 0x1003E260.
 * GOTCHA: the body text pointer is the lookup result PLUS ONE. The caption
 * pointer is not adjusted. Whatever the first byte of a string record is, it
 * is skipped for the body and kept for the caption. Faithfully reproduced --
 * this is also why a failed lookup (which returns 0) becomes the pointer 1
 * in the original. */
void BrErrShow(const BrErrHost *pHost, int32_t idx);

/* ==========================================================================
 * 0x1003E310 / 0x1003F2B0 / 0x1003F320 -- the options block
 * ========================================================================== */

#define BR_OPT_CFG_COUNT     6    /* 0x100AC648 + 4*i, contiguous */
#define BR_OPT_SEL_COUNT     7    /* 0x10AA2A00  + 4*i            */
#define BR_OPT_SCRATCH_COUNT 12   /* 0x10B4E710  + 4*i, packed    */

typedef struct BrOptState {
    int32_t aCfg[BR_OPT_CFG_COUNT];
    int32_t aSel[BR_OPT_SEL_COUNT];   /* index 1 (0x10AA2A04) is never touched */
} BrOptState;

typedef struct BrOptScratch {
    int32_t a[BR_OPT_SCRATCH_COUNT];
} BrOptScratch;

/* 0x1003E310. Copies the live options into the 12-dword scratch block.
 *
 * GOTCHA: the scratch block is PACKED and INTERLEAVED. The twelve slots are,
 * in order, cfg0 sel0 sel2 cfg1 cfg2 cfg3 sel3 cfg4 sel4 sel5 cfg5 sel6 --
 * there is no pattern to it, and sel1 (0x10AA2A04) is skipped entirely. The
 * mapping is confirmed in both directions: 0x1003E3A0 restores the same
 * twelve pairs the other way round.
 *
 * The original takes no arguments; destination-first is chosen here to match
 * br_vec.h. */
void BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc);

/* State read by the two availability predicates.
 *
 * GOTCHA: `maskPair` is ONE dword at 0x10AA27E0 holding TWO 16-bit masks.
 * 0x1003F2B0 reads the word at 0x10AA27E2 -- the HIGH half -- while
 * 0x1003F320 reads the dword and uses only the LOW half, additionally
 * treating bit 15 of that low half as a flag. Splitting this into two
 * independent globals loses the aliasing; keeping it as one dword preserves
 * it. */
typedef struct BrOptCaps {
    int32_t  mode;           /* 0x100AA010 */
    int32_t  fForceAvailA;   /* 0x10AA28F8 */
    int32_t  fLowAlwaysB;    /* 0x10AA28FC */
    int32_t  fRebaseB;       /* 0x10AA28F4 */
    int32_t  fLowAlways;     /* 0x10AA28F0 */
    int32_t  fAlt;           /* 0x10AA289C */
    uint32_t maskPair;       /* 0x10AA27E0 (low 16) / 0x10AA27E2 (high 16) */
    uint32_t maskA;          /* 0x10A9D010 */
    uint32_t maskAMode;      /* 0x100AB3EC */
    uint32_t maskB;          /* 0x10AA2598 */
    uint32_t maskBMode6;     /* 0x100AB3E8 */
    int16_t  maskBDefault;   /* 0x100AB3E4 -- SIGN-extended when used */
    int32_t  nAlwaysB;       /* 0x10AD0984 */
} BrOptCaps;

/* 0x1003F2B0. Availability of entry `n` in the first option list; the caller
 * at 0x1003E510 sweeps n over 0..11 or 0..14.
 *
 * GOTCHA: n == 12 is a RESERVED SENTINEL -- it returns 0 before anything
 * else is consulted, including the force-available flag.
 * Returns the masked bit, not a normalised 0/1; the original's callers only
 * test it against zero. */
int32_t BrOptAvailA(const BrOptCaps *pCaps, uint32_t n);

/* 0x1003F320. Availability of entry `n` in the second option list; the
 * caller at 0x1003E510 sweeps n over 0..31.
 *
 * GOTCHA 1: index 15 is remapped before the mask test -- to 11 in every mode
 * EXCEPT mode 6, where it is remapped to 7.
 * GOTCHA 2: the final mask (maskBDefault) is loaded with `movsx`, so if its
 * bit 15 is ever set every index from 15 up reads as available. The sibling
 * path zero-extends its 16-bit mask instead. The asymmetry is in the
 * original.
 * Returns the masked bit, not a normalised 0/1. */
int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n);

/* ==========================================================================
 * 0x1005CB90 -- fixed 100-slot name array constructor
 * ========================================================================== */

/* thiscall. Stores a vtable pointer at +0x00, zeroes 0x1964 dwords from
 * +0x04 (== 100 * 0x104 exactly, which is what fixes both the slot count and
 * the stride), then copies one source string into all 100 slots. */
#define BR_NAMELIST_COUNT  100
#define BR_NAMELIST_STRIDE 0x104

typedef struct BrNameList {
    const void *pVtbl;                                       /* +0x00 */
    char        asz[BR_NAMELIST_COUNT][BR_NAMELIST_STRIDE];  /* +0x04 */
} BrNameList;

/* The original hardcodes the vtable (0x1008F788) and the fill source
 * (0x1039B720). Both are parameters here. 0x1039B720 lies past the end of
 * the DLL's initialised data, so at load time the fill string is empty and
 * whatever it holds at call time is written into every slot. Returns pThis,
 * as the original does. */
BrNameList *BrNameListInit(BrNameList *pThis, const void *pVtbl,
                           const char *pszFill);

/* ==========================================================================
 * 0x1005D440 (partial) -- UI asset path table
 * ========================================================================== */

/* The original zeroes 0x106D dwords at 0x10A9E360 and then performs 145
 * identical calloc(0x104,1) + strcpy sequences, one per UI asset. The
 * destinations are 0x74 bytes apart with the pointer at +0x70 of each
 * record, and 0x10A9E360 + 145*0x74 lands exactly on the end of the zeroed
 * block -- which is what fixes the record size and the count. The other
 * 0x70 bytes of each record are zeroed here and filled in elsewhere.
 *
 * Only the table and the allocate-and-copy loop are ported. See the .c for
 * what the rest of 0x1005D440 does and why it is not here. */
#define BR_UIASSET_COUNT     145
#define BR_UIASSET_STRIDE    0x74   /* bytes per record, in the original  */
#define BR_UIASSET_PATH_OFF  0x70   /* pointer offset within a record     */
#define BR_UIASSET_PATH_MAX  0x104  /* bytes calloc'd per path            */

extern const char *const g_apszBrUiAssets[BR_UIASSET_COUNT];

/* The two absolute save-file paths the same function copies into fixed
 * buffers at 0x11782CD0 and 0x11782BC8. Hardcoded to the C: root in the
 * retail build. */
extern const char *const g_pszBrRallySeasonDat;   /* 0x11782CD0 */
extern const char *const g_pszBrRallyGhostDat;    /* 0x11782BC8 */

/* Allocate BR_UIASSET_PATH_MAX zeroed bytes per entry and copy the path in.
 * Returns 0 on success, -1 on allocation failure (see DEVIATION in the .c;
 * the original does not check). On failure everything already allocated is
 * released and every slot is left NULL. */
int  BrUiAssetPathsInit(char *apszOut[BR_UIASSET_COUNT]);
void BrUiAssetPathsFree(char *apszOut[BR_UIASSET_COUNT]);

/* ==========================================================================
 * NOT PORTED, and why
 * ==========================================================================
 *   0x1003E100  CD-ROM volume-label check. Win32-only (GetDriveTypeA,
 *               GetVolumeInformationA).
 *
 *               THE "three unresolved helper calls" ARE RESOLVED, and the
 *               reason they were unresolvable here is worth keeping: they are
 *               STATICALLY LINKED CRT in BRD3D.dll and IMPORT THUNKS in
 *               BRGlide.dll, so the Glide build names them and the D3D build
 *               cannot. Glide 0x100377A0 is the same function and its
 *               listing reads:
 *
 *                 D3D 0x1007C830  ==  MSVCRT sprintf
 *                 D3D 0x1007F1C0  ==  MSVCRT _chdrive
 *                 D3D 0x1007F0D0  ==  MSVCRT _chdir
 *
 *               so "%C:\" is formatting ONE argument, `drive + 0x40`, into a
 *               root path such as "D:\" -- 3 == 'C'. The function is
 *                   sprintf(root, "%C:\\", drive + 0x40);
 *                   _chdrive(drive) == 0 && GetDriveTypeA(NULL) == 5 &&
 *                   _chdir("\\") == 0 &&
 *                   GetVolumeInformationA(root, vol, 0x104, 0,0,0,0,0) &&
 *                   strcmp(vol, "Boss Rally") == 0
 *               and its caller D3D 0x10045A00 / Glide 0x1003EE90 is the loop
 *               that runs it over drives 3..26 ('C'..'Z'), saving and
 *               restoring the current drive and directory around the scan.
 *
 *               Still NOT PORTED, and now for a stated rather than an
 *               inferred reason: every instruction in both bodies is an OS
 *               call. There is no game logic to transcribe, and enumerating
 *               removable volumes is a host service (port/host), not
 *               portable C. See BrExt_10045A00 in slice3_31.h -- a machine
 *               with no "Boss Rally" disc in it makes this return 0, and the
 *               menu's status-line refusal is then the ORIGINAL's correct
 *               behaviour, not a gap in the port.
 *   0x1003E3A0  Options apply. Reaches through pointer-valued globals whose
 *               targets are outside this range and calls 0x1003E2C0, which
 *               is not in this packet.
 *   0x1003E510  Mode selection. Same problem, plus five index tables and two
 *               further out-of-range calls.
 *   0x100419D0  A four-argument virtual dispatch through an object of
 *               unknown type; nothing about it is recoverable beyond the
 *               vtable slot.
 *   0x1005D440  Ported only as the asset table above; the remainder builds
 *               two objects of types defined outside this range.
 */

#endif /* SLICE1_06_H */
