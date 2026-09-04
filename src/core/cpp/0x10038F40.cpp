/* WHAT IT DOES: build the caption for the pick item -- a fixed string on the
 * root page, otherwise a name assembled from the current mode and selection. */
/* @implements 0x10038F40 glide BrItemSetPickLabel_10038F40
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetPickLabel_10038F40@@YAHPAVObj38F40@@@Z
 *
 * cdecl, one arg, `ret`, 567 B. Put the name of the current selection
 * into the owner's +0x2B5C item label. One special case (root phase, spare
 * flag clear) uses a fixed catalogue entry; otherwise the entry is looked
 * up three different ways into a scratch buffer, and when the selection's
 * descriptor carries the 0x10 bit a "locked" caption is flashed through
 * the item first -- the owner's +0x40 geometry is parked, the item's
 * +0x414 forced to a constant, the caption laid out and applied, and
 * +0x414 restored from the parked value.
 *
 * Same 0x438 item record as 0x10041300. The parked geometry is FLOAT --
 * both the fields and the local. That single typing decision is worth
 * 415 diffs here. Typed int, the local lives in a callee-saved register,
 * so the prologue grows a `push ebp` and the frame SHRINKS by the 4 bytes
 * the original reserves for it. Typed float it has to be homed, which is
 * what the original does; the copies in and out are still plain integer
 * `mov`s (VC5 moves float memory with GP registers), and the constant is
 * a straight immediate store -- all of which read like int code and are
 * exactly why the int typing looked right at first.
 * TELL: recomp has one more callee-saved push than the original and a
 * frame that is 4 bytes SMALLER. That pairing means an int local where
 * the original had a float one.
 *
 * Two structural points worth keeping:
 *  - the special case and the ordinary path both end in
 *    `strcpy(label, <something>)`, and VC5 MERGES the two inline strcpy
 *    expansions: the special case jumps straight into the tail's copy with
 *    its source already in edi. Writing them as one shared tail is what
 *    produces that.
 *  - each arm's index expression appears TWICE in the source -- once
 *    inside the catalogue lookup and once for the descriptor -- because
 *    the original recomputes it after the copy (the globals are reloaded
 *    across the call). Caching it in a local before the strcpy does not
 *    match.
 *
 * PARKED at 10 diffs, T3a. Only the FIRST index arm differs, and only in
 * register naming (orig loads the global into ecx and lands the map byte
 * in eax; ours uses edx and ecx). The other two arms and everything else
 * are byte-identical, and the original's own two arms do not agree with
 * each other either, so this is the allocator.
 * DO NOT RE-PROBE -- unchanged by swapping the addition's operand order in
 * either arm, and swapping the two arms costs one more diff.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item438L {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 relayout */
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();
    virtual void  s11();

    int   f004;
    char  b008;
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;
    float f414;                 /* +0x414 */
};

typedef char chk_nameL[(unsigned)&((Item438L *)0)->szName == 9 ? 1 : -1];
typedef char chk_f414L[(unsigned)&((Item438L *)0)->f414 == 0x414 ? 1 : -1];

class Obj38F40 {
public:
    char     pad000[0x40];
    float    f040;              /* +0x040 */
    char     pad044[0x2B5C - 0x44];
    Item438L m2B5C;             /* +0x2B5C */
};

typedef char chk_f040L[(unsigned)&((Obj38F40 *)0)->f040 == 0x40 ? 1 : -1];
typedef char chk_itemL[(unsigned)&((Obj38F40 *)0)->m2B5C == 0x2B5C ? 1 : -1];

struct BrDesc38F40 {
    char pad00[4];
    int  f04;                   /* +0x04 -- bit 4 = locked */
};

extern "C" {
int            g_brPhase5C5C;   /* 0x10AC5C5C */
int            g_brRoot5CBC;    /* 0x10AC5CBC */
int            g_brFlag5C40;    /* 0x10AC5C40 */
int            g_brFlag0A9360;  /* 0x100A9360 */
char           g_brFlag5C00;    /* 0x10AC5C00 */
char           g_brSel5C10;     /* 0x10AC5C10 */
int            g_brIdx5C04;     /* 0x10AC5C04 */
int            g_brIdx5BFC;     /* 0x10AC5BFC */
int            g_brIdx0ABDE8;   /* 0x100ABDE8 */
unsigned char  g_brMap3028[];   /* 0x100B3028 */
int            g_brTblABAA8[];  /* 0x100ABAA8 */
BrDesc38F40   *g_brTblBCAB0[];  /* 0x100BCAB0 */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int BrItemSetPickLabel_10038F40(Obj38F40 *pObj)
{
    char szName[128];

    if (g_brPhase5C5C == g_brRoot5CBC && g_brFlag5C40 == 0) {
        strcpy(pObj->m2B5C.szName, BrStrByIndex(0x1B));
    } else {
        int k;

        if (g_brFlag0A9360 == 0) {
            if (g_brFlag5C00 != 0) {
                strcpy(szName, BrStrByIndex(g_brTblABAA8[
                    g_brMap3028[(g_brIdx5C04 + g_brSel5C10 * 12) * 2]]));
                k = g_brMap3028[(g_brIdx5C04 + g_brSel5C10 * 12) * 2];
            } else {
                strcpy(szName, BrStrByIndex(g_brTblABAA8[
                    g_brMap3028[(g_brIdx5BFC + g_brSel5C10 * 12) * 2]]));
                k = g_brMap3028[(g_brIdx5BFC + g_brSel5C10 * 12) * 2];
            }
        } else {
            strcpy(szName, BrStrByIndex(g_brTblABAA8[g_brIdx0ABDE8]));
            k = g_brIdx0ABDE8;
        }

        if (g_brTblBCAB0[k]->f04 & 0x10) {
            float save = pObj->f040;

            pObj->m2B5C.f414 = 130.0f;

            strcpy(pObj->m2B5C.szName, BrStrByIndex(0xB0));

            pObj->m2B5C.s1();
            BrItemApply_10038380(pObj, 0);

            pObj->m2B5C.f414 = save;
        }

        strcpy(pObj->m2B5C.szName, szName);
    }

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
