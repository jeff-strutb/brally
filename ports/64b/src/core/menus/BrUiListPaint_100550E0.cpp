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

    if (w1a936 >= 0)
        Draw(w1a936, f24, f28);

    if ((i18 & 0x1000000) == 0 && sel >= 0 && (aItem[sel].f004 & 0x10) == 0)
        aItem[sel].b008 = 4;

    for (i = 0; i < wVisible; i++) {
        int n = i + w1a92e;
        if (n >= 0 && n < wCount) {
            if (aItem[n].szName != 0) {
                aItem[n].s1();
                aItem[n].x0 = x1a93c;
                aItem[n].x1 = x1a944;
                y = y1a940 + i * 0x13;
                aItem[n].y0 = y;
                aItem[n].y1 = y + 0x12;
                aItem[n].s3((float)x1a93c, (float)y);
            }
            if (i18 & 0x2000000) {
                int x = x1a93c - 0x13;
                if (BrSlotsFindById(aItem[n].f434) != 0)
                    Draw(0x8b, (float)x, (float)y);
                else
                    Draw(0x8a, (float)x, (float)y);
            }
        }
    }
    if ((i18 & 0x200000) == 0) {
        short s = w1a932;
        if (s > 0) {
            if (f1a99c != 0)
                Draw(0x2f, (float)x1a94c, (float)y1a950);
            else
                Draw(s, (float)x1a94c, (float)y1a950);
        }
        s = w1a934;
        if (s > 0) {
            if (f1a9a0 != 0)
                Draw(0x2d, (float)x1a95c, (float)y1a960);
            else
                Draw(s, (float)x1a95c, (float)y1a960);
        }
        s = w1a938;
        if (s > 0)
            Draw(s, f1a9ac, f1a9b0);
    }
    return 1;
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" int BrUiListPaint_100550E0(void *self, int sel)
{
    return ((class Ctl550E0 *)self)->Paint(sel);
}
/* end of C entry points */
