/* WHAT IT DOES: paints a scrolling list control: its title icon, the
 * highlighted row (unless disabled), each visible row's text item placed
 * one row pitch below the last (with a tick or cross icon beside it when
 * the list shows marks), and the up / down / extra arrow icons, which
 * switch to their pressed images while held. */
/* @implements 0x100550E0 glide BrUiListPaint_100550E0
 * @cpp_kind method
 * @cpp_symbol ?Paint@Ctl550E0@@QAEHH@Z
 *
 * Thiscall, one stack arg (`ret 4`), 581 B. Same 0x438-byte row records as
 * the sibling slot controls (0x10054E20, 0x100553B0), addressed as members
 * of the row array so the row base stays `this + n*0x438` and every field
 * is a displacement from it.  The row's y is `y1a940 + i*0x13`, which VC5
 * strength-reduces into the dead `sel` argument slot, and the running y
 * survives into the mark icon even when the row has no text.
 * Two source facts: the TU is big enough to include a CRT header (the
 * symbol-table size decides which of i and w1a92e the visible-row add
 * lands on -- with an empty TU VC5 adds i into the member's register), and
 * the first arrow is an if/else of two Draw calls like the second (VC5
 * merges the two calls back into one with the `push 0x2f; jmp` argument
 * select; a `?:` argument instead hoists `mov eax,0x2f` above the loads).
 */
#include <string.h>
#include "slice3_39.h"   /* BrTextList, the canonical record */
/* FUN_10051580: prototype in br_funcs.h */

class Item550E0 {
public:
    virtual void  s0();
    virtual void  s1();                 /* +0x04 */
    virtual void  s2();
    virtual void  s3(float x, float y); /* +0x0C */

    int   f004;                         /* +0x004 */
    char  b008;                         /* +0x008 */
    char  szName[0x424 - 0x009];        /* +0x009 */
    int   x0;                           /* +0x424 */
    int   y0;                           /* +0x428 */
    int   x1;                           /* +0x42C */
    int   y1;                           /* +0x430 */
    int   f434;                         /* +0x434 */
};

class Ctl550E0 {
public:
    virtual void  s0();
    virtual void  Draw(short id, float x, float y);   /* +0x04 */

    char           pad004[0x18 - 0x04];
    int            i18;                 /* +0x18 */
    char           pad01C[0x24 - 0x1C];
    float          f24;                 /* +0x24 */
    float          f28;                 /* +0x28 */
    Item550E0      aItem[1];            /* +0x2C, 0x438 each */
    char           pad[0x1A92C - 0x2C - 0x438];
    unsigned short wCount;              /* +0x1A92C */
    short          w1a92e;              /* +0x1A92E */
    unsigned short wVisible;            /* +0x1A930 */
    short          w1a932;              /* +0x1A932 */
    short          w1a934;              /* +0x1A934 */
    short          w1a936;              /* +0x1A936 */
    short          w1a938;              /* +0x1A938 */
    char           pad1a93a[0x1A93C - 0x1A93A];
    int            x1a93c;              /* +0x1A93C */
    int            y1a940;              /* +0x1A940 */
    int            x1a944;              /* +0x1A944 */
    char           pad1a948[0x1A94C - 0x1A948];
    int            x1a94c;              /* +0x1A94C */
    int            y1a950;              /* +0x1A950 */
    char           pad1a954[0x1A95C - 0x1A954];
    int            x1a95c;              /* +0x1A95C */
    int            y1a960;              /* +0x1A960 */
    char           pad1a964[0x1A99C - 0x1A964];
    int            f1a99c;              /* +0x1A99C */
    int            f1a9a0;              /* +0x1A9A0 */
    char           pad1a9a4[0x1A9AC - 0x1A9A4];
    float          f1a9ac;              /* +0x1A9AC */
    float          f1a9b0;              /* +0x1A9B0 */

    int Paint(int sel);
};





int Ctl550E0::Paint(int sel)
{
    int y = 0;
    int i;

    if ((*(short *)&((BrTextList *)(this))->f1A936) >= 0)
        Draw((*(short *)&((BrTextList *)(this))->f1A936), (*(float *)&((BrTextList *)(this))->f24), (*(float *)&((BrTextList *)(this))->f28));

    if (((*(int *)&((BrTextList *)(this))->f18) & 0x1000000) == 0 && sel >= 0 && ((*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[sel])))->f04) & 0x10) == 0)
        (*(char *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[sel])))->f08) = 4;

    for (i = 0; i < (*(unsigned short *)&((BrTextList *)(this))->f1A930); i++) {
        int n = i + (*(short *)&((BrTextList *)(this))->f1A92E);
        if (n >= 0 && n < (*(unsigned short *)&((BrTextList *)(this))->count)) {
            if ((*(char (*)[1051])&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->sz[0]) != 0) {
                (*(Item550E0 *)&((BrTextList *)(this))->aItems[n]).s1();
                (*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->left) = (*(int *)&((BrTextList *)(this))->f1A93C);
                (*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->right) = (*(int *)&((BrTextList *)(this))->f1A944);
                y = (*(int *)&((BrTextList *)(this))->f1A940) + i * 0x13;
                (*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->f428) = y;
                (*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->f430) = y + 0x12;
                (*(Item550E0 *)&((BrTextList *)(this))->aItems[n]).s3((float)(*(int *)&((BrTextList *)(this))->f1A93C), (float)y);
            }
            if ((*(int *)&((BrTextList *)(this))->f18) & 0x2000000) {
                int x = (*(int *)&((BrTextList *)(this))->f1A93C) - 0x13;
                if (BrSlotsFindById((*(int *)&((BrTextBox *)&((*(Item550E0 *)&((BrTextList *)(this))->aItems[n])))->f434)) != 0)
                    Draw(0x8b, (float)x, (float)y);
                else
                    Draw(0x8a, (float)x, (float)y);
            }
        }
    }
    if (((*(int *)&((BrTextList *)(this))->f18) & 0x200000) == 0) {
        short s = (*(short *)&((BrTextList *)(this))->f1A932);
        if (s > 0) {
            if ((*(int *)&((BrTextList *)(this))->f1A99C[0]) != 0)
                Draw(0x2f, (float)(*(int *)&((BrTextList *)(this))->f1A94C), (float)(*(int *)&((BrTextList *)(this))->f1A950));
            else
                Draw(s, (float)(*(int *)&((BrTextList *)(this))->f1A94C), (float)(*(int *)&((BrTextList *)(this))->f1A950));
        }
        s = (*(short *)&((BrTextList *)(this))->f1A934);
        if (s > 0) {
            if ((*(int *)&((BrTextList *)(this))->f1A99C[1]) != 0)
                Draw(0x2d, (float)(*(int *)&((BrTextList *)(this))->f1A95C), (float)(*(int *)&((BrTextList *)(this))->f1A960));
            else
                Draw(s, (float)(*(int *)&((BrTextList *)(this))->f1A95C), (float)(*(int *)&((BrTextList *)(this))->f1A960));
        }
        s = (*(short *)&((BrTextList *)(this))->f1A938);
        if (s > 0)
            Draw(s, (*(float *)&((BrTextList *)(this))->f1A99C[4]), (*(float *)&((BrTextList *)(this))->f1A99C[5]));
    }
    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrUiListPaint_100550E0(void *self, int sel)
{
    return ((class Ctl550E0 *)self)->Paint(sel);
}
/* end of C entry points */
