/* WHAT IT DOES: add a row to a slot list, refusing once the list is full
 * (100 rows) and refusing a null name. Returns whether the row was added. */
/* @implements 0x10054A30 glide BrSlotAdd_10054A30
 * @cpp_kind method
 * @cpp_symbol ?Add@Slots54A30@@QAEHPBDHDPBHH@Z
 *
 * Thiscall, five stack args (`ret 0x14`), 999 B. Appends one entry to the
 * same slot array as 0x10054E20 / 0x10054730 / 0x100549A0. That array is
 * based at `this` ITSELF -- record n starts at `this + n*0x438` and its
 * own polymorphic object sits at +0x2C inside it, so the owner's header
 * fields (+0x0C, +0x18, +0x20) and record 0's first 0x2C bytes are the
 * same storage. A `Rec aRecs[100]` member cannot express that; raw
 * offsets off a `char *` can, and that is why this TU is written the way
 * 0x10054E20 is.
 *
 * The original re-reads the count and rebuilds the whole `*135*8` index
 * chain before EVERY store. That is not scheduling noise -- it is what
 * the source says. Spelling the count in each statement reproduces it;
 * hoisting the record into a pointer or the count into a local collapses
 * the chains and rewrites the function.
 *
 * `strcpy` and `strcat` are intrinsics under /O2 and come out as inline
 * `rep movs`; `strncpy` and `_stricmp` are not, and go through the import
 * table. The two copy arms share one `and ecx,3 / rep movsb` tail -- that
 * is VC5 cross-jumping the identical tails, not one copy.
 */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "slice3_39.h"   /* BrTextList, the canonical record */

/* The object embedded at record+0x2C. */
class Item54A30 {
public:
    virtual void s0();
    virtual void s1();              /* +0x04 */
    virtual void s2();              /* +0x08 */
};

class Slots54A30 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6();
    virtual void s7();
    virtual void s8();
    virtual void s9();
    virtual void s10();
    virtual void s11(int);          /* +0x2C */

    char           pad004[0x0C - 0x04];
    void         (*pfn0C)(void);    /* +0x0C */
    char           pad010[0x18 - 0x10];
    int            i18;             /* +0x18 */
    char           pad01C[0x20 - 0x1C];
    float          f20;             /* +0x20 */
    char           pad024[0x1A92C - 0x24];
    unsigned short wCount;          /* +0x1A92C */
    short          w1a92e;
    short          w1a930;
    char           pad1a932[0x1A990 - 0x1A932];
    int            i1a990;
    char           pad1a994[0x1A998 - 0x1A994];
    int            i1a998;
    char           pad1a99c[0x1A9B0 - 0x1A99C];
    float          f1a9b0;
    char           pad1a9b4[0x1A9C8 - 0x1A9B4];
    float          f1a9c8;
    float          f1a9cc;
    float          f1a9d0;

    int Add(const char *pszName, int flags, char kind, const int *pRect,
            int bPlain);
};








extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

#define BR_SLOT 0x438

int Slots54A30::Add(const char *pszName, int flags, char kind,
                    const int *pRect, int bPlain)
{
    short          row;
    unsigned short d;

    if (pszName == 0)
        return 0;

    if ((*(unsigned short *)&((BrTextList *)(this))->count) >= 100) {
        s11(0);
        (*(unsigned short *)&((BrTextList *)(this))->count) = 99;
    }

    if (bPlain != 0) {
        strcpy(((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].sz, pszName);
    } else {
        strncpy(((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].sz, pszName, 10);
        strcat(((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].sz, g_szBrAC5DD0);
    }

    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f04) |= flags;
    ((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f08 = kind;
    *(short *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f41C) = 0;
    ((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].height = 0;   /* list +(n+1)*0x438 */
    *(short *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].width) = 0;
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].left) = pRect[0];
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].right) = pRect[2];
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f428) = (int)(*(float *)&((BrTextList *)(this))->f20) + 19 * (*(unsigned short *)&((BrTextList *)(this))->count);
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f430) = *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f428) + 0x12;
    *(float *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].x) = (float)pRect[0];
    *(float *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].y) =
        (float)*(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f428);
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f418) = 0;
    *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f420) = 0;

    if (kind == 3)
        ((Item54A30 *)&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)])->s2();
    else
        ((Item54A30 *)&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)])->s1();

    *(short *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].f41C) =
        (short)(*(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].right)
                - *(int *)(&((BrTextList *)(this))->aItems[(*(unsigned short *)&((BrTextList *)(this))->count)].left) - 0x10);

    (*(unsigned short *)&((BrTextList *)(this))->count)++;

    if (((*(int *)&((BrTextList *)(this))->f18) & 0x800000) != 0) {
        row = (short)((*(short *)&((BrTextList *)(this))->f1A930) + (*(short *)&((BrTextList *)(this))->f1A92E));
        if (row >= 100)
            row = (short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);

        if (_stricmp(((BrTextList *)(this))->aItems[row].sz, g_aBr39B720) == 0)
            return 0;

        (*(short *)&((BrTextList *)(this))->f1A92E)++;
        if ((int)(*(short *)&((BrTextList *)(this))->f1A92E) >= (int)(*(unsigned short *)&((BrTextList *)(this))->count))
            (*(short *)&((BrTextList *)(this))->f1A92E) = (short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);

        if ((*(void (**)(void))&((BrTextList *)(this))->f0C) != 0)
            (*(void (**)(void))&((BrTextList *)(this))->f0C)();

        /* `d <= 0` on an unsigned, not `d == 0`: the original's test is
         * `cmp ax,si / ja`, an unsigned RELATIONAL against the shared zero
         * register. The equality spelling gives `jne` and is the whole
         * one-byte residue this function had left. */
        d = (unsigned short)((*(unsigned short *)&((BrTextList *)(this))->count) - 1);
        if (d <= 0)
            d = 1;
        (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[5]) + (*(float *)&((BrTextList *)(this))->f1A99C[13]) / (float)d;
        if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) < (*(float *)&((BrTextList *)(this))->f1A99C[11]))
            (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[11]);
        else if ((*(float *)&((BrTextList *)(this))->f1A99C[5]) > (*(float *)&((BrTextList *)(this))->f1A99C[12]))
            (*(float *)&((BrTextList *)(this))->f1A99C[5]) = (*(float *)&((BrTextList *)(this))->f1A99C[12]);

        (*(int *)&((BrTextList *)(this))->f1A990) = (int)(*(float *)&((BrTextList *)(this))->f1A99C[5]);
        (*(int *)&((BrTextList *)(this))->f1A998) = (*(int *)&((BrTextList *)(this))->f1A990) + 0x10;
    }

    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrSlotAdd_10054A30(void *self, const char * pszName, int flags, char kind, const int * pRect, int bPlain)
{
    return ((class Slots54A30 *)self)->Add(pszName, flags, kind, pRect, bPlain);
}
/* end of C entry points */
