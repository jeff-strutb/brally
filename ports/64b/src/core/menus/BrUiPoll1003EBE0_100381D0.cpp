/* WHAT IT DOES: the same highlight-sync as its siblings, and additionally
 * caches the selected row's record where the rest of the page can read it. */
/* @implements 0x100381D0 glide BrUiPoll1003EBE0
 * @cpp_kind free
 * @cpp_symbol ?BrUiPoll1003EBE0@@YAHPAVObj381D0@@@Z
 *
 * cdecl, one arg, `ret`, 72 B. Ask the +0x3838 selector widget which row
 * it is on (its +0x20 vcall, seeded with the last answer); keep the new
 * row when it gives one and fall back to the stored one when it returns
 * negative, then publish that row's record pointer out of the owner's
 * 0x438-stride array at +0x3C98. Returns 1.
 *
 * The port body in slice2_23.c reaches its globals through a BrUiGlobals*
 * second parameter; the original is cdecl with ONE argument and direct
 * global addresses. Same split as the 0x10038650 sibling.
 */
#define _CRTIMP __declspec(dllimport)

class Sel381D0 {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual void s5();
    virtual void s6();
    virtual void s7();
    virtual int  s8(int row);   /* +0x20 */
};

struct BrRec381D0 {
    char raw[0x438];
};

class Obj381D0 {
public:
    char        pad0000[0x3838];
    Sel381D0    m3838;          /* +0x3838 */
    char        pad383C[0x3C98 - 0x383C];
    BrRec381D0  aRows[1];       /* +0x3C98 */
};

typedef char chk_sel381D0[(unsigned)&((Obj381D0 *)0)->m3838 == 0x3838 ? 1 : -1];


extern "C" {
/* 64-bit core: g_brRow5BD8 is defined once, in br_globals.c */
/* 64-bit core: g_brRec0AAB80 is defined once, in br_globals.c */
}

int BrUiPoll1003EBE0(Obj381D0 *pObj)
{
    int row = pObj->m3838.s8(g_brRow5BD8);

    if (row >= 0)
        g_brRow5BD8 = row;
    else
        row = g_brRow5BD8;

    g_brRec0AAB80 = *(int *)&pObj->aRows[row];

    return 1;
}
