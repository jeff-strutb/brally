/* stdint.h for the tools' reading of the core at the N64's ABI (ILP32), where
 * no C library headers exist. */
#ifndef TGR_TOOLS_STDINT_H
#define TGR_TOOLS_STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
#endif
