/* @implements 0x10008A30 glide M8A30
 * @cpp_kind method
 * @cpp_symbol ?M8A30@Tbl8900@@QAEHIH@Z
 *
 * Tbl8900 family: bounds warn, slot-5 self-vcall (i, x), return x.
 * Vtbl cached across the call (C++ member-call order).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Tbl8900 {
public:
    virtual void v0();
    virtual void v1();
    virtual int v2(int);
    virtual void v3();
    virtual int v4(int);
    virtual void v5(int, int);
    int pad[3];
    unsigned count;
    int M8A30(unsigned i, int x);
};

typedef char chk_cnt[(unsigned)&((Tbl8900 *)0)->count == 0x10 ? 1 : -1];

extern "C" {
char s_err[1];
void BrLogFatalPrintf(char *, ...);
}

int Tbl8900::M8A30(unsigned i, int x)
{
    if (i >= count)
        BrLogFatalPrintf(s_err, i);
    v5(i, x);
    return x;
}
