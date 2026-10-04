/* WHAT IT DOES: initialise one menu item -- its label, its flags, its kind
 * and its rectangle. The common setup every page builder calls for each
 * control it adds. */
/* @implements 0x10041300 glide BrItemInit_10041300
 * @cpp_kind method
 * @cpp_symbol ?Init@Obj41300@@QAEXPADHDPAH@Z
 *
 * Thiscall, four stack args (`ret 0x10`), 247 B. Initialise the embedded
 * +0x2B5C item: copy the caller's label in, fold the flag bits into the
 * item's flag word, stamp its kind byte, clear the four measurement
 * fields, seed its geometry from the owner's +0x3C/+0x40 pair and the
 * caller's rect, then let the item lay itself out through one of two
 * vcalls (kind 3 takes the +0x08 slot, everything else +0x04) and, when
 * bit 0 of the flags is set, through the +0x28 measure vcall whose float
 * result is thrown away. Finally the owner caches the laid-out numbers.
 *
 * THE ITEM IS THE 0x438 RECORD from 0x10054E20's slot array, and this is
 * where its head is pinned down: the dword at +0x00 is a VTABLE pointer
 * (the three vcalls all go through the one cached load), +0x04 is the
 * flag word, +0x08 the kind byte, +0x09 the label. Everything from
 * +0x40A on matches the offsets that function clears, and the fields sum
 * to exactly 0x438. Keep the two files' layouts in step.
 *
 * The owner's +0x3C/+0x40 pair reaches the item as integer movs, so it is
 * copied with the dword-pun spelling; the SAME +0x40 is then read as a
 * float for the `(int)` conversion, which is the usual VC5 `call __ftol`.
 * That conversion's result is kept in a local -- the original adds the
 * item's +0x40C short to the value still in eax rather than reloading
 * +0x54.
 */
#define _CRTIMP __declspec(dllimport)
#include <string.h>
#include "slice3_39.h"   /* BrTextBox, the canonical record */
#include "br_ui.h"   /* BrUiCtl_, the canonical record */

class Rec438 {
public:
    virtual void  s0();
    virtual void  s1();         /* +0x04 */
    virtual void  s2();         /* +0x08 */
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();
    virtual void  s7();
    virtual void  s8();
    virtual void  s9();
    virtual float s10();        /* +0x28 -- measure, result discarded */

    int   f004;                 /* +0x004 flag word */
    char  b008;                 /* +0x008 kind */
    char  szName[0x401];        /* +0x009 */
    short w40A;                 /* +0x40A */
    short w40C;                 /* +0x40C */
    short pad40E;
    int   f410;                 /* +0x410 */
    int   f414;
    int   f418;
    short w41C;                 /* +0x41C */
    short pad41E;
    int   f420;                 /* +0x420 */
    int   a424[4];              /* +0x424 */
    int   f434;                 /* +0x434 */
};                              /* 0x438 */





class Obj41300 {
public:
    char   pad000[0x3C];
    int    f03C;                /* +0x03C */
    int    f040;                /* +0x040 */
    char   pad044[4];
    short  w048;                /* +0x048 */
    short  w04A;                /* +0x04A */
    char   pad04C[4];
    int    f050;                /* +0x050 */
    int    f054;                /* +0x054 */
    int    f058;                /* +0x058 */
    int    f05C;                /* +0x05C */
    char   pad060[0x2B5C - 0x60];
    Rec438 m2B5C;               /* +0x2B5C */

    void Init(char *pName, int flags, char kind, int *pRect);
};



void Obj41300::Init(char *pName, int flags, char kind, int *pRect)
{
    int v;

    strcpy((*(char (*)[1025])&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->sz[0]), pName);

    (*(int *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->f04) |= flags;
    (*(char *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->f08) = kind;
    (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->f41C) = 0;
    (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->height) = 0;
    (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->width) = 0;
    (*(int (*)[4])&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->left)[0] = pRect[0];
    (*(int (*)[4])&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->left)[2] = pRect[2];
    (*(int *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->x) = (*(int *)&((BrUiCtl_ *)(this))->x);
    (*(int *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->y) = (*(int *)&((BrUiCtl_ *)(this))->y);
    (*(int *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->f418) = 0;
    (*(int *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->f420) = 0;

    if (kind == 3)
        (*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0]).s2();
    else
        (*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0]).s1();

    if (flags & 1)
        (*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0]).s10();

    v = (int)*(float *)&(*(int *)&((BrUiCtl_ *)(this))->y);
    (*(int *)&((BrUiCtl_ *)(this))->rcTop) = v;
    (*(int *)&((BrUiCtl_ *)(this))->rcLeft) = pRect[0];
    (*(int *)&((BrUiCtl_ *)(this))->rcRight) = pRect[2];
    (*(int *)&((BrUiCtl_ *)(this))->rcBottom) = v + (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->height);
    (*(short *)&((BrUiCtl_ *)(this))->w48) = (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->width);
    (*(short *)&((BrUiCtl_ *)(this))->w4A) = (*(short *)&((BrTextBox *)&((*(class Rec438 *)&((BrUiCtl_ *)(this))->aText[0])))->height);
}

/* C entry points (generated by ports/brally/tools/methodfwd.py) */
extern "C" void BrItemInit_10041300(void *self, char * pName, int flags, char kind, int * pRect)
{
    ((class Obj41300 *)self)->Init(pName, flags, kind, pRect);
}
/* end of C entry points */
