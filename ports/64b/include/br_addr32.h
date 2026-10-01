/* br_addr32.h: 32-bit addresses for the data formats that hold them.
 *
 * The game's N64-format data -- display lists, models, tracks -- stores
 * addresses in 4-byte fields, and the original rebased those fields in place
 * to host pointers, which fit because its pointers were 4 bytes. The 64-bit
 * core keeps the data formats exactly as they are and stores a 32-bit address
 * instead: the high byte names a 16 MB window of host memory, the low 24 bits
 * the offset inside it. Windows are assigned on first use; ids run 0x20..0x7F,
 * above the N64 segment numbers (0..15) and below the N64's 0x80000000
 * addresses, so neither kind of unrebased address is mistaken for one.
 * 0 stays NULL.
 *
 * Every write of a pointer into such a field goes through br_addr32(), every
 * read of one through br_ptr32(). Nothing else in the core uses 32-bit
 * pointers. */
#ifndef BR_ADDR32_H
#define BR_ADDR32_H
#ifdef __cplusplus
extern "C" {  /* BR_CLINK_BEGIN: every original function has C linkage */
#endif
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t br_addr32(const void *p);
void    *br_ptr32(uint32_t a);
void     br_addr32_window(const void *base);
#ifdef __cplusplus
}
#endif
#define BR_PTR32(T, a)  ((T)br_ptr32((uint32_t)(a)))
#ifdef __cplusplus
}  /* BR_CLINK_END */
#endif
#endif
