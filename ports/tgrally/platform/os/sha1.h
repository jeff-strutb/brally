#ifndef TGR_SHA1_H
#define TGR_SHA1_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t n; } Sha1;
void sha1_init(Sha1 *s);
void sha1_update(Sha1 *s, const void *data, size_t len);
void sha1_hex16(Sha1 *s, char out[17]);     /* the first 8 bytes, as hex */
#endif
