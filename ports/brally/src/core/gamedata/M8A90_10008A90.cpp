/* WHAT IT DOES: the same lookup-then-handle pattern over two arguments. */
/* @implements 0x10008A90 glide M8A90
 * @cpp_kind method
 * @cpp_symbol ?M8A90@Vt8A90@@QAEHHH@Z
 *
 * Twin of 0x10008A70: return v7(v3(a, b)); vtbl cached in edi.
 */
#include "br_podarc.h"

/* The original pushes (name, dst) for slot 3, which takes one argument and
 * pops one, so dst is still on the stack as slot 7's second argument:
 * ReadInto(Lookup(name), dst). */
void *BrPodArcObj::LoadInto(const char *name, void *dst)
{
    return ReadInto(Lookup(name), dst);
}

extern "C" void *M8A90(void *self, const char *name, void *dst)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::LoadInto(name, dst);
}
