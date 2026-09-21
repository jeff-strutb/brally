/* WHAT IT DOES: build and send one network packet describing a game event --
 * fills a stack packet buffer with a tag and its fields, then hands it to
 * the sender. The packet object's destructor is what releases the buffer,
 * including if the send throws. */
/* @implements 0x10004900 glide BrNetSend4900
 * @cpp_kind method
 * @cpp_symbol ?BrNetSend4900@@YAHPAXHHHHPADE@Z
 *
 * Stack-dtor: named local `Pkt pkt` (sizeof 0x214). Ctor/dtor/methods
 * DECLARED, not defined. Unwind `lea ecx,[ebp-0x220]; jmp ~Pkt`.
 * Free cdecl. PutByte takes unsigned char so flags stays a byte load
 * (`mov bl,[esp+0x248]`), not a dword. volatile g_id keeps `a1`.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Pkt {
    char b[0x214];
public:
    Pkt();
    ~Pkt();
    void Reset();
    void PutByte(unsigned char);
    void Put24(unsigned);
};

typedef char chk_pkt[sizeof(Pkt) == 0x214 ? 1 : -1];

volatile int g_id;
unsigned char g_226E7C;

int SendPkt(void *, Pkt *);

int BrNetSend4900(void *dest, int a1, int a2, int a3, int a4,
                  char *name, unsigned char flags)
{
    Pkt pkt;
    int i;
    int seen;
    int r;

    pkt.Reset();
    pkt.Put24(0);
    pkt.PutByte((unsigned char)((g_id & 0xf) | flags | 0xE0));
    pkt.PutByte((unsigned char)a1);
    pkt.PutByte(g_226E7C);
    pkt.PutByte((unsigned char)a2);
    pkt.PutByte((unsigned char)a3);
    pkt.PutByte((unsigned char)a4);
    seen = 0;
    i = 0;
    while (i < 0x18) {
        if (seen != 0)
            pkt.PutByte(0);
        else {
            pkt.PutByte((unsigned char)name[i]);
            if (name[i] == 0)
                seen = 1;
        }
        i++;
    }
    r = SendPkt(dest, &pkt);
    return r;
}
