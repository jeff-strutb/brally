/* test_br_phasecur.c -- the ONE current-phase slot, 0x10AA2904.
 *
 * What is under test is an IDENTITY, not a value: several ranges of this port
 * name that dword differently and every one of them must land on the same
 * bytes.  So the assertions compare ADDRESSES, and a value assertion is only
 * used to show that a write through one name is visible through another --
 * which is the property that was actually broken.  Asserting "the value is
 * what I just stored" through a single name would pass under both the right
 * and the wrong arrangement, which is the failure mode CONVENTIONS.md
 * describes.
 */
#include "br_phasecur.h"

#include <stdio.h>
#include <stddef.h>

static int g_fail;

#define CHECK(c, msg)                                                       \
    do {                                                                    \
        if (!(c)) {                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));          \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

/* Two stand-ins for phase objects.  Nothing is called on them; only their
 * addresses matter. */
static BrPhase_ g_phaseA;
static BrPhase_ g_phaseB;

/* A stand-in for port/host/brally.c's `g_nav.pAA2904` -- the member the frame
 * loop reads, and the storage the host binds. */
static BrPhase_ *g_hostSlot;

/* --------------------------------------------------------------------------
 * 1. There is ALWAYS a slot.
 *
 * The unbound state has to be a real slot, not NULL, because every use site
 * assigns through it.  A NULL g_ppBrPhaseCur would turn `BR_PHASE_CUR = p`
 * into a fault rather than into a diagnostic, and the unbound state is how
 * every unit suite in this tree runs.
 * ----------------------------------------------------------------------- */
/* The slot as it is BEFORE anything binds -- captured in main() ahead of every
 * other call.  Checking this after a BrPhaseCurBind(NULL) would have masked a
 * NULL initialiser, because that call installs the fallback itself; the first
 * version of this test did exactly that and a "starts NULL" mutation survived
 * it. */
static BrPhase_ **g_slotAtStart;

static void TestAlwaysASlot(void)
{
    CHECK(g_slotAtStart != NULL, "the slot is a real slot before any bind");
    BrPhaseCurBind(NULL);
    CHECK(BrPhaseCurSlot() != NULL, "unbound still names a slot");
    BR_PHASE_CUR = &g_phaseA;
    CHECK(BR_PHASE_CUR == &g_phaseA, "and the slot is writable");

    /* The original's dword is .bss, so NULL is a state it genuinely starts
     * in and must be storable. */
    BR_PHASE_CUR = NULL;
    CHECK(BR_PHASE_CUR == NULL, "NULL is a value, not an unbind");
    CHECK(BrPhaseCurSlot() != NULL, "storing NULL does not lose the slot");
}

/* --------------------------------------------------------------------------
 * 2. Binding REDIRECTS the slot -- it does not copy it.
 *
 * This is the whole point.  A host that copied the value across on bind would
 * pass a value check and still leave two objects to drift apart on the next
 * write, which is precisely the defect this module exists to remove.
 * ----------------------------------------------------------------------- */
static void TestBindRedirects(void)
{
    BrPhaseCurBind(NULL);
    BR_PHASE_CUR = &g_phaseA;

    g_hostSlot = NULL;
    BrPhaseCurBind(&g_hostSlot);
    CHECK(BrPhaseCurSlot() == &g_hostSlot, "the bound slot IS the host's");

    /* A write through the shared name lands in the host's storage. */
    BR_PHASE_CUR = &g_phaseB;
    CHECK(g_hostSlot == &g_phaseB, "a write through BR_PHASE_CUR reaches the "
                                   "host's member");

    /* ...and a write the host makes directly is visible through the name.
     * This is the direction the frame loop depends on. */
    g_hostSlot = &g_phaseA;
    CHECK(BR_PHASE_CUR == &g_phaseA, "and the host's write is visible back");
}

/* --------------------------------------------------------------------------
 * 3. Taking the ADDRESS of the slot yields the bound storage.
 *
 * br_phaseact.c passes `&BR_PHASE_CUR` where the original passes the address
 * of the global (`BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR)`), so the
 * address-of form has to reach the same bytes as the assignment form.  If it
 * did not, the leave prologue would notify a phase nobody else can see.
 * ----------------------------------------------------------------------- */
static void TestAddressOf(void)
{
    g_hostSlot = &g_phaseA;
    BrPhaseCurBind(&g_hostSlot);

    CHECK(&BR_PHASE_CUR == &g_hostSlot, "&BR_PHASE_CUR is the bound storage");

    /* Write through the taken address; read through the macro. */
    *(&BR_PHASE_CUR) = &g_phaseB;
    CHECK(g_hostSlot == &g_phaseB, "a write through the taken address lands "
                                   "in the same object");
}

/* --------------------------------------------------------------------------
 * 4. Rebinding to NULL restores the module's own slot rather than unbinding.
 * ----------------------------------------------------------------------- */
static void TestRebindToFallback(void)
{
    BrPhase_ **pFirst;

    BrPhaseCurBind(NULL);
    pFirst = BrPhaseCurSlot();
    CHECK(pFirst != NULL, "fallback exists");
    CHECK(pFirst == g_slotAtStart, "and it is the SAME slot the module "
                                   "started with, not a second one");

    BrPhaseCurBind(&g_hostSlot);
    CHECK(BrPhaseCurSlot() == &g_hostSlot, "bound away");

    BrPhaseCurBind(NULL);
    CHECK(BrPhaseCurSlot() == pFirst, "and back to the SAME fallback, not a "
                                      "new one");
}

int main(void)
{
    /* FIRST, before anything else touches the module. */
    g_slotAtStart = BrPhaseCurSlot();

    TestAlwaysASlot();
    TestBindRedirects();
    TestAddressOf();
    TestRebindToFallback();

    if (g_fail == 0)
        printf("br_phasecur: all checks passed\n");
    else
        printf("%d FAILURE(S)\n", g_fail);
    return g_fail != 0;
}
