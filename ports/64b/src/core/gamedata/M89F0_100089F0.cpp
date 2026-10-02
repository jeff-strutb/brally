/* WHAT IT DOES: allocate a buffer the size of one archive entry, remember it
 * against that entry, and return it. */
/* @implements 0x100089F0 glide M89F0
 * @cpp_kind method
 * @cpp_symbol ?M89F0@Tbl8900@@QAEHI@Z
 *
 * Tbl8900 family: bounds warn, p = alloc(v4(i)), v5(i, p), return p.
 * Vtbl cached in edi across both vcalls.
 */
#include "br_podarc.h"

extern "C" char DAT_1007b584[1];

void *BrPodArcObj::ReadNew(unsigned i)
{
    void *p;

    if (i >= (unsigned)cEntries)
        BrLogFatalPrintf(DAT_1007b584, i);
    p = BrOperatorNew(Size(i));
    Read(i, p);
    return p;
}

extern "C" void *M89F0(void *self, unsigned int i)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::ReadNew(i);
}
