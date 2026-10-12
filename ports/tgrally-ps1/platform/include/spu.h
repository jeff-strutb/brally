/* spu.h: the sound processing unit (hw/spu.c) */
#ifndef PS1_SPU_H
#define PS1_SPU_H
#include <stdint.h>
#define SPU_RAM_FIRST 0x1010                    /* sound RAM below this is the SPU's own */
#define SPU_RAM_END   0x80000
void spu_init(void);
void spu_write(uint32_t addr, const void *data, uint32_t bytes);   /* to sound RAM (8-byte aligned) */
void spu_voice_start(int v, uint32_t addr);     /* key on from addr */
void spu_voice_off(int v);
void spu_voice_pitch(int v, uint32_t pitch);    /* 0x1000 = 44.1 kHz */
void spu_voice_volume(int v, int l, int r);     /* 0..0x3FFF */
void spu_keys(void);                            /* the key-ons and -offs gathered, at once */
#endif
