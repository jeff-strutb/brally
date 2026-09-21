/* WHAT IT DOES: allocate a buffer the size of one archive entry, remember it
 * against that entry, and return it. */
/* @implements 0x100089F0 glide M89F0
 * @cpp_kind method
 * @cpp_symbol ?M89F0@Tbl8900@@QAEHI@Z
 *
 * Tbl8900 family: bounds warn, p = alloc(v4(i)), v5(i, p), return p.
 * Vtbl cached in edi across both vcalls.
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
    int M89F0(unsigned i);
};

typedef char chk_cnt[(unsigned)&((Tbl8900 *)0)->count == 0x10 ? 1 : -1];

extern "C" {
char s_err[1];
void BrLogFatalPrintf(char *, ...);
int BrAlloc74572(int);
}

int Tbl8900::M89F0(unsigned i)
{
    int p;

    if (i >= count)
        BrLogFatalPrintf(s_err, i);
    p = BrAlloc74572(v4(i));
    v5(i, p);
    return p;
}
