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
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

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

volatile int g_id;
int g_226A2C;

void InitPkt(Pkt *);
int SendPkt(void *, Pkt *);

int BrNetSend4AD0(void *dest, int a1, int a2, unsigned char r,
                  unsigned char g, unsigned char b, int a6,
                  char *text, int a8, unsigned char a9)
{
    Pkt pkt;
    int i;
    int seen;
    int r0;

    InitPkt(&pkt);
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
        pkt.Put24(g_226A2C);
    r0 = SendPkt(dest, &pkt);
    return r0;
}
