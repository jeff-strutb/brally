#include "br_phase.h"   /* BrPhase_, the canonical record */
/* WHAT IT DOES: destroy a page object, releasing the two sub-objects it
 * owns. The compiler-emitted destructor. */
/* @implements 0x10041CC0 glide BrPhaseDtor_10048870
 * @cpp_kind dtor
 * @cpp_symbol ??1Phase32@@QAE@XZ
 *
 * 63 B. Plain (non-deleting) dtor: vtbl reset to 0x100776C8, then
 * `delete` of the two owned members at +0xC0/+0xC4 (slot-0 scalar-deleting
 * vcalls with flag 1), each pointer zeroed after. No EH frame.
 */
#include "br_vtables.h"

/* The phase destructor: vtable back to the phase's (0x100776C8), then the
 * two owned name lists deleted (slot 0, flag 1) and their pointers cleared. */
extern "C" void BrPhaseDtor_10048870(void *self)
{
    BrPhase_ *ph = (BrPhase_ *)self;

    ph->pVtbl = (const BrPhaseVtbl_ *)g_brVtbl_100776C8;
    br_vdelete(ph->fC0);
    ph->fC0 = 0;
    br_vdelete(ph->fC4);
    ph->fC4 = 0;
}
