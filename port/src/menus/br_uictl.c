/* br_uictl.c -- BrUiCtlCtor, the control constructor at 0x100476C0.
 *
 * WHY THIS ONE, AND WHY NOW
 *
 * The host harness (port/host/brally.c) booted the phase and ran a real screen
 * builder, and the builder produced a page with cCtl=0. The stub report named
 * exactly one reason: BrUiCtlCtor was the only unported function the boot path
 * reached. Every control the menu wants to create dies here. So this is the
 * single function standing between a built page and a built menu, and it was
 * identified by running the program rather than by guessing.
 *
 * WHAT THE ORIGINAL DOES (0x100476C0, 478 bytes)
 *
 * It is almost entirely initialisation: a vtable store, ~30 scalar writes, six
 * `rep stosd` block fills, one vector-constructor call and one sub-object
 * constructor call. Recovered writes, in the original's own order:
 *
 *   +0x1C   = 1                    +0x44   = 0x3F7D70A4 (0.99f exactly)
 *   +0x2C   = 0xFF (a BYTE)        +0x2AEC = 1
 *   +0x2B54 = 1                    +0x00   = vtable 0x1008F6B8
 *   everything else scalar         = 0
 *
 *   block fills (dword counts):
 *     +0x2904  25   <- 0            +0x2978  50   <- 0
 *     +0x2A40  25   <- -1           +0x012A  2500 <- -1
 *     +0x0060  50   <- 0            +0x283C  50   <- 0
 *     +0x2AB6  12   <- 0, then one word  (stosw after rep stosd)
 *     +0x2AF0  25   <- -1
 *
 *   +0x2B5C is an array of THREE 0x438-byte elements, built by the MSVC vector
 *   constructor iterator (0x1007F680) with element ctor 0x1005B050 and element
 *   dtor 0x1005B0C0 for the unwind path. The count and stride are the pushed
 *   literals 3 and 0x438, not an inference.
 *
 *   +0x3838 gets its own __thiscall constructor, 0x1005B7F0.
 *
 * -1 VERSUS 0 IS LOad-BEARING. Four of the nine fills write 0xFFFFFFFF, not
 * zero -- notably the 2500-dword one at +0x12A, which is the item table. A
 * calloc'd control is NOT an initialised control: every one of those slots
 * would read 0, which is a valid index, instead of -1, which means "empty".
 * That is exactly the class of bug that survives a clean build and a passing
 * link and then picks the wrong menu item at runtime.
 *
 * DEVIATION -- SPARSE STRUCT, NOT A BYTE IMAGE
 *
 * The port models BrUiCtl_ as a sparse struct: it names the fields passes have
 * needed and leaves the gaps unnamed. It is deliberately NOT byte-exact, and on
 * LP64 it cannot be. So the block fills above have no modelled destination for
 * most of their range. This function therefore does what the original's writes
 * MEAN rather than replaying their addresses:
 *
 *   - zero the whole object first (covers every `<- 0` fill and scalar), then
 *   - set the specific non-zero fields the disassembly pins.
 *
 * The named `<- -1` field is set. The unnamed ones cannot be, because there is
 * nothing to set; when a later pass gives the item table a modelled home it
 * MUST be filled with -1 here. That is recorded as a TODO below rather than
 * left as a comment nobody greps for -- and it is the reason this file does not
 * claim to be a complete constructor.
 */
#include "br_uictl.h"
#include <string.h>

/* 0x1008F6B8 -- the control vtable. Its slots live in other packets; what is
 * established here is only that the constructor stores THIS address, so the
 * object must not be left with a NULL vtable. A zeroed vtable object is the
 * honest stand-in: non-NULL, so the store is faithful, and NULL in every slot,
 * so an unported virtual call crashes loudly instead of running wrong code. */
static const BrUiCtlVtbl_ g_vtblZeroed;

/* g_brUiCtlVtbl_1008F6B8 itself is DEFINED IN br_uivt.c, which owns the two
 * slots that are ported (+0x34 and +0x38). It used to be an all-zero object
 * here, which made it indistinguishable from g_vtblZeroed. */

/* The vtable the constructor actually stores.
 *
 * The original hardcodes 0x1008F6B8. This port reaches it through a settable
 * pointer for the same reason br_phase.h's context wires 0x1008F700 rather than
 * hardcoding it: the slots belong to functions in other packets, so until those
 * land the only honest default is a vtable of NULLs -- and a NULL slot means the
 * first virtual call jumps to address 0.
 *
 * That is the correct default for CORRECTNESS (an unported method must not
 * silently do nothing), and the wrong default for BRING-UP (it kills the run
 * before anything can be observed). A settable pointer serves both: the default
 * still crashes loudly, and a host can install instrumented slots to see how far
 * the boot gets. Callers that want the original's behaviour simply leave it
 * alone. */
const BrUiCtlVtbl_ *g_pBrUiCtlVtbl = &g_vtblZeroed;

/* 0.99f. The original stores the bit pattern 0x3F7D70A4 directly; it is
 * written here as the float it denotes, which is exact in binary32. */
#define BR_UICTL_F44   0.99f

BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis)
{
    int i;

    if (!pThis)
        return NULL;                    /* DEVIATION: the original faults. */

    /* Covers every `<- 0` scalar and every `<- 0` block fill in one go. */
    memset(pThis, 0, sizeof(*pThis));

    pThis->pVtbl = g_pBrUiCtlVtbl;

    /* The four function-pointer slots the builders later overwrite. Explicit
     * rather than relying on the memset, because their being NULL on exit from
     * the constructor is a fact the builders depend on. */
    pThis->pfn04 = NULL;
    pThis->pfn08 = NULL;
    pThis->pfn0C = NULL;
    pThis->pfn10 = NULL;
    pThis->pfn14 = NULL;
    pThis->pfn18 = NULL;    /* the sixth slot, which the old model omitted */

    /* The 25-dword -1 fill at +0x2A40 is br_ui.h's aStepId[50] -- 50 uint16
     * over the same 100 bytes (ADJ-3), and what two headers used to call the
     * scalars f2A40 and f2A42 are elements 0 and 1 (ADJ-4). Every one of the
     * fifty must be 0xFFFF, and specifically not 0: 0x10047FB0 later stores a
     * code word into element 0, and -1 is what "no code yet" looks like. The
     * whole array is modelled now, so the whole array is filled -- the old
     * model could only reach the first dword's two halves. */
    for (i = 0; i < BR_UI_CTL_STEPS; ++i) {
        pThis->aStepId[i] = (uint16_t)0xFFFFu;
    }

    /* The other three -1 fills the old sparse model had nowhere to put.
     * +0x12A is the dangerous one: 0 is a valid index there and -1 means
     * empty, so a calloc'd control is NOT an initialised control. */
    for (i = 0; i < BR_UI_CTL_A012A; ++i) {
        pThis->a012A[i] = -1;
    }
    for (i = 0; i < BR_UI_CTL_A2AF0; ++i) {
        pThis->a2AF0[i] = -1;
    }

    /* Scalars the original sets non-zero. All of them have a modelled field
     * now; the ledger at the bottom is down to the two sub-object
     * constructors. */
    pThis->flags1C = 1;

    /* 1004770A  mov dword ptr [esi + 0x44], 0x3f7d70a4
     *
     * BUG FIXED IN PASSING, and it is exactly the hazard this migration is
     * about: this line used to read `pThis->f1E1E8 = BR_UICTL_F44;` -- the
     * banner above it, the comment on it and test_uictl.c all said "+0x44",
     * but slice6_73.h's sparse control had no field at +0x44, so the constant
     * went into +0x1E1E8 instead. That is 0x1E1A4 bytes away, and it is a
     * slot 0x1004DFC0 uses as the car-list interpolation output. br_ui.h
     * names +0x44, so the store lands where the disassembly puts it. */
    pThis->f44 = BR_UICTL_F44;

    pThis->b2C   = 0xFFu;    /* +0x002C, a BYTE  */
    pThis->f2AEC = 1;        /* +0x2AEC          */
    pThis->f2B54 = 1;        /* +0x2B54          */

    /* +0x2B5C is three 0x438-byte BrTextBox elements built by the MSVC vector
     * constructor iterator with element ctor 0x1005B050 == slice3_39.h's
     * BrTextBoxInit. br_ui.h models all THREE (BrUiCtl_::aText, ADJ-1); of
     * BrTextBoxInit's writes only two are not zeroes:
     *
     *   f08 = 1          -- made here
     *   pVtbl = 0x1008F728 -- NOT made here, deliberately
     *
     * The vtable is left NULL for exactly the reason g_pBrUiCtlVtbl defaults
     * to an all-NULL vtable: 0x1008F728's slots are BrTextBoxMeasureA/B and
     * BrTextBoxCentreX, which live in slice3_39.c, and this module is kept
     * free of link dependencies so test_uictl can link br_uictl.o alone. A
     * host that wants the real methods installs slice3_39.h's
     * g_pBrTextBoxVtbl and re-runs BrTextBoxInit, or sets f2B5C.pVtbl
     * itself. Every caller in the tree already NULL-checks it.
     *
     * Elements 1 and 2 have a home now too, but BrTextBoxInit's only non-zero
     * write is that same f08, and the memset above already covers the rest --
     * so they are set here for the same reason element 0 is. */
    pThis->aText[0].f08 = 1;
    pThis->aText[1].f08 = 1;
    pThis->aText[2].f08 = 1;

    return pThis;                       /* original returns `this` in eax */
}

/* ==========================================================================
 * BR_UICTL_UNMODELLED -- writes the original makes that this port cannot yet
 * make, because BrUiCtl_ does not name the destination.
 *
 * Listed explicitly so the gap is greppable and countable rather than buried
 * in prose. Each entry is a field a future pass must add to BrUiCtl_ AND set
 * here in the same change; adding the field without the initialiser is worse
 * than not adding it, because it will read 0 and look deliberate.
 *
 * Every scalar and every block fill now has one. What remains is the ONE
 * write this file still cannot make faithfully:
 *
 *   offset    value        note
 *   +0x2B5C   3 elements   the vector-constructor iterator's unwind path. The
 *                          three elements are constructed (f08 = 1, the rest
 *                          zero by the memset) but 0x1008F728 is deliberately
 *                          NOT planted -- see the note above.
 *   +0x3838   ctor call    0x1005B7F0 == slice3_39.h's BrTextListInit. The
 *                          field EXISTS now (br_ui.h ADJ-6 embeds the whole
 *                          0x1A9D4-byte BrTextList), so this is a link
 *                          question, not a modelling one: slice3_39.o pulls
 *                          in BrTextBoxDtor and BrDikGetDeviceState, both of
 *                          which live in slice6_72.c, and this module is kept
 *                          free of link dependencies so test_uictl can link
 *                          br_uictl.o alone. The memset covers every zero the
 *                          list constructor writes; what is missed is
 *                          f1A932/34/36/38 == -1 and f08 == 1 on the hundred
 *                          items. Nothing in the tree reads either yet.
 *
 * +0x001C left this list when BrUiCtl_ gained f1C. +0x002C, +0x2AEC, +0x2B54,
 * the +0x012A / +0x2A40 / +0x2AF0 fills and the +0x3838 sub-object all left
 * it when the model became br_ui.h's, which names every one of them.
 * ========================================================================== */
const int g_brUiCtlUnmodelledWrites = 1;
