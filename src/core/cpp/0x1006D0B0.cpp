/* WHAT IT DOES: write a number into a packed bit stream n bits at a time,
 * splitting across byte boundaries as needed. The write side of the bit
 * stream reader. */
/* @implements 0x1006D0B0 glide BrBitStreamWriteBits_1006D0B0
 * @cpp_kind method
 * @cpp_symbol ?WriteBits@BitStream6D0B0@@QAEXII@Z
 *
 * Thiscall, two stack args (`ret 8`), 164 B. Write the low `nbits` bits
 * of `value` into the stream MSB-first, one partial byte per iteration:
 * take as many bits as the current byte has room for, extract that field
 * from the top of what is left of `value`, shift it down to where it
 * lands in the byte, and merge it under a mask of the bits already
 * written. Advancing past bit 7 resets the bit cursor and steps the byte
 * index. The same BrBitStream shape slice1_09.c uses: +0x08 write bit,
 * +0x0C write byte, +0x10 buffer.
 *
 * The zero-length guard sits before edi/ebx are pushed, so the early exit
 * pops only two registers -- write it as a plain `while`, VC5 rotates it.
 * `bit` and `8 - bit` are each read/computed twice inside the loop
 * (once for the room test, once for the keep mask); caching either in a
 * local removes one of the `mov ecx,[esi+8]`s.
 *
 * NARROWING IS THE WHOLE FUNCTION. The store is a byte, so VC5 pushes the
 * truncation back up the expression as far as it legally can, and where it
 * stops decides whether each mask is built in 8-bit or 32-bit registers.
 * The original builds BOTH masks 32-bit and narrows only at the final
 * `shl bl,cl` / `and cl,al` / `or bl,cl`. Getting that took three separate
 * dams, each worth 30+ diffs:
 *   - no `(unsigned char)` cast inside the expression; one cast on the
 *     whole `(field << sh) | (*p & keep)` result. An inner cast lets the
 *     narrowing reach the field mask, which then builds in `bl`.
 *   - the byte pointer in its own `unsigned char *p` local. Writing
 *     `pBuf[byteIdx]` on both sides re-associates the field extraction
 *     into `(value >> nbits) & mask` and costs the 32-bit field mask.
 *   - `keep` in its own `unsigned int` local. Inline, the narrowing runs
 *     into the keep mask and builds THAT in `bl` instead.
 * Locals are otherwise scarce on purpose: a fourth one (holding the
 * extracted field) makes VC5 set up an ebp frame, and the original is
 * frameless with ebp as a general register and one spill slot for `sh`.
 *
 * PARKED at 40 diffs, T3a register pairing: `n` and the byte pointer are
 * swapped between edi and edx (orig n=edi p=edx, recomp n=edx p=edi), and
 * recomp hoists the byteIdx load a few instructions earlier. Register-blind
 * the bodies are the same. DO NOT RE-PROBE -- declaring p before keep (42),
 * swapping the n/sh declarations (40), and `&pBuf[byteIdx]` (40) all leave
 * the pairing alone.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BitStream6D0B0 {
public:
    char           pad[8];
    int            bit;         /* +0x08 -- bits used in the current byte */
    int            byteIdx;     /* +0x0C */
    unsigned char *pBuf;        /* +0x10 */

    void WriteBits(unsigned int value, unsigned int nbits);
};

typedef char chk_bit[(unsigned)&((BitStream6D0B0 *)0)->bit == 8 ? 1 : -1];
typedef char chk_byteIdx[(unsigned)&((BitStream6D0B0 *)0)->byteIdx == 0xC ? 1 : -1];
typedef char chk_pBuf[(unsigned)&((BitStream6D0B0 *)0)->pBuf == 0x10 ? 1 : -1];

void BitStream6D0B0::WriteBits(unsigned int value, unsigned int nbits)
{
    while (nbits != 0) {
        unsigned int n;
        unsigned int sh;

        if (8 - bit > (int)nbits) {
            sh = 8 - bit - nbits;
            n  = nbits;
        } else {
            sh = 0;
            n  = 8 - bit;
        }

        nbits -= n;

        {
            unsigned int   keep = ((1 << bit) - 1) << (8 - bit);
            unsigned char *p    = pBuf + byteIdx;

            *p = (unsigned char)
                ((((value & (((1 << n) - 1) << nbits)) >> nbits) << sh)
                 | (*p & keep));
        }

        bit += n;
        if (bit >= 8) {
            bit = 0;
            byteIdx++;
        }
    }
}
