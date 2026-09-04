/* slice3_32.c -- BRD3D.dll 0x10047930-0x1004A260, a later pass.
 *
 * See port/include/slice3_32.h for the model (three classes, the vtable
 * overlap that pins down every `this`, and the naming rationale).
 *
 * Not ported, and why -- also repeated in the report:
 *   0x100491B0 (2659 bytes)  SEH frame + a construction sequence whose object
 *                            layouts this packet does not establish.
 *   0x10049C20 (794 bytes)   ditto
 *   0x10049F40 (800 bytes)   ditto
 *   0x1004A260 (800 bytes)   ditto
 * 0x100484E0 sits inside the address range but was not in the packet listing,
 * so it is imported (BrSub100484E0) rather than guessed at.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

/* --- DUPLICATE SYMBOL, not duplicate work (host link only) ---------------
 * 0x10048470 is declared by slice3_32.h over `BrUiPage` and by slice6_73.h
 * over `BrUiPage_`, under ONE name. The two structs are not the same shape --
 * slice3_32's has the pfn0C and +0x16 fields, and until br_uivt.c existed
 * slice6_73's did not -- so binding one model's caller to the other model's
 * body wrote every field from +0x00C onward at the wrong offset and ran eight
 * bytes past the end of what BR73_ALLOC asks for.
 *
 * br_uivt.c owns the BrUiPage_ body. Under BR_HOST_LINK this module's
 * BrUiPage layout body is renamed so both exist and each caller reaches the
 * one built for its own struct. This file's internal calls are renamed with
 * it, so the module stays self-consistent; its own test binary links this .o
 * alone, without BR_HOST_LINK, and is unaffected.
 *
 * This is a stopgap, exactly like the one at the top of slice6_73.c. The real
 * fix is to merge BrUiPage and BrUiPage_ the way br_phase.h merged the three
 * views of the phase object; the two are already field-for-field identical,
 * so the merge is mechanical and only the naming differs.
 * ------------------------------------------------------------------------ */
#ifdef BR_HOST_LINK
#define BrUiPageCtor_10048470 BrUiPageCtor_10048470_scr32
#endif

#ifdef BR_MATCHING_BUILD
/* slice3_32.h adds a leading BrScrGlobals* that the original does not have.
 * Hide that prototype so the matching body can be the real __stdcall(code,x,y). */
#define BrUiDrawIndex_100479D0 BrUiDrawIndex_100479D0_port
/* Original is thiscall + one stack arg (`ret 4`); the port is cdecl. */
#define BrUiTweenCurve_10047CE0 BrUiTweenCurve_10047CE0_port
#define BrUiTweenBegin_10047CB0 BrUiTweenBegin_10047CB0_port
#define BrUiTweenReset_10047D10 BrUiTweenReset_10047D10_port
#define BrUiDrawCode_10047930   BrUiDrawCode_10047930_port
#define BrUiDrawCodeRect_10047980 BrUiDrawCodeRect_10047980_port
#define BrUiStepCode_10047A10   BrUiStepCode_10047A10_port
#define BrUiTweenStep_10047D30  BrUiTweenStep_10047D30_port
#define BrUiInit_10047FB0       BrUiInit_10047FB0_port
#define BrUiItemInit_10047EB0   BrUiItemInit_10047EB0_port
#endif
#include "slice3_32.h"
#ifdef BR_MATCHING_BUILD
#undef BrUiDrawIndex_100479D0
#undef BrUiTweenCurve_10047CE0
#undef BrUiTweenBegin_10047CB0
#undef BrUiTweenReset_10047D10
#undef BrUiDrawCode_10047930
#undef BrUiDrawCodeRect_10047980
#undef BrUiStepCode_10047A10
#undef BrUiTweenStep_10047D30
#undef BrUiInit_10047FB0
#undef BrUiItemInit_10047EB0
#endif

/* ==========================================================================
 * 0. Byte-offset accessors -- memcpy only, no alignment or aliasing
 *    assumption. Same technique as slice2_23.h's BrUiLd32 family.
 * ========================================================================== */

uint32_t BrScrLd32(const void *pObj, size_t off)
{
    uint32_t v;
    memcpy(&v, (const unsigned char *)pObj + off, sizeof v);
    return v;
}

void BrScrSt32(void *pObj, size_t off, uint32_t v)
{
    memcpy((unsigned char *)pObj + off, &v, sizeof v);
}

int16_t BrScrLd16(const void *pObj, size_t off)
{
    uint16_t v;
    memcpy(&v, (const unsigned char *)pObj + off, sizeof v);
    /* Explicit two's-complement widening, not an implementation-defined
     * narrowing conversion. */
    return (int16_t)((v & 0x8000u) ? -(int32_t)(uint32_t)(0x10000u - v)
                                   :  (int32_t)(uint32_t)v);
}

uint16_t BrScrLd16u(const void *pObj, size_t off)
{
    uint16_t v;
    memcpy(&v, (const unsigned char *)pObj + off, sizeof v);
    return v;
}

void BrScrSt16(void *pObj, size_t off, uint16_t v)
{
    memcpy((unsigned char *)pObj + off, &v, sizeof v);
}

uint8_t BrScrLd8(const void *pObj, size_t off)
{
    return ((const unsigned char *)pObj)[off];
}

void BrScrSt8(void *pObj, size_t off, uint8_t v)
{
    ((unsigned char *)pObj)[off] = v;
}

float BrScrLdF(const void *pObj, size_t off)
{
    float f;
    memcpy(&f, (const unsigned char *)pObj + off, sizeof f);
    return f;
}

void BrScrStF(void *pObj, size_t off, float f)
{
    memcpy((unsigned char *)pObj + off, &f, sizeof f);
}

void *BrScrLdSlot(const void *pObj, int k)
{
    void *p;
    memcpy(&p, (const unsigned char *)pObj + BR_SCR_UIOBJ_SIZE
                   + (size_t)k * sizeof(void *), sizeof p);
    return p;
}

void BrScrStSlot(void *pObj, int k, void *p)
{
    memcpy((unsigned char *)pObj + BR_SCR_UIOBJ_SIZE
               + (size_t)k * sizeof(void *), &p, sizeof p);
}

void *BrScrLdPtr(const void *pObj, size_t off)
{
    void *p;
    memcpy(&p, (const unsigned char *)pObj + off, sizeof p);
    return p;
}

void BrScrStPtr(void *pObj, size_t off, void *p)
{
    memcpy((unsigned char *)pObj + off, &p, sizeof p);
}

/* Local shorthands. */
static const BrScrUiVtbl *BrScrUiV(const BrUiObj *pObj)
{
    return (const BrScrUiVtbl *)BrScrLdSlot(pObj, BR_SCR_SLOT_VTBL);
}

static const BrScrItemVtbl *BrScrItemV(const BrUiObj *pObj)
{
    return (const BrScrItemVtbl *)BrScrLdSlot(pObj, BR_SCR_SLOT_ITEMVTBL);
}

/* The `this` the item vtable is invoked on is the item itself, i.e. the
 * object's address plus 0x2B5C. */
static void *BrScrItemThis(BrUiObj *pObj)
{
    return (void *)((unsigned char *)pObj + BR_SCR_UI_ITEMVTBL);
}

/* ==========================================================================
 * 1. 0x10047930 / 0x10047980 / 0x100479D0 -- feeding 0x1005F5A0
 * ========================================================================== */

/* WHAT IT DOES: draws the picture a menu row is currently showing, at the
 * row's own position, taking the picture's shape and draw flags from the
 * shared picture table. A row whose picture number is negative draws
 * nothing. */
/* port-only body; Glide match is src/core/cpp/0x10040D80.cpp */
int BrUiDrawCode_10047930(const BrScrGlobals *pG, BrUiObj *pObj)
{
    int16_t wCode = BrScrLd16(pObj, BR_UI_OFF_W1E20C);

    if (wCode >= 0) {
        const BrScrRectEnt *pEnt = &pG->aAB568[wCode];
        int32_t x, y;
        uint32_t nId;

        /* GOTCHA, reproduced exactly. The original computes the byte offset
         * `code * 0x18` in EAX, then overwrites only AX with the entry's id
         * word and pushes the WHOLE of EAX. The high half of the argument is
         * therefore the high half of code*0x18 -- zero only while
         * code*0x18 < 0x10000, i.e. code <= 0xAAA. */
        nId = (((uint32_t)(int32_t)wCode * 0x18u) & 0xFFFF0000u)
            | (uint32_t)(uint16_t)(uint32_t)pEnt->f00;

        /* fld [+0x40] happens first, then fld [+0x3C]; __ftol is pure, so
         * the C evaluation order does not matter. */
        y = BrFtolTrunc(BrScrLdF(pObj, BR_UI_OFF_F40));
        x = BrFtolTrunc(BrScrLdF(pObj, BR_UI_OFF_F3C));
        BrSub1005F5A0(x, y, (int32_t)nId, &pEnt->rc, pEnt->f14);
    }
    return 1;
}

/* WHAT IT DOES: the same as its neighbour above, but draws the picture into a
 * rectangle the caller supplies instead of the table's own. It does NOT check
 * for a negative picture number first, so where the other one would draw
 * nothing this one reads outside the table. */
/* port-only body; Glide match is src/core/cpp/0x10040DD0.cpp */
int BrUiDrawCodeRect_10047980(const BrScrGlobals *pG, BrUiObj *pObj,
                              const void *pRect)
{
    /* Unlike 0x10047930 this does NOT check the sign of the code first, so a
     * negative code indexes the table out of bounds. Preserved. */
    int16_t wCode = BrScrLd16(pObj, BR_UI_OFF_W1E20C);
    const BrScrRectEnt *pEnt = &pG->aAB568[wCode];
    int32_t y = BrFtolTrunc(BrScrLdF(pObj, BR_UI_OFF_F40));
    int32_t x = BrFtolTrunc(BrScrLdF(pObj, BR_UI_OFF_F3C));

    /* DEVIATION: the original pushes EAX after `mov ax, word[obj+0x1E20C]`,
     * and on this path nothing has written EAX beforehand -- its high half is
     * whatever the caller left there, which C cannot reproduce. The port
     * zero-extends the code word. */
    BrSub1005F5A0(x, y, (int32_t)(uint32_t)(uint16_t)wCode, pRect, pEnt->f14);
    return 1;
}

/* WHAT IT DOES: draws a numbered picture at an outright screen position,
 * without any menu row being involved. It passes on the caller's number as the
 * picture's identity rather than the one filed in the table -- in the shipped
 * table those always agree. */
/* @implements 0x100479D0 d3d BrUiDrawIndex_100479D0 */
#ifdef BR_MATCHING_BUILD
/* Original is __stdcall(code, x, y) and indexes the table at 0x100AB568 by
 * absolute address (`lea eax,[ecx+ecx*2]/shl eax,3` then
 * `[eax+0x100AB57C]` / `lea eax,[eax+0x100AB56C]`).  The port's extra
 * BrScrGlobals* is a pointer indirection the original does not have. */
extern unsigned char g_aBrUiSprite[];
int BR_STDCALL BrUiDrawIndex_100479D0(int32_t code, int32_t x, int32_t y)
{
    /* Spell *24 as *3 then <<3 so VC5 emits `lea`/`shl` rather than
     * a scaled-index `[r*8+table]` operand. */
    uint32_t off = (uint32_t)code * 3u;
    off <<= 3;
    BrSub1005F5A0(x, y, code,
                  g_aBrUiSprite + off + 4u,
                  *(int32_t *)(g_aBrUiSprite + off + 0x14u));
    return 1;
}
#else
int BrUiDrawIndex_100479D0(const BrScrGlobals *pG, int32_t code,
                           int32_t x, int32_t y)
{
    const BrScrRectEnt *pEnt = &pG->aAB568[code];

    /* GOTCHA: the id handed on is the CALLER'S index, not pEnt->f00. In the
     * shipped table the two are equal for every entry. */
    BrSub1005F5A0(x, y, code, &pEnt->rc, pEnt->f14);
    return 1;
}
#endif

/* WHAT IT DOES: advances an animated menu element to its next frame -- swaps
 * in that frame's picture and hands the element the frame's data. An element
 * that is not animated is simply redrawn as it stands. */
/* port-only body; Glide match is src/core/cpp/0x10040E60.cpp */
#ifdef BR_MATCHING_BUILD
typedef struct BrUiMVtbl {
    void *f00, *f04, *f08, *f0C, *f10, *f14;
    void (__fastcall *f18)(void *pThis, void *edx, void *p);
    void (BR_THISCALL1 *f1C)(void *pThis);
} BrUiMVtbl;
typedef struct BrUiMStep {
    struct BrUiMVtbl *pVtbl;
    unsigned char pad04[0x128 - 4];
    short w128;
    unsigned char pad12a[0x296c - 0x12a];
    int f296c;
    unsigned char pad2970[0x2a40 - 0x2970];
    short w2a40[1];
    unsigned char pad2a42[0x1e20c - 0x2a42];
    short w1e20c;
    unsigned char pad1e20e[2];
    unsigned char *p1e210;
} BrUiMStep;

int BR_THISCALL1 BrUiStepCode_10047A10(BrUiMStep *p)
{
    int i;
    if (p->f296c == 0) {
        p->pVtbl->f1C(p);
        return 1;
    }
    i = p->w128;
    p->w1e20c = p->w2a40[i];
    p->pVtbl->f18(p, p->pVtbl, p->p1e210 + (i << 4));
    return 1;
}
#else
int BrUiStepCode_10047A10(BrUiObj *pObj)
{
    if (BrScrLd32(pObj, BR_SCR_UI_F296C) == 0) {
        BrScrUiV(pObj)->f1C(pObj);
        return 1;
    }
    {
        int32_t i = BrScrLd16(pObj, BR_SCR_UI_W128);
        uint16_t w = BrScrLd16u(pObj,
                        BR_UI_OFF_W2A40 + (size_t)((uint32_t)i * 2u));
        unsigned char *pRec =
            (unsigned char *)BrScrLdSlot(pObj, BR_SCR_SLOT_P1E210);

        BrScrSt16(pObj, BR_UI_OFF_W1E20C, w);
        BrScrUiV(pObj)->f18(pObj, pRec + (size_t)((uint32_t)i * 16u));
    }
    return 1;
}
#endif

/* ==========================================================================
 * 2. 0x10047CB0 .. 0x10047D30 -- the two-axis tween
 * ========================================================================== */

/* WHAT IT DOES: starts a menu element sliding to a new place. It remembers
 * where the element is now and works out how far it must travel per step to
 * arrive in the number of steps asked for. */
/* port-only body; Glide match is src/core/cpp/0x10041100.cpp */
#ifdef BR_MATCHING_BUILD
typedef struct BrUiTwBegin {
    unsigned char pad00[0x30];
    float f30, f34, f38, f3c, f40, f44;
    unsigned char pad48[0x381c - 0x48];
    float twlo;
    float twhi;
    float twrate;
} BrUiTwBegin;

int __fastcall BrUiTweenBegin_10047CB0(BrUiTwBegin *p, int _edx, int n)
{
    float r;
    r = (p->twhi - p->twlo) / n;
    p->f30 = p->f3c;
    p->f38 = p->f44;
    p->f34 = p->f40;
    p->twrate = r;
    return 1;
}
#else
int BrUiTweenBegin_10047CB0(BrUiObj *pObj, int32_t n)
{
    /* fld [+0x3820]; fsub [+0x381C]; fidiv [n]. `n` is an INTEGER divisor;
     * n == 0 gives the masked x87 result, +/-inf, which IEEE division in C
     * reproduces. */
    double d = ((double)BrScrLdF(pObj, BR_SCR_UI_TWHI)
              - (double)BrScrLdF(pObj, BR_SCR_UI_TWLO)) / (double)n;

    BrScrSt32(pObj, BR_SCR_UI_F30, BrScrLd32(pObj, BR_UI_OFF_F3C));
    BrScrSt32(pObj, BR_SCR_UI_F38, BrScrLd32(pObj, BR_SCR_UI_F44));
    BrScrSt32(pObj, BR_SCR_UI_F34, BrScrLd32(pObj, BR_UI_OFF_F40));
    BrScrStF(pObj, BR_SCR_UI_TWRATE, (float)d);
    return 1;
}
#endif

/* WHAT IT DOES: says how far along its slide an element should be after a
 * given number of milliseconds. The distance grows with the square of the time,
 * so the element starts slowly and speeds up rather than moving evenly. */
/* @implements 0x10047CE0 d3d BrUiTweenCurve_10047CE0 */
#ifdef BR_MATCHING_BUILD
/* thiscall + one stack arg (`ret 4`). Sequential `x *= 0.5f; x *= 1e-3f`
 * keeps two `fmul dword [const]` -- a single expression folds them. */
typedef struct BrUiTwCurve {
    unsigned char pad[0x3824];
    float twrate;
} BrUiTwCurve;

float __fastcall BrUiTweenCurve_10047CE0(BrUiTwCurve *p, int _edx, int n)
{
    float x;
    n = n * n;
    x = (float)n;
    x *= p->twrate;
    x *= 0.5f;
    x *= 1.0e-3f;
    return x;
}
#else
float BrUiTweenCurve_10047CE0(const BrUiObj *pObj, int32_t n)
{
    /* imul n, n -- a 32-bit signed multiply that WRAPS; done in unsigned so
     * the port has no signed-overflow UB. */
    uint32_t uSq = (uint32_t)n * (uint32_t)n;
    int32_t  nSq = (uSq & 0x80000000u) ? -(int32_t)(0u - uSq) : (int32_t)uSq;

    /* 0x1008F678 = 0.5f and 0x1008F67C = 0.001f, read out of .rdata. */
    double d = (double)nSq;
    d *= (double)BrScrLdF(pObj, BR_SCR_UI_TWRATE);
    d *= (double)0.5f;
    d *= (double)1.0e-3f;

    /* The original leaves the product in st(0) at the x87's working
     * precision, which is 53-bit (CRT control word 0x027F -- CONVENTIONS.md)
     * and not the 80-bit this note used to claim. The `double` chain above is
     * therefore an exact model of it. The one remaining departure is the
     * narrowing to float on return, which is real: the caller sees st(0)
     * unrounded. */
    return (float)d;
}
#endif

/* WHAT IT DOES: snaps a sliding element back to where it started and sets it
 * moving again from there. */
/* port-only body; Glide match is src/core/cpp/0x10041160.cpp */
#ifdef BR_MATCHING_BUILD
typedef struct BrUiTwReset {
    unsigned char pad00[0x30];
    float f30, f34, f38, f3c, f40, f44;
    unsigned char pad48[0x3818 - 0x48];
    int twactive;
} BrUiTwReset;

/* @n64 0x8022445C located */
int BR_THISCALL1 BrUiTweenReset_10047D10(BrUiTwReset *p)
{
    p->f3c = p->f30;
    p->f40 = p->f34;
    p->twactive = 1;
    p->f44 = p->f38;
    return 1;
}
#else
int BrUiTweenReset_10047D10(BrUiObj *pObj)
{
    BrScrSt32(pObj, BR_UI_OFF_F3C, BrScrLd32(pObj, BR_SCR_UI_F30));
    BrScrSt32(pObj, BR_UI_OFF_F40, BrScrLd32(pObj, BR_SCR_UI_F34));
    BrScrSt32(pObj, BR_SCR_UI_TWACTIVE, 1u);
    BrScrSt32(pObj, BR_SCR_UI_F44, BrScrLd32(pObj, BR_SCR_UI_F38));
    return 1;
}
#endif

/* One axis of 0x10047D30. Returns 1 when the axis counts as FINISHED. */
static int BrScrTweenAxis(BrUiObj *pObj, size_t offOn, size_t offDir,
                          size_t offOrigin, size_t offCur, size_t offEnd)
{
    uint8_t bDir;
    float   fEnd, fCur;

    if (BrScrLd32(pObj, offOn) == 0)
        return 1;                       /* an axis that is off is "done"     */

    bDir = BrScrLd8(pObj, offDir);
    if (bDir == 0xFFu) {
        /* fsubr: st(0) = [+origin] - curve */
        fCur = BrScrLdF(pObj, offOrigin)
             - BrScrUiV(pObj)->f28(pObj,
                   (int32_t)BrScrLd32(pObj, BR_SCR_UI_TWMS));
        BrScrStF(pObj, offCur, fCur);
        fEnd = BrScrLdF(pObj, offEnd);
        /* `test ah,0x41` -> C0|C3, i.e. clamp unless strictly greater.
         * An unordered compare sets both, so NaN clamps here. */
        if (!(fCur > fEnd)) {
            BrScrStF(pObj, offCur, fEnd);
            return 1;
        }
        return 0;
    }
    if (bDir == 0)
        return 1;
    if (bDir == 1u) {
        fCur = BrScrUiV(pObj)->f28(pObj,
                   (int32_t)BrScrLd32(pObj, BR_SCR_UI_TWMS))
             + BrScrLdF(pObj, offOrigin);
        BrScrStF(pObj, offCur, fCur);
        fEnd = BrScrLdF(pObj, offEnd);
        /* `test ah,1` -> C0 alone: clamp only when the compare was ORDERED
         * and st(0) >= limit. NaN does NOT clamp here -- the asymmetry with
         * the 0xFF arm above is in the original. */
        if (fCur >= fEnd) {
            BrScrStF(pObj, offCur, fEnd);
            return 1;
        }
        return 0;
    }
    /* Any other direction byte: never finishes. */
    return 0;
}

/* WHAT IT DOES: moves a sliding element on by however much real time has
 * passed since the last frame, on both axes independently, and stops the slide
 * once both have reached their destinations. An axis that was never set moving
 * counts as already arrived, and an axis that overshoots is pinned to its
 * target. */
/* port-only body; Glide match is src/core/cpp/0x10041180.cpp */
int BrUiTweenStep_10047D30(BrUiObj *pObj)
{
    int32_t nNow, nDelta;
    int fX, fY;

    if (BrScrLd32(pObj, BR_SCR_UI_TWACTIVE) == 0)
        return 1;

    nNow = BrSub10075020();
    if ((int32_t)BrScrLd32(pObj, BR_SCR_UI_TWTICK) <= 0)
        BrScrSt32(pObj, BR_SCR_UI_TWTICK, (uint32_t)nNow);
    nDelta = (int32_t)((uint32_t)nNow
                     - BrScrLd32(pObj, BR_SCR_UI_TWTICK));
    BrScrSt32(pObj, BR_SCR_UI_TWTICK, (uint32_t)nNow);
    BrScrSt32(pObj, BR_SCR_UI_TWMS,
              BrScrLd32(pObj, BR_SCR_UI_TWMS) + (uint32_t)nDelta);

    fX = BrScrTweenAxis(pObj, BR_SCR_UI_TWXON, BR_SCR_UI_TWXDIR,
                        BR_SCR_UI_F30, BR_UI_OFF_F3C, BR_SCR_UI_TWXEND);
    fY = BrScrTweenAxis(pObj, BR_SCR_UI_TWYON, BR_SCR_UI_TWYDIR,
                        BR_SCR_UI_F34, BR_UI_OFF_F40, BR_SCR_UI_TWYEND);

    if (fX && fY) {
        BrScrSt32(pObj, BR_SCR_UI_TWMS, 0u);
        BrScrSt32(pObj, BR_SCR_UI_TWACTIVE, 0u);
    }
    return 1;
}

/* ==========================================================================
 * 3. 0x10047EB0 / 0x10047FB0 -- setting the object up
 * ========================================================================== */

/* WHAT IT DOES: gives a menu row its wording and the settings that go with it,
 * asks the text to lay itself out, and then records the height and width that
 * came back so the page can position everything around it. Every measurement it
 * saves is re-read after the layout call, because laying out can move the row.
 * There is no limit on the text it copies in. */
/* port-only body; Glide match is src/core/cpp/0x10041300.cpp */
void BrUiItemInit_10047EB0(BrUiObj *pObj, const char *psz, uint32_t nFlags,
                           uint8_t bKind, const int32_t *pSrc)
{
    const BrScrItemVtbl *pV = BrScrItemV(pObj);
    int32_t  nY;
    uint16_t w40A, w40C;

    /* `repne scasb` + `rep movs`: strlen(psz)+1 bytes, unbounded. The
     * original does not check the item's text room either. */
    memcpy((unsigned char *)pObj + BR_SCR_UI_ITEMTEXT, psz, strlen(psz) + 1u);

    BrScrSt32(pObj, BR_SCR_UI_ITEMFLAGS,
              BrScrLd32(pObj, BR_SCR_UI_ITEMFLAGS) | nFlags);
    BrScrSt8(pObj, BR_SCR_UI_ITEMKIND, bKind);
    BrScrSt16(pObj, BR_SCR_UI_ITEMW41C, 0u);
    BrScrSt16(pObj, BR_SCR_UI_ITEMW40C, 0u);
    BrScrSt16(pObj, BR_SCR_UI_ITEMW40A, 0u);
    BrScrSt32(pObj, BR_SCR_UI_ITEMF424, (uint32_t)pSrc[0]);
    BrScrSt32(pObj, BR_SCR_UI_ITEMF42C, (uint32_t)pSrc[2]);
    BrScrSt32(pObj, BR_SCR_UI_ITEMF410, BrScrLd32(pObj, BR_UI_OFF_F3C));
    BrScrSt32(pObj, BR_SCR_UI_ITEMF414, BrScrLd32(pObj, BR_UI_OFF_F40));
    BrScrSt32(pObj, BR_SCR_UI_ITEMF418, 0u);
    BrScrSt32(pObj, BR_SCR_UI_ITEMF420, 0u);

    if (bKind == 3u)
        pV->f08(BrScrItemThis(pObj));
    else
        pV->f04(BrScrItemThis(pObj));

    /* GOTCHA: called for its side effects only -- `fstp st(0)` drops the
     * float the original gets back. */
    if (nFlags & 1u)
        (void)pV->f28(BrScrItemThis(pObj));

    /* Everything below RE-READS state the hooks above may have changed. */
    nY = BrFtolTrunc(BrScrLdF(pObj, BR_UI_OFF_F40));
    BrScrSt32(pObj, BR_SCR_UI_F54, (uint32_t)nY);
    BrScrSt32(pObj, BR_SCR_UI_F50, (uint32_t)pSrc[0]);
    BrScrSt32(pObj, BR_SCR_UI_F58, (uint32_t)pSrc[2]);
    w40C = BrScrLd16u(pObj, BR_SCR_UI_ITEMW40C);
    /* `movsx edx, cx` -- the word is SIGN-extended before the add. */
    BrScrSt32(pObj, BR_SCR_UI_F5C,
              (uint32_t)nY + (uint32_t)BrScrLd16(pObj, BR_SCR_UI_ITEMW40C));
    w40A = BrScrLd16u(pObj, BR_SCR_UI_ITEMW40A);
    BrScrSt16(pObj, BR_SCR_UI_W48, w40A);
    BrScrSt16(pObj, BR_SCR_UI_W4A, w40C);
}

/* WHAT IT DOES: places a menu element: tells it which screen owns it, where it
 * sits, three separate sets of behaviour flags, and which picture it starts
 * with -- that last one being written into two fields at once, the live picture
 * and the one to fall back to. */
/* @implements 0x10047FB0 d3d BrUiInit_10047FB0 */
#ifdef BR_MATCHING_BUILD
/* Orig is thiscall / ret 0x20. BR_THISCALL1 is 1-arg only; a dummy edx
 * slot keeps pPhase on the stack (no xor edx,edx — the param is unused). */
void __fastcall BrUiInit_10047FB0(BrUiObj *pObj, void *_edx,
                                   BrPhaseFull *pPhase, float f3C, float f40,
                                   uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                                   uint32_t n2968, int16_t wCode)
{
    unsigned char *p = (unsigned char *)pObj;

    (void)_edx;
    *(BrPhaseFull **)(void *)(p + 0x2AE8) = pPhase;
    *(uint32_t *)(void *)(p + 0x1C) |= nOr1C;
    *(uint32_t *)(void *)(p + 0x24) |= nOr24;
    *(uint32_t *)(void *)(p + 0x28) |= nOr28;
    *(uint32_t *)(void *)(p + 0x2968) = n2968;
    /* Mention f3C first so it hoists into edx; the wCode load clobbers
     * eax, so f40 (eax) stores first and f3C (edx) after — orig order. */
    *(float *)(void *)(p + 0x3C) = f3C;
    *(float *)(void *)(p + 0x40) = f40;
    *(uint16_t *)(void *)(p + 0x2A40) = (uint16_t)wCode;
    *(uint16_t *)(void *)(p + 0x1E20C) = (uint16_t)wCode;
}
#else
void BrUiInit_10047FB0(BrUiObj *pObj, BrPhaseFull *pPhase,
                       float f3C, float f40,
                       uint32_t nOr1C, uint32_t nOr24, uint32_t nOr28,
                       uint32_t n2968, int16_t wCode)
{
    BrScrStSlot(pObj, BR_SCR_SLOT_PHASE, pPhase);
    BrScrSt32(pObj, BR_UI_OFF_FLAGS, BrScrLd32(pObj, BR_UI_OFF_FLAGS) | nOr1C);
    BrScrSt32(pObj, BR_SCR_UI_FLAGS24,
              BrScrLd32(pObj, BR_SCR_UI_FLAGS24) | nOr24);
    BrScrSt32(pObj, BR_SCR_UI_FLAGS28,
              BrScrLd32(pObj, BR_SCR_UI_FLAGS28) | nOr28);
    BrScrSt32(pObj, BR_SCR_UI_F2968, n2968);
    BrScrStF(pObj, BR_UI_OFF_F40, f40);
    BrScrStF(pObj, BR_UI_OFF_F3C, f3C);
    /* GOTCHA: one word, two destinations. */
    BrScrSt16(pObj, BR_UI_OFF_W2A40, (uint16_t)wCode);
    BrScrSt16(pObj, BR_UI_OFF_W1E20C, (uint16_t)wCode);
}
#endif

/* ==========================================================================
 * 4. 0x10048010 / 0x10048060 / 0x100480A0 / 0x10048180
 * ========================================================================== */

/* WHAT IT DOES: the "the player chose this row" step. Depending on the row's
 * flags it either passes the choice to the row's text object -- which is how a
 * typing field takes the keystroke -- ignores it entirely, or asks the row's own
 * handler and reports whether that handler was happy. */
/* port-only body; Glide match is src/core/cpp/0x10041460.cpp */
int BrUiEnter_10048010(BrUiObj *pObj)
{
    uint32_t f;

    if ((BrScrLd8(pObj, BR_SCR_UI_FLAGS28) & 1u) == 0)
        return 1;

    f = BrScrLd32(pObj, BR_UI_OFF_FLAGS);
    if (f & BR_SCR_F1C_100000) {
        /* The original tests `&this->itemText != NULL` here -- the address of
         * a member, so the branch is dead. Kept as this comment only. */
        BrScrItemV(pObj)->f10(BrScrItemThis(pObj));
        return 1;
    }
    if (f & BR_SCR_F1C_200000)
        return 1;

    if (BrScrUiV(pObj)->f10(pObj) != 0)
        return 1;
    return 0;
}

/* WHAT IT DOES: asks whether some OTHER menu row currently has the player's
 * exclusive attention -- a name being typed in, for instance -- so the rest of
 * the frame knows to keep out of the way. If the row asking is itself that row,
 * the answer is no and the shared flag is deliberately left as it was. */
/* port-only body; Glide match is src/core/cpp/0x100414B0.cpp */
int BrUiCheckOther_10048060(BrScrGlobals *pG, const BrUiObj *pObj)
{
    BrUiObj *pOther = pG->pAA29C0;
    BrPhaseFull *pPhase;

    if (pOther == NULL) {
        pG->nAA2858 = 0;
        return 0;
    }
    pPhase = (BrPhaseFull *)BrScrLdSlot(pOther, BR_SCR_SLOT_PHASE);
    if (pPhase->aFlags[1] != 1) {
        pG->nAA2858 = 0;
        return 0;
    }
    if (pObj == pOther)
        return 0;              /* nAA2858 left ALONE on this path */
    pG->nAA2858 = 1;
    return 1;
}

/* WHAT IT DOES: runs a menu element's animation clock. An element with its own
 * table of frame durations advances when the current frame's time is up and
 * loops back to the start when it runs off the end; everything else simply
 * ticks over every sixty milliseconds. Either way it raises the flag that tells
 * the drawing code to move to the next picture, which is what makes the
 * highlighted row pulse. A frame given a duration of zero never elapses. */
/* port-only body; Glide match is src/core/cpp/0x100414F0.cpp */
int BrUiTickSteps_100480A0(BrUiObj *pObj)
{
    int32_t nNow, nDelta;

    if (BrScrLd32(pObj, BR_SCR_UI_F2968) == 0)
        return 1;

    nNow   = BrSub10075020();
    nDelta = (int32_t)((uint32_t)nNow - BrScrLd32(pObj, BR_SCR_UI_F2970));
    BrScrSt32(pObj, BR_SCR_UI_F2970, (uint32_t)nNow);
    BrScrSt32(pObj, BR_SCR_UI_F2974,
              BrScrLd32(pObj, BR_SCR_UI_F2974) + (uint32_t)nDelta);

    if (BrScrLd32(pObj, BR_SCR_UI_F296C) != 0) {
        int32_t i = BrScrLd16(pObj, BR_SCR_UI_W128);

        /* `jle` -- a step of length 0 can never elapse. */
        if ((int32_t)BrScrLd32(pObj, BR_SCR_UI_F2974)
            <= (int32_t)BrScrLd32(pObj,
                    BR_SCR_UI_A2978 + (size_t)((uint32_t)i * 4u)))
            return 1;

        BrScrSt32(pObj, BR_SCR_UI_F2974, 0u);
        BrScrSt32(pObj, BR_UI_OFF_FLAGS,
                  BrScrLd32(pObj, BR_UI_OFF_FLAGS) | BR_SCR_BIT100);
        BrScrSt32(pObj, BR_SCR_UI_F3850,
                  BrScrLd32(pObj, BR_SCR_UI_F3850) | BR_SCR_BIT100);
        ++i;
        BrScrSt16(pObj, BR_SCR_UI_W128, (uint16_t)(uint32_t)i);
        i = (int32_t)BrScrLd16(pObj, BR_SCR_UI_W128);   /* movsx of the store */
        if ((int32_t)BrScrLd32(pObj,
                BR_SCR_UI_A2978 + (size_t)((uint32_t)i * 4u)) > 0)
            return 1;
        BrScrSt16(pObj, BR_SCR_UI_W128, 0u);
        return 1;
    }

    /* `jle 0x3C` -- STRICTLY more than 60 ms. */
    if ((int32_t)BrScrLd32(pObj, BR_SCR_UI_F2974) > 0x3C) {
        BrScrSt32(pObj, BR_SCR_UI_F2974, 0u);
        BrScrSt32(pObj, BR_UI_OFF_FLAGS,
                  BrScrLd32(pObj, BR_UI_OFF_FLAGS) | BR_SCR_BIT100);
        BrScrSt32(pObj, BR_SCR_UI_F3850,
                  BrScrLd32(pObj, BR_SCR_UI_F3850) | BR_SCR_BIT100);
    }
    return 1;
}

/* The child lookup 0x10048180 performs six times over: the object's phase,
 * that phase's current page, then the page item named by the int16 at
 * obj+0x2AB6+2*i. */
static BrUiObj *BrScrChild(BrUiObj *pObj, int32_t i)
{
    BrPhaseFull *pPhase = (BrPhaseFull *)BrScrLdSlot(pObj, BR_SCR_SLOT_PHASE);
    int32_t k = BrScrLd16(pObj, BR_SCR_UI_A2AB6 + (size_t)((uint32_t)i * 2u));
    return pPhase->pCur->aItems[k];
}

/* WHAT IT DOES: one frame of one menu element, and the heart of how the menus
 * behave. It skips an element that is switched off, runs any slide in progress,
 * lets the element's own per-frame hook veto or take over the rest, and then
 * asks whether the player is acting on it. If so it runs the element's action
 * -- playing the appropriate click sound first, unless the action is one of two
 * particular ones -- and afterwards ticks any child elements the row owns. If
 * the player is not on it, the element falls back to its resting picture. */
/* port-only body; Glide match is src/core/cpp/0x100415D0.cpp */
int BrUiFrame_10048180(BrScrGlobals *pG, BrUiObj *pObj)
{
    const BrScrUiVtbl *pV;
    uint16_t wSaved = BrScrLd16u(pObj, BR_SCR_UI_W128);
    uint32_t f;
    BrScrUiHookFn pfn;

    if (BrScrLd8(pObj, BR_UI_OFF_FLAGS) & 0x10u) {
        BrScrUiV(pObj)->f08(pObj);
        return 1;
    }
    pV = BrScrUiV(pObj);
    if (pV->f3C(pObj) != 0) {
        pV->f08(pObj);
        return 1;
    }
    if (BrScrLd32(pObj, BR_SCR_UI_TWACTIVE) != 0)
        pV->f30(pObj);
    pV->f04(pObj);

    pfn = (BrScrUiHookFn)BrScrLdSlot(pObj, BR_SCR_SLOT_PFN04);
    if (pfn != NULL) {
        int32_t r = pfn(pObj);
        if (r == -2) {
            /* GOTCHA: -2 skips the whole body but still reports success --
             * and, unlike every other exit, does NOT call vtable +0x08. */
            return 1;
        }
        if (r == -1)
            return 0;
    }

    if (pV->f20(pObj) != 0 && pG->nAA28D8 == 0) {
        f = BrScrLd32(pObj, BR_UI_OFF_FLAGS);

        if (f & BR_SCR_F1C_400000) {
            const BrObjAA2E80 *p = pG->pAA2E80;
            if (p->f2C != 0 || p->f30 != 0)
                BrScrSt16(pObj, BR_UI_OFF_W1E20C,
                          BrScrLd16u(pObj, BR_SCR_UI_W2A42));
        }

        if (f & BR_SCR_F1C_0002) {
            void *p8 = BrScrLdSlot(pObj, BR_SCR_SLOT_PFN08);
            if (p8 != NULL) {
                int32_t r;
                if (p8 == pG->pfn10043760) {
                    BrSub10072AF0(2, 0x200020);
                    pG->nAA2854 = 2;
                } else if (p8 != pG->pfn10042CF0) {
                    BrSub10072AF0(1, 0x200020);
                    pG->nAA2854 = 1;
                }
                /* the field is RE-READ for the call */
                r = ((BrScrUiHookFn)BrScrLdSlot(pObj, BR_SCR_SLOT_PFN08))(pObj);
                if (r == 0)
                    return 0;
                if (BrScrLdSlot(pObj, BR_SCR_SLOT_PFN08) == pG->pfn10042CF0) {
                    BrSub10072AF0(1, 0x200020);
                    pG->nAA2854 = 1;
                }
                pG->nAA33E4 = 0;
            }
            BrScrSt32(pObj, BR_UI_OFF_FLAGS,
                      BrScrLd32(pObj, BR_UI_OFF_FLAGS) & ~BR_SCR_F1C_0002);
        } else {
            BrScrUiHookFn pfnC =
                (BrScrUiHookFn)BrScrLdSlot(pObj, BR_SCR_SLOT_PFN0C);
            if (pfnC != NULL)
                (void)pfnC(pObj);
        }

        if ((BrScrLd32(pObj, BR_UI_OFF_FLAGS) & BR_SCR_F1C_10000) != 0
            && BrScrLd16(pObj, BR_SCR_UI_W2AB4) > 0) {
            int32_t i = 0;
            do {
                /* The original repeats this lookup five times before the
                 * dispatch; with no intervening call the results are
                 * identical, so it is done once. It is repeated after the
                 * dispatch, which the original also does. */
                BrUiObj *pKid = BrScrChild(pObj, i);

                BrScrSt32(pKid, BR_UI_OFF_FLAGS,
                          BrScrLd32(pKid, BR_UI_OFF_FLAGS)
                          | BR_SCR_F1C_20000);
                BrScrSt16(pKid, BR_SCR_UI_W128, wSaved);
                BrScrSt32(pKid, BR_SCR_UI_F2974, 0u);
                BrScrSt32(pKid, BR_SCR_UI_F2970, 0u);
                BrScrSt32(pObj, BR_SCR_UI_F58,
                          BrScrLd32(pObj, BR_SCR_UI_F58)
                          + (uint32_t)(int32_t)BrScrLd16(pKid,
                                                     BR_SCR_UI_W48));
                BrScrUiV(pKid)->f0C(pKid);

                pKid = BrScrChild(pObj, i);
                BrScrSt32(pKid, BR_UI_OFF_FLAGS,
                          BrScrLd32(pKid, BR_UI_OFF_FLAGS)
                          & ~BR_SCR_F1C_20000);
                ++i;
            } while (i < (int32_t)BrScrLd16(pObj, BR_SCR_UI_W2AB4));

            pV->f08(pObj);
            return 1;
        }
        pV->f08(pObj);
        return 1;
    }

    /* 0x100483D3 -- the "vtable +0x20 said no" tail. */
    f = BrScrLd32(pObj, BR_UI_OFF_FLAGS);
    if (f & BR_SCR_F1C_400000)
        BrScrSt16(pObj, BR_UI_OFF_W1E20C, BrScrLd16u(pObj, BR_UI_OFF_W2A40));

    if ((f & BR_SCR_F1C_0004) == 0 && (f & BR_SCR_F1C_20000) == 0) {
        BrScrSt16(pObj, BR_SCR_UI_W128, 0u);
        if ((f & BR_SCR_F1C_100000) != 0
            && (f & BR_SCR_F1C_0010) == 0
            && BrScrLdSlot(pObj, BR_SCR_SLOT_PFN0C) != NULL) {
            BrScrSt16(pObj, BR_UI_OFF_W1E20C, 3u);
            BrScrSt8(pObj, BR_SCR_UI_ITEMKIND, 1u);
            pV->f08(pObj);
            return 1;
        }
    } else {
        BrScrUiHookFn pfnC = (BrScrUiHookFn)BrScrLdSlot(pObj, BR_SCR_SLOT_PFN0C);
        if (pfnC != NULL)
            (void)pfnC(pObj);
    }
    pV->f08(pObj);
    return 1;
}

/* ==========================================================================
 * 5. BrUiPage
 * ========================================================================== */

/* WHAT IT DOES: makes a fresh, empty page: no rows, no owner, no hooks. Every
 * menu screen is one or more of these. One field is deliberately left holding
 * whatever the memory did, because the original never wrote it either. */
/* port-only body; Glide match is src/core/generated/0x100418C0.c */
BrUiPage *BrUiPageCtor_10048470(BrUiPage *pThis)
{
    int i;

    pThis->f10    = 0;
    pThis->nItems = 0;
    /* GOTCHA: +0x16 is NOT written by the original and operator new does not
     * zero -- it stays indeterminate. Left alone here for the same reason. */
    pThis->f338 = 0.0f;             /* the original stores an integer 0 */
    pThis->f33C = 0.0f;
    pThis->pVtbl = &BrUiPageVtbl_1008F6F8;
    pThis->pfn04 = NULL;
    pThis->pfn08 = NULL;
    pThis->pfn0C = NULL;
    for (i = 0; i < BR_UI_PAGE_ITEMS; ++i)
        pThis->aItems[i] = NULL;
    pThis->pOwner = NULL;
    pThis->f344 = 0;
    pThis->f346 = 0;
    return pThis;
}

/* WHAT IT DOES: tears a page down, and frees its memory too if the caller asks
 * for that. It hands the page's address back afterwards even when it has just
 * been freed -- that is what a C++ deleting destructor compiles to. */
/* @implements 0x100484C0 d3d BrUiPageDelete_100484C0 */
#ifdef BR_MATCHING_BUILD
/* C++ scalar deleting destructor: thiscall via __fastcall with unused EDX
 * (BR_THISCALL1 idiom, as BrVt55A10DeleteDtor); byte-typed flags give the
 * `test byte [esp+8],1`; the dtor body is thiscall too (ECX copy-prop). */
void __fastcall FUN_10041930(void *pThis);
void *__fastcall BrUiPageDelete_100484C0(BrUiPage *pThis, int _edx_unused,
                                         unsigned char nFlags)
{
    FUN_10041930(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}
#else
void *BrUiPageDelete_100484C0(BrUiPage *pThis, int32_t nFlags)
{
    BrSub100484E0(pThis);
    if (nFlags & 1)
        BrOperatorDelete(pThis);
    /* The MSVC scalar deleting destructor returns `this` even after freeing
     * it. Preserved; callers in this range discard it. */
    return pThis;
}
#endif

/* WHAT IT DOES: keeps the highlighted row on a page in range, wrapping round:
 * moving past the last row lands on the first and moving above the first lands
 * on the last. When the highlight is already in range it records it on the page
 * without writing the shared position back. */
/* port-only body; Glide match is src/core/cpp/0x10041940.cpp */
int BrUiPageSelect_100484F0(BrScrGlobals *pG, BrUiPage *pThis)
{
    uint32_t nMod = (uint32_t)pThis->f344;          /* zero-extended */
    int32_t  nCur = (int32_t)(int16_t)pG->wAA286C;  /* SIGN-extended */
    uint16_t res;

    if (nCur >= (int32_t)nMod) {
        res = 0;
        pG->wAA286C = res;
    } else if (nCur >= 0) {
        res = (uint16_t)nCur;
        /* GOTCHA: the global is deliberately NOT written on this arm. */
    } else {
        res = (uint16_t)(nMod - 1u);
        pG->wAA286C = res;
    }
    pThis->f346 = res;
    return 1;
}

/* WHAT IT DOES: runs one page for one frame. It walks the page's rows in
 * order, giving each its turn, skipping the hidden ones, and stepping the
 * highlight past any row that cannot be selected so the cursor never rests on
 * a heading. Rows that own children get those ticked too, and moving the
 * highlight onto a new row resets which page of the screen is showing. Any row
 * that reports failure abandons the rest of the page. */
/* port-only body; Glide match is src/core/cpp/0x10041980.cpp */
int BrUiPageFrame_10048530(BrScrGlobals *pG, BrUiPage *pThis)
{
    int32_t i;

    if (pThis->pfn04 != NULL)
        pThis->pfn04();
    if (pThis->pfn0C != NULL)
        pThis->pfn0C();

    pG->wAA2870 = 0;
    (void)BrUiPageSelect_100484F0(pG, pThis);

    for (i = 0; i < (int32_t)pThis->nItems; ++i) {
        BrUiObj *pItem = pThis->aItems[i];
        uint32_t f;
        BrScrUiHookFn pfn;

        if (pItem == NULL)
            return 0;

        pfn = (BrScrUiHookFn)BrScrLdSlot(pItem, BR_SCR_SLOT_PFN14);
        if (pfn != NULL && pfn(pItem) == 0)
            return 0;

        f = BrScrLd32(pItem, BR_UI_OFF_FLAGS);
        if (f & BR_SCR_F1C_1000) {
            BrScrUiV(pItem)->f04(pItem);
            pfn = (BrScrUiHookFn)BrScrLdSlot(pItem, BR_SCR_SLOT_PFN04);
            if (pfn != NULL)
                (void)pfn(pItem);

            if (BrScrLd8(pItem, BR_UI_OFF_FLAGS) & 0x10u) {
                if (pG->wAA286C == pG->wAA2870) {
                    pG->wAA286C = (uint16_t)(pG->wAA286C + pG->w0AB3DC);
                    (void)BrUiPageSelect_100484F0(pG, pThis);
                }
                ++pG->wAA2870;
            }
            f = BrScrLd32(pItem, BR_UI_OFF_FLAGS);   /* RE-READ */
            if ((f & BR_SCR_F1C_0010) == 0)
                continue;
        }
        if (f & BR_SCR_F1C_0800)
            continue;

        if (BrScrUiV(pItem)->f0C(pItem) == 0) {
            pG->bAA28A8 = 0;
            return 0;
        }

        f = BrScrLd32(pItem, BR_UI_OFF_FLAGS);
        if (f & (BR_SCR_F1C_2000 | BR_SCR_F1C_4000)) {
            uint32_t nSel = (uint32_t)pThis->pOwner->fBC;
            if (nSel == (uint32_t)i || (f & BR_SCR_F1C_4000)) {
                int16_t nKids = BrScrLd16(pItem, BR_SCR_UI_W2AB4);
                int32_t k = 0;
                while (k < nKids) {
                    int32_t idx = BrScrLd16(pItem,
                        BR_SCR_UI_A2AB6 + (size_t)((uint32_t)k * 2u));
                    BrUiObj *pKid = pThis->aItems[idx];
                    BrScrUiV(pKid)->f0C(pKid);
                    nKids = BrScrLd16(pItem, BR_SCR_UI_W2AB4);  /* re-read */
                    ++k;
                }
            }
        }

        pfn = (BrScrUiHookFn)BrScrLdSlot(pItem, BR_SCR_SLOT_PFN18);
        if (pfn != NULL && pfn(pItem) == 0)
            return 0;

        f = BrScrLd32(pItem, BR_UI_OFF_FLAGS);
        if ((f & BR_SCR_F1C_0020) != 0
            && pG->nAA28D8 == 0
            && (f & BR_SCR_F1C_2000) != 0) {
            BrPhaseFull *pOwner = pThis->pOwner;
            if ((uint32_t)pOwner->fBC != (uint32_t)i) {
                int k;
                pOwner->fBC = (uint16_t)(uint32_t)i;
                for (k = 0; k < BR_PHASE_PAGES; ++k)
                    pThis->pOwner->aFlags[k] = 0;
                pThis->pOwner->aFlags[0] = 1;
            }
        }
    }

    if (pThis->pfn08 != NULL)
        pThis->pfn08();
    return 1;
}

/* ==========================================================================
 * 6. BrPhaseFull
 * ========================================================================== */

/* WHAT IT DOES: tears a menu screen down and frees it if asked, returning its
 * address either way -- the standard C++ deleting destructor shape. */
/* @implements 0x10048850 d3d BrPhaseDelete_10048850 */
#ifdef BR_MATCHING_BUILD
void __fastcall FUN_10041cc0(void *pThis);
void *__fastcall BrPhaseDelete_10048850(BrPhaseFull *pThis, int _edx_unused,
                                        unsigned char nFlags)
{
    FUN_10041cc0(pThis);
    if ((nFlags & 1) != 0) {
        BrOperatorDelete(pThis);
    }
    return pThis;
}
#else
void *BrPhaseDelete_10048850(BrPhaseFull *pThis, int32_t nFlags)
{
    BrPhaseDtor_10048870(pThis);
    if (nFlags & 1)
        BrOperatorDelete(pThis);
    return pThis;
}
#endif

/* WHAT IT DOES: tidies a menu screen up by letting go of the two list objects
 * it owns -- the file list and the graphics list a screen may have been given
 * -- and pointing it back at its base behaviours. */
/* port-only body; Glide match is src/core/cpp/0x10041CC0.cpp */
void BrPhaseDtor_10048870(BrPhaseFull *pThis)
{
    BrScrRef *pRef;

    pThis->pVtbl = &BrPhaseVtbl_1008F700;

    pRef = (BrScrRef *)pThis->fC0;
    if (pRef != NULL)
        (void)pRef->pVtbl->f00(pRef, 1);
    /* +0xC4 is read BEFORE +0xC0 is cleared -- preserved. */
    {
        BrScrRef *pRef2 = (BrScrRef *)pThis->fC4;
        pThis->fC0 = NULL;
        if (pRef2 != NULL)
            (void)pRef2->pVtbl->f00(pRef2, 1);
    }
    pThis->fC4 = NULL;
}

/* WHAT IT DOES: calls one particular slot of a screen's own behaviour table
 * and always reports success. Nothing in this packet fills that slot, so what
 * the call actually does depends entirely on the screen. */
/* @implements 0x10041D00 glide BrPhaseFn_100488B0 */
int BR_THISCALL1 BrPhaseFn_100488B0(BrPhaseFull *pThis)
{
    /* vtable +0x20 -- a slot no function in this packet implements. */
    pThis->pVtbl->f20(pThis);
    return 1;
}

/* WHAT IT DOES: the once-a-frame housekeeping that runs behind whatever screen
 * is showing. It keeps the current CD music track number up to date -- normally
 * re-reading it only every hundred and twenty frames, but every frame in one
 * mode -- and moves the mouse pointer element to wherever the mouse now is. It
 * does that with the root screen temporarily made current and then puts the
 * real one back. */
/* port-only body; Glide match is src/core/cpp/0x10041D10.cpp */
int BrPhaseTick_100488C0(BrScrGlobals *pG, BrPhaseFull *pThis)
{
    int fReTrack = 0;
    BrPhaseFull *pPrev;
    BrUiObj *pItem;

    /* GOTCHA: +0x08 is a function pointer everywhere else, but here its LOW
     * BYTE is AND-ed with 0x10. Reproduced on the pointer value. */
    if ((((uintptr_t)pThis->pfn08) & (uintptr_t)BR_PHASE_PFN08_BIT10) != 0)
        return 0;

    if (pG->n0940A4 == 2) {
        fReTrack = 1;
    } else {
        if (pG->nAA2A4C % 0x78 == 0)     /* idiv: truncates toward zero */
            fReTrack = 1;
        ++pG->nAA2A4C;                   /* only on THIS arm */
    }
    if (fReTrack)
        pG->nAA2A34 = BrCdTrackGet() - 2;

    pPrev = pG->pAA2904;
    pG->pAA2904 = pG->pAA2908;
    pItem = pG->pAA2908->aPages[0]->aItems[BR_UI_PAGE_ITEM334];
    BrScrStF(pItem, BR_UI_OFF_F3C, (float)pG->pAA2E80->f00);
    BrScrStF(pItem, BR_UI_OFF_F40, (float)pG->pAA2E80->f04);
    BrScrUiV(pItem)->f0C(pItem);
    pG->pAA2904 = pPrev;

    if (pG->nAA2874 == 0)
        pThis->pVtbl->f14(pThis);
    return 1;
}

/* --- DUPLICATE OWNERSHIP: 0x100489A0 -------------------------------------
 * This body is the byte-image transcription, over BrPhaseFull / BrUiPage.
 * port/src/br_uinav.c carries a SECOND transcription of the same address,
 * BrUiNavPhaseRun_100489A0(BrUiNav *, BrPhase_ *), over br_ui.h's struct
 * model -- for the reason br_uinav.h's DUPLICATE OWNERSHIP banner gives for
 * the seven addresses it already twinned: a byte-offset body cannot be aimed
 * at a struct whose fields have moved under LP64, and the packets the host
 * boots (slice6_71/72/73) build struct objects. That twin is what the host
 * harness now installs at phase vtable +0x0C.
 *
 * No storage is forked. Both bodies reach 0x10AA2904 / 0x10AA2908 /
 * 0x10AA2868 / 0x10B4DF30 / 0x10B4FBE8 through ONE object each -- this one
 * through BrScrGlobals, the twin through BrScrGlobals plus the two BrUiNav
 * phase-slot members whose "a host populates exactly one view" rule br_uinav.h
 * states on pAA29C0. A future merge of BrPhaseFull and BrPhase_ deletes one of
 * the two bodies; until then, a behavioural change to either belongs in both.
 * ------------------------------------------------------------------------ */

/* The two-call teardown 0x100489A0 runs on both of its failure exits. */
static void BrScrPhaseBail(BrScrGlobals *pG, BrPhaseFull *pThis,
                           const BrPhaseFullVtbl *pV)
{
    BrSub1003E310();
    BrSub1006A4A0(pG->pB4DF30, pG->pB4FBE8);
    pThis->iPage = 0;
    pV->f18(pThis, NULL);
}

/* WHAT IT DOES: runs one frame of a menu screen -- reads the keyboard, then
 * gives every page of the screen its turn. If the screen has been asked to
 * close, either before it starts or by the time it finishes, it saves the
 * settings out and tells the screen to shut down rather than carrying on. */
/* port-only body; Glide match is src/core/cpp/0x10041DD0.cpp */
int BrPhaseRun_100489A0(BrScrGlobals *pG, BrPhaseFull *pThis)
{
    const BrPhaseFullVtbl *pV;
    int32_t i;

    if (pThis->f68 == 0) {
        BrScrPhaseBail(pG, pThis, pThis->pVtbl);
        return 0;
    }

    (void)pThis->pVtbl->f04(pThis);

    {
        BrPhaseFull *pPrev = pG->pAA2904;
        pG->pAA2904 = pG->pAA2908;
        BrSub10060260(pG->pAA2900);
        pG->pAA2904 = pPrev;
    }
    BrDikPollAndEdge();
    pG->nAA2868 = (pG->pAA2904 == pG->pAA2908) ? 1 : 0;

    pThis->iPage = 0;
    for (i = 0; i < (int32_t)pThis->nPages; ++i) {
        BrUiPage *pPg = pThis->aPages[i];

        /* GOTCHA: pCur is written BEFORE the NULL test. */
        pThis->pCur = pPg;
        if (pPg == NULL)
            return 0;
        pThis->iPage = (uint16_t)(uint32_t)i;
        if (pThis->aFlags[i] != 0) {
            BrUiPage *pCur = pThis->pCur;   /* the original re-reads +0x64 */
            if (pCur->pVtbl->f04(pCur) == 0)
                return 0;
        }
    }

    pV = pThis->pVtbl;
    (void)pV->f08(pThis);
    if (pThis->f68 != 0)
        return 1;
    BrScrPhaseBail(pG, pThis, pV);
    return 0;
}

/* WHAT IT DOES: throws a screen away entirely -- every row of every page, then
 * the pages themselves -- and puts the menu highlight back to the top. This is
 * what runs when the player leaves a screen. */
/* port-only body; Glide match is src/core/cpp/0x10041ED0.cpp */
void BrPhaseReleasePages_10048AA0(BrScrGlobals *pG, BrPhaseFull *pThis)
{
    int32_t i;

    for (i = 0; i < (int32_t)pThis->nPages; ++i) {
        BrUiPage *pPg = pThis->aPages[i];

        /* DEVIATION: the original walks pPg->aItems[0..199] and only THEN
         * null-checks pPg, so a NULL slot dereferences address 0x18. The
         * port skips the whole entry instead. */
        if (pPg != NULL) {
            int k;
            for (k = 0; k < BR_UI_PAGE_ITEMS; ++k) {
                BrScrRef *pRef = (BrScrRef *)pPg->aItems[k];
                if (pRef != NULL)
                    (void)pRef->pVtbl->f00(pRef, 1);
                pPg->aItems[k] = NULL;
            }
            (void)pPg->pVtbl->f00(pPg, 1);
        }
    }
    pG->wAA286C = 0;
}

/* One slot of 0x10048B20's drop chain. Returns 1 when the slot HELD
 * something, which is what gates each slot's extra clears. */
static int BrScrDropPhase(BrPhaseFull **ppSlot)
{
    BrPhaseFull *p = *ppSlot;

    if (p == NULL)
        return 0;
    p->pVtbl->f1C(p);
    /* GOTCHA: the global is RE-READ here. A +0x1C that clears its own slot
     * therefore skips the release below. */
    p = *ppSlot;
    if (p != NULL)
        (void)p->pVtbl->f00(p, 1);
    *ppSlot = NULL;
    return 1;
}

/* WHAT IT DOES: closes the whole menu system down. It first stalls for a fixed
 * time -- about four and a half seconds in one case -- so that a sound still
 * playing can finish, then walks roughly forty screen slots in turn, telling
 * each screen to release its pages and letting it go. Some slots also clear
 * associated state as they empty. One slot appears twice in the list, so it is
 * dropped and then dropped again. When called with no argument it also releases
 * the root screen and the shared picture list. */
/* port-only body; Glide match is src/core/cpp/0x10041F50.cpp */
void BrPhaseShutdown_10048B20(BrScrGlobals *pG, void *pArg)
{
    uint32_t nEnd;
    int32_t  nWait = 0;

    /* `this` (ecx) is never touched by the original; this is global work. */

    if (pG->nAA2854 == 2)
        nWait = 0x11DA;
    else if (pG->nAA2854 == 3)
        nWait = 0x604;
    nEnd = (uint32_t)nWait + (uint32_t)BrSub10075020();
    while ((uint32_t)BrSub10075020() < nEnd)
        BrScrSleep(0);

    if (pArg == NULL) {
        int32_t k;
        pG->n0AC300 = 0;
        pG->pAA2904 = NULL;
        BrSub1005F530();
        for (k = 0; k < pG->nA9E3D0; ++k) {
            if (pG->apA9E3D0[k] != NULL)
                BrOperatorDelete(pG->apA9E3D0[k]);
            pG->apA9E3D0[k] = NULL;
        }
    }

    if (BrScrDropPhase(&pG->pAA2940)) pG->nA9CFFC = 0;
    if (BrScrDropPhase(&pG->pAA290C)) pG->nAA29AC = 0;
    (void)BrScrDropPhase(&pG->pAA2910);
    if (BrScrDropPhase(&pG->pAA2914)) pG->pAA29B4 = NULL;
    (void)BrScrDropPhase(&pG->pAA2918);
    (void)BrScrDropPhase(&pG->pAA291C);
    if (BrScrDropPhase(&pG->pAA2920)) pG->nAA29A8 = 0;
    (void)BrScrDropPhase(&pG->pAA2924);
    if (BrScrDropPhase(&pG->pAA2928)) {
        pG->pAA29C0 = NULL;
        pG->nAA29CC = 0;
        pG->pAA29F4 = NULL;
    }
    if (BrScrDropPhase(&pG->pAA292C)) pG->pAA29B0 = NULL;
    (void)BrScrDropPhase(&pG->pAA2930);
    (void)BrScrDropPhase(&pG->pAA2934);
    (void)BrScrDropPhase(&pG->pAA2938);
    (void)BrScrDropPhase(&pG->pAA293C);
    /* GOTCHA: 0x10AA2940 again -- the fifteenth slot repeats the first. */
    (void)BrScrDropPhase(&pG->pAA2940);
    (void)BrScrDropPhase(&pG->pAA2944);
    if (BrScrDropPhase(&pG->pAA2948)) {
        pG->nAA29B8 = 0;
        pG->pAA29D8 = NULL;
        pG->nAA29D4 = 0;
        pG->nAA2880 = 0;
    }
    if (BrScrDropPhase(&pG->pAA294C)) pG->nAA29B8 = 0;
    (void)BrScrDropPhase(&pG->pAA2950);
    if (BrScrDropPhase(&pG->pAA2954)) {
        pG->nAA29E4 = 0;
        pG->nAA29E0 = 0;
    }
    if (BrScrDropPhase(&pG->pAA2958)) pG->nAA29A8 = 0;
    if (BrScrDropPhase(&pG->pAA298C)) pG->nAA29E8 = 0;
    (void)BrScrDropPhase(&pG->pAA295C);
    (void)BrScrDropPhase(&pG->pAA2960);
    (void)BrScrDropPhase(&pG->pAA2964);
    if (BrScrDropPhase(&pG->pAA2968)) {
        pG->nAA29C4 = 0;
        pG->nAA29D0 = 0;
    }
    (void)BrScrDropPhase(&pG->pAA296C);
    (void)BrScrDropPhase(&pG->pAA2970);
    (void)BrScrDropPhase(&pG->pAA2974);
    (void)BrScrDropPhase(&pG->pAA297C);
    (void)BrScrDropPhase(&pG->pAA2980);
    (void)BrScrDropPhase(&pG->pAA2984);
    (void)BrScrDropPhase(&pG->pAA2988);
    if (BrScrDropPhase(&pG->pAA2990)) pG->nAA29F0 = 0;
    if (BrScrDropPhase(&pG->pAA2994)) pG->nAA29EC = 0;
    (void)BrScrDropPhase(&pG->pAA2998);

    if (pArg == NULL) {
        (void)BrScrDropPhase(&pG->pAA2908);
        if (pG->pAA2900 != NULL) {
            BrStub8B80_1p(pG->pAA2900);
            BrOperatorDelete(pG->pAA2900);
            pG->pAA2900 = NULL;
        }
        BrSub1005FCF0();
    }
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
int operator_delete();
int __fastcall FUN_10040d10(void *pThis);
typedef int (*funcptr)();
extern funcptr DAT_106b7ab8;
extern int DAT_10ac5d84;
int FUN_100014e0();
extern funcptr PTR_FUN_100776C0;
extern funcptr PTR_FUN_100776c0;

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_100776C0 (fastcall). */
/* @implements 0x10041930 glide BrVtInit41930 */
/* @n64 0x8021C6E4 located */

int __fastcall BrVtInit41930(int *param_1)

{
  *param_1 = &PTR_FUN_100776c0;
  return;
}

/* WHAT IT DOES: menu teardown callback: release a resource and invoke the cleanup funcptr. */
/* @implements 0x10041DB0 glide BrMenuCallback41DB0 */

int BrMenuCallback41DB0(void)

{
  FUN_100014e0(DAT_10ac5d84);
                    
                    
  (*DAT_106b7ab8)();
  return;
}


#endif /* BR_MATCHING_BUILD */
