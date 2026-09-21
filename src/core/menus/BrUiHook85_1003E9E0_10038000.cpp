/* WHAT IT DOES: draws the music volume bar -- one "head" icon (code 0x74)
 * at the control's own position, then one repeat icon (code 0x75) per
 * counted step of the volume level, each 12 pixels further right on a
 * baseline 19 pixels below the control's y.  The level is re-read from the
 * global on every step.  Always reports success. */
/* @implements 0x10038000 glide BrUiHook85_1003E9E0
 * @cpp_symbol _BrUiHook85_1003E9E0
 *
 * Byte-for-byte the shape of BrItemDrawIconRow (0x10037FA0.cpp) over the
 * other volume level global; see that file for the vcall-imm8 and the
 * `x + i * 0xc` strength-reduction levers.  The C body in
 * src/core/menus/br_uictlhook.c is tagged at the D3D twin. */
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

extern "C" unsigned int g_brB4E70C;   /* 0x10B71A6C in BRGlide */

extern "C"
int BrUiHook85_1003E9E0(BrIconItem *pItem)
{
    int x;
    int y;
    unsigned int i;

    x = (int)pItem->x;
    y = (int)pItem->y + 0x13;
    pItem->s5(0x74, x, y);
    for (i = 0; i < g_brB4E70C; i++)
        pItem->s5(0x75, x + i * 0xc, y);
    return 1;
}
