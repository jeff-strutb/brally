/* WHAT IT DOES: release the input object this navigation controller holds,
 * if it still holds one. */
/* @implements 0x10059320 glide BrNavRelease_10059320
 * @cpp_kind method
 * @cpp_symbol ?Release@Nav59320@@QAEXXZ
 *
 * Thiscall receiver releasing a C-style object at +0x50: the member's
 * FUNCTION-POINTER TABLE (not a C++ vtbl — the object is pushed as a
 * stack arg, ecx carries the table) is called at slot 8 then slot 2,
 * the second through a fresh member reload, then the member is zeroed.
 * The table entries are __stdcall (no add esp after either call) —
 * COM-interface shape (slot 2 = Release).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

struct CObj;

struct CTbl {
    void (__stdcall *f0)(CObj *);
    void (__stdcall *f1)(CObj *);
    void (__stdcall *f2)(CObj *);
    void (__stdcall *f3)(CObj *);
    void (__stdcall *f4)(CObj *);
    void (__stdcall *f5)(CObj *);
    void (__stdcall *f6)(CObj *);
    void (__stdcall *f7)(CObj *);
    void (__stdcall *f8)(CObj *);
};

struct CObj {
    CTbl *tbl;
};

class Nav59320 {
public:
    char pad[0x50];
    CObj *f50;

    void Release();
};

typedef char chk_50[(unsigned)&((Nav59320 *)0)->f50 == 0x50 ? 1 : -1];

void Nav59320::Release()
{
    CObj *p;

    p = f50;
    if (p != 0) {
        p->tbl->f8(p);
        f50->tbl->f2(f50);
        f50 = 0;
    }
}
