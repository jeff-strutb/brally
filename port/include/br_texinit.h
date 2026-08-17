/* br_texinit.h -- 0x10029B50, the texture subsystem's WIRING POINT.
 *
 * RESPONSIBILITY: drawing -- installing the texture/RDP handler table and
 * choosing a texture-detail level from how much texture memory the card has.
 *
 * WHY THIS FUNCTION MATTERS OUT OF PROPORTION TO ITS SIZE
 *
 * ARCHITECTURE.md establishes that 86% of this engine is reachable only
 * through stored function pointers: it installs roughly 1,148 hooks at run
 * time and dispatches through them. tools/hookmap.py ranks the installers, and
 * this is one of the densest -- THIRTEEN hooks in 285 bytes, almost nothing but
 * stores.
 *
 * That makes it a wiring point. Nothing below it runs until it does, and a
 * display-list opcode handler that calls through one of these slots reaches a
 * NULL if it has not. Two of the slots are already known to be reached that
 * way: opcode 0xDC calls [0x118ED1CC] and opcode 0xDD calls [0x118ED1D0].
 *
 * THE THIRTEEN SLOTS, in the order the original writes them:
 *
 *   0x118ED1BC <- 0x10023D20      0x118ED1C0 <- 0x10024E60
 *   0x118ED1C4 <- 0x100272F0      0x118ED1C8 <- 0x10027F00
 *   0x118ED1CC <- 0x100284E0  (opcode 0xDC calls through this)
 *   0x118ED1D0 <- 0x100285E0  (opcode 0xDD calls through this)
 *   0x118ED1D4 <- 0x10028620      0x118ED1D8 <- 0x100287E0
 *   0x118ED1DC <- 0x10028820      0x118ED1E0 <- 0x100297F0
 *   0x118ED19C <- 0x100298C0  <-- OUT OF SEQUENCE, and deliberately so:
 *                                 it sits between 0x1E0 and 0x1E4 in the
 *                                 listing but 0x20 lower in memory. Preserved
 *                                 in the original's ORDER, because that is
 *                                 what the original does; a tidied version
 *                                 that sorted them would be a different
 *                                 program if any slot were read mid-install.
 *   0x118ED1E4 <- 0x100299A0      0x118ED1E8 <- 0x10029CD0
 *
 * TEXTURE MEMORY, and the detail level it decides
 *
 * After installing, the original measures the card:
 *
 *   texmem  = grTexMaxAddress(0) - grTexMinAddress(0)        0x1186C95C
 *   if (0x105CCBD0 > 1)                                      a second TMU
 *       texmem += grTexMaxAddress(1) - grTexMinAddress(1)
 *
 * then 0x10029B10 turns that into a level in 0x100B8498:
 *
 *   texmem <= [0x1186C960]          -> 2      (the low-memory path)
 *   else if [0x10226E78] <= 0x2000000 -> 1    (32 MB of system RAM)
 *   else                            -> (texmem < 0x3D0900) ? 1 : 0
 *
 * 0x3D0900 is 4,000,000 -- four megabytes of texture memory, in decimal, not
 * 4 MiB. Worth stating because a reader who assumes 0x400000 gets a different
 * threshold and no test would notice on typical hardware.
 *
 * NOTE THE COMPARISONS ARE UNSIGNED (`jbe`, and `sbb/neg` off an unsigned
 * `cmp`), which matters: grTexMaxAddress and grTexMinAddress return card
 * addresses, and their difference is not meaningfully signed.
 *
 * The `sbb eax,eax / neg eax` idiom at 0x10029B30 is a branchless
 * `texmem < 0x3D0900`. It is easy to read backwards -- `sbb` yields -1 when
 * the borrow is SET, i.e. when the value is LESS -- so the level is 1 for a
 * SMALL card and 0 for a large one. Higher number means less detail.
 */
#ifndef BR_TEXINIT_H
#define BR_TEXINIT_H

#include <stdint.h>

/* The Glide entry points this needs, behind a host hook: the port does not
 * link glide2x. Returning equal min/max is the honest "no card" answer and
 * yields texmem 0, which selects the low-memory level -- the same thing the
 * original does on a card with no texture memory. */
typedef struct BrTexInitHost {
    uint32_t (*pfnTexMinAddress)(void *pUser, int32_t tmu);
    uint32_t (*pfnTexMaxAddress)(void *pUser, int32_t tmu);
    void     *pUser;
} BrTexInitHost;

/* The thirteen slots, in the original's write order. Exposed so a test can
 * assert both WHICH slot got WHICH function and the ORDER of the stores. */
#define BR_TEXINIT_NSLOTS 13

typedef void (*BrTexHookFn)(void);

/* Slot addresses, and the function each receives. */
uint32_t     BrTexInitSlotAddr(int i);
uint32_t     BrTexInitSlotValue(int i);

/* 0x105CCBD0 -- the TMU count. > 1 adds the second unit's memory. */
extern int32_t g_brTexTmuCount;

/* 0x1186C960 -- the low-memory threshold the level test compares against. */
extern uint32_t g_brTexLowThreshold;

/* 0x10226E78 -- system memory in bytes, from the start-up probe. */
extern uint32_t g_brTexSysMem;

/* 0x1186C95C -- the measured total texture memory. */
uint32_t BrTexInitMemory(void);

/* 0x100B8498 -- the chosen level. 0 = most detail, 2 = least. */
int32_t  BrTexInitLevel(void);

/* 0x10029B10 -- the level decision alone, so it can be tested across the
 * whole input space without a card. */
int32_t  BrTexChooseLevel(uint32_t texmem);

/* 0x10029B50, WHOLE. An earlier revision transcribed only as far as
 * 0x10029C22 and declared the rest "frontier" in this comment. The
 * equivalence audit called that DIVERGENT and was right to: the header was
 * honest, but the address still counted as ported in the coverage figure while
 * roughly half its body did not exist. Being candid about an omission is not
 * the same as not having one.
 *
 * The tail, in the original's order, now present:
 *
 *   0x10029BD4  call 0x100281C0            BEFORE the measurement, not after
 *   0x10029C1C  [0x105E1820] = -1
 *   0x10029C22  the level decision (0x10029B10)
 *   0x10029C2C  [0x105E1808] = -1
 *   0x10029C3F  free([0x106B7AA0]); [0x106B7AA0] = 0
 *   0x10029C33..0x10029C5A  zero 0x10697A58, 0x10697A5C, 0x10697A50,
 *                           0x10697A48, 0x106B7A7C
 *   0x10029C60  call 0x10029C70
 *   0x10029C65  call 0x1006E180
 *
 * The three calls are genuinely unported and are COUNTED frontier entries; the
 * stores and the free are transcribed. That distinction is the whole point --
 * a frontier entry is a named edge with a counter, not a licence to omit
 * straight-line code around it. */
void     BrTexInit(const BrTexInitHost *pHost);

/* The five globals the tail zeroes, and the two set to -1, exposed so the
 * order and the values can be asserted rather than described. */
int32_t  BrTexInitGlobal5E1820(void);
int32_t  BrTexInitGlobal5E1808(void);
int32_t  BrTexInitZeroedCount(void);
uint32_t BrTexInitZeroedAt(int nth);
int      BrTexInitFreeCalls(void);
int      BrTexInitTailCalls(int which);   /* 0 = 0x100281C0, 1 = 0x10029C70,
                                             2 = 0x1006E180 */

void     BrTexInitResetForTest(void);

/* How many slots have been installed, and in what order. */
int          BrTexInitInstalledCount(void);
uint32_t     BrTexInitInstalledAt(int nth);   /* slot address, install order */

#endif /* BR_TEXINIT_H */
