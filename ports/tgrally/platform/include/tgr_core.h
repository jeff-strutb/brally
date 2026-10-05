/* tgr_core.h: forced into every core TU after ultra64.h.  The host C
 * library's own declarations, so a TU's loose declaration of memcpy,
 * sprintf or strlen (IDO's int for size_t) is an error to fix, not a
 * silent ABI mismatch; and the address translation the core's 32-bit
 * slots go through. */
#ifndef TGR_CORE_H
#define TGR_CORE_H
#ifndef TGR_NO_LIBC                 /* (the tools read the core for the N64's ABI, which has no libc headers) */
#include <string.h>
#include <stdio.h>
#endif
#include "tgr_addr.h"
#endif

#ifndef TGR_CORE_HELPERS
#define TGR_CORE_HELPERS
#include <stdint.h>

/* Big-endian memory.  What the RSP and RDP read (display lists, vertices,
 * matrices, viewports, lights, texels and palettes) and the data the game
 * loads from the cartridge stay in the cartridge's byte order, byte for byte
 * what the N64's RAM held.  Their fields are these wrapper types, so the
 * compiler stops every access that does not go through the accessors. */
typedef struct { uint8_t b[2]; } be16_t;
typedef struct { uint8_t b[4]; } be32_t;
typedef struct { uint8_t b[4]; } bef_t;     /* a float */

static inline uint32_t tgr_rd32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}
static inline uint16_t tgr_rd16(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)(b[0] << 8 | b[1]);
}
static inline void tgr_wr32(void *p, uint32_t v)
{
    uint8_t *b = (uint8_t *)p;
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16); b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
}
static inline void tgr_wr16(void *p, uint16_t v)
{
    uint8_t *b = (uint8_t *)p;
    b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v;
}
static inline float tgr_rdf(const void *p)
{
    union { uint32_t u; float f; } x;
    x.u = tgr_rd32(p);
    return x.f;
}
static inline void tgr_wrf(void *p, float f)
{
    union { uint32_t u; float f; } x;
    x.f = f;
    tgr_wr32(p, x.u);
}
static inline uint32_t tgr_be32(uint32_t v) { return __builtin_bswap32(v); }

/* field accessors, typed so a float field cannot be read as an integer or
 * the reverse: BE32(f) unsigned, BES32 signed, BEF float, BE16/BES16,
 * BEPTR(T, f) the original address in f as a native pointer; SET* write */
static inline uint32_t be32_rd(const be32_t *p) { return tgr_rd32(p); }
static inline uint16_t be16_rd(const be16_t *p) { return tgr_rd16(p); }
static inline float    bef_rd(const bef_t *p)   { return tgr_rdf(p); }
static inline void be32_wr(be32_t *p, uint32_t v) { tgr_wr32(p, v); }
static inline void be16_wr(be16_t *p, uint16_t v) { tgr_wr16(p, v); }
static inline void bef_wr(bef_t *p, float v)      { tgr_wrf(p, v); }
#define BE32(f)        _Generic((f), be32_t: be32_rd(&(f)))
#define BES32(f)       ((int32_t)_Generic((f), be32_t: be32_rd(&(f))))
#define BE16(f)        _Generic((f), be16_t: be16_rd(&(f)))
#define BES16(f)       ((int16_t)_Generic((f), be16_t: be16_rd(&(f))))
#define BEF(f)         _Generic((f), bef_t: bef_rd(&(f)))
#define BEPTR(T, f)    ((T)tgr_ptr32(_Generic((f), be32_t: be32_rd(&(f)))))
#define SET32(f, v)    _Generic((f), be32_t: be32_wr(&(f), (uint32_t)(v)))
#define SET16(f, v)    _Generic((f), be16_t: be16_wr(&(f), (uint16_t)(v)))
#define SETF(f, v)     _Generic((f), bef_t: bef_wr(&(f), (v)))
#define SETPTR(f, p)   _Generic((f), be32_t: be32_wr(&(f), tgr_addr32(p)))

/* A float to an unsigned integer as the game's code does it.  IDO converts
 * through the FPU with exceptions checked: a value outside [0, 2^32)
 * (negatives and NaN included) gives 0xFFFFFFFF, and a narrower unsigned
 * type keeps the low bits of that (0xFF for a negative byte); the 64-bit
 * case is libultra's __d_to_ull (0x80266A08), all ones outside [0, 2^64).
 * The host's conversion clamps a negative to 0 instead. */
static inline uint32_t tgr_f2u(double x)
{
    return (x > -1.0 && x < 4294967296.0) ? (uint32_t)x : 0xFFFFFFFFu;
}
static inline uint64_t tgr_f2ull(double x)
{
    return (x > -1.0 && x < 18446744073709551616.0) ? (uint64_t)x : ~0ull;
}
#endif
