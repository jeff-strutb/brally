/* @implements 0x100393C0 glide BrItemSetModeLabel_100393C0
 * @cpp_kind free
 * @cpp_symbol ?BrItemSetModeLabel_100393C0@@YAHPAVObj393C0@@@Z
 *
 * cdecl, one arg, `ret`, 330 B. Put a catalogue string into the owner's
 * +0x2B5C item label and relayout. Which string depends on the state: in
 * the one special case (the current phase is the root one and the extra
 * flag is clear) it is a fixed entry and the item's +0x414 geometry field
 * is nudged twice, once before the text is set and once after the apply
 * hook; otherwise the entry comes from an index table reached three
 * different ways.
 *
 * Same 0x438 item record as 0x10041300 -- here its +0x414 field is read
 * as a FLOAT (fld/fsub/fstp), which is why this TU types it that way.
 *
 * The special-case arm and everything up to the index selection is
 * byte-exact; the arm ORDER matters and is settled: testing
 * `g_brFlag0A9360 == 0` with the odd-one-out arm as the ELSE is what puts
 * that block last, the way the original does (writing it as the first
 * `if` arm costs 26 diffs).
 *
 * PARKED at 94 diffs. What is left is how deep VC5 tail-merges the three
 * index arms, and it is allocator-driven, not source-driven. The original
 * merges TWO of the three -- its 5BFC and 5D58 arms both leave the index
 * in ecx and share the final `ABB50[k]` lookup, while the 5C04 arm leaves
 * it in eax and keeps its own copy (330 B). Measured alternatives:
 *   this shape, three full calls               352 B, 94 diffs
 *   one shared `k` and one shared lookup       320 B, 99 diffs (merges
 *                                              all three -- 10 B SHORT)
 *   shared `k` in two arms, inline in the 5C04 336 B, 101 diffs
 * Since the clean single-lookup source comes out SHORTER than the
 * original, the likelier reading is that the original's source is that
 * clean form and its allocator failed to merge the third arm. Either way
 * no source shape lands on 330. DO NOT RE-PROBE the three above.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#endif

class Item438K {
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

    int   f004;                 /* +0x004 */
    char  b008;                 /* +0x008 */
    char  szName[0x401];        /* +0x009 */
    short w40A;
    short w40C;
    short pad40E;
    int   f410;                 /* +0x410 */
    float f414;                 /* +0x414 */
};

typedef char chk_nameK[(unsigned)&((Item438K *)0)->szName == 9 ? 1 : -1];
typedef char chk_f414K[(unsigned)&((Item438K *)0)->f414 == 0x414 ? 1 : -1];

class Obj393C0 {
public:
    char      pad000[0x2B5C];
    Item438K  m2B5C;            /* +0x2B5C */
};

typedef char chk_itemK[(unsigned)&((Obj393C0 *)0)->m2B5C == 0x2B5C ? 1 : -1];

extern "C" {
int           g_brPhase5C5C;    /* 0x10AC5C5C */
int           g_brRoot5CBC;     /* 0x10AC5CBC */
int           g_brFlag5C40;     /* 0x10AC5C40 */
int           g_brFlag0A9360;   /* 0x100A9360 */
char          g_brFlag5C00;     /* 0x10AC5C00 */
char          g_brSel5C10;      /* 0x10AC5C10 */
int           g_brIdx5C04;      /* 0x10AC5C04 */
int           g_brIdx5BFC;      /* 0x10AC5BFC */
int           g_brSel5D58;      /* 0x10AC5D58 */
unsigned char g_brMap3029[];    /* 0x100B3029 */
int           g_brTblABB50[];   /* 0x100ABB50 */
float         g_f077628;        /* 0x10077628 */
float         g_f07762C;        /* 0x1007762C */

char *BrStrByIndex(int idx);                    /* 0x1006D280 */
void  BrItemApply_10038380(void *pObj, int a);  /* 0x10038380 */
}

int BrItemSetModeLabel_100393C0(Obj393C0 *pObj)
{
    char *s;

    if (g_brPhase5C5C == g_brRoot5CBC && g_brFlag5C40 == 0) {
        pObj->m2B5C.f414 = pObj->m2B5C.f414 - g_f077628;

        strcpy(pObj->m2B5C.szName, BrStrByIndex(0x1C));

        pObj->m2B5C.s1();
        BrItemApply_10038380(pObj, 0);

        pObj->m2B5C.f414 = pObj->m2B5C.f414 - g_f07762C;
        return 1;
    }

    if (g_brFlag0A9360 == 0) {
        if (g_brFlag5C00 != 0)
            s = BrStrByIndex(g_brTblABB50[
                    g_brMap3029[(g_brIdx5C04 + g_brSel5C10 * 12) * 2]]);
        else
            s = BrStrByIndex(g_brTblABB50[
                    g_brMap3029[(g_brIdx5BFC + g_brSel5C10 * 12) * 2]]);
    } else {
        s = BrStrByIndex(g_brTblABB50[g_brSel5D58]);
    }

    strcpy(pObj->m2B5C.szName, s);

    pObj->m2B5C.s1();
    BrItemApply_10038380(pObj, 0);

    return 1;
}
