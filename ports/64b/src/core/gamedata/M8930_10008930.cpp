/* WHAT IT DOES: look one entry up in the archive's table of contents by key,
 * aborting with a message if it is not there. The callers treat a missing
 * entry as unrecoverable. */
/* @implements 0x10008930 glide M8930
 * @cpp_kind method
 * @cpp_symbol ?M8930@Tbl8900@@QAEHH@Z
 *
 * Container-class method family (0x10008930..0x10008A30): thiscall
 * with stack args (`ret 4`), slot-2 vcall on self with the vtbl read
 * before the arg pushes (`mov eax,[ecx]` at +0: C++ member-call
 * order), error printf through 0x10008EC0 on -1. No EH.
 */
#include "br_podarc.h"

extern "C" char DAT_1007b52c[1];   /* "%s not found in pod" style message */

int BrPodArcObj::Lookup(const char *name)
{
    int r = Find(name);
    if (r == -1) {
        BrLogFatalPrintf(DAT_1007b52c, name);
        return -1;
    }
    return r;
}

extern "C" int M8930(void *self, const char *name)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::Lookup(name);
}
