/* @implements 0x10008990 glide M8990
 * @cpp_kind method
 * @cpp_symbol ?M8990@Tbl8900@@QAEXIPAX@Z
 *
 * Tbl8900 family: bounds warn, fseek(fFile, items[i].f0, SEEK_SET),
 * then sub.Read(fFile, dst, items[i].f4) — thiscall on the member
 * object at +4 (0x10008E60).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>

struct Ent {
    int f0;
    int f4;
    int rest[17];
};

class Sub8E60 {
public:
    int d0;
    int d4;
    int d8;
    void Read(FILE *f, void *dst, int n);
};

class Tbl8900 {
public:
    virtual void v0();
    virtual void v1();
    virtual int v2(int);
    Sub8E60 sub;
    unsigned count;
    int f14;
    Ent *items;
    FILE *fFile;
    void M8990(unsigned i, void *dst);
};

typedef char chk_sub[(unsigned)&((Tbl8900 *)0)->sub == 4 ? 1 : -1];
typedef char chk_cnt[(unsigned)&((Tbl8900 *)0)->count == 0x10 ? 1 : -1];
typedef char chk_it[(unsigned)&((Tbl8900 *)0)->items == 0x18 ? 1 : -1];
typedef char chk_f[(unsigned)&((Tbl8900 *)0)->fFile == 0x1C ? 1 : -1];
typedef char chk_ent[sizeof(Ent) == 76 ? 1 : -1];

extern "C" {
char s_err[1];
void BrLogFatalPrintf(char *, ...);
}

void Tbl8900::M8990(unsigned i, void *dst)
{
    Ent *e;

    if (i >= count)
        BrLogFatalPrintf(s_err, i);
    e = &items[i];
    fseek(fFile, e->f0, 0);
    sub.Read(fFile, dst, e->f4);
}
