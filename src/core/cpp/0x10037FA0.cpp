/* WHAT IT DOES: paints the row of little icons that shows a menu setting's
 * value -- one "head" icon (code 0x74) at the item's own position, then one
 * repeat icon (code 0x75) per counted step, each 12 pixels further right on
 * the same baseline.  The baseline sits 19 pixels below the item's y.  The
 * count is re-read from the global on every step, so the drawing call is
 * free to change it.  Always reports success. */
/* @implements 0x10037FA0 glide BrItemDrawIconRow
 * @cpp_symbol _BrItemDrawIconRow
 *
 * The C transcription (src/core/menus/br_itemiconrow.c, same VA) was
 * size-exact but pushed the icon code through a struct wrapper (`mov R,N;
 * push R`) where the original's thiscall vcall pushes an imm8 -- the
 * calling TU was C++.  Here the vtable slot +0x14 is a virtual method with
 * three int parameters; VC5 caches the slot in a stack local and re-calls
 * through it in the loop, exactly as the original does.
 *
 * The loop is `x + i * 0xc`, NOT `x += 0xc`: VC5 strength-reduces the
 * product into a running register that is created AFTER the counter's,
 * which is what puts the counter in esi and the running x in edi (the
 * original's colouring).  The incremented-local spelling creates x's web
 * first and lands esi/edi the other way round; every declaration order and
 * statement order of that spelling is inert.  Sibling: 0x10038000.cpp. */
class BrIconItem {
public:
    virtual void s0();
    virtual void s1();
    virtual void s2();
    virtual void s3();
    virtual void s4();
    virtual int  s5(int code, int x, int y);   /* slot +0x14 */
    char  p0[0x3C - 4];
    float x;      /* +0x3C */
    float y;      /* +0x40 */
};

extern "C" unsigned int g_brItemIconCount;   /* 0x10B71A68 */

extern "C"
int BrItemDrawIconRow(BrIconItem *pItem)
{
    int x;
    int y;
    unsigned int i;

    x = (int)pItem->x;
    y = (int)pItem->y + 0x13;
    pItem->s5(0x74, x, y);
    for (i = 0; i < g_brItemIconCount; i++)
        pItem->s5(0x75, x + i * 0xc, y);
    return 1;
}
