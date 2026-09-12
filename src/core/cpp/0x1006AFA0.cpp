/* WHAT IT DOES: appends a small tagged field to an outgoing packet if six
 * more bytes still fit in the 256-byte buffer: a tag byte (the field kind
 * OR'd with 0xC0), a 24-bit value and a 16-bit value.  Writes nothing and
 * reports failure when the field would not fit, so a half-written field can
 * never go out. */
/* @implements 0x1006AFA0 glide BrNetWriteTagC0
 * @cpp_symbol _BrNetWriteTagC0
 *
 * Same wall as BrNetWriteTag20 (0x1006B080.cpp): the tag byte is pushed
 * with eax's upper bytes dirty, which only a BYTE-typed thiscall parameter
 * produces.  The 16-bit argument is loaded as a whole dword out of its
 * parameter slot and pushed -- the forwarding of a narrow parameter to a
 * narrow parameter. */
#include <stdint.h>

class BrBitStream {
public:
    int  CountedTotal();                 /* 0x1006D180 */
    void WriteU8(unsigned char v);       /* 0x1006CFA0 */
    void WriteU16(unsigned short v);     /* 0x1006CFC0 */
    void WriteU24(unsigned int v);       /* 0x1006D000 */
};

extern "C"
int BrNetWriteTagC0(BrBitStream *pBs, unsigned char kind, unsigned int a,
                    unsigned short b)
{
    if (pBs->CountedTotal() + 6 <= 0x100) {
        pBs->WriteU8((unsigned char)(kind | 0xc0));
        pBs->WriteU24(a);
        pBs->WriteU16(b);
        return 1;
    }
    return 0;
}
