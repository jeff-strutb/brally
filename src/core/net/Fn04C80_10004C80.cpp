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

volatile int g_id;

/* InitFn was a stand-in; the original calls C function BrNetPktStamp.  Declared under
 * its real symbol so the relocation resolves by name. */
extern "C" void BrNetPktStamp(void);
#define InitFn ((void (*)(Buf *))BrNetPktStamp)
/* FinishFn was a stand-in; the original calls C function BrCountedNetSend.  Declared under
 * its real symbol so the relocation resolves by name. */
extern "C" void BrCountedNetSend(void);
#define FinishFn ((int (*)(void *, Buf *))BrCountedNetSend)

int Fn04C80(void *a, void *b)
{
    Buf obj;

    InitFn(&obj);
    obj.PutByte((unsigned char)((g_id & 0xf) | 0xd0));
    obj.PutVal((unsigned)b);
    return FinishFn(a, &obj);
}
