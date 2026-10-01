/* br_lp64.h: markers for the places where the decompiled code mixes a
 * pointer with a 32-bit integer. Each one is a site still to be retyped;
 * `grep BR_LP64_` lists them. The expansion keeps the original's bits. */
#ifndef BR_LP64_H
#define BR_LP64_H
#include <stdint.h>
#define BR_LP64_PTR_AS_INT(p)  ((int32_t)(intptr_t)(p))
#endif
