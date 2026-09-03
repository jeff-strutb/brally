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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

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

typedef char chk_cnt[(unsigned)&((Slots54A30 *)0)->wCount == 0x1A92C ? 1 : -1];
typedef char chk_18[(unsigned)&((Slots54A30 *)0)->i18 == 0x18 ? 1 : -1];
typedef char chk_20[(unsigned)&((Slots54A30 *)0)->f20 == 0x20 ? 1 : -1];
typedef char chk_990[(unsigned)&((Slots54A30 *)0)->i1a990 == 0x1A990 ? 1 : -1];
typedef char chk_9b0[(unsigned)&((Slots54A30 *)0)->f1a9b0 == 0x1A9B0 ? 1 : -1];
typedef char chk_9d0[(unsigned)&((Slots54A30 *)0)->f1a9d0 == 0x1A9D0 ? 1 : -1];

extern "C" {
char g_szBrAC5DD0[];            /* 0x10AC5DD0 -- the suffix appended */
char g_szBr396F08[];            /* 0x10396F08 -- the empty-slot name */
}

#define BR_SLOT 0x438
#define BR_REC(n) ((char *)this + (n) * BR_SLOT)

int Slots54A30::Add(const char *pszName, int flags, char kind,
                    const int *pRect, int bPlain)
{
    short          row;
    unsigned short d;

    if (pszName == 0)
        return 0;

    if (wCount >= 100) {
        s11(0);
        wCount = 99;
    }

    if (bPlain != 0) {
        strcpy(BR_REC(wCount) + 0x35, pszName);
    } else {
        strncpy(BR_REC(wCount) + 0x35, pszName, 10);
        strcat(BR_REC(wCount) + 0x35, g_szBrAC5DD0);
    }

    *(int *)(BR_REC(wCount) + 0x30) |= flags;
    *(BR_REC(wCount) + 0x34) = kind;
    *(short *)(BR_REC(wCount) + 0x448) = 0;
    *(short *)BR_REC(wCount + 1) = 0;
    *(short *)(BR_REC(wCount) + 0x436) = 0;
    *(int *)(BR_REC(wCount) + 0x450) = pRect[0];
    *(int *)(BR_REC(wCount) + 0x458) = pRect[2];
    *(int *)(BR_REC(wCount) + 0x454) = (int)f20 + 19 * wCount;
    *(int *)(BR_REC(wCount) + 0x45C) = *(int *)(BR_REC(wCount) + 0x454) + 0x12;
    *(float *)(BR_REC(wCount) + 0x43C) = (float)pRect[0];
    *(float *)(BR_REC(wCount) + 0x440) =
        (float)*(int *)(BR_REC(wCount) + 0x454);
    *(int *)(BR_REC(wCount) + 0x444) = 0;
    *(int *)(BR_REC(wCount) + 0x44C) = 0;

    if (kind == 3)
        ((Item54A30 *)(BR_REC(wCount) + 0x2C))->s2();
    else
        ((Item54A30 *)(BR_REC(wCount) + 0x2C))->s1();

    *(short *)(BR_REC(wCount) + 0x448) =
        (short)(*(int *)(BR_REC(wCount) + 0x458)
                - *(int *)(BR_REC(wCount) + 0x450) - 0x10);

    wCount++;

    if ((i18 & 0x800000) != 0) {
        row = (short)(w1a930 + w1a92e);
        if (row >= 100)
            row = (short)(wCount - 1);

        if (_stricmp(BR_REC(row) + 0x35, g_szBr396F08) == 0)
            return 0;

        w1a92e++;
        if ((int)w1a92e >= (int)wCount)
            w1a92e = (short)(wCount - 1);

        if (pfn0C != 0)
            pfn0C();

        /* `d <= 0` on an unsigned, not `d == 0`: the original's test is
         * `cmp ax,si / ja`, an unsigned RELATIONAL against the shared zero
         * register. The equality spelling gives `jne` and is the whole
         * one-byte residue this function had left. */
        d = (unsigned short)(wCount - 1);
        if (d <= 0)
            d = 1;
        f1a9b0 = f1a9b0 + f1a9d0 / (float)d;
        if (f1a9b0 < f1a9c8)
            f1a9b0 = f1a9c8;
        else if (f1a9b0 > f1a9cc)
            f1a9b0 = f1a9cc;

        i1a990 = (int)f1a9b0;
        i1a998 = i1a990 + 0x10;
    }

    return 1;
}
