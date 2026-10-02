/* WHAT IT DOES: appends the race-options field to an outgoing network packet
 * -- a tag byte, then the handful of settings every player has to agree on
 * before a race can start. Like its siblings it writes nothing and reports
 * failure when the packet has no room for the whole field, so a half-written
 * option block can never go out. */
/* @implements 0x1006AFF0 glide BrNetWriteRaceOpts
 * @cpp_symbol _BrNetWriteRaceOpts
 *
 * Eight byte-writer calls, each pushing a byte global with the register's
 * upper bytes dirty (`mov cl,[g]; push ecx`) -- the BYTE-typed thiscall
 * parameter wall of BrNetWriteTag20 (0x1006B080.cpp), eight times over.
 * The one 16-bit setting goes through the short writer the same way
 * (`mov cx,[g]; push ecx`). */
#include <stdint.h>

class BrBitStream {
public:
    int  CountedTotal();                 /* 0x1006D180 */
    void WriteU8(unsigned char v);       /* 0x1006CFA0 */
    void WriteU16(unsigned short v);     /* 0x1006CFC0 */
};

extern "C" {
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
}

extern "C"
int BrNetWriteRaceOpts(BrBitStream *pBs, unsigned char kind)
{
    if (pBs->CountedTotal() + 9 <= 0x100) {
        pBs->WriteU8((unsigned char)(kind | 0xe0));
        pBs->WriteU8((*(unsigned char *)&g_brCfgPlayers));
        pBs->WriteU8((*(unsigned char *)&g_Br0B380C));
        pBs->WriteU8((*(unsigned char *)&g_226e80));
        pBs->WriteU16((*(unsigned short *)&DAT_1021ce50));
        pBs->WriteU8((*(unsigned char *)&DAT_1021cdb0));
        pBs->WriteU8((*(unsigned char *)&DAT_10226a40));
        pBs->WriteU8((*(unsigned char *)&DAT_10226a3c));
        return 1;
    }
    return 0;
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006D180: the original calls BrCountedTotal by address */
int BrBitStream::CountedTotal()
{
    return (int)BrCountedTotal((const struct BrCounted *)this);
}

/* 0x1006CFA0: the original calls BrBitStreamWriteU8 by address */
void BrBitStream::WriteU8(unsigned char a1)
{
    BrBitStreamWriteU8((struct BrBitStream *)this, (unsigned int)a1);
}

/* 0x1006CFC0: the original calls BrBitStreamWriteU16 by address */
void BrBitStream::WriteU16(unsigned short a1)
{
    BrBitStreamWriteU16((struct BrBitStream *)this, (unsigned short)a1);
}
