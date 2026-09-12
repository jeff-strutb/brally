/* WHAT IT DOES: draw a string in the sprite font starting from a pen
 * position the CALLER passes in -- the same glyph walk as the widget's
 * plain draw, but the pen x and the row y come in as arguments instead of
 * the stored members. A flag can still override the pen with the centred
 * position from the vtable. */
/* @implements 0x100541B0 glide BrSprFontDrawAt_100541B0
 * @cpp_kind method
 * @cpp_symbol ?DrawAt@Text541B0@@QAEXMH@Z
 *
 * Thiscall, TWO stack args (`ret 8`), 196 B. The at-position variant of
 * 0x100540D0's font-A glyph walk: pen starts from the float argument
 * (overridden by the +0x28 vcall when f04 bit 0 is set), the row is the
 * int argument rather than f414, everything else identical -- +0x18 blit
 * per printable glyph, font-A metrics at 0x100ABE84, space steps the pen
 * by SUBTRACTING the -6.0f constant, tail +0x24 vcall gated on f420.
 *
 * `pen` is a separate local VC5 unifies with the argument's slot: the
 * else arm's `pen = x` survives as the original's redundant
 * `mov ecx,[esp+0x14]; mov [esp+0x14],ecx` self-copy. The advance temp
 * is homed in the DEAD y-argument slot once y is hoisted to ebp.
 * Same short-push / movsx-subscript idioms as the two siblings.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct Metric12 {
    short          advance;     /* +0x00 -- signed: movsx before the fild */
    unsigned short height;      /* +0x02 */
    short          sprite;      /* +0x04 */
    unsigned short f06;
    unsigned short f08;
    unsigned short f0a;
};

class Text541B0 {
public:
    virtual void  s0();
    virtual void  s1();
    virtual void  s2();
    virtual void  s3();
    virtual void  s4();
    virtual void  s5();
    virtual void  s6(short, float, int, int);   /* +0x18 blit one glyph */
    virtual void  s7();                         /* +0x1C */
    virtual void  s8();                         /* +0x20 */
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

    void DrawAt(float x, int y);
};

typedef char chk_f08[(unsigned)&((Text541B0 *)0)->f08 == 8 ? 1 : -1];
typedef char chk_sz[(unsigned)&((Text541B0 *)0)->sz == 9 ? 1 : -1];
typedef char chk_f420[(unsigned)&((Text541B0 *)0)->f420 == 0x420 ? 1 : -1];

extern "C" {
Metric12 g_BrGlyphFontA12[95];      /* 0x100ABE84 */
float    g_077674;                  /* 0x10077674 -- -6.0f */
}

void Text541B0::DrawAt(float x, int y)
{
    float pen;
    short i;
    char  c;

    if (f04 & 1)
        pen = s10();
    else
        pen = x;

    i = 0;
    c = sz[0];
    while (c != 0) {
        short g = (short)(c - 0x20);

        if (!((g >= 0 && g <= 0x7f) || c == ' '))
            break;

        if (c >= 0x21 && c <= 0x7e) {
            short sp = g_BrGlyphFontA12[g].sprite;
            if (sp != -1) {
                s6(sp, pen, y, f08);
                pen = (float)g_BrGlyphFontA12[g].advance + pen;
            }
        } else if (c == ' ') {
            pen = pen - g_077674;
        }

        i++;
        c = sz[i];
    }

    if (f420 != 0)
        s9(pen, y);
}
