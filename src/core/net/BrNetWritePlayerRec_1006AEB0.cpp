/* WHAT IT DOES: write one player record onto an outgoing bitstream if it
 * still fits in 256 bytes: six field bytes, a 32-bit id, a 24-byte name when
 * the type is 0..2, and a 24-bit extra when the type is 4. Returns 0 if the
 * record would not fit. */
/* @implements 0x1006AEB0 glide BrNetWritePlayerRec
 * @cpp_symbol _BrNetWritePlayerRec
 *
 * The C transcription (src/core/net/br_netpkt.c, same VA) was complete; its
 * wall was the name-byte write homing to a slot before the push, which only
 * a BYTE-typed thiscall parameter avoids (see 0x1006B080.cpp).  The six raw
 * parameter bytes are forwarded as whole dwords out of their slots
 * (`mov eax,[esp+N]; push eax`) -- narrow parameter to narrow parameter.
 *
 * Two more levers took it from 231/86 to byte-exact (234 B / 87 insns):
 *  - NO named `type` local: `(int)(flags & 0x3f)` is spelled at each of its
 *    four uses and VC5 CSEs it into a temp.  The CSE temp is what the
 *    original has -- computed into ebx, stored at once into the SPENT pBs
 *    argument slot ([esp+0x14]), and reloaded after the name loop -- which
 *    frees ebp for the loop's seen-null flag.  A named `int type` local
 *    instead keeps ebp for itself and homes the flag (every declaration
 *    order, block scope, `unsigned`, `char`/`bool` flag and `for` shape
 *    probed: inert).
 *  - The size guard is written positively (`if (n <= 0x100) { ...; return
 *    1; } return 0;`) so the failure exit is the function's tail, as in
 *    BrNetWriteTagC0 (0x1006AFA0.cpp). */
#include <stdint.h>

class BrBitStream {
public:
    int  CountedTotal();                 /* 0x1006D180 */
    void WriteU8(unsigned char v);       /* 0x1006CFA0 */
    void WriteU24(unsigned int v);       /* 0x1006D000 */
    void WriteU32(unsigned int v);       /* 0x1006D050 */
};

extern "C" unsigned int DAT_1184c074;

extern "C"
int BrNetWritePlayerRec(BrBitStream *pBs, unsigned char a, unsigned int flags,
                        unsigned char b, unsigned char c, unsigned char d,
                        unsigned char e, char *pszName, unsigned int id)
{
    int n;
    int i;
    int done;

    n = pBs->CountedTotal() + 10;
    if ((int)(flags & 0x3f) <= 2)
        n += 0x18;
    if ((int)(flags & 0x3f) == 4)
        n += 3;
    if (n <= 0x100) {
        pBs->WriteU8(a);
        pBs->WriteU8((unsigned char)flags);
        pBs->WriteU8(b);
        pBs->WriteU8(c);
        pBs->WriteU8(d);
        pBs->WriteU8(e);
        pBs->WriteU32(id);

        if ((int)(flags & 0x3f) <= 2) {
            done = 0;
            i = 0;
            do {
                if (done) {
                    pBs->WriteU8(0);
                } else {
                    pBs->WriteU8((unsigned char)pszName[i]);
                    if (pszName[i] == 0)
                        done = 1;
                }
                i++;
            } while (i < 0x18);
        }
        if ((int)(flags & 0x3f) == 4)
            pBs->WriteU24(DAT_1184c074);
        return 1;
    }
    return 0;
}
