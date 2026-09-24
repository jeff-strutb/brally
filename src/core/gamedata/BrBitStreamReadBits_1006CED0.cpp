/* WHAT IT DOES: pull the next n bits out of a packed bit stream and return
 * them, most significant first, advancing the read position a byte at a time
 * as each byte is used up. The primitive underneath every compressed format
 * the game reads. */
/* @implements 0x1006CED0 glide BrBitStreamReadBits
 * @cpp_symbol ?ReadBits@BrBitStream@@QAEIH@Z
 *
 * A __thiscall method (`this` in ecx, `ret 4`): the C twin in
 * br_bitstream.c, a __fastcall with a struct argument, gets the ABI right but
 * not the register allocation -- only the C++ front end reproduces it.
 *
 * Two source facts carry the last bytes:
 *   - the buffer pointer is copied into a local (`buf`) inside the loop.  That
 *     is what makes the loaded pointer, not the index, the SIB base of the
 *     `movsx ecx, byte ptr [ecx+ebx]` read (`pBuf[readByte]` spelled any
 *     other way gives [ebx+ecx]);
 *   - the bit cursor is bumped inside the test (`(readBit += take) >= 8`),
 *     which keeps the new value in a register for the compare.
 * `consumed` is dead but real: the original keeps it in its one stack slot. */

class BrBitStream {
public:
    int   readBit;      /* +0x00 */
    int   readByte;     /* +0x04 */
    int   writeBit;     /* +0x08 */
    int   writeByte;    /* +0x0C */
    char *pBuf;         /* +0x10 */

    unsigned int ReadBits(int n);
};

unsigned int BrBitStream::ReadBits(int n)
{
    unsigned int acc = 0;
    int consumed = 0;

    while (n != 0) {
        int take = 8 - readBit;
        int shift;
        char *buf = pBuf;

        if (take > n) {
            shift = take - n;
            take = n;
        } else {
            shift = 0;
        }
        acc = (acc << take) | ((((1u << take) - 1) << shift & buf[readByte]) >> shift);
        consumed += take;
        if ((readBit += take) >= 8) {
            readByte++;
            readBit = 0;
        }
        n -= take;
    }
    return acc;
}
