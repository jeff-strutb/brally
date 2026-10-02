/* WHAT IT DOES: attach an already-allocated buffer to an archive entry, so
 * the entry knows where its data lives. */
/* @implements 0x10008A30 glide M8A30
 * @cpp_kind method
 * @cpp_symbol ?M8A30@Tbl8900@@QAEHIH@Z
 *
 * Tbl8900 family: bounds warn, slot-5 self-vcall (i, x), return x.
 * Vtbl cached across the call (C++ member-call order).
 */
#include "br_podarc.h"

extern "C" char DAT_1007b584[1];

void *BrPodArcObj::ReadInto(unsigned i, void *dst)
{
    if (i >= (unsigned)cEntries)
        BrLogFatalPrintf(DAT_1007b584, i);
    Read(i, dst);
    return dst;
}

extern "C" void *M8A30(void *self, unsigned int i, void *dst)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::ReadInto(i, dst);
}
