/* slice2_23.h -- BRD3D.dll 0x1003DC10-0x10040330, a later pass.
 *
 * This range is one module: the front-end / options menu. Apart from the
 * DirectPlay error-name table at the start, every routine here is a menu-item
 * callback of the form `int32_t f(BrUiObj *pObj)` (__cdecl, one argument,
 * always returns 1), reading its inputs out of file-scope globals.
 *
 * MODELLING NOTE -- why BrUiObj is a byte array and not a struct.
 * The object the callbacks receive is addressed at fixed byte offsets that
 * OVERLAP under any single struct layout:
 *   - 0x1003EE50 indexes an array based at +0x2B5C with stride 0x438;
 *   - 0x1003EBE0 indexes a second array based at +0x3C98 with the SAME stride
 *     0x438, and 0x3C98 - 0x2B5C == 0x113C is not a multiple of 0x438;
 *   - +0x3838 and +0x1E20C do not sit on either grid.
 * Rather than invent a layout that reconciles them, the object is left
 * byte-addressed and every offset the original touches is exported as a
 * BR_UI_OFF_* constant. Accessors below do the loads/stores through memcpy,
 * so no alignment or aliasing assumption is made.
 *
 * Globals are gathered into BrUiGlobals / BrStartupState and passed in, the
 * precedent set by br_pool.h / br_span.h / slice1_06.h. Field names are
 * positional (the address they came from); no meaning is asserted that could
 * not be read off the code.
 *
 * Argument order: pObj first, then the globals block (destination-first,
 * br_vec.h convention). The original takes only pObj.
 */
#ifndef SLICE2_23_H
#define SLICE2_23_H

#include <stddef.h>
#include <stdint.h>

#include "br_state.h"   /* BrActiveFlags / BrIsAnyActive -- 0x1003E080 */

/* ==========================================================================
 * 0x1003DC10 -- DirectPlay HRESULT -> name
 * ========================================================================== */

/* Returns the DPERR_* / DP_OK spelling of `hr`, or, for a value not in the
 * table, "0x%08X" of it rendered into a STATIC buffer (the original writes
 * the global at 0x10A9BFE0 through wsprintfA). The returned pointer is
 * therefore only valid until the next unknown-code call, and this routine is
 * not re-entrant -- that is the original's contract, not an artefact.
 *
 * GOTCHA: the original's binary search compares SIGNED, and every table entry
 * except DP_OK is negative as int32. Passing an unsigned value works only
 * because the comparisons are on the same bit pattern; the parameter is
 * int32_t to keep that explicit. */
const char *BrDPlayErrName(int32_t hr);

/* ==========================================================================
 * 0x1003DFC0 -- one-shot assignment of nine unrelated globals
 * ========================================================================== */

typedef struct BrStartupState {
    int32_t  g0B380C;   /* 0x100B380C <- 0 */
    int32_t  g22B350;   /* 0x1022B350 <- 0 */
    int32_t  g22B34C;   /* 0x1022B34C <- 0 */
    int32_t  g094350;   /* 0x10094350 <- 1 */
    int32_t  g094354;   /* 0x10094354 <- 1 */
    int32_t  g094358;   /* 0x10094358 <- 1 */
    int32_t  g09435C;   /* 0x1009435C <- 2 */
    int32_t  gB4E1D0;   /* 0x10B4E1D0 <- 0 */
    void    *gB4E1D4;   /* 0x10B4E1D4 <- &g_B4DF30 (0x10B4DF30) */
} BrStartupState;

/* pB4DF30 is the object at 0x10B4DF30 -- the same one 0x100400E0 and
 * 0x10040330 use as the `this` of their two thiscall helpers. */
void BrUiFn1003DFC0(BrStartupState *pState, void *pB4DF30);

/* ==========================================================================
 * The menu object
 * ========================================================================== */

typedef unsigned char BrUiObj;      /* byte-addressed; see the note above */

/* Offsets the original touches on the object itself. */
#define BR_UI_OFF_VTBL      0x0000u /* const BrUiObjVtbl *                   */
#define BR_UI_OFF_ONAPPLY   0x0010u /* void (*)(BrUiObj *), may be NULL      */
#define BR_UI_OFF_FLAGS     0x001Cu /* int32 bitfield; bit 1 and bit 4 used  */
#define BR_UI_OFF_F3C       0x003Cu /* float                                 */
#define BR_UI_OFF_F40       0x0040u /* float                                 */
#define BR_UI_OFF_W2A40     0x2A40u /* int16                                 */
#define BR_UI_OFF_ITEM      0x2B5Cu /* item[0]                               */
#define BR_UI_OFF_SEL       0x3838u /* nested widget dispatched through +0x20 */
#define BR_UI_OFF_TBL3C98   0x3C98u /* void *[] , stride BR_UI_ITEM_STRIDE   */
#define BR_UI_OFF_W1E20C    0x1E20Cu/* int16 -- the "code" every BrUiCode* writes */

#define BR_UI_ITEM_STRIDE   0x0438u

/* Offsets inside one item, relative to BR_UI_OFF_ITEM + stride*i. */
#define BR_UI_ITEM_OFF_VTBL 0x0000u /* const BrUiWidgetVtbl *   (obj 0x2B5C) */
#define BR_UI_ITEM_OFF_B08  0x0008u /* uint8                    (obj 0x2B64) */
#define BR_UI_ITEM_OFF_TEXT 0x0009u /* NUL-terminated text      (obj 0x2B65) */
#define BR_UI_ITEM_OFF_W40A 0x040Au /* int16                    (obj 0x2F66) */
#define BR_UI_ITEM_OFF_F410 0x0410u /* float                    (obj 0x2F6C) */
#define BR_UI_ITEM_OFF_F414 0x0414u /* float                    (obj 0x2F70) */
#define BR_UI_ITEM_OFF_I420 0x0420u /* int32                    (obj 0x2F7C) */

/* The text buffer's true capacity is NOT established; 0x40A - 0x009 == 0x401
 * is merely the distance to the next field this range reads. Nothing here
 * bounds-checks a copy into it -- neither does the original. */
#define BR_UI_ITEM_TEXT_ROOM 0x401u

/* Enough to cover every fixed offset above. The object's real size is larger
 * and is not established here. */
#define BR_UI_OBJ_MIN_SIZE  0x1E210u

/* The object's own vtable. Only slot +0x14 is reached from this range; it is
 * a three-integer message send. */
typedef struct BrUiObjVtbl {
    void *f00, *f04, *f08, *f0C, *f10;
    void (*f14)(BrUiObj *pThis, int32_t msg, int32_t a, int32_t b);
} BrUiObjVtbl;

/* The vtable of the item at +0x2B5C AND of the nested widget at +0x3838.
 * The slots the two use are disjoint (+0x04/+0x08/+0x10/+0x14/+0x2C for the
 * item, +0x20/+0x24 for the +0x3838 widget), so the original gives NO
 * evidence that they are the same class. One C type is used for both because
 * nothing here distinguishes them -- do not read that as a claim. */
typedef struct BrUiWidgetVtbl {
    void    *f00;
    void   (*f04)(BrUiObj *pThis);                  /* +0x04 "text changed"  */
    void   (*f08)(BrUiObj *pThis);                  /* +0x08                 */
    void    *f0C;
    void   (*f10)(BrUiObj *pThis);                  /* +0x10                 */
    int32_t (*f14)(BrUiObj *pThis);                 /* +0x14, low byte tested
                                                     *  SIGNED (`test al,al`
                                                     *  + `jle`)             */
    void    *f18;
    void    *f1C;
    int32_t (*f20)(BrUiObj *pThis, int32_t v);      /* +0x20, <0 means "no"  */
    void   (*f24)(BrUiObj *pThis, int32_t v);       /* +0x24                 */
    void    *f28;
    void   (*f2C)(BrUiObj *pThis);                  /* +0x2C                 */
} BrUiWidgetVtbl;

/* Byte-offset accessors. memcpy-based: no alignment or aliasing assumption. */
uint32_t  BrUiLd32(const BrUiObj *pObj, size_t off);
void      BrUiSt32(BrUiObj *pObj, size_t off, uint32_t v);
int16_t   BrUiLd16(const BrUiObj *pObj, size_t off);
void      BrUiSt16(BrUiObj *pObj, size_t off, int16_t v);
float     BrUiLdF(const BrUiObj *pObj, size_t off);
void      BrUiStF(BrUiObj *pObj, size_t off, float v);
void     *BrUiLdPtr(const BrUiObj *pObj, size_t off);
void      BrUiStPtr(BrUiObj *pObj, size_t off, void *p);

/* &obj[BR_UI_OFF_ITEM + BR_UI_ITEM_STRIDE*i] -- the `this` the item vtable
 * is invoked on. No range check; neither has the original. */
BrUiObj   *BrUiItem(BrUiObj *pObj, int32_t i);
char      *BrUiItemText(BrUiObj *pObj, int32_t i);
const BrUiWidgetVtbl *BrUiItemVtblOf(BrUiObj *pObj, int32_t i);

/* ==========================================================================
 * The globals block
 * ========================================================================== */

typedef struct BrUiGlobals {
    /* ---- plain scalars, named for their address ---- */
    int32_t  g0AA010;    /* 0x100AA010 */
    int32_t  g220B20;    /* 0x10220B20 */
    int32_t  g0AB3D8;    /* 0x100AB3D8 -- see aAB334 below: this IS the
                          *  second dword of aAB334's 21st record. Kept as a
                          *  separate field because the original reaches it
                          *  through its own absolute address. */
    void    *g0AB3E0;    /* 0x100AB3E0 -- written by 0x1003EBE0 */
    int32_t  g0AB3F4;    /* 0x100AB3F4 */
    int32_t  g0AC648;    /* 0x100AC648 */
    int32_t  g0AC64C;    /* 0x100AC64C */
    int32_t  g0AC650;    /* 0x100AC650 */
    int32_t  g0AC654;    /* 0x100AC654 */
    int32_t  g0AC65C;    /* 0x100AC65C */
    int32_t  g680584;    /* 0x10680584 */
    int32_t  gA9D008;    /* 0x10A9D008 */
    int32_t  gA9D010;    /* 0x10A9D010 <- 0x37 by 0x1003E040 */
    int32_t  gAA2598;    /* 0x10AA2598 <- 0x102 by 0x1003E010 */
    int16_t  gAA27E0;    /* 0x10AA27E0 <- 0x0102 (word) by 0x1003E010 */
    int16_t  gAA27E2;    /* 0x10AA27E2 <- 0x0037 (word) by 0x1003E040;
                          *  adjacent to gAA27E0 in the original */
    int32_t  gAA26F0;    /* 0x10AA26F0 */
    int32_t  gAA26F4;    /* 0x10AA26F4 -- only the low byte is used */
    int32_t  gAA2840;    /* 0x10AA2840 */
    int32_t  gAA2844;    /* 0x10AA2844 */
    int32_t  gAA2850;    /* 0x10AA2850 */
    int32_t  gAA285C;    /* 0x10AA285C -- the same override flag br_state.h
                          *  documents for 0x1003E080 */
    int32_t  gAA287C;    /* 0x10AA287C */
    int32_t  gAA2880;    /* 0x10AA2880 */
    int32_t  gAA28A4;    /* 0x10AA28A4 */
    uint8_t  gAA28A8;    /* 0x10AA28A8 -- read as a byte */
    int32_t  gAA28AC;    /* 0x10AA28AC */
    int8_t   gAA28B8;    /* 0x10AA28B8 -- read movsx byte */
    int32_t  gAA28D8;    /* 0x10AA28D8 */
    int32_t  gAA28E8;    /* 0x10AA28E8 */
    int32_t  gAA2904;    /* 0x10AA2904 */
    int32_t  gAA2964;    /* 0x10AA2964 */
    int32_t  gAA2A00;    /* 0x10AA2A00 */
    int32_t  gAA2A08;    /* 0x10AA2A08 */
    int32_t  gAA2A0C;    /* 0x10AA2A0C */
    uint32_t gAA2A18;    /* 0x10AA2A18 -- compared UNSIGNED against 4 */
    int32_t  gAA2A1C;    /* 0x10AA2A1C */
    int32_t  gAA2A20;    /* 0x10AA2A20 */
    int32_t  gAA2A24;    /* 0x10AA2A24 */
    int32_t  gAA2A28;    /* 0x10AA2A28 */
    int32_t  gAA2A2C;    /* 0x10AA2A2C */
    int32_t  gAA2A30;    /* 0x10AA2A30 */
    int32_t  gAA2A34;    /* 0x10AA2A34 */
    uint32_t gB4E708;    /* 0x10B4E708 -- loop bound, compared UNSIGNED */
    uint32_t gB4E70C;    /* 0x10B4E70C -- ditto */
    int32_t  g18ABDBC;   /* 0x118ABDBC */

    /* ---- read-only tables ---- */
    const int32_t *tAC308;   /* 0x100AC308  string ids */
    const int32_t *tAC348;   /* 0x100AC348 */
    const int32_t *tAC358;   /* 0x100AC358 */
    const int32_t *tAC368;   /* 0x100AC368 */
    const int32_t *tAC3A8;   /* 0x100AC3A8 */
    const int32_t *tAC3B0;   /* 0x100AC3B0 */
    const int32_t *tAC3E0;   /* 0x100AC3E0 */
    const int32_t *tAC3F0;   /* 0x100AC3F0 */
    const int32_t *tAC400;   /* 0x100AC400 */
    const int32_t *tAC408;   /* 0x100AC408 */
    const int32_t *tAC410;   /* 0x100AC410 */
    const int32_t *tAC418;   /* 0x100AC418 */
    /* 0x100AC5A8: dword-strided, but only the LOW 16 bits are ever read
     * (`mov dx, word ptr [ecx*4 + 0x100AC5A8]`). */
    const int32_t *tAC5A8;
    /* 0x100B3820: records of 2 bytes. 0x1003FA00 reads byte 0 of record k,
     * 0x1003FE80 reads byte 1 (it addresses 0x100B3821). Indexed
     * BYTE-WISE here so the packing is explicit. */
    const uint8_t *tB3820;
    const int8_t  *tAA26E8;  /* 0x10AA26E8, signed bytes */
    const int16_t *tA9D068;  /* 0x10A9D068, signed 16-bit */
    void * const  *tBD2A8;   /* 0x100BD2A8, pointers; byte +4 of the target
                              *  is tested against 0x10 */
    /* 0x100AB334: 21 records of 8 bytes; only the first dword is read.
     * Indexed as aAB334[2*i]. */
    const uint32_t *aAB334;
    int32_t        *aA9D5C0; /* 0x10A9D5C0, 21 int32 conflict flags */

    /* ---- objects whose +0x1C flag word is the only thing touched ---- */
    BrUiObj *pAA29A8;    /* 0x10AA29A8 */
    BrUiObj *pAA29BC;    /* 0x10AA29BC */
    BrUiObj *pAA29E8;    /* 0x10AA29E8 */

    /* ---- writable text buffers ---- */
    char *szB4E2E8;      /* 0x10B4E2E8 */
    char *szA9CDF0;      /* 0x10A9CDF0 */
    char *szB4E1E4;      /* 0x10B4E1E4 */
    char *szB4E740;      /* 0x10B4E740 */
    char *szB4E760;      /* 0x10B4E760 */
    char *szA9DD28;      /* 0x10A9DD28 */
    char *szA9D018;      /* 0x10A9D018 */
    char *sz39B720;      /* 0x1039B720 */
    const char *sz0AD300;/* 0x100AD300 -- only ever read */
} BrUiGlobals;

#define BR_UI_AB334_COUNT 21

/* ==========================================================================
 * Cross-slice dependencies
 * ========================================================================== */

/* XSLICE 0x10074030 */
extern const char *BrStrGet(int32_t id);

/* XSLICE 0x1003E070 */
extern void BrFn1003E070(void);

/* XSLICE 0x1005FFD0 */
extern int32_t BrFn1005FFD0(void);

/* XSLICE 0x1003D210 */
extern void BrFn1003D210(int32_t a, int32_t b, int32_t c);

/* XSLICE 0x10069BC0  (__thiscall on the object at 0x10B4DF30) */
extern int32_t BrFn10069BC0(void *pThis, int32_t kind, uint32_t key);

/* XSLICE 0x10069C30  (__thiscall on the object at 0x10B4DF30; returns a byte) */
extern uint8_t BrFn10069C30(void *pThis, int32_t kind, uint32_t key);

/* ==========================================================================
 * 0x1003E010 / 0x1003E040 -- constant stores
 * ========================================================================== */

/* Both are entered through an 11-byte hot-patch stub (`jmp` + `nop` padding)
 * in the original; the stub carries no behaviour. */
void BrUiFn1003E010(BrUiGlobals *pG);   /* 0x102 -> gAA27E0 (word) and gAA2598 */
void BrUiFn1003E040(BrUiGlobals *pG);   /* 0x37  -> gAA27E2 (word) and gA9D010 */

/* ==========================================================================
 * 0x1003E0E0
 * ========================================================================== */

/* 1 if BrFn1005FFD0() >= 0, else 1 if BrIsAnyActive(pFlags) != 0, else 0.
 * Note the first test is `jge` -- a return of exactly 0 from 0x1005FFD0 is
 * already a yes. */
int32_t BrUiFn1003E0E0(const BrActiveFlags *pFlags);

/* ==========================================================================
 * 0x1003EE50 -- the shared "item changed" path
 * ========================================================================== */

/* Runs item[i].vtbl->f04, then:
 *   - if item[i].I420 == 0: runs f10 and returns 0;
 *   - otherwise consults f14 and the object's flag bit 1, may clear
 *     gAA28D8 / item[i].I420 / flag bit 1, calls 0x1003E070 and the object's
 *     +0x10 callback, then runs f10 and returns 1.
 *
 * GOTCHA: the index is sign-extended from 16 bits by the original
 * (`movsx eax, word ptr [esp+8]`), hence int16_t here.
 * GOTCHA: the early-out path guards its f10 call with `test esi,esi` where
 * esi is `pObj + stride*i + 0x2B65` -- an address that can never be null.
 * The guard is dead and is preserved as a comment only. */
int32_t BrUiItemApply(BrUiObj *pObj, int16_t index, BrUiGlobals *pG);

/* ==========================================================================
 * Draw / measure callbacks (object vtable slot +0x14)
 * ========================================================================== */

/* 0x1003E7A0. Emits message 0x3D once, then 0x3B `n` times stepping x by
 * 0x10, then 0x3C once.
 *
 * GOTCHA: n is `item[0].W40A / 16 + 1` with the division truncating toward
 * zero, and the loop guard is `test/jbe` -- UNSIGNED. A W40A of -16 or less
 * makes n <= 0 and the original then loops 2^32-|n| times. That is preserved
 * here; do not "fix" it by making the counter signed. */
int32_t BrUiDraw1003E7A0(BrUiObj *pObj);

/* 0x1003E980 / 0x1003E9E0. Message 0x74 once, then 0x75 once per element of
 * gB4E708 / gB4E70C respectively, stepping x by 0x0C.
 * GOTCHA: the bound is re-read from the global on every iteration. */
int32_t BrUiDraw1003E980(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiDraw1003E9E0(BrUiObj *pObj, BrUiGlobals *pG);

/* ==========================================================================
 * Text callbacks -- copy a string into item[0], notify, apply
 * ========================================================================== */

/* 0x1003E840. String id 0x51 when gAA010 and g220B20 are BOTH zero, else
 * 0x0C. GOTCHA: unlike every other member of this family it does NOT call
 * BrUiItemApply -- it stops after item[0].vtbl->f04. */
int32_t BrUiText1003E840(BrUiObj *pObj, BrUiGlobals *pG);

int32_t BrUiText1003F760(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003F7F0(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003F860(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003F8D0(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003F990(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FA00(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FC40(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FCB0(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FD30(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FDA0(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FE10(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FE80(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiText1003FFD0(BrUiObj *pObj, BrUiGlobals *pG);

/* 0x1003E8D0 / 0x1003EA90. Render a table entry as decimal into item[0]'s
 * text with the CRT's _itoa(v, buf, 10) (0x1008C000), then f08, then f2C.
 * GOTCHA: the f2C call is guarded by `test edi,edi` on the text buffer's
 * address, which can never be null -- f2C always runs. */
int32_t BrUiNum1003E8D0(BrUiObj *pObj, BrUiGlobals *pG);  /* tAA26E8[gAA28AC] */
int32_t BrUiNum1003EA90(BrUiObj *pObj, BrUiGlobals *pG);  /* tA9D068[gAA28AC] */

/* ==========================================================================
 * Geometry callbacks
 * ========================================================================== */

/* 0x1003E920. obj.F3C = (float)(11 * g0AC65C + 0x3D). */
int32_t BrUiFn1003E920(BrUiObj *pObj, BrUiGlobals *pG);

/* 0x1003EA40. obj.F3C = (float)(8 * n + 0x4A) where n is gB4E708 when
 * g0AB3D8 is non-zero and gB4E70C otherwise. */
int32_t BrUiFn1003EA40(BrUiObj *pObj, BrUiGlobals *pG);

/* ==========================================================================
 * Code callbacks -- write an int16 to obj+0x1E20C
 * ========================================================================== */

/* 0x1003E950. 0x68 when g0AB3D8 is non-zero, 0x69 when it is zero (note the
 * inversion relative to the usual "flag set -> higher value" reading), to
 * BOTH obj+0x2A40 and obj+0x1E20C. */
int32_t BrUiCode1003E950(BrUiObj *pObj, BrUiGlobals *pG);

/* 0x1003F440 / 0x1003F540. Two stages against gAA26F0 and then, only when
 * gAA26F0 == 0, against the low byte of gAA26F4.
 *
 * GOTCHA: when gAA26F0 < 0 NEITHER stage runs and obj+0x1E20C is left
 * completely untouched -- the routine still returns 1.
 * GOTCHA: 0x1003F440's second stage folds inputs 4, 5 and 6 onto the same
 * output as input 4 (0x4D). */
int32_t BrUiCode1003F440(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiCode1003F540(BrUiObj *pObj, BrUiGlobals *pG);

/* 0x1003F5E0 / 0x1003F680. Five-way switch on gAA2A18 (compared UNSIGNED).
 * GOTCHA: the two disagree about the default. 0x1003F5E0 maps out-of-range
 * to 0x56, which is also what index 0 maps to, so a bad index is
 * indistinguishable from index 0. 0x1003F680 maps BOTH index 0 and
 * out-of-range to -1 (0xFFFF). */
int32_t BrUiCode1003F5E0(BrUiObj *pObj, BrUiGlobals *pG);
int32_t BrUiCode1003F680(BrUiObj *pObj, BrUiGlobals *pG);

/* 0x1003F720.
 * GOTCHA: this is the ONE callback in the whole range that does not return 1.
 * When gAA2904 == gAA2964 and gAA28E8 == 0 it returns -2 and writes nothing.
 * Otherwise it stores the LOW 16 BITS of the dword-strided tAC5A8[g0AC654]. */
int32_t BrUiCode1003F720(BrUiObj *pObj, BrUiGlobals *pG);

/* ==========================================================================
 * Poll callbacks -- ask the widget at +0x3838 for a new value
 * ========================================================================== */

/* All of these call sel->f20(sel, <global>) and write the result back over
 * the global only when it is >= 0. */
int32_t BrUiPoll1003EAE0(BrUiObj *pObj, BrUiGlobals *pG);  /* g0AB3F4          */
int32_t BrUiPoll1003EB10(BrUiObj *pObj, BrUiGlobals *pG);  /* g0AB3F4 + f24    */
int32_t BrUiPoll1003EB60(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA28AC          */
int32_t BrUiPoll1003EB90(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2880          */
int32_t BrUiPoll1003EBC0(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2880, RESULT
                                                            *  DISCARDED       */
int32_t BrUiPoll1003EBE0(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2880 -> g0AB3E0 */
int32_t BrUiPoll1003EC30(BrUiObj *pObj, BrUiGlobals *pG);  /* byte-identical to
                                                            *  0x1003EB10      */
int32_t BrUiPoll1003EC80(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2840          */
int32_t BrUiPoll1003ED10(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2A2C          */
int32_t BrUiPoll1003EDF0(BrUiObj *pObj, BrUiGlobals *pG);  /* gAA2A30          */

/* 0x1003EE20. Same shape, but gAA2A34 is first replaced by -1 unless it is
 * in [0, 12). The clamp is applied to the value SENT, not to the value
 * stored: whatever f20 returns is stored back unfiltered as long as it is
 * >= 0. */
int32_t BrUiPoll1003EE20(BrUiObj *pObj, BrUiGlobals *pG);

/* ==========================================================================
 * Text read-back callbacks
 * ========================================================================== */

/* Each of these looks at item[0]'s text after (optionally) applying it.
 * `<obj>->f1C &= ~0x10` fires only when the text is NON-EMPTY. */
int32_t BrUiFn1003EEF0(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29A8, szB4E2E8 */
int32_t BrUiFn1003EF60(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29A8 only      */
int32_t BrUiFn1003EF90(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29E8, szA9CDF0,
                                                          *  then szB4E1E4    */
int32_t BrUiFn1003F020(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29E8 only      */
int32_t BrUiFn1003F050(BrUiObj *pObj, BrUiGlobals *pG);  /* szB4E740          */
int32_t BrUiFn1003F0B0(BrUiObj *pObj, BrUiGlobals *pG);  /* szB4E760          */
int32_t BrUiFn1003F110(BrUiObj *pObj, BrUiGlobals *pG);  /* szA9DD28          */
int32_t BrUiFn1003F210(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29BC, szA9D018 */
int32_t BrUiFn1003F280(BrUiObj *pObj, BrUiGlobals *pG);  /* pAA29BC only      */

/* 0x1003F170. Unconditionally copies item[0]'s text to szA9DD28, calls
 * 0x1003D210(g680584, gA9D008, 0), then copies sz39B720 over szA9DD28 AND
 * over item[0]'s text.
 * GOTCHA: the third argument to 0x1003D210 is a literal 0 that the compiler
 * sourced from the register left over by the preceding string scan; it is
 * not a variable. GOTCHA: this one never calls BrUiItemApply. */
int32_t BrUiFn1003F170(BrUiObj *pObj, BrUiGlobals *pG);

/* ==========================================================================
 * 0x10040040 / 0x100400E0 / 0x10040330 -- the control-binding tables
 * ========================================================================== */

/* One record of the four tables 0x10040040 searches. Stride 0x24. */
typedef struct BrCfgRec {
    uint32_t key;         /* +0x00 -- what 0x10040040 compares */
    char     szText[32];  /* +0x04 -- what 0x100400E0 copies out */
} BrCfgRec;

#define BR_CFG_T0_COUNT 120   /* 0x100B4338 .. 0x100B5418 */
#define BR_CFG_T1_COUNT 134   /* 0x10B4E910 .. 0x10B4FBE8 */
#define BR_CFG_T3_COUNT  10   /* 0x10B4E7A8 .. 0x10B4E910 */

typedef struct BrCfgTables {
    const BrCfgRec *aT0;  /* 0x100B4338, BR_CFG_T0_COUNT records */
    const BrCfgRec *aT1;  /* 0x10B4E910, BR_CFG_T1_COUNT records */
    const BrCfgRec *aT3;  /* 0x10B4E7A8, BR_CFG_T3_COUNT records */
} BrCfgTables;

/* 0x10040040. Linear search of the table selected by `type` for a record
 * whose key == `key`; returns its index, or 0 when there is no match.
 *
 * GOTCHA: types 1 and 2 select the SAME table (0x10B4E910). Only 0 and 3 are
 * distinct. type 3 is the small table, type 0 the large one.
 * GOTCHA: "not found" and "found at index 0" both return 0. Callers in this
 * range cannot tell them apart and neither can this port.
 * GOTCHA: `type` is compared UNSIGNED against 3, so a negative type is out
 * of range and yields 0. */
int32_t BrCfgLookupIndex(const BrCfgTables *pT, int32_t type, uint32_t key);

/* 0x100400E0. Chooses a source string and copies it into item[0]'s text,
 * then f04 + BrUiItemApply(pObj, 0).
 * GOTCHA: when gAA2844 == 0 and gAA2A0C is > 3, the routine skips the copy
 * ENTIRELY and still runs f04 + apply on whatever text was already there. */
int32_t BrUiText100400E0(BrUiObj *pObj, BrUiGlobals *pG,
                         const BrCfgTables *pT, void *pB4DF30);

/* 0x10040330. All-pairs scan of the 21 records at aAB334 looking for two
 * whose (BrFn10069BC0, BrFn10069C30) answers for `kind` agree and are not
 * both zero; marks aA9D5C0[i] and aA9D5C0[j] and returns 1 if any pair hit.
 *
 * GOTCHA (a real bug in the original, preserved): pass i begins by zeroing
 * aA9D5C0[i]. Since the inner loop only ever visits j > i, any flag pass i
 * sets on index j is WIPED when the outer loop reaches j. Only flags whose
 * partner is found on the final surviving pass persist.
 * GOTCHA: the inner loop is suppressed on the last outer pass (i == 20), and
 * records whose key is 0x0C, 0x0D or 0x0E are skipped as `j` only while
 * i < 12. */
int32_t BrCfgFindConflicts(BrUiGlobals *pG, int32_t kind, void *pB4DF30);

#endif /* SLICE2_23_H */
