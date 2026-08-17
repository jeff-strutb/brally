/* br_texinit.c -- see br_texinit.h. Glide 0x10029B50 and 0x10029B10. */
#include "br_texinit.h"

#include <stddef.h>

int32_t  g_brTexTmuCount    = 0;          /* 0x105CCBD0 */
uint32_t g_brTexLowThreshold = 0;         /* 0x1186C960 */
uint32_t g_brTexSysMem      = 0;          /* 0x10226E78 */

static uint32_t s_texmem;                 /* 0x1186C95C */
static int32_t  s_level = -1;             /* 0x100B8498 */

/* The thirteen stores of 0x10029B52..0x10029BD4, in the original's order.
 * 0x118ED19C is eleventh in the LISTING while being lowest in memory; the
 * order is the original's and is kept. */
static const uint32_t s_aSlot[BR_TEXINIT_NSLOTS] = {
    0x118ED1BC, 0x118ED1C0, 0x118ED1C4, 0x118ED1C8, 0x118ED1CC,
    0x118ED1D0, 0x118ED1D4, 0x118ED1D8, 0x118ED1DC, 0x118ED1E0,
    0x118ED19C, 0x118ED1E4, 0x118ED1E8
};
static const uint32_t s_aValue[BR_TEXINIT_NSLOTS] = {
    0x10023D20, 0x10024E60, 0x100272F0, 0x10027F00, 0x100284E0,
    0x100285E0, 0x10028620, 0x100287E0, 0x10028820, 0x100297F0,
    0x100298C0, 0x100299A0, 0x10029CD0
};

static uint32_t s_aInstalled[BR_TEXINIT_NSLOTS];
static int      s_cInstalled;

uint32_t BrTexInitSlotAddr(int i)
{
    return (i >= 0 && i < BR_TEXINIT_NSLOTS) ? s_aSlot[i] : 0;
}
uint32_t BrTexInitSlotValue(int i)
{
    return (i >= 0 && i < BR_TEXINIT_NSLOTS) ? s_aValue[i] : 0;
}
uint32_t BrTexInitMemory(void)          { return s_texmem; }
int32_t  BrTexInitLevel(void)           { return s_level; }
int      BrTexInitInstalledCount(void)  { return s_cInstalled; }
uint32_t BrTexInitInstalledAt(int n)
{
    return (n >= 0 && n < s_cInstalled) ? s_aInstalled[n] : 0;
}

void BrTexInitResetForTest(void)
{
    int i;
    for (i = 0; i < BR_TEXINIT_NSLOTS; ++i) s_aInstalled[i] = 0;
    s_cInstalled = 0;
    s_texmem = 0;
    s_level  = -1;
    g_brTexTmuCount = 0;
    g_brTexLowThreshold = 0;
    g_brTexSysMem = 0;
}

/* ------------------------------------------------------------------ *
 * 0x10029B10 -- texture memory to detail level.
 *
 *   10029B1B  cmp eax, ecx          texmem vs [0x1186C960]
 *   10029B1D  jbe 0x10029B45        <=  -> level 2      (UNSIGNED)
 *   10029B1F  cmp [0x10226E78], 0x2000000
 *   10029B29  jbe 0x10029B3A        <=  -> level 1
 *   10029B2B  cmp eax, 0x3D0900
 *   10029B30  sbb eax, eax          -1 when texmem <  0x3D0900
 *   10029B32  neg eax                1 when texmem <  0x3D0900, else 0
 *
 * `sbb/neg` is easy to read backwards: the borrow is SET when the value is
 * LESS, so a SMALL card gets level 1 and a large one gets level 0. Higher
 * level means LESS detail.
 *
 * 0x3D0900 is 4,000,000 -- four million bytes, decimal. A reader who assumes
 * 4 MiB (0x400000) gets a threshold 194,304 bytes low, and no test on typical
 * hardware would notice.
 * ------------------------------------------------------------------ */
int32_t BrTexChooseLevel(uint32_t texmem)
{
    if (texmem <= g_brTexLowThreshold)          /* jbe -- UNSIGNED */
        return 2;
    if (g_brTexSysMem <= 0x2000000u)            /* 32 MB of system RAM */
        return 1;
    return (texmem < 0x3D0900u) ? 1 : 0;
}

/* ------------------------------------------------------------------ *
 * 0x10029B50 -- install the thirteen hooks, then measure the card.
 * ------------------------------------------------------------------ */
void BrTexInit(const BrTexInitHost *pHost)
{
    uint32_t texmem;
    int      i;

    /* 0x10029B52..0x10029BD4. The port cannot write to the original's .data
     * addresses, so the installation is RECORDED -- which slot received which
     * function, and in what order. That is what a consumer of this module
     * needs to know and it is what a test can check; inventing storage at
     * those addresses would create a second owner for slots that other
     * modules will eventually declare. */
    s_cInstalled = 0;
    for (i = 0; i < BR_TEXINIT_NSLOTS; ++i)
        s_aInstalled[s_cInstalled++] = s_aSlot[i];

    /* 0x10029BDB..0x10029C14. grTexMaxAddress(tmu) - grTexMinAddress(tmu),
     * summed over the second TMU when 0x105CCBD0 > 1.
     *
     * No host means no card: equal min and max, hence texmem 0, which selects
     * level 2. That is the original's own answer for a card with no texture
     * memory, not a substitute for one. */
    texmem = 0;
    if (pHost != NULL &&
        pHost->pfnTexMinAddress != NULL && pHost->pfnTexMaxAddress != NULL) {
        texmem = pHost->pfnTexMaxAddress(pHost->pUser, 0)
               - pHost->pfnTexMinAddress(pHost->pUser, 0);
        if (g_brTexTmuCount > 1) {
            texmem += pHost->pfnTexMaxAddress(pHost->pUser, 1)
                    - pHost->pfnTexMinAddress(pHost->pUser, 1);
        }
    }
    s_texmem = texmem;

    /* 0x10029C22 */
    s_level = BrTexChooseLevel(texmem);
}
