/* stdint.h: the port's C library.  32-bit types are int, as on every ILP32
 * target the game's code was written for (the bare-metal compiler's own
 * header makes them long). */
#ifndef PS1_STDINT_H
#define PS1_STDINT_H
typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;
typedef int                intptr_t;
typedef unsigned int       uintptr_t;
typedef long long          intmax_t;
typedef unsigned long long uintmax_t;
#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  255
#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 65535
#define INT32_MIN  (-2147483647 - 1)
#define INT32_MAX  2147483647
#define UINT32_MAX 4294967295u
#define INT64_MAX  9223372036854775807ll
#define INT64_MIN  (-INT64_MAX - 1)
#define UINT64_MAX 18446744073709551615ull
#define UINTPTR_MAX UINT32_MAX
#define SIZE_MAX   UINT32_MAX
#endif
