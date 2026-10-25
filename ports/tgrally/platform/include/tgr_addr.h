/* tgr_addr.h: the game's memory and its 32-bit addresses.
 *
 * The game's memory is one 8 MB arena, tgr_rdram, laid out as the N64's
 * RAM: every data symbol (.data, .bss, and the fixed buffers the game uses
 * outside its image) sits at its original address, with the original's
 * layout.  An address the game keeps in memory is therefore always the
 * value the cartridge would keep there: a 4-byte original address, KSEG0
 * 0x80XXXXXX (or its KSEG1 alias 0xA0XXXXXX), in a TgrAddr.  Native code
 * holds native pointers; these translate:
 *
 *   TGR_PTR(T, a)    original address -> native pointer
 *   tgr_addr32(p)    native pointer -> original address
 *   TGR_FN(T, a)     a code address the game keeps -> the native function
 *   tgr_fnaddr(f)    a native game function -> its original address
 *
 * A native pointer outside the arena that the game nevertheless stores
 * (a string literal, a platform buffer) is given a window id 0x20..0x7F, so
 * it still round-trips; 0 stays NULL. */
#ifndef TGR_ADDR_H
#define TGR_ADDR_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TGR_RDRAM_SIZE 0x800000
extern uint8_t tgr_rdram[TGR_RDRAM_SIZE];

void    *tgr_ptr32_slow(uint32_t a);
uint32_t tgr_addr32_slow(const void *p);
void    *tgr_fn(uint32_t a);
uint32_t tgr_fnaddr(const void *f);

/* pages of the arena holding a pointer table the port keeps natively
 * (tgr_natives): an address there may name the native table */
extern uint8_t tgr_natpage[TGR_RDRAM_SIZE >> 12];

static inline void *tgr_ptr32(uint32_t a)
{
    if (((a & 0xDFFFFFFFu) - 0x80000000u) < TGR_RDRAM_SIZE &&   /* KSEG0 or KSEG1 */
        !tgr_natpage[(a & (TGR_RDRAM_SIZE - 1)) >> 12])
        return tgr_rdram + (a & (TGR_RDRAM_SIZE - 1));
    return tgr_ptr32_slow(a);
}
static inline uint32_t tgr_addr32(const void *p)
{
    uintptr_t d = (uintptr_t)p - (uintptr_t)tgr_rdram;
    if (d < TGR_RDRAM_SIZE)
        return 0x80000000u + (uint32_t)d;
    return tgr_addr32_slow(p);
}

#define TGR_PTR(T, a)  ((T)tgr_ptr32((uint32_t)(a)))
#define TGR_FN(T, a)   ((T)tgr_fn((uint32_t)(a)))
#define TGR_A32(p)     tgr_addr32(p)
#define TGR_FA(f)      tgr_fnaddr((const void *)(f))

/* a display-list word that may be an address: a pointer is translated, an
 * integer kept */
#define TGR_W1(x) (__builtin_classify_type(x) == 5 ? tgr_addr32((const void *)(uintptr_t)(x)) \
                                                    : (unsigned int)(uintptr_t)(x))
#ifdef __cplusplus
}
#endif
#endif
