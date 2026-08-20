/* stdint.h shim for MSVC 5.0 (pre-C99). */
#ifndef _STDINT_H_SHIM
#define _STDINT_H_SHIM

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;

typedef int                intptr_t;
typedef unsigned int       uintptr_t;

#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  255
#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 65535
#define INT32_MIN  (-2147483647 - 1)
#define INT32_MAX  2147483647
#define UINT32_MAX 0xFFFFFFFFU

#endif /* _STDINT_H_SHIM */
