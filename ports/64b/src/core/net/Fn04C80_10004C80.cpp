/* WHAT IT DOES: build and send the small acknowledgement packet -- a tag
 * byte carrying a sequence number in its low bits, plus one value. */
/* @implements 0x10004C80 glide Fn04C80
 * @cpp_kind method
 * @cpp_symbol ?Fn04C80@@YAHPAX0@Z
 *
 * Stack-dtor (maxState=1): named local of class type, sizeof 0x214.
 * Ctor and dtor DECLARED, not defined (dtor orig is a 1-byte ret).
 * Unwind: lea ecx,[ebp-0x220]; jmp dtor. Free cdecl, not thiscall
 * (no unused-this push ecx). g_id is volatile int so the load is
 * `mov ecx,[g]` (8b 0d) not `mov cl,[g]` (8a 0d) before and cl / or cl.
 */
#define _CRTIMP __declspec(dllimport)

class Buf {
public:
    char _[0x214];
    Buf();
    ~Buf();
    void PutByte(unsigned char);
    void PutVal(unsigned);
};

typedef char chk_sz[sizeof(Buf) == 0x214 ? 1 : -1];

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* InitFn was a stand-in; the original calls C function BrNetPktStamp.  Declared under
 * its real symbol so the relocation resolves by name. */
/* BrNetPktStamp: prototype in br_funcs.h */
#define InitFn ((void (*)(Buf *))BrNetPktStamp)
/* FinishFn was a stand-in; the original calls C function BrCountedNetSend.  Declared under
 * its real symbol so the relocation resolves by name. */
/* BrCountedNetSend: prototype in br_funcs.h */
#define FinishFn ((int (*)(void *, Buf *))BrCountedNetSend)

int Fn04C80(void *a, unsigned int b)
{
    Buf obj;

    InitFn(&obj);
    obj.PutByte((unsigned char)((g_id & 0xf) | 0xd0));
    obj.PutVal(b);
    return FinishFn(a, &obj);
}

/* Methods of the local classes above that other files define: each is
 * the function at its original address, reached through its C entry. */
/* 0x1006CD80: the original calls FUN_1006cd80 by address */
inline Buf::Buf()
{
    FUN_1006cd80((int *)this);
}

/* 0x10008D60: the original calls BrPodNop by address */
inline Buf::~Buf()
{
    BrPodNop();
}

/* 0x1006CFA0: the original calls BrBitStreamWriteU8 by address */
inline void Buf::PutByte(unsigned char a1)
{
    BrBitStreamWriteU8((struct BrBitStream *)this, (unsigned int)a1);
}

/* 0x1006D000: the original calls BrBitStreamWriteU24 by address */
inline void Buf::PutVal(unsigned int a1)
{
    BrBitStreamWriteU24((struct BrBitStream *)this, (unsigned int)a1);
}
