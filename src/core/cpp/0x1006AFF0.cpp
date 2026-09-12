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
extern unsigned char  DAT_1021cdf8;
extern unsigned char  DAT_100b3014;
extern unsigned char  DAT_10226e80;
extern unsigned short DAT_1021ce50;
extern unsigned char  DAT_1021cdb0;
extern unsigned char  DAT_10226a40;
extern unsigned char  DAT_10226a3c;
}

extern "C"
int BrNetWriteRaceOpts(BrBitStream *pBs, unsigned char kind)
{
    if (pBs->CountedTotal() + 9 <= 0x100) {
        pBs->WriteU8((unsigned char)(kind | 0xe0));
        pBs->WriteU8(DAT_1021cdf8);
        pBs->WriteU8(DAT_100b3014);
        pBs->WriteU8(DAT_10226e80);
        pBs->WriteU16(DAT_1021ce50);
        pBs->WriteU8(DAT_1021cdb0);
        pBs->WriteU8(DAT_10226a40);
        pBs->WriteU8(DAT_10226a3c);
        return 1;
    }
    return 0;
}
