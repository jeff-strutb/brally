/* WHAT IT DOES: the same sprite-font string drawing for the SECOND typeface,
 * which has its own glyph table. */
/* @implements 0x10054280 glide BrSprFontDrawB_10054280
 * @cpp_kind method
 * @cpp_symbol ?Draw@Text54280@@QAEXXZ
 *
 * Thiscall, no args (`ret`), 219 B. The font-B twin of 0x100540D0's glyph
 * walk: same widget, same pen selection (+0x28 vcall when f04 bit 0 is
 * set, else f410), same classification, but
 *   - the printable range stops at 'z' (0x7A), not '~' -- font B only has
 *     64 real records (see slice3_39.h), and
 *   - the metrics come from the 12-byte font-B table at 0x100AC2FC, and
 *   - the blit is the +0x20 vcall, not +0x18, and
 *   - the pen advances by `advance - 6`, the integer subtraction landing
 *     before the fild.
 *
 * The `advance - 6` is 4 bytes the font-A walk does not have, which is
 * why this one's loop back-edge is a NEAR jne where 0x100540D0's is short.
 *
 * PARKED at 1 diff, the SAME residue as 0x100540D0: the loop's char read
 * encodes as `8a 44 38 09` (SIB base=i, index=this) where the original
 * has `8a 44 07 09` (base=this, index=i) -- identical effective address,
 * identical registers, opposite SIB operand order. Read the
 * do-not-re-probe list in 0x100540D0.cpp before touching this; 21 source
 * spellings across four orthogonal axes and eight flag sets leave it
 * unchanged. Both functions convert the moment that encoding is cracked.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct Metric12B {
    short          advance;     /* +0x00 -- signed: movsx before the fild */
    unsigned short height;      /* +0x02 */
    short          sprite;      /* +0x04 */
    unsigned short f06;
    unsigned short f08;
    unsigned short f0a;
};

class Text54280 {
public:
    virtual void  s0();
    virtual void  s1();
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6();                         /* +0x18 */
    virtual void  s7();                         /* +0x1C */
    virtual void  s8(short, float, int, int);   /* +0x20 blit one glyph */
    virtual void  s9(float, int);               /* +0x24 tail */
    virtual float s10();                        /* +0x28 centred pen x */

    unsigned char f04;          /* +0x04 bit 0: pen comes from the vcall */
    char          pad05[3];
    char          f08;          /* +0x08 */
    char          sz[0x407];    /* +0x09 */
    float         f410;         /* +0x410 */
    int           f414;         /* +0x414 */
    char          pad418[8];
    int           f420;         /* +0x420 */

    void Draw();
};

typedef char chk_f08b[(unsigned)&((Text54280 *)0)->f08 == 8 ? 1 : -1];
typedef char chk_szb[(unsigned)&((Text54280 *)0)->sz == 9 ? 1 : -1];
typedef char chk_f410b[(unsigned)&((Text54280 *)0)->f410 == 0x410 ? 1 : -1];
typedef char chk_f420b[(unsigned)&((Text54280 *)0)->f420 == 0x420 ? 1 : -1];

extern "C" {
Metric12B g_BrGlyphFontB12[95];     /* 0x100AC2FC */
float     g_077674;                 /* 0x10077674 -- -6.0f */
}

void Text54280::Draw()
{
    float pen;
    short i;
    char  c;

    if (f04 & 1)
        pen = s10();
    else
        pen = f410;

    i = 0;
    c = sz[0];
    while (c != 0) {
        short g = (short)(c - 0x20);

        if (!((g >= 0 && g <= 0x7f) || c == ' '))
            break;

        if (c >= 0x21 && c <= 0x7a) {
            short sp = g_BrGlyphFontB12[g].sprite;
            if (sp != -1) {
                s8(sp, pen, f414, f08);
                pen = (float)(g_BrGlyphFontB12[g].advance - 6) + pen;
            }
        } else if (c == ' ') {
            pen = pen - g_077674;
        }

        i++;
        c = sz[i];
    }

    if (f420 != 0)
        s9(pen, f414);
}
