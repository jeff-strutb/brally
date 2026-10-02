/* WHAT IT DOES: the same build-and-send for a larger event, with ten fields
 * rather than six. */
/* @implements 0x10004AD0 glide BrNetSend4AD0
 * @cpp_kind method
 * @cpp_symbol ?BrNetSend4AD0@@YAHPAXHHEEEHPADHE@Z
 *
 * Stack-dtor, maxState=1, unwind `lea ecx,[ebp-0x220]; jmp ~Pkt`.
 * Ten cdecl args. InitPkt is 0x10004C40. Ctor/dtor/methods DECLARED.
 * PutByte(unsigned char) so char args stay byte loads. volatile g_id
 * keeps the dword load. `if ((a8 & 0x3F) <= 2)` / `== 4` CSE's to
 * `mov ebp,esi; and ebp,0x3f` (copy then and); a stored `kind = a8 &
 * 0x3F` ands esi in place (5 diffs).
 */
#define _CRTIMP __declspec(dllimport)

class Pkt {
    char b[0x214];
public:
    Pkt();
    ~Pkt();
    void PutByte(unsigned char);
    void Put24(unsigned);
    void Put32(unsigned);
};

typedef char chk_pkt[sizeof(Pkt) == 0x214 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* InitPkt: prototype in br_funcs.h */
/* SendPkt was a stand-in; the original calls C function BrCountedNetSend.  Declared under
 * its real symbol so the relocation resolves by name. */
/* BrCountedNetSend: prototype in br_funcs.h */
#define SendPkt ((int (*)(void *, Pkt *))BrCountedNetSend)

int BrNetSend4AD0(void *dest, int a1, int a2, unsigned char r,
                  unsigned char g, unsigned char b, int a6,
                  char *text, int a8, unsigned char a9)
{
    Pkt pkt;
    int i;
    int seen;
    int r0;

    BrNetPktStamp(&pkt);
    pkt.PutByte((unsigned char)(g_id | a9));
    pkt.PutByte((unsigned char)a1);
    pkt.PutByte((unsigned char)a8);
    pkt.PutByte((unsigned char)a2);
    pkt.PutByte(r);
    pkt.PutByte(g);
    pkt.PutByte(b);
    pkt.Put32(a6);
    if ((a8 & 0x3F) <= 2) {
        seen = 0;
        i = 0;
        while (i < 0x18) {
            if (seen != 0)
                pkt.PutByte(0);
            else {
                pkt.PutByte((unsigned char)text[i]);
                if (text[i] == 0)
                    seen = 1;
            }
            i++;
        }
    }
    if ((a8 & 0x3F) == 4)
        pkt.Put24((*(int *)&DAT_10226a2c));
    r0 = SendPkt(dest, &pkt);
    return r0;
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006CD80: the original calls FUN_1006cd80 by address */
inline Pkt::Pkt()
{
    FUN_1006cd80((int *)this);
}

/* 0x10008D60: the original calls BrPodNop by address */
inline Pkt::~Pkt()
{
    BrPodNop();
}

/* 0x1006CFA0: the original calls BrBitStreamWriteU8 by address */
inline void Pkt::PutByte(unsigned char a1)
{
    BrBitStreamWriteU8((struct BrBitStream *)this, (unsigned int)a1);
}

/* 0x1006D000: the original calls BrBitStreamWriteU24 by address */
inline void Pkt::Put24(unsigned int a1)
{
    BrBitStreamWriteU24((struct BrBitStream *)this, (unsigned int)a1);
}

/* 0x1006D050: the original calls BrBitStreamWriteU32 by address */
inline void Pkt::Put32(unsigned int a1)
{
    BrBitStreamWriteU32((struct BrBitStream *)this, (unsigned int)a1);
}
