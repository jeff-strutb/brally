/* WHAT IT DOES: look a value up and pass the result straight to the handler
 * for it -- a two-step lookup written as one call. */
/* @implements 0x10008A70 glide BrVt8A70CallPair
 * @cpp_kind method
 * @cpp_symbol ?CallPair@Vt8A70@@QAEXH@Z
 *
 * 25 B thiscall, one stack arg. Nested vcall pair: slot 3 transforms the
 * argument, slot 9 consumes the result. C++ pushes the inner result
 * before reloading ecx -- the C fastcall twin cannot order it that way.
 */
#include "br_podarc.h"

void *BrPodArcObj::LoadNew(const char *name)
{
    return ReadNew(Lookup(name));
}

extern "C" void *BrVt8A70CallPair(void *self, const char *name)
{
    return ((BrPodArcObj *)self)->BrPodArcObj::LoadNew(name);
}
