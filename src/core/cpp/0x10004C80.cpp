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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

void InitFn(Buf *);
int FinishFn(void *, Buf *);

int Fn04C80(void *a, void *b)
{
    Buf obj;

    InitFn(&obj);
    obj.PutByte((unsigned char)((g_id & 0xf) | 0xd0));
    obj.PutVal((unsigned)b);
    return FinishFn(a, &obj);
}
