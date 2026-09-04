/* br_textmode.c -- drawing.  See br_textmode.h. */
#include "br_textmode.h"

#include <stddef.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
extern uint8_t  g_4B0360;
extern uint8_t  g_4B035C;
extern uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
extern uint32_t g_364308, g_363F68;
#else
uint8_t  g_4B0360;
uint8_t  g_4B035C;
uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
uint32_t g_364308[32];
uint32_t g_363F68[32];
#endif

/* WHAT IT DOES: turn off a one-byte text-pass flag sitting next to the
 * alignment byte.  The next string is drawn without that special pass. */
/* @implements 0x10016810 glide BrClear_10019250 */
/* @n64 0x8022F4F8 located */
void BrClear_10019250(void)
{
    g_4B0360 = 0;
}

/* WHAT IT DOES: centre the next string the HUD/menu text emitter draws.
 * 0 is left, 1 is right, 2 is centre -- this is the centre setter. */
/* @implements 0x10016830 glide BrSet_10019270 */
/* @n64 0x8022F4CC located */
void BrSet_10019270(void)
{
    g_4B035C = 2;
}

/* WHAT IT DOES: wipe two 128-byte scratch tables used by a later pass. */
/* @implements 0x1000F620 d3d BrClearTables_1000F620 */
void BrClearTables_1000F620(void)
{
    memset(&g_364308, 0, 0x80);
    memset(&g_363F68, 0, 0x80);
}

/* WHAT IT DOES: rewind a two-word cursor so the next read starts at the
 * beginning. */
/* @implements 0x1006CDD0 glide BrPairReset_10073B90 */
void BR_THISCALL1 BrPairReset_10073B90(uint32_t *pThis)
{
    pThis[0] = 0;
    pThis[1] = 0;
}

/* WHAT IT DOES: rebuilds the clipper's free list: 64 vertex nodes chained
 * together, last node first. */
/* @implements 0x1000C9C0 glide BrNodeChainReset_1000F460 */
void BrNodeChainReset_1000F460(void)
{
    /* prev is zeroed BEFORE the cursor is materialised (xor ecx,ecx first). */
    uint32_t prev = 0;
    uint32_t *p = &g_2E5E98;

    do {
        *p = prev;
        prev = (uint32_t)(uintptr_t)p;
        p = (uint32_t *)((char *)p - 0x28);
    } while ((int)(uintptr_t)p >= (int)(uintptr_t)&g_2E54C0);
    g_2E5ECC = prev;
}

#ifdef BR_MATCHING_BUILD
extern int DAT_102e16ac;
extern int DAT_102e16b0;
extern char DAT_102e1710;
extern int DAT_1035f7d8;
extern int DAT_1035f7dc;
extern int DAT_1035faec;
extern int DAT_1035fba4;
extern char DAT_1035fba8;
extern char DAT_103874a8;
extern int DAT_106ed67c;

/* WHAT IT DOES: point the three per-view scratch buffers at view index
 * 0x106ED67C's slice -- each base is stored to two cursors (base and write
 * head).  Strides 80000 / 32000 / 256000 bytes lower as lea chains. */
/* @implements 0x1000CB20 glide BrViewBuffersRebase */

void BrViewBuffersRebase(void)

{
  /* ABSOLUTE base addresses (add ecx,imm32, no reloc) -- the same
   * absolute-address spelling br_scenedl.c proved for its row transforms;
   * a symbol base emits lea reg,[reg+disp32] instead. */
  DAT_1035f7d8 = 0x1035fba8 + DAT_106ed67c * 80000;
  DAT_102e16b0 = 0x1035fba8 + DAT_106ed67c * 80000;
  /* RESIDUE (4B): the 32000 product's last lea lands in edx and folds the
   * base add into a lea (orig keeps ecx and a plain add) -- coalescing
   * residue; temp-binding and addend order probed, both no better. */
  DAT_1035faec = 0x103874a8 + DAT_106ed67c * 32000;
  DAT_1035fba4 = 0x103874a8 + DAT_106ed67c * 32000;
  DAT_102e16ac = 0x102e1710 + DAT_106ed67c * 0x3e800;
  DAT_1035f7dc = 0x102e1710 + DAT_106ed67c * 0x3e800;
  return;
}
extern uint8_t g_br4B0358;

/* 0x10019260, 12 call sites. */
/* WHAT IT DOES: clears one of the text drawing flags. A wrapper onto the
 * body that lives with the rest of the text code. */
/* @implements 0x10016820 glide BrTextFlag358Clear */
void BrTextFlag358Clear(void)
{
    g_br4B0358 = 0;
}

#endif /* BR_MATCHING_BUILD */
