/* WHAT IT DOES: appends a nibble-packed table field to an outgoing packet,
 * but only if nine more bytes still fit in the 256-byte buffer. It writes a
 * tag byte (the field kind OR'd with 0x20), then walks an eight-entry global
 * table writing one byte per entry -- each entry's high field in the top
 * nibble, its low field in the bottom -- and reports success. If the field
 * would not fit it writes nothing and reports failure, so a half-written
 * field can never go out. */
/* @implements 0x1006B080 glide BrNetWriteTag20
 * @cpp_symbol _BrNetWriteTag20
 *
 * The C transcription (src/core/net/br_netpkt.c, same VA) is shape-exact
 * except for the byte argument of the stream writer: the original pushes
 * `kind | 0x20` with the upper three bytes of eax still dirty, which MSVC
 * emits only when the thiscall callee's parameter is a BYTE type.  C cannot
 * spell a byte stack argument on a thiscall (a __fastcall byte rides dl; a
 * struct/union byte homes to a slot and reloads).  This TU is the calling
 * TU's shape: the stream is a class whose writer is a declared-not-defined
 * native-thiscall method taking `unsigned char`, and the body is the C
 * transcription verbatim. */
#include <stdint.h>

class BrBitStream {
public:
    int  CountedTotal();                 /* 0x1006D180, thiscall */
    void WriteU8(unsigned char v);       /* 0x1006CFA0, thiscall, byte stack arg */
};

extern "C" unsigned char DAT_11849e68[];   /* 0x11849E68: eight 8-byte entries */

extern "C"
int BrNetWriteTag20(BrBitStream *pBs, unsigned char kind)
{
    unsigned char *p;

    if (pBs->CountedTotal() + 9 <= 0x100) {
        pBs->WriteU8((unsigned char)(kind | 0x20));
        p = DAT_11849e68;
        do {
            pBs->WriteU8((unsigned char)((p[4] << 4) | p[0]));
            p += 8;
        } while ((int)p < (int)&DAT_11849e68[0x40]);
        return 1;
    }
    return 0;
}
