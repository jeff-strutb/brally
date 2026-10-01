/* lp64audit.py only: the 64-bit views take size_t and ptrdiff_t from the
 * compiler instead of MSVC 5.0's 32-bit typedefs. */
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#define _SIZE_T_DEFINED
#define _PTRDIFF_T_DEFINED
