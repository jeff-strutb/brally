/* test_br_ui.c -- the canonical page and control types (port/include/br_ui.h).
 *
 * Most of br_ui.h's claims are compile-time assertions inside the header, so
 * merely building this file exercises them; a bad count fails the BUILD.
 *
 * What is left for run time is deliberately NOT a restatement of those. Per
 * CONVENTIONS.md ("assert behaviour and invariants, not volume"), the two
 * things checked here are properties of the recovered model that no single
 * assertion in the header can express:
 *
 *   1. THE ORIGINAL LAYOUT TILES. br_ui.h's whole argument is that the control
 *      constructor initialises the object in address order and every region it
 *      touches abuts the next. That is a claim about the 32-BIT layout, which
 *      this host does not reproduce -- so it is checked as arithmetic over the
 *      original's offsets, which is host-independent. If any element count is
 *      ever "corrected", the tiling develops a gap or an overlap and this
 *      fails, pointing at the adjudication that owns the count.
 *
 *      This is the check that would have caught slice6_71.h's 24-entry step
 *      array (leaves a 104-byte hole) and slice6_73.h's flat +0x2F80 fields
 *      (they land inside aText[0] and overlap it).
 *
 *   2. THE PORT NEVER UNDER-ALLOCATES. On LP64 both structs are larger than
 *      the original literal; the allocation macros must return the larger.
 */
#include "br_ui.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *what)
{
    if (!cond) {
        ++g_fail;
        printf("  FAIL: %s\n", what);
    }
}

/* ------------------------------------------------------------------------
 * 1. The original layout, region by region, in the order the constructor at
 *    0x100476C0 writes them. Each entry is {first byte, one past the last}
 *    under the ORIGINAL 32-bit layout. Sizes are derived from the element
 *    counts br_ui.h adjudicated, never from literals, so a changed count
 *    moves the region.
 * ---------------------------------------------------------------------- */
typedef struct Region {
    unsigned    lo, hi;
    const char *name;
} Region;

#define R(lo, len, name) { (lo), (lo) + (unsigned)(len), (name) }

static const Region g_ctl[] = {
    R(0x00000,  4,                          "pVtbl"),
    R(0x00004,  4 * 6,                      "pfn04..pfn18"),
    R(0x0001C,  4,                          "flags1C"),
    R(0x00020,  4,                          "f20"),
    R(0x00024,  4,                          "flags24"),
    R(0x00028,  4,                          "flags28"),
    R(0x0002C,  1,                          "b2C"),
    R(0x0002D,  3,                          "pad2D"),
    R(0x00030,  4 * 3,                      "fTweenX/Y/Z"),
    R(0x0003C,  4,                          "x"),
    R(0x00040,  4,                          "y"),
    R(0x00044,  4,                          "f44"),
    R(0x00048,  2,                          "w48"),
    R(0x0004A,  2,                          "w4A"),
    R(0x0004C,  4,                          "f4C"),
    R(0x00050,  4 * 4,                      "rcLeft..rcBottom"),
    R(0x00060,  4 * BR_UI_CTL_A0060,        "a0060"),
    R(0x00128,  2,                          "wStep"),
    R(0x0012A,  4 * BR_UI_CTL_A012A,        "a012A"),
    R(0x0283A,  2,                          "pad283A"),
    R(0x0283C,  4 * BR_UI_CTL_A283C,        "a283C"),
    R(0x02904,  4 * BR_UI_CTL_A2904,        "a2904"),
    R(0x02968,  4,                          "f2968"),
    R(0x0296C,  4,                          "f296C"),
    R(0x02970,  4,                          "f2970"),
    R(0x02974,  4,                          "f2974"),
    R(0x02978,  4 * BR_UI_CTL_STEPS,        "aStepMs"),
    R(0x02A40,  2 * BR_UI_CTL_STEPS,        "aStepId"),
    R(0x02AA4,  4,                          "f2AA4"),
    R(0x02AA8,  4,                          "f2AA8"),
    R(0x02AAC,  2,                          "w2AAC"),
    R(0x02AAE,  6,                          "pad2AAE"),
    R(0x02AB4,  2,                          "cChild"),
    R(0x02AB6,  2 * BR_UI_CTL_CHILDREN,     "aChild"),
    R(0x02AE8,  4,                          "pOwner"),
    R(0x02AEC,  4,                          "f2AEC"),
    R(0x02AF0,  4 * BR_UI_CTL_A2AF0,        "a2AF0"),
    R(0x02B54,  4,                          "f2B54"),
    R(0x02B58,  4,                          "f2B58"),
    R(0x02B5C,  BR_UI_TEXTBOX_ORIG_SIZE * BR_UI_CTL_TEXTS, "aText[3]"),
    R(0x03804,  4,                          "twXOn"),
    R(0x03808,  4,                          "twYOn"),
    R(0x0380C,  1,                          "twXDir"),
    R(0x0380D,  1,                          "twYDir"),
    R(0x0380E,  2,                          "pad380E"),
    R(0x03810,  4,                          "twXEnd"),
    R(0x03814,  4,                          "twYEnd"),
    R(0x03818,  4,                          "twActive"),
    R(0x0381C,  4,                          "twLo"),
    R(0x03820,  4,                          "twHi"),
    R(0x03824,  4,                          "twRate"),
    R(0x03828,  4,                          "twTick"),
    R(0x0382C,  4,                          "twMs"),
    R(0x03830,  4,                          "f3830"),
    R(0x03834,  2,                          "w3834"),
    R(0x03836,  2,                          "w3836"),
    R(0x03838,  BR_UI_TEXTLIST_ORIG_SIZE,   "list"),
    R(0x1E20C,  2,                          "w1E20C"),
    R(0x1E20E,  2,                          "pad1E20E"),
    R(0x1E210,  4,                          "p1E210"),
};

static const Region g_page[] = {
    R(0x000,  4,                        "pVtbl"),
    R(0x004,  4 * 3,                    "pfn04/08/0C"),
    R(0x010,  4,                        "f10"),
    R(0x014,  2,                        "cCtl"),
    R(0x016,  2,                        "w16"),
    R(0x018,  4 * BR_UI_PAGE_CTL_MAX,   "apCtl"),
    R(0x338,  4,                        "fX"),
    R(0x33C,  4,                        "fY"),
    R(0x340,  4,                        "pOwner"),
    R(0x344,  2,                        "cSel"),
    R(0x346,  2,                        "iSel"),
};

/* A tiling: strictly ascending, no gap, no overlap, ending exactly on the
 * allocation the original asks `operator new` for. */
static void check_tiles(const Region *a, size_t n, unsigned total,
                        const char *what)
{
    size_t   i;
    unsigned at = 0;
    char     msg[160];

    for (i = 0; i < n; ++i) {
        if (a[i].lo != at) {
            sprintf(msg, "%s: %s starts at 0x%X, previous region ends at 0x%X",
                    what, a[i].name, a[i].lo, at);
            check(0, msg);
            return;
        }
        if (a[i].hi <= a[i].lo) {
            sprintf(msg, "%s: %s is empty or inverted", what, a[i].name);
            check(0, msg);
            return;
        }
        at = a[i].hi;
    }
    if (at != total) {
        sprintf(msg, "%s: regions end at 0x%X, original allocates 0x%X",
                what, at, total);
        check(0, msg);
    }
}

int main(void)
{
    struct BrUiPage_ page;
    BrUiCtl_        *pCtl;
    static BrUiCtl_  ctl;    /* ~123 KB: static, not on the stack */

    /* --- 1. the original layout tiles, with no gap and no overlap -------- */
    check_tiles(g_ctl,  sizeof(g_ctl)  / sizeof(g_ctl[0]),
                BR_UI_CTL_ORIG_SIZE,  "control");
    check_tiles(g_page, sizeof(g_page) / sizeof(g_page[0]),
                BR_UI_PAGE_ORIG_SIZE, "page");

    /* --- 2. the allocation macros never under-allocate ------------------- */
    check(BR_UI_PAGE_ALLOC_SIZE >= BR_UI_PAGE_ORIG_SIZE,
          "BR_UI_PAGE_ALLOC_SIZE covers the original literal");
    check(BR_UI_PAGE_ALLOC_SIZE >= sizeof(struct BrUiPage_),
          "BR_UI_PAGE_ALLOC_SIZE covers the host struct");
    check(BR_UI_CTL_ALLOC_SIZE >= BR_UI_CTL_ORIG_SIZE,
          "BR_UI_CTL_ALLOC_SIZE covers the original literal");
    check(BR_UI_CTL_ALLOC_SIZE >= sizeof(BrUiCtl_),
          "BR_UI_CTL_ALLOC_SIZE covers the host struct");

    /* --- 3. a child index must be able to address a page slot ------------
     * ADJ-5 makes aChild[] signed indices into the owning page's apCtl. That
     * is only coherent if the child array cannot outrun the page array. */
    check(BR_UI_CTL_CHILDREN <= BR_UI_PAGE_CTL_MAX,
          "aChild cannot hold more entries than apCtl can address");

    /* --- 4. the two objects genuinely nest, on THIS host too -------------
     * Not a layout claim: just that aText[] and list are distinct storage
     * inside one control, which is what the four partial models could not say
     * (they cast between views of the same bytes). */
    memset(&ctl, 0, sizeof(ctl));
    ctl.aText[0].f04 = 0x11111111u;
    ctl.aText[2].f04 = 0x33333333u;
    ctl.list.f08     = 0x44444444u;   /* control +0x3840 */
    check(ctl.aText[0].f04 == 0x11111111u &&
          ctl.aText[2].f04 == 0x33333333u &&
          ctl.list.f08     == 0x44444444u,
          "aText[0], aText[2] and list are distinct storage");

    /* --- 5. the page's owner is the phase, the control's owner is too ----
     * br_phase.h's aPages[] and this header's pOwner must close the loop, or
     * the two headers are still modelling different objects. */
    memset(&page, 0, sizeof(page));
    pCtl = &ctl;
    page.apCtl[0] = pCtl;
    page.cCtl     = 1;
    check(page.apCtl[0] == pCtl && page.cCtl == 1,
          "a control installs into the page's apCtl under one type");

    {
        BrPhase_ phase;
        memset(&phase, 0, sizeof(phase));
        page.apCtl[0]->pOwner = &phase;
        page.pOwner           = &phase;
        check(page.pOwner == page.apCtl[0]->pOwner,
              "page and control agree on the phase type (br_phase.h BrPhase_)");
    }

    printf("  page %zu bytes (orig 0x%X)   control %zu bytes (orig 0x%X)\n",
           sizeof(struct BrUiPage_), BR_UI_PAGE_ORIG_SIZE,
           sizeof(BrUiCtl_), BR_UI_CTL_ORIG_SIZE);
    /* status LAST: the harness reads the final line. */
    printf("br_ui: %d failures\n", g_fail);
    return g_fail != 0;
}
