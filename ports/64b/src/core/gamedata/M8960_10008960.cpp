/* WHAT IT DOES: return the size of the entry at an index, aborting if the
 * index is out of range. */
/* @implements 0x10008960 glide M8960
 * @cpp_kind method
 * @cpp_symbol ?M8960@Tbl8900@@QAEHI@Z
 *
 * Tbl8900 family: bounds-checked getter: warn printf on overflow,
 * then return items[i].f4 (76-byte entries, field at +4).
 */
#include "br_podarc.h"


int BrPodArcObj::Size(unsigned i)
{
    if (i >= (unsigned)cEntries)
        BrLogFatalPrintf(DAT_1007b54c, i);
    return aEntries[i].cbData;
}

extern "C" int M8960(void *self, unsigned int i)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::Size(i);
}
