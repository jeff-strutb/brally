/* WHAT IT DOES: return the size of the entry at an index, aborting if the
 * index is out of range. */
/* @implements 0x10008960 glide M8960
 * @cpp_kind method
 * @cpp_symbol ?M8960@Tbl8900@@QAEHI@Z
 *
 * Tbl8900 family: bounds-checked getter — warn printf on overflow,
 * then return items[i].f4 (76-byte entries, field at +4).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct Ent {
    int f0;
    int f4;
    int rest[17];
};

class Tbl8900 {
public:
    virtual void v0();
    virtual void v1();
    virtual int v2(int);
    int pad[3];
    unsigned count;
    int f14;
    Ent *items;
    int M8960(unsigned i);
};

typedef char chk_cnt[(unsigned)&((Tbl8900 *)0)->count == 0x10 ? 1 : -1];
typedef char chk_it[(unsigned)&((Tbl8900 *)0)->items == 0x18 ? 1 : -1];
typedef char chk_ent[sizeof(Ent) == 76 ? 1 : -1];

extern "C" {
char s_err[1];
void BrLogFatalPrintf(char *, ...);
}

int Tbl8900::M8960(unsigned i)
{
    if (i >= count)
        BrLogFatalPrintf(s_err, i);
    return items[i].f4;
}
