/* WHAT IT DOES: read one archive entry into the caller's buffer -- seeks to
 * its offset and reads its recorded length. */
/* @implements 0x10008990 glide M8990
 * @cpp_kind method
 * @cpp_symbol ?M8990@Tbl8900@@QAEXIPAX@Z
 *
 * Tbl8900 family: bounds warn, fseek(fFile, items[i].f0, SEEK_SET),
 * then sub.Read(fFile, dst, items[i].f4): thiscall on the member
 * object at +4 (0x10008E60).
 */
#include <stdio.h>
#include "br_podarc.h"

extern "C" char DAT_1007b56c[1];

void BrPodArcObj::Read(unsigned i, void *dst)
{
    BrPodArcEntry *e;

    if (i >= (unsigned)cEntries)
        BrLogFatalPrintf(DAT_1007b56c, i);
    e = &aEntries[i];
    fseek(pFile, e->offData, 0);
    BrFileReadChecked(pFile, dst, e->cbData);
}

extern "C" void M8990(void *self, unsigned int i, void *dst)
{
    ((BrPodArcObj *)self)->BrPodArcObj::Read(i, dst);
}
