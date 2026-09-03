/* @implements 0x100540D0 glide BrSprFontDraw_1005B2B0
 * @cpp_kind method
 * @cpp_symbol ?Draw@Text540D0@@QAEXXZ
 *
 * Thiscall, no args (`ret`), 212 B. The glyph walk of the 0x438-byte text
 * widget: pick the starting pen x (vtable +0x28 when f04 bit 0 is set,
 * else the f410 member), then walk sz[] one char at a time, blitting each
 * printable glyph through the +0x18 vcall and advancing the pen by the
 * 12-byte metric's `advance`; a space with no glyph advances by the
 * -6.0f constant SUBTRACTED (see br_sprfont.h). Tail fires the +0x24
 * vcall with the final pen when f420 is set.
 *
 * The draw gates on the metric's `sprite` (+4), the measurers gate on
 * `advance`/`height` -- that disagreement is the original's, kept.
 *
 * Idioms: the glyph index is a `short` throughout, so the sprite reaches
 * the vcall via the short-push (`mov ax,[m]; push eax`, upper half left
 * holding the sign-extended index) and the loop counter is a short with
 * `movsx eax,bx` on every subscript. The metric's `advance` is SIGNED --
 * as unsigned it becomes `xor edx,edx; mov dx,[m]` (+2 B), which pushed
 * the loop back-edge past -0x80 and turned the `jne` near (+4 B): those
 * six bytes were the whole 83-diff first draft.
 *
 * PARKED at 1 diff, +0xAA -- the loop's char read. Orig encodes it
 * `8a 44 07 09` (SIB base=edi/this, index=eax/i); recomp emits
 * `8a 44 38 09`, the same effective address with base and index swapped.
 * Registers and instruction are identical; only the SIB operand order
 * differs, so this is an emitter choice, not a source shape.
 * DO NOT RE-PROBE -- all of these leave it unchanged:
 *   subscript spelling: sz[i], *(sz+i), *(i+sz), i[sz], sz[(int)i],
 *     *((char*)this+i+9), ((char*)this)[i+9], *(((char*)this+i)+9)
 *   loop shape: bottom-read while, top-read for(;;)+break,
 *     while ((c = sz[i]) != 0), c = sz[++i]
 *   index temp: hoisted `char *psz = sz`, explicit `int j = i`
 *   flags: /O2, /Ox, /O2 /Op (71), /O2 /Ob1, /O2 /Gy, /O1 (163), /Od, /O2 /Oy-
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct Metric12 {
    short          advance;     /* +0x00 -- signed: the pen advance is movsx'd */
    unsigned short height;      /* +0x02 */
    short          sprite;      /* +0x04 */
    unsigned short f06;
    unsigned short f08;
    unsigned short f0a;
};

class Text540D0 {
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

    void Draw();
};

typedef char chk_f08[(unsigned)&((Text540D0 *)0)->f08 == 8 ? 1 : -1];
typedef char chk_sz[(unsigned)&((Text540D0 *)0)->sz == 9 ? 1 : -1];
typedef char chk_f410[(unsigned)&((Text540D0 *)0)->f410 == 0x410 ? 1 : -1];
typedef char chk_f420[(unsigned)&((Text540D0 *)0)->f420 == 0x420 ? 1 : -1];

extern "C" {
Metric12 g_BrGlyphFontA12[95];      /* 0x100ABE84 */
float    g_077674;                  /* 0x10077674 -- -6.0f */
}

void Text540D0::Draw()
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

        if (c >= 0x21 && c <= 0x7e) {
            short sp = g_BrGlyphFontA12[g].sprite;
            if (sp != -1) {
                s6(sp, pen, f414, f08);
                pen = (float)g_BrGlyphFontA12[g].advance + pen;
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
