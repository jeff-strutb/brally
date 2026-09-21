/* WHAT IT DOES: look one entry up in the archive's table of contents by key,
 * aborting with a message if it is not there. The callers treat a missing
 * entry as unrecoverable. */
/* @implements 0x10008930 glide M8930
 * @cpp_kind method
 * @cpp_symbol ?M8930@Tbl8900@@QAEHH@Z
 *
 * Container-class method family (0x10008930..0x10008A30): thiscall
 * with stack args (`ret 4`), slot-2 vcall on self with the vtbl read
 * before the arg pushes (`mov eax,[ecx]` at +0 — C++ member-call
 * order), error printf through 0x10008EC0 on -1. No EH.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class Tbl8900 {
public:
    virtual void v0();
    virtual void v1();
    virtual int v2(int);
    int M8930(int a);
};

extern "C" {
char s_err[1];
void BrLogFatalPrintf(char *, ...);
}

int Tbl8900::M8930(int a)
{
    int r;

    r = v2(a);
    if (r == -1) {
        BrLogFatalPrintf(s_err, a);
        return -1;
    }
    return r;
}
