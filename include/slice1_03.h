/* slice1_03.h -- decompiled from BRD3D.dll, pass-03 packet
 * (address range 0x100088D0 - 0x1001DC70).
 *
 * Three unrelated clusters ended up in this packet:
 *
 *   1. Homogeneous polygon clipping over a CIRCULAR singly-linked vertex
 *      list, with a fixed 64-node recycling pool.
 *        0x1001D940  BrClipLerpVert
 *        0x1001D810  BrClipPlaneW
 *        0x1001D9F0  BrClipPlaneWPlusF04
 *        0x1001DB30  BrClipPlaneWMinusF04
 *        0x1001DC70  BrClipPlaneWPlusF08
 *
 *   2. Text / HUD output, driven entirely by globals.
 *        0x100192A0  BrTextSetColors
 *        0x10019300  BrTextDraw
 *        0x100171F0  BrHudDrawTimeEntry
 *
 *   3. Glue: an application-message dispatcher and two COM-ish forwarders.
 *        0x1000BEA0  BrAppMsgDispatch
 *        0x1000C4A0  BrComHolderRelease
 *        0x1000C4D0  BrComCallLocked68
 *
 * Everything the original reached through fixed addresses is modelled as a
 * file-static state block reachable through a Get...() accessor, so the
 * ported functions keep the original's argument lists exactly.
 */
#ifndef SLICE1_03_H
#define SLICE1_03_H

#include <stddef.h>
#include <stdint.h>

/* =====================================================================
 * 1. Clipping
 * ===================================================================== */

/* The clipped vertex record. Only +0x00 (the link) and the nine floats at
 * +0x04..+0x24 are touched by this code: BrClipLerpVert interpolates exactly
 * offsets 4, 8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20 and 0x24, and never
 * writes +0x00. Record stride in the original is 0x28 (40 bytes), which is
 * what the 0xA00-byte / 64-entry node pool divides into exactly.
 *
 * Field meanings are NOT established, so they are named by offset. The four
 * plane functions test  f18,  f18+f04,  f18-f04  and  f08+f18  against 0,
 * which is the signature of homogeneous frustum clipping with f18 = w and
 * f04 / f08 = x / y -- but nothing in this packet proves that, and which of
 * f04 / f08 is horizontal is pure guesswork, so no such names are used.
 *
 * THE FIELD MEANINGS ARE NOW ESTABLISHED, from the caller.  BRGlide.dll's
 * 0x1001EE70 -- the triangle submitter that drives all seven planes, which
 * has no counterpart in the D3D map and so was invisible from this packet --
 * builds its list out of `&BrDlVtx.f40` and reads the result back at fixed
 * offsets (port/src/br_dl.c, PART 2).  That pins every field:
 *
 *     f04 = clip x     f08 = clip y     f0C = clip z
 *     f10 = s          f14 = t          f18 = clip w
 *     f1C, f20, f24 = the N64 Vtx's three trailing bytes (colour or normal)
 *
 * So f04 IS horizontal, the guess above was right, and the seven planes are
 * NEAR, LEFT, RIGHT, TOP, FAR, BOTTOM, W -- in the order 0x1001EE70 calls
 * them, which is not the order of the outcode bits. */
typedef struct BrClipVert {
    struct BrClipVert *pNext;    /* +0x00 -- never written by the lerp */
    float f04, f08, f0C, f10;    /* +0x04 .. +0x10 */
    float f14, f18, f1C, f20;    /* +0x14 .. +0x20 */
    float f24;                   /* +0x24 */
} BrClipVert;

/* The polygon being clipped. +0x00 is the head of a CIRCULAR list; +0x04 is
 * the vertex count, which the clip routines both read and update. */
typedef struct BrClipList {
    BrClipVert *pHead;           /* +0x00 */
    int         cVerts;          /* +0x04 */
} BrClipList;

/* NOT in the original: the original's pool is the static byte range
 * 0x104C01A8 .. 0x104C0BA8 (0xA00 bytes = 64 records of 0x28), with the
 * free-list head at 0x104C0BBC. Call this once to install an equivalent
 * array; it also threads every node onto the free list.
 *
 * The pool bounds matter for behaviour, not just for allocation: on the way
 * out each clip routine only returns a discarded vertex to the free list if
 * its address lies inside the pool. Vertices from anywhere else are silently
 * dropped -- so a caller may hand in a polygon built from its own storage
 * and the clipper will not try to recycle it. */
void BrClipPoolInit(BrClipVert *aNodes, int cNodes);

/* NOT in the original: number of nodes currently on the free list. */
int  BrClipPoolCount(void);

/* NOT in the original as a function -- 0x1001EE70 open-codes exactly this,
 * twice (once in its give-up path and once as it emits each surviving
 * vertex), and the pool-bounds test is the same one BrClipPlane uses on the
 * way out.  Exposed so the driver does not need its own copy of the bounds,
 * which would be a second model of one object. */
void BrClipPoolFree(BrClipVert *pNode);

/* 0x1001D940  pop a node off the free list and fill it with
 *
 *     out.fN = (pB->fN - pA->fN) * t + pA->fN      for the nine floats
 *
 * i.e. t = 0 yields pA and t = 1 yields pB. out.pNext is left holding
 * whatever the free list had there; every caller overwrites it immediately.
 *
 * Callers always pass the OUTSIDE vertex as pA and the INSIDE vertex as pB,
 * with t = dOut / (dOut - dIn).
 *
 * Returns NULL when the pool is exhausted -- see the DEVIATION in the .c. */
BrClipVert *BrClipLerpVert(const BrClipVert *pA, const BrClipVert *pB,
                           float t);

/* The four clip planes. A vertex is INSIDE when its distance is >= 0.0f.
 *
 * The original compares with `fcomp` against the 0.0f constant at 0x1008F3C8
 * and branches on C0, so an unordered (NaN) result counts as OUTSIDE; the
 * ports keep that by testing `>= 0.0f` rather than `< 0.0f`.
 *
 * All four rotate pList->pHead forward by one node per call (see the .c). */
void BrClipPlaneW(BrClipList *pList);          /* 0x1001D810  d = f18        */
void BrClipPlaneWPlusF04(BrClipList *pList);   /* 0x1001D9F0  d = f18 + f04  */
void BrClipPlaneWMinusF04(BrClipList *pList);  /* 0x1001DB30  d = f18 - f04  */
void BrClipPlaneWPlusF08(BrClipList *pList);   /* 0x1001DC70  d = f08 + f18  */

/* The other three, added exactly as slice1_04.h prescribed: three distance
 * functions plus three wrappers, so that the 64-node pool and its free list
 * stay ONE object.  Glide addresses in brackets, because the reference build
 * is BRGlide (CONVENTIONS.md) and the D3D map is what named the first four. */
void BrClipPlaneWMinusF08(BrClipList *pList);  /* 0x1001DDB0 [0x1001F670]
                                                * d = f18 - f08             */
void BrClipPlaneWPlusF0C(BrClipList *pList);   /* 0x1001DEF0 [0x1001F7B0]
                                                * d = f0C + f18             */
void BrClipPlaneWMinusF0C(BrClipList *pList);  /* 0x1001E030 [0x1001F8F0]
                                                * d = f18 - f0C             */

/* =====================================================================
 * 2. Text / HUD
 * ===================================================================== */

#define BR_TEXT_ALIGN_LEFT    0
#define BR_TEXT_ALIGN_RIGHT   1
#define BR_TEXT_ALIGN_CENTER  2

/* Every global 0x10019300 and 0x100192A0 touch, gathered up. The two
 * callees they reach are exposed as hooks because they live outside this
 * packet (0x100193C0 measures a string, 0x10018590 emits the glyphs). */
typedef struct BrTextState {
    uint32_t *pGfx;        /* 0x106C0680 -- display-list write cursor      */
    int  x;                /* 0x104B0340 */
    int  y;                /* 0x104B0344 */
    int  scale;            /* 0x104B0348 -- handed to the measure callback */
    signed char align;     /* 0x104B035C -- read with movsx, so signed 8b  */
    int  f0A74A8;          /* 0x100A74A8 */
    int  f0A74AC;          /* 0x100A74AC */
    int  f0A74B0;          /* 0x100A74B0 */
    int  f4B0364;          /* 0x104B0364 -- set to 1 by BrTextSetColors    */
    int  f4B0368;          /* 0x104B0368 */
    int  f4B036C;          /* 0x104B036C */
    int  f4B0370;          /* 0x104B0370 */

    int  (*pfnMeasure)(const char *psz, int scale);  /* 0x100193C0 */
    void (*pfnDrawString)(const char *psz);          /* 0x10018590 */
} BrTextState;

BrTextState *BrTextGetState(void);

/* 0x100192A0  store two triples of ints and raise the 0x104B0364 flag.
 * Six positional arguments; almost certainly two colours, but nothing here
 * establishes that, so they keep offset names. */
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6);

/* 0x10019300  emit a 2-dword gfx command, resolve alignment, draw.
 *
 * Alignment comes from the global, not from an argument:
 *   0  left    -- x is used as given
 *   1  right   -- x -= measure(psz, scale)
 *   2  centre  -- x -= measure(psz, scale) >> 1
 *   anything else -- x is NOT written at all, so the previous call's x is
 *                    reused. That is the original's behaviour, bug or not. */
void BrTextDraw(const char *psz, int x, int y);

/* NOT in the original as a separate function: the formatting half of
 * 0x100171F0, split out so the (surprising) integer arithmetic is testable.
 *
 *     total      = (int)(fSeconds * 100.0f)     truncated toward zero
 *     hundredths = total % 100
 *     total     /= 100
 *     minutes    = total / 60
 *     seconds    = total % 60
 *     "%s%d:%02d.%02d"  <- pszPrefix, minutes, seconds, hundredths
 *
 * All four divisions are SIGNED and truncate toward zero, so a negative
 * fSeconds produces negative fields and output like "0:-1.-50". */
void BrFormatTime(char *pszOut, size_t cbOut, const char *pszPrefix,
                  float fSeconds);

/* 0x100171F0  draw pszLabel at (x, y) and the formatted time at (x, y + 15).
 * Note the ORDER: the original emits the time line first, then the label. */
void BrHudDrawTimeEntry(const char *pszLabel, const char *pszPrefix,
                        float fSeconds, int x, int y);

/* =====================================================================
 * 3. Glue
 * ===================================================================== */

/* The message record 0x1000BEA0 walks. Only these five dwords are read. */
typedef struct BrAppMsg {
    int32_t id;    /* +0x00 */
    int32_t f04;   /* +0x04 -- not read by this function */
    int32_t f08;   /* +0x08 */
    int32_t f0C;   /* +0x0C */
    int32_t f10;   /* +0x10 */
} BrAppMsg;

typedef struct BrAppMsgHooks {
    int32_t f0AC300;                                  /* 0x100AC300 gate */
    void (*pfnMsg5)(int32_t f08);                     /* 0x10005FE0 */
    void (*pfnMsg107)(void *pv1, int32_t f0C, int32_t f10,
                      int32_t f08, void *pv5);        /* 0x10003580 */
} BrAppMsgHooks;

BrAppMsgHooks *BrAppMsgGetHooks(void);

/* 0x1000BEA0  five-argument cdecl dispatcher. pv3 and pv4 are pushed by
 * every caller but never read; they are kept so the signature matches.
 *
 * The original looks like a wide switch, but both of its tables decode to
 * almost nothing (see the .c): the ONLY live cases are
 *
 *   id == 0x005 and hooks->f0AC300 == 0  ->  pfnMsg5(pMsg->f08)
 *   id == 0x107                          ->  pfnMsg107(pv1, pMsg->f0C,
 *                                                      pMsg->f10,
 *                                                      pMsg->f08, pv5)
 *
 * Every other id, including all of 0x00..0x21 and 0x31..0x106, returns. */
void BrAppMsgDispatch(void *pv1, const BrAppMsg *pMsg, void *pv3, void *pv4,
                      void *pv5);

/* COM-style objects reached by fixed vtable slot. The padding arrays are
 * SLOT counts (9 and 16), which reproduce the original's byte offsets 0x24
 * and 0x68 on a 32-bit build; on a 64-bit host the byte offsets double but
 * the slot indices stay right. */
struct BrComObj;

typedef struct BrComVtbl {
    void *aSlots00[9];                                    /* +0x00..+0x20 */
    int (*pfn24)(struct BrComObj *pThis, void *pv);       /* +0x24 */
    void *aSlots28[16];                                   /* +0x28..+0x64 */
    int (*pfn68)(struct BrComObj *pThis, void *a2, void *a3,
                 void *a4, void *a5, void *a6);           /* +0x68 */
} BrComVtbl;

typedef struct BrComObj {
    const BrComVtbl *pVtbl;      /* +0x00 */
} BrComObj;

/* The three-dword record at 0x10A9D008. */
typedef struct BrComHolder {
    BrComObj *pObj;    /* +0x00 */
    void     *f04;     /* +0x04 -- not read here */
    void     *pArg;    /* +0x08 -- passed on, then cleared */
} BrComHolder;

/* NOT in the original: 0x10A9D008 is itself a pointer global, so this is
 * the slot holding it. Write through it before calling. */
BrComHolder **BrComGetHolderSlot(void);

/* 0x1000C4A0  if the holder, its object and its +0x08 argument are all
 * non-NULL, call vtable slot 9 (+0x24) as (pObj, pArg), then clear +0x08.
 * Returns whatever that call returned, or 0 if any of the three guards
 * failed. The +0x08 clear happens ONLY on the call path. */
int BrComHolderRelease(void);

typedef struct BrComLockHooks {
    void *pCrit;                     /* 0x10277B28 */
    void (*pfnEnter)(void *pCrit);   /* EnterCriticalSection */
    void (*pfnLeave)(void *pCrit);   /* LeaveCriticalSection */
} BrComLockHooks;

BrComLockHooks *BrComGetLockHooks(void);

/* 0x1000C4D0  enter the critical section, forward all six arguments to
 * vtable slot 26 (+0x68), leave the critical section, return the result.
 * The lock is released even though the original has no exception handling,
 * because the call cannot longjmp out in the ported form. */
int BrComCallLocked68(BrComObj *pThis, void *a2, void *a3, void *a4,
                      void *a5, void *a6);

#endif /* SLICE1_03_H */
