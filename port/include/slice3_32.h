/* slice3_32.h -- BRD3D.dll 0x10047930-0x1004A260, a later pass.
 *
 * WHAT THIS RANGE IS
 * ------------------
 * The run loop of the front-end. Three classes cooperate:
 *
 *   BrUiObj     the 0x1E214-byte menu/widget object. This is EXACTLY the
 *               object slice2_23.h models (same +0x1C flags, +0x3C/+0x40
 *               floats, +0x2A40 / +0x1E20C codes, same item block at
 *               +0x2B5C), so this header includes slice2_23.h and reuses its
 *               `BrUiObj` typedef and BR_UI_* offsets rather than inventing a
 *               second name for the same thing.
 *
 *   BrUiPage    a 0x348-byte container of up to 200 BrUiObj*, built by the
 *               constructor at 0x10048470 (vtable 0x1008F6F8) and driven one
 *               item at a time by 0x10048530.  "Page" is descriptive of that
 *               role only; the original asserts no name.
 *
 *   BrPhaseFull the 0xC8-byte phase object slice2_26.h calls `BrPhase`.
 *               Its constructor 0x10048710 is NOT in this packet (slice2_26.h
 *               imports it as `BrPhaseCtor`); its destructor 0x10048870 and
 *               vtable 0x1008F700 ARE, and the 0xC8 layout below is what they
 *               establish.
 *
 *               NAMING: slice2_26.h's `BrPhase` deliberately models only five
 *               fields and states it is "NOT laid out to match".  This range
 *               needs the whole 0xC8 layout (a 20-entry page array at +0x14,
 *               a parallel 20-entry flag array at +0x6C, ...), so the fuller
 *               model is given a SUFFIXED name per the contract's collision
 *               rule.  BrPhaseFull and slice2_26.h's BrPhase are the same
 *               original class; integration should merge them, keeping
 *               these offsets.
 *
 * THE VTABLE OVERLAP (a real, verified oddity)
 * --------------------------------------------
 * 0x1008F6F8 (BrUiPage's vtable) and 0x1008F700 (BrPhaseFull's vtable) are
 * eight bytes apart and the shipped .rdata makes the second the tail of the
 * first:
 *     0x1008F6F8: 100484C0 10048530 | 10048850 100488B0 100488C0 100489A0 ...
 *     0x1008F700:                     10048850 100488B0 100488C0 100489A0 ...
 * The classes are NOT the same (page +0x14 is a count word, phase +0x14 is a
 * pointer array), so this is a linker/data artefact, not a hierarchy. It is
 * however what pins down every `this` in this packet:
 *   phase vtbl +0x00 = 0x10048850  +0x04 = 0x100488B0  +0x08 = 0x100488C0
 *                +0x0C = 0x100489A0  +0x18 = 0x10048B20  +0x1C = 0x10048AA0
 *   page  vtbl +0x00 = 0x100484C0  +0x04 = 0x10048530
 *
 * CONVENTIONS
 * -----------
 * - BrUiObj is reached through byte offsets, never a struct overlay:
 *   slice2_23.h already showed the menu object's offsets do not reconcile
 *   under any single layout.  Its POINTER fields are the one exception and
 *   live in a port-only slot array -- see the DEVIATION in section 2.
 * - BrUiPage and BrPhaseFull are plain structs: their offsets do reconcile.
 *   Neither is laid out to match the original on a 64-bit host, exactly as
 *   slice2_26.h says of its own BrPhase.
 * - GOTCHA carried over into the model: BrPhaseFull+0x08 is a function
 *   pointer everywhere else in the game (slice2_26.h) yet 0x100488C0 reads
 *   its LOW BYTE and tests bit 4.
 * - The file-scope globals this range owns are gathered into BrScrGlobals and
 *   passed in as an added first argument -- the precedent of br_pool.h /
 *   slice2_23.h / slice2_26.h.  Where the original also took arguments they
 *   keep their original order after it.
 * - Every exported name carries its original address as a suffix.  Nothing in
 *   this packet had an establishable semantic name.
 *
 * NOT PORTED (see the report): 0x100491B0, 0x10049C20, 0x10049F40, 0x1004A260.
 */
#ifndef SLICE3_32_H
#define SLICE3_32_H

#include <stddef.h>
#include <stdint.h>

#include "slice2_23.h"   /* BrUiObj + the BR_UI_* offsets of the same object */

/* ==========================================================================
 * 1. Byte-offset accessors
 *
 * Same technique (and same reasons) as slice2_23.h's BrUiLd32 family; the
 * names differ only so the two can coexist in one translation unit.  All go
 * through memcpy, so no alignment or aliasing assumption is made.
 * ========================================================================== */

uint32_t  BrScrLd32 (const void *pObj, size_t off);
void      BrScrSt32 (void *pObj, size_t off, uint32_t v);
int16_t   BrScrLd16 (const void *pObj, size_t off);
uint16_t  BrScrLd16u(const void *pObj, size_t off);
void      BrScrSt16 (void *pObj, size_t off, uint16_t v);
uint8_t   BrScrLd8  (const void *pObj, size_t off);
void      BrScrSt8  (void *pObj, size_t off, uint8_t v);
float     BrScrLdF  (const void *pObj, size_t off);
void      BrScrStF  (void *pObj, size_t off, float v);
void     *BrScrLdPtr(const void *pObj, size_t off);
void      BrScrStPtr(void *pObj, size_t off, void *p);

/* The pointer-slot accessors -- see the DEVIATION note in section 2. `k` is
 * one of the BR_SCR_SLOT_* constants. */
void     *BrScrLdSlot(const void *pObj, int k);
void      BrScrStSlot(void *pObj, int k, void *p);

/* ==========================================================================
 * 2. BrUiObj -- offsets this range touches that slice2_23.h does not name
 *
 * slice2_23.h already gives BR_UI_OFF_VTBL/ONAPPLY(0x10)/FLAGS(0x1C)/
 * F3C/F40/W2A40/ITEM(0x2B5C)/W1E20C and the item-relative BR_UI_ITEM_OFF_*.
 * ========================================================================== */

/* The original's allocation size: 0x10049C20 and friends call operator new
 * with it. It is the size of the 32-BIT object. */
#define BR_SCR_UIOBJ_SIZE   0x1E214u

/* --------------------------------------------------------------------------
 * DEVIATION -- pointer fields.
 *
 * This range reads NINE pointer-typed fields of BrUiObj, and six of them sit
 * FOUR bytes apart (+0x00/+0x04/+0x08/+0x0C and +0x14/+0x18), as do
 * +0x2B5C/+0x2B60. A 64-bit host pointer does not fit in four bytes, so
 * storing them at the original offsets would make them overlap.
 *
 * The port therefore keeps every pointer-typed field in a slot array
 * appended AFTER the original's 0x1E214 bytes, keyed by the constants below.
 * Every non-pointer field keeps its original offset, so the two schemes only
 * differ where they must.
 *
 * INTEGRATION NOTE: slice2_23.c stores the object's vtable at RAW offset
 * 0x00 (it never touches +0x04/+0x08/+0x0C, so it gets away with it). The
 * integration has to unify the two; the slot scheme is the one that
 * generalises, and BR_SCR_UI_PFN* below record the original offsets so the
 * mapping stays visible.
 * -------------------------------------------------------------------------- */
#define BR_SCR_SLOT_VTBL      0   /* original +0x00     */
#define BR_SCR_SLOT_PFN04     1   /* original +0x04     */
#define BR_SCR_SLOT_PFN08     2   /* original +0x08     */
#define BR_SCR_SLOT_PFN0C     3   /* original +0x0C     */
#define BR_SCR_SLOT_PFN14     4   /* original +0x14     */
#define BR_SCR_SLOT_PFN18     5   /* original +0x18     */
#define BR_SCR_SLOT_PHASE     6   /* original +0x2AE8   */
#define BR_SCR_SLOT_ITEMVTBL  7   /* original +0x2B5C   */
#define BR_SCR_SLOT_P1E210    8   /* original +0x1E210  */
#define BR_SCR_SLOT_COUNT     9

/* What the port must actually allocate for one BrUiObj. */
#define BR_SCR_UIOBJ_ALLOC \
    (BR_SCR_UIOBJ_SIZE + (unsigned)BR_SCR_SLOT_COUNT * (unsigned)sizeof(void *))

/* The original offsets of the pointer fields, kept as documentation. */
#define BR_SCR_UI_PFN04     0x0004u /* int32 (*)(BrUiObj*)  __cdecl, may be 0 */
#define BR_SCR_UI_PFN08     0x0008u /* int32 (*)(BrUiObj*)  __cdecl, may be 0 */
#define BR_SCR_UI_PFN0C     0x000Cu /* int32 (*)(BrUiObj*)  __cdecl, may be 0 */
#define BR_SCR_UI_PFN14     0x0014u /* int32 (*)(BrUiObj*)  __cdecl, may be 0 */
#define BR_SCR_UI_PFN18     0x0018u /* int32 (*)(BrUiObj*)  __cdecl, may be 0 */

/* --- plain fields -------------------------------------------------------- */
#define BR_SCR_UI_FLAGS24   0x0024u /* int32 bitfield, OR-ed by 0x10047FB0    */
#define BR_SCR_UI_FLAGS28   0x0028u /* int32 bitfield, bit 0 gates 0x10048010 */
#define BR_SCR_UI_F30       0x0030u /* float -- tween origin x                */
#define BR_SCR_UI_F34       0x0034u /* float -- tween origin y                */
#define BR_SCR_UI_F38       0x0038u /* float -- tween origin z                */
#define BR_SCR_UI_F44       0x0044u /* float -- companion of +0x3C / +0x40    */
#define BR_SCR_UI_W48       0x0048u /* int16                                  */
#define BR_SCR_UI_W4A       0x004Au /* int16                                  */
#define BR_SCR_UI_F50       0x0050u /* int32                                  */
#define BR_SCR_UI_F54       0x0054u /* int32 = (int)obj->f40                  */
#define BR_SCR_UI_F58       0x0058u /* int32, accumulator                     */
#define BR_SCR_UI_F5C       0x005Cu /* int32                                  */
#define BR_SCR_UI_W128      0x0128u /* int16 -- step index into +0x2978       */
#define BR_SCR_UI_F2968     0x2968u /* int32 -- gates 0x100480A0              */
#define BR_SCR_UI_F296C     0x296Cu /* int32 -- selects the two 0x100480A0 arms*/
#define BR_SCR_UI_F2970     0x2970u /* int32 -- last tick                     */
#define BR_SCR_UI_F2974     0x2974u /* int32 -- accumulated ms                */
#define BR_SCR_UI_A2978     0x2978u /* int32[] -- per-step durations          */
#define BR_SCR_UI_W2A42     0x2A42u /* int16 -- alternate code (see 0x10048180)*/
#define BR_SCR_UI_W2AB4     0x2AB4u /* int16 -- child count                   */
#define BR_SCR_UI_A2AB6     0x2AB6u /* int16[] -- child indices into the page  */
#define BR_SCR_UI_PHASE     0x2AE8u /* BrPhaseFull * -- the owning phase
                                     *  (port: BR_SCR_SLOT_PHASE)            */
#define BR_SCR_UI_PW1E210   0x1E210u/* void * -- base of a stride-0x10 array
                                     *  (port: BR_SCR_SLOT_P1E210)           */
#define BR_SCR_UI_F3850     0x3850u /* int32 bitfield                          */

/* --- the item block at +0x2B5C (== slice2_23.h's item 0) ------------------ */
#define BR_SCR_UI_ITEMVTBL  0x2B5Cu /* const BrScrItemVtbl *  (item +0x000)
                                     *  (port: BR_SCR_SLOT_ITEMVTBL)         */
#define BR_SCR_UI_ITEMFLAGS 0x2B60u /* int32                  (item +0x004)   */
#define BR_SCR_UI_ITEMKIND  0x2B64u /* uint8                  (item +0x008)   */
#define BR_SCR_UI_ITEMTEXT  0x2B65u /* char[]                 (item +0x009)   */
#define BR_SCR_UI_ITEMW40A  0x2F66u /* int16                  (item +0x40A)   */
#define BR_SCR_UI_ITEMW40C  0x2F68u /* int16                  (item +0x40C)   */
#define BR_SCR_UI_ITEMF410  0x2F6Cu /* int32                  (item +0x410)   */
#define BR_SCR_UI_ITEMF414  0x2F70u /* int32                  (item +0x414)   */
#define BR_SCR_UI_ITEMF418  0x2F74u /* int32                  (item +0x418)   */
#define BR_SCR_UI_ITEMW41C  0x2F78u /* int16                  (item +0x41C)   */
#define BR_SCR_UI_ITEMF420  0x2F7Cu /* int32                  (item +0x420)   */
#define BR_SCR_UI_ITEMF424  0x2F80u /* int32                  (item +0x424)   */
#define BR_SCR_UI_ITEMF42C  0x2F88u /* int32                  (item +0x42C)   */

/* --- the tween block at +0x3804 ------------------------------------------ */
#define BR_SCR_UI_TWXON     0x3804u /* int32 -- x axis enabled                */
#define BR_SCR_UI_TWYON     0x3808u /* int32 -- y axis enabled                */
#define BR_SCR_UI_TWXDIR    0x380Cu /* uint8 -- 1 up, 0xFF down, 0 = "done"   */
#define BR_SCR_UI_TWYDIR    0x380Du /* uint8                                  */
#define BR_SCR_UI_TWXEND    0x3810u /* float -- x limit                       */
#define BR_SCR_UI_TWYEND    0x3814u /* float -- y limit                       */
#define BR_SCR_UI_TWACTIVE  0x3818u /* int32                                  */
#define BR_SCR_UI_TWLO      0x381Cu /* float                                  */
#define BR_SCR_UI_TWHI      0x3820u /* float                                  */
#define BR_SCR_UI_TWRATE    0x3824u /* float = (TWHI - TWLO) / n              */
#define BR_SCR_UI_TWTICK    0x3828u /* int32 -- last tick                     */
#define BR_SCR_UI_TWMS      0x382Cu /* int32 -- accumulated ms                */

/* Flag bits the range tests on +0x1C. Named for their value only. */
#define BR_SCR_F1C_0002     0x00000002u
#define BR_SCR_F1C_0004     0x00000004u
#define BR_SCR_F1C_0010     0x00000010u
#define BR_SCR_F1C_0020     0x00000020u
#define BR_SCR_F1C_0800     0x00000800u
#define BR_SCR_F1C_1000     0x00001000u
#define BR_SCR_F1C_2000     0x00002000u
#define BR_SCR_F1C_4000     0x00004000u
#define BR_SCR_F1C_10000    0x00010000u
#define BR_SCR_F1C_20000    0x00020000u
#define BR_SCR_F1C_100000   0x00100000u
#define BR_SCR_F1C_200000   0x00200000u
#define BR_SCR_F1C_400000   0x00400000u
#define BR_SCR_BIT100       0x00000100u  /* OR-ed into +0x1C and +0x3850     */

/* ==========================================================================
 * 3. Vtables
 * ========================================================================== */

typedef struct BrUiPage     BrUiPage;
typedef struct BrPhaseFull  BrPhaseFull;

/* The __cdecl hooks stored in BrUiObj's +0x04/+0x08/+0x0C/+0x14/+0x18 fields.
 * All are called with the object as their one argument; the return value is
 * only inspected for +0x04 (-1 / -2 are sentinels), +0x08, +0x14 and +0x18. */
typedef int32_t (*BrScrUiHookFn)(BrUiObj *pObj);

/* The BrUiObj vtable.  This is the same vtable slice2_23.h models as
 * BrUiObjVtbl; that header only needed slots up to +0x14, this one needs
 * up to +0x3C, so a second VIEW of it is declared here rather than a
 * conflicting redefinition of the same name.  Slots this range never calls
 * are left as void * on purpose -- do not read a signature into them. */
typedef struct BrScrUiVtbl {
    void   *f00;
    void  (*f04)(BrUiObj *pThis);                 /* +0x04 */
    void  (*f08)(BrUiObj *pThis);                 /* +0x08 */
    int32_t (*f0C)(BrUiObj *pThis);               /* +0x0C */
    int32_t (*f10)(BrUiObj *pThis);               /* +0x10 */
    void   *f14;
    void  (*f18)(BrUiObj *pThis, void *p);        /* +0x18 */
    void  (*f1C)(BrUiObj *pThis);                 /* +0x1C */
    int32_t (*f20)(BrUiObj *pThis);               /* +0x20 */
    void   *f24;
    float (*f28)(BrUiObj *pThis, int32_t ms);     /* +0x28 -- 0x10047CE0     */
    void   *f2C;
    void  (*f30)(BrUiObj *pThis);                 /* +0x30 */
    void   *f34;
    void   *f38;
    int32_t (*f3C)(BrUiObj *pThis);               /* +0x3C -- nonzero = skip */
} BrScrUiVtbl;

/* The vtable of the object at BrUiObj+0x2B5C. Only +0x04/+0x08/+0x28 are
 * reached from this packet. slice2_23.h calls the same thing BrUiWidgetVtbl. */
typedef struct BrScrItemVtbl {
    void   *f00;
    void  (*f04)(void *pThis);                    /* +0x04 */
    void  (*f08)(void *pThis);                    /* +0x08 */
    void   *f0C;
    void  (*f10)(void *pThis);                    /* +0x10 -- 0x10048010     */
    void   *f14, *f18, *f1C, *f20, *f24;
    float (*f28)(void *pThis);                    /* +0x28 -- result DROPPED */
} BrScrItemVtbl;

/* 0x1008F6F8. Only slot 0 is invoked from this packet, as f00(this, 1) --
 * the MSVC "scalar deleting destructor" shape. */
typedef struct BrUiPageVtbl {
    void *(*f00)(BrUiPage *pThis, int32_t nFlags);   /* 0x100484C0 */
    int32_t (*f04)(BrUiPage *pThis);                 /* 0x10048530 */
} BrUiPageVtbl;

/* 0x1008F700. */
typedef struct BrPhaseFullVtbl {
    void *(*f00)(BrPhaseFull *pThis, int32_t nFlags);/* 0x10048850 */
    int32_t (*f04)(BrPhaseFull *pThis);              /* 0x100488B0 */
    int32_t (*f08)(BrPhaseFull *pThis);              /* 0x100488C0 */
    int32_t (*f0C)(BrPhaseFull *pThis);              /* 0x100489A0 */
    void   *f10;
    void  (*f14)(BrPhaseFull *pThis);                /* 0x10048960, foreign */
    void  (*f18)(BrPhaseFull *pThis, void *pArg);    /* 0x10048B20 */
    void  (*f1C)(BrPhaseFull *pThis);                /* 0x10048AA0 */
    void  (*f20)(BrPhaseFull *pThis);                /* 0x1005AFA0, foreign */
} BrPhaseFullVtbl;

/* ==========================================================================
 * 4. BrUiPage -- the 0x348-byte object built by 0x10048470
 * ========================================================================== */

#define BR_UI_PAGE_ITEMS   200      /* 0xC8 -- the rep stosd count in the ctor */
#define BR_UI_PAGE_ORIG_SIZE 0x348u /* the literal 0x10049C20 hands to new     */

/* +0x04/+0x08/+0x0C are called with NO arguments at all (`call eax`, no
 * push, no stack adjust). */
typedef void (*BrUiPageHookFn)(void);

struct BrUiPage {
    const BrUiPageVtbl *pVtbl;              /* +0x000 = 0x1008F6F8          */
    BrUiPageHookFn      pfn04;              /* +0x004                        */
    BrUiPageHookFn      pfn08;              /* +0x008                        */
    BrUiPageHookFn      pfn0C;              /* +0x00C                        */
    int32_t             f10;                /* +0x010                        */
    uint16_t            nItems;             /* +0x014                        */
    uint16_t            f16;                /* +0x016 -- the ctor leaves it   */
    BrUiObj            *aItems[BR_UI_PAGE_ITEMS]; /* +0x018 .. +0x338        */
    float               f338;               /* +0x338 (0x10049C20 -> 195.0f) */
    float               f33C;               /* +0x33C (0x10049C20 -> 130.0f) */
    BrPhaseFull        *pOwner;             /* +0x340                        */
    uint16_t            f344;               /* +0x344 -- selection modulus   */
    uint16_t            f346;               /* +0x346 -- selection           */
};

/* GOTCHA: 0x100488C0 reads the object at byte offset 0x334 of the page, which
 * under this (verified) layout is aItems[199] -- the LAST slot, not a field
 * of its own. Nothing else in the range writes that slot. */
#define BR_UI_PAGE_ITEM334  199

/* ==========================================================================
 * 5. BrPhaseFull -- the 0xC8-byte phase (slice2_26.h's BrPhase, in full)
 * ========================================================================== */

#define BR_PHASE_PAGES   20     /* +0x14..+0x64 and +0x6C..+0xBC             */

struct BrPhaseFull {
    const BrPhaseFullVtbl *pVtbl;       /* +0x00 = 0x1008F700                */
    void       *pfn04;                  /* +0x04 (slice2_26: BrPhaseEnterFn) */
    void       *pfn08;                  /* +0x08 (slice2_26: BrPhaseHookFn)  */
    int32_t     f0C;                    /* +0x0C                             */
    uint16_t    nPages;                 /* +0x10                             */
    uint16_t    iPage;                  /* +0x12                             */
    BrUiPage   *aPages[BR_PHASE_PAGES]; /* +0x14 .. +0x64                    */
    BrUiPage   *pCur;                   /* +0x64                             */
    int32_t     f68;                    /* +0x68 -- slice2_26's f68          */
    int32_t     aFlags[BR_PHASE_PAGES]; /* +0x6C .. +0xBC                    */
    uint16_t    fBC;                    /* +0xBC                             */
    uint16_t    fBE;
    void       *fC0;                    /* +0xC0 -- released by 0x10048870   */
    void       *fC4;                    /* +0xC4 -- released by 0x10048870   */
};

/* +0x08 is a function pointer everywhere else in the game, yet 0x100488C0
 * tests bit 4 of its LOW BYTE. Reproduced here on the pointer VALUE. */
#define BR_PHASE_PFN08_BIT10 0x10u

/* What the refcounted things at +0xC0/+0xC4 look like from here: a vtable
 * whose slot 0 is the scalar deleting destructor, invoked as f00(this, 1). */
typedef struct BrScrRefVtbl BrScrRefVtbl;
typedef struct BrScrRef { const BrScrRefVtbl *pVtbl; } BrScrRef;
struct BrScrRefVtbl { void *(*f00)(BrScrRef *pThis, int32_t nFlags); };

/* ==========================================================================
 * 6. The rectangle table at 0x100AB568 and the object at 0x10AA2E80
 * ========================================================================== */

/* Verified by reading .rdata: entry i is
 *     { i, {0,0,w,h}, flag }
 * with entry 0 = {0,0,640,480}. 0x18 bytes. Only +0x00 (as a word), the
 * ADDRESS of +0x04 and the dword at +0x14 are used. */
typedef struct BrScrRect {
    int32_t f00, f04, f08, f0C;
} BrScrRect;

typedef struct BrScrRectEnt {
    int32_t   f00;   /* +0x00 -- equals the entry's own index                */
    BrScrRect rc;    /* +0x04 -- passed BY ADDRESS to 0x1005F5A0            */
    int32_t   f14;   /* +0x14                                               */
} BrScrRectEnt;

/* *(void**)0x10AA2E80. Only these four ints are read in this range; +0x00 and
 * +0x04 are fild-ed (signed 32-bit), +0x2C/+0x30 are only compared to 0.
 *
 * +0x34 and +0x38 are NOT read in this range and are here so that this model
 * and slice3_31.h's model of the SAME original object are the same object.
 * They were two shapes of different lengths -- slice3_31.h stops at +0x38
 * because 0x10047360 tests four consecutive dwords, this one stopped at +0x30
 * because 0x10047A60 tests only two -- and a host that allocates one and
 * passes it to a consumer typed over the other must not be handing over a
 * short object. br_sprfont.c is that consumer: its struct-model transcription
 * of 0x10047360 reads all four. */
typedef struct BrObjAA2E80 {
    int32_t f00;
    int32_t f04;
    int32_t aPad[9];   /* +0x08 .. +0x28 */
    int32_t f2C;
    int32_t f30;
    int32_t f34;
    int32_t f38;
} BrObjAA2E80;

/* ==========================================================================
 * 7. The globals
 * ========================================================================== */

typedef struct BrScrGlobals {
    /* --- tables / foreign objects ---------------------------------------- */
    const BrScrRectEnt *aAB568;   /* 0x100AB568 -- stride 0x18               */
    uint16_t            w0AB3DC;  /* 0x100AB3DC -- the step 0x10048530 adds
                                   *  to g_AA286C; == 1 in the shipped data  */
    BrObjAA2E80        *pAA2E80;  /* 0x10AA2E80                              */
    void               *pB4DF30;  /* 0x10B4DF30 -- slice2_25.h's g_aBrB4DF30 */
    void               *pB4FBE8;  /* 0x10B4FBE8 -- passed BY ADDRESS         */

    /* --- scalars --------------------------------------------------------- */
    int32_t   n0940A4;   /* 0x100940A4 */
    int32_t   n0AC300;   /* 0x100AC300 */
    int32_t   nA9CFFC;   /* 0x10A9CFFC */
    int32_t   nAA2854;   /* 0x10AA2854 -- 2 or 3 lengthen 0x10048B20's wait  */
    int32_t   nAA2858;   /* 0x10AA2858 */
    int32_t   nAA2868;   /* 0x10AA2868 */
    /* Held unsigned so the port never relies on an implementation-defined
     * narrowing conversion; every place the ORIGINAL sign-extends
     * (`movsx esi, ax` in 0x100484F0) casts through int16_t explicitly. */
    uint16_t  wAA286C;   /* 0x10AA286C -- the page-wide selection cursor     */
    uint16_t  wAA2870;   /* 0x10AA2870 -- items seen this pass               */
    int32_t   nAA2874;   /* 0x10AA2874 */
    int32_t   nAA2880;   /* 0x10AA2880 */
    uint8_t   bAA28A8;   /* 0x10AA28A8 -- byte                               */
    int32_t   nAA28D8;   /* 0x10AA28D8 */
    int32_t   nAA29A8;   /* 0x10AA29A8 */
    int32_t   nAA29AC;   /* 0x10AA29AC */
    int32_t   nAA29B8;   /* 0x10AA29B8 */
    int32_t   nAA29C4;   /* 0x10AA29C4 */
    int32_t   nAA29CC;   /* 0x10AA29CC */
    int32_t   nAA29D0;   /* 0x10AA29D0 */
    int32_t   nAA29D4;   /* 0x10AA29D4 */
    int32_t   nAA29E0;   /* 0x10AA29E0 */
    int32_t   nAA29E4;   /* 0x10AA29E4 */
    int32_t   nAA29E8;   /* 0x10AA29E8 */
    int32_t   nAA29EC;   /* 0x10AA29EC */
    int32_t   nAA29F0;   /* 0x10AA29F0 */
    int32_t   nAA2A34;   /* 0x10AA2A34 */
    int32_t   nAA2A4C;   /* 0x10AA2A4C -- free-running counter, mod 0x78     */
    int32_t   nAA33E4;   /* 0x10AA33E4 */

    /* --- phase slots ------------------------------------------------------ */
    BrPhaseFull *pAA2900;  /* 0x10AA2900 -- NOT a phase slot: 0x10048B20
                            *  deletes it directly, no vtable call           */
    BrPhaseFull *pAA2904;  /* 0x10AA2904 -- the CURRENT phase                */
    BrPhaseFull *pAA2908;
    BrPhaseFull *pAA290C;
    BrPhaseFull *pAA2910;
    BrPhaseFull *pAA2914;
    BrPhaseFull *pAA2918;
    BrPhaseFull *pAA291C;
    BrPhaseFull *pAA2920;
    BrPhaseFull *pAA2924;
    BrPhaseFull *pAA2928;
    BrPhaseFull *pAA292C;
    BrPhaseFull *pAA2930;
    BrPhaseFull *pAA2934;
    BrPhaseFull *pAA2938;
    BrPhaseFull *pAA293C;
    BrPhaseFull *pAA2940;
    BrPhaseFull *pAA2944;
    BrPhaseFull *pAA2948;
    BrPhaseFull *pAA294C;
    BrPhaseFull *pAA2950;
    BrPhaseFull *pAA2954;
    BrPhaseFull *pAA2958;
    BrPhaseFull *pAA295C;
    BrPhaseFull *pAA2960;
    BrPhaseFull *pAA2964;
    BrPhaseFull *pAA2968;
    BrPhaseFull *pAA296C;
    BrPhaseFull *pAA2970;
    BrPhaseFull *pAA2974;
    BrPhaseFull *pAA297C;
    BrPhaseFull *pAA2980;
    BrPhaseFull *pAA2984;
    BrPhaseFull *pAA2988;
    BrPhaseFull *pAA298C;
    BrPhaseFull *pAA2990;
    BrPhaseFull *pAA2994;
    BrPhaseFull *pAA2998;
    BrPhaseFull *pAA29B0;
    BrPhaseFull *pAA29B4;
    BrPhaseFull *pAA29F4;

    /* 0x10AA29C0 is a BrUiObj, NOT a phase: 0x10048060 dereferences its
     * +0x2AE8 (the owning phase). 0x10048B20 only ever NULLs it. */
    BrUiObj     *pAA29C0;
    /* 0x10AA29D8 -- an entity record (slice2_26.h); only NULLed here. */
    void        *pAA29D8;

    /* 0x10048180 compares BrUiObj+0x08 against two literal code addresses.
     * The port cannot compare against a 32-bit image address, so the two are
     * carried as function pointers integration fills in. */
    void       *pfn10043760;  /* 0x10043760 */
    void       *pfn10042CF0;  /* 0x10042CF0 */

    /* 0x10048B20 walks 145 slots of stride 0x74 from 0x10A9E3D0 to
     * 0x10AA2584, freeing the pointer at the start of each. The port takes
     * the array as an explicit pointer + count so no address arithmetic on a
     * fabricated base is needed. */
    void      **apA9E3D0;     /* 145 entries, each a `void *` slot           */
    int32_t     nA9E3D0;      /* == BR_SCR_A9E3D0_COUNT                      */
} BrScrGlobals;

#define BR_SCR_A9E3D0_COUNT   145u   /* (0x10AA2584-0x10A9E3D0)/0x74 exactly */
#define BR_SCR_A9E3D0_STRIDE  0x74u

/* ==========================================================================
 * 8. Cross-slice imports. Stand-ins live in port/tests/test_slice3_32.c and
 *    nowhere else.
 * ========================================================================== */

/* XSLICE 0x1007C8A0 */
/* __ftol: truncate toward zero, low dword of a `fistp qword`. The name other
 * slices already use (slice2_21.h). */
extern int32_t BrFtolTrunc(float f);

/* XSLICE 0x10075020 */
/* The platform millisecond tick. Monotonic, wraps as int32. */
extern int32_t BrSub10075020(void);

/* XSLICE 0x1007DE40 */ extern void  BrOperatorDelete(void *p);  /* slice3_39 */

/* XSLICE 0x100484E0 */
/* The BrUiPage destructor body. It sits INSIDE this packet's address range
 * but was not in the packet listing, so it is imported rather than guessed. */
extern void  BrSub100484E0(BrUiPage *pThis);

/* XSLICE 0x1005F5A0 */
/* Five __cdecl arguments. In every call site here the first two are screen
 * coordinates, the third an id, the fourth the ADDRESS of a BrScrRect and
 * the fifth the table entry's +0x14 dword. */
extern void BrSub1005F5A0(int32_t x, int32_t y, int32_t id,
                          const void *pRect, int32_t f14);

/* NAME COLLISIONS ALREADY IN THE TREE: 0x1003E310, 0x1006A4A0 and 0x10072AF0
 * are declared as BrSub* by slice2_25.h and as BrExt_* by slice3_31.h. The
 * older slice2_25.h spelling is used here; integration has to pick one. */
/* XSLICE 0x1003E310 */ extern void BrSub1003E310(void);   /* slice2_25 name */
/* XSLICE 0x1006A4A0 */ extern void BrSub1006A4A0(void *pThis, void *pArg);
/* XSLICE 0x10060260 */ extern void BrSub10060260(void *pThis);  /* thiscall */
/* XSLICE 0x1005FFB0 */ extern void BrDikPollAndEdge(void);  /* slice3_39 name */
/* XSLICE 0x1005F530 */ extern void BrSub1005F530(void);
/* XSLICE 0x1005FCF0 */ extern void BrSub1005FCF0(void);   /* slice2_25 name */
/* XSLICE 0x10002910 */ extern int  BrCdTrackGet(void);    /* slice2_11 name */
/* XSLICE 0x10072AF0 */ extern void BrSub10072AF0(int a, int b);

/* XSLICE 0x10008B80 */
/* A bare `ret` in this build. slice2_18.h already declares one name per
 * observed arity; 0x10048B20 calls it __thiscall with a pointer. */
extern void BrStub8B80_1p(const void *p0);

/* KERNEL32!Sleep, imported at 0x118AE548. */
extern void BrScrSleep(uint32_t ms);

/* The two vtables the constructors install. They are DATA, not code, and
 * their slots are functions from several slices, so they are imported rather
 * than defined here. See the overlap note at the top of this header. */
/* XSLICE 0x1008F6F8 */ extern const BrUiPageVtbl    BrUiPageVtbl_1008F6F8;
/* XSLICE 0x1008F700 */ extern const BrPhaseFullVtbl BrPhaseVtbl_1008F700;

/* ==========================================================================
 * 9. The range
 * ========================================================================== */

/* --- 0x10047930 / 0x10047980 / 0x100479D0 -- three ways to hand the code at
 *     BrUiObj+0x1E20C, or an explicit one, to 0x1005F5A0 ------------------- */

/* 0x10047930  __thiscall, no argument. Does nothing when the code is
 * negative. Always returns 1.
 *
 * GOTCHA (reproduced exactly): the original loads the table entry's id with
 * `mov ax, word ptr [...]`, which leaves the HIGH half of EAX holding the
 * high half of `code * 0x18` from the index computation, and then pushes the
 * whole 32-bit EAX. For code <= 0xAAA the high half is zero and the argument
 * is just the id; from 0xAAB up it is not. */
int BrUiDrawCode_10047930(const BrScrGlobals *pG, BrUiObj *pObj);

/* 0x10047980  __stdcall(pRect): as 0x10047930 but the caller supplies the
 * fourth argument instead of the table's rectangle, and the sign check is
 * NOT done -- a negative code indexes the table out of bounds.
 *
 * DEVIATION: here the original's `mov ax, ...` leaves the high half of EAX
 * holding whatever the CALLER left in it -- genuinely indeterminate. The port
 * zero-extends. See the .c for the exact line. */
int BrUiDrawCodeRect_10047980(const BrScrGlobals *pG, BrUiObj *pObj,
                              const void *pRect);

/* 0x100479D0  __stdcall(code, x, y), no object at all. Always returns 1.
 * GOTCHA: unlike 0x10047930 this passes the CALLER'S index as the id, not
 * the table entry's own +0x00 dword (they are equal in the shipped table). */
int BrUiDrawIndex_100479D0(const BrScrGlobals *pG, int32_t code,
                           int32_t x, int32_t y);

/* 0x10047A10  __thiscall. With +0x296C clear it just fires the object's
 * vtable +0x1C and returns. Otherwise it republishes the code for the
 * current step index (+0x128) out of the int16 table at +0x2A40 and hands
 * the matching stride-0x10 record from +0x1E210 to vtable +0x18.
 * Always returns 1. */
int BrUiStepCode_10047A10(BrUiObj *pObj);

/* --- 0x10047CB0 .. 0x10047D30 -- the two-axis tween ---------------------- */

/* 0x10047CB0  __stdcall(n). rate = (TWHI - TWLO) / n and snapshot the
 * current +0x3C/+0x40/+0x44 into +0x30/+0x34/+0x38. Returns 1.
 * GOTCHA: `n` is divided as an INTEGER by fidiv; n == 0 raises the x87
 * zero-divide exception in the original (masked -> infinity). */
int BrUiTweenBegin_10047CB0(BrUiObj *pObj, int32_t n);

/* 0x10047CE0  __stdcall(n) -> st(0). (float)(n*n) * rate * 0.5f * 0.001f.
 * The two constants are 0x1008F678 = 0.5f and 0x1008F67C = 0.001f, read out
 * of .rdata. `n*n` overflows exactly as a 32-bit signed multiply does. */
float BrUiTweenCurve_10047CE0(const BrUiObj *pObj, int32_t n);

/* 0x10047D10  __thiscall. Copies +0x30/+0x34/+0x38 back into
 * +0x3C/+0x40/+0x44 -- the OPPOSITE direction to 0x10047CB0 -- and arms the
 * tween (+0x3818 = 1). Returns 1. */
int BrUiTweenReset_10047D10(BrUiObj *pObj);

/* 0x10047D30  __thiscall. One tween step; returns 1 always.
 *
 * GOTCHAs:
 *  - the accumulator +0x382C is NEVER reset except when BOTH axes have
 *    finished in the same call, at which point +0x3818 is cleared too;
 *  - an axis whose direction byte is neither 0, 1 nor 0xFF counts as NOT
 *    finished, so a tween with a bad direction byte never stops;
 *  - an axis that is switched off (+0x3804 / +0x3808 == 0) counts as
 *    finished immediately;
 *  - the clamps are asymmetric: direction 1 clamps on >=, direction 0xFF on
 *    <= (that is x87 C0 vs C0|C3, so an unordered compare clamps for 0xFF
 *    and does not for 1). */
int BrUiTweenStep_10047D30(BrUiObj *pObj);

/* --- 0x10047EB0 / 0x10047FB0 -- setting the object up -------------------- */

/* 0x10047EB0  __stdcall(psz, nFlags, bKind, pSrc). Copies psz (WITH its NUL,
 * unbounded -- the original uses rep movs and does not check) into the item
 * text at +0x2B65, ORs nFlags into the item's +0x2B60, stores bKind at
 * +0x2B64, seeds a block of item fields from pSrc[0] and pSrc[2], then
 * dispatches the item's vtable +0x08 when bKind == 3 and +0x04 otherwise.
 *
 * GOTCHA: when nFlags bit 0 is set it additionally calls the item's vtable
 * +0x28 and THROWS THE FLOAT AWAY (fstp st(0)) -- it is called purely for
 * its side effects.
 *
 * GOTCHA: +0x2F66 and +0x2F68 are zeroed BEFORE the dispatch and RE-READ
 * after it, so a hook that writes them decides +0x48/+0x4A/+0x5C. */
void BrUiItemInit_10047EB0(BrUiObj *pObj, const char *psz, uint32_t nFlags,
                           uint8_t bKind, const int32_t *pSrc);

/* 0x10047FB0  __stdcall, eight arguments, in the original's order.
 * a3/a4/a5 are OR-ed into +0x1C/+0x24/+0x28; the others are stored.
 * GOTCHA: the code word goes to BOTH +0x2A40 and +0x1E20C. */
void BrUiInit_10047FB0(BrUiObj *pObj, BrPhaseFull *pPhase,
                       float f3C, float f40,
                       uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                       uint32_t n2968, int16_t wCode);

/* --- 0x10048010 / 0x10048060 / 0x100480A0 / 0x10048180 ------------------- */

/* 0x10048010  __thiscall. Returns 1 in every case except one: when +0x28 bit
 * 0 is set, +0x1C has neither 0x100000 nor 0x200000, and the object's vtable
 * +0x10 returns 0, it returns 0. */
int BrUiEnter_10048010(BrUiObj *pObj);

/* 0x10048060  __thiscall. Three outcomes:
 *   no 0x10AA29C0, or its phase's aFlags[1] != 1  -> nAA2858 = 0, return 0
 *   pObj IS 0x10AA29C0                            -> return 0, nAA2858 kept
 *   otherwise                                     -> nAA2858 = 1, return 1 */
int BrUiCheckOther_10048060(BrScrGlobals *pG, const BrUiObj *pObj);

/* 0x100480A0  __thiscall. Advances the step index +0x128 through the
 * duration table at +0x2978, or (when +0x296C is clear) just raises bit 0x100
 * every 0x3C ms. Always returns 1.
 * GOTCHA: the +0x296C arm compares with `jle`, so a zero-length step never
 * fires; the +0x296C-clear arm compares with `jle 0x3C`, i.e. it needs
 * STRICTLY MORE than 60 ms. */
int BrUiTickSteps_100480A0(BrUiObj *pObj);

/* 0x10048180  __thiscall, the per-object frame. Returns 0 only when the
 * object's own +0x04 hook returns -1; 1 otherwise.
 * GOTCHA: a +0x04 hook returning -2 skips the entire body but still
 * returns 1. */
int BrUiFrame_10048180(BrScrGlobals *pG, BrUiObj *pObj);

/* --- BrUiPage ------------------------------------------------------------ */

/* 0x10048470  __thiscall constructor; returns `this`. Zeroes +0x10..+0x346
 * and installs the 0x1008F6F8 vtable.
 * GOTCHA: the two bytes at +0x16 are NOT touched -- and operator new does not
 * zero -- so they are indeterminate after construction. */
BrUiPage *BrUiPageCtor_10048470(BrUiPage *pThis);

/* 0x100484C0  __stdcall(nFlags) scalar deleting destructor; returns `this`
 * even after freeing it, exactly as MSVC emits. */
void *BrUiPageDelete_100484C0(BrUiPage *pThis, int32_t nFlags);

/* 0x100484F0  __thiscall. Re-clamps the global selection cursor against this
 * page's +0x344 and copies the result to +0x346. Returns 1.
 * GOTCHA: the comparison is SIGNED between the sign-extended cursor and the
 * ZERO-extended +0x344, and the middle case (0 <= cursor < f344) leaves the
 * global alone while still writing +0x346. */
int BrUiPageSelect_100484F0(BrScrGlobals *pG, BrUiPage *pThis);

/* 0x10048530  __thiscall, the page's frame. Returns 0 as soon as any item
 * refuses (a NULL slot, a +0x14 / +0x18 hook returning 0, or vtable +0x0C
 * returning 0 -- which also clears bAA28A8); 1 when the whole page ran.
 * GOTCHA: the +0x08 tail hook runs ONLY on the success path. */
int BrUiPageFrame_10048530(BrScrGlobals *pG, BrUiPage *pThis);

/* --- BrPhaseFull --------------------------------------------------------- */

/* 0x10048850  __stdcall(nFlags) scalar deleting destructor. */
void *BrPhaseDelete_10048850(BrPhaseFull *pThis, int32_t nFlags);

/* 0x10048870  __thiscall destructor body: re-seats the vtable, then releases
 * +0xC0 and +0xC4 through their own slot 0 with the argument 1.
 * GOTCHA: +0xC4 is READ before +0xC0 is cleared -- irrelevant unless the two
 * alias, which the original does not check. */
void BrPhaseDtor_10048870(BrPhaseFull *pThis);

/* 0x100488B0  __thiscall. Fires the phase's own vtable +0x20 and returns 1. */
int BrPhaseFn_100488B0(BrPhaseFull *pThis);

/* 0x100488C0  __thiscall, phase vtable +0x08. Returns 0 (and does nothing)
 * when +0x08's low byte has bit 4 set; 1 otherwise.
 * Every 0x78th call (or every call when n0940A4 == 2) it re-reads the CD
 * track. It then borrows 0x10AA2908 as the current phase, pushes the
 * 0x10AA2E80 ints into aPages[0]->aItems[199]'s +0x3C/+0x40, fires that
 * item's vtable +0x0C, and restores the previous current phase.
 * GOTCHA: nAA2A4C is incremented ONLY on the n0940A4 != 2 path. */
int BrPhaseTick_100488C0(BrScrGlobals *pG, BrPhaseFull *pThis);

/* 0x100489A0  __thiscall, phase vtable +0x0C. Runs every page whose parallel
 * flag is set. Returns 1 only when +0x68 is still set at the end; 0
 * otherwise, and on the 0 paths it re-runs the same two-call teardown.
 * GOTCHA: pCur (+0x64) is written with the page pointer BEFORE the NULL test,
 * so a NULL entry leaves pCur NULL. */
int BrPhaseRun_100489A0(BrScrGlobals *pG, BrPhaseFull *pThis);

/* 0x10048AA0  __thiscall, phase vtable +0x1C. Releases all 200 item slots of
 * every page, then the page, then clears the selection cursor.
 * GOTCHA: the original walks a NULL page's item array before it null-checks
 * the page -- see the DEVIATION in the .c. */
void BrPhaseReleasePages_10048AA0(BrScrGlobals *pG, BrPhaseFull *pThis);

/* 0x10048B20  __thiscall(pArg), phase vtable +0x18. The global shutdown: it
 * spins on the tick for a while, then drops 36 phase slots in a fixed order,
 * and -- only when pArg is NULL -- the 0x10A9E3D0 table, 0x10AA2908 and
 * 0x10AA2900 as well.
 *
 * GOTCHA: the `this` pointer is IGNORED; the function is pure global work.
 * GOTCHA: 0x10AA2940 is dropped TWICE (first and fifteenth); the second pass
 *         is a no-op only because the first cleared the slot.
 * GOTCHA: each slot's vtable +0x1C runs first, then the slot is RE-READ
 *         before the release through slot 0 -- a +0x1C that clears its own
 *         global therefore skips the release. */
void BrPhaseShutdown_10048B20(BrScrGlobals *pG, void *pArg);

#endif /* SLICE3_32_H */
