/* simload.h -- the game's car records to the simulation's (simload.c) */
#ifndef SIMLOAD_H
#define SIMLOAD_H
#include <stddef.h>
#include "sim.h"

enum { SL_F32, SL_I32, SL_U8, SL_S8, SL_U16, SL_HIT };
typedef struct { int off, type; size_t at; int n; const char *name; } Field;
extern const Field sl_body[], sl_car[];
extern const int sl_nbody, sl_ncar, sl_bodies[5];

uint32_t sl_be32(const uint8_t *p);
uint16_t sl_be16(const uint8_t *p);
fx sl_fx(uint32_t bits);
void sl_field_load(const Field *f, uint8_t *obj, const uint8_t *rec);
void sl_car_load(Car *c, Pad *pad, const uint8_t *rec, const uint8_t *padrec, uint32_t linkFlags);
#endif
