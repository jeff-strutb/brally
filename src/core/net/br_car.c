/* br_car.c -- net.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD


/* 0x10005900 */
/* WHAT IT DOES: keeps one of a car's two horizontal position coordinates
 * inside the track's 0-to-2048 world, so a rogue value cannot fling a
 * networked car off the map. */
/* @implements 0x10005900 d3d BrCarClampPosXY */
void BrCarClampPosXY(float *pv)
{
    if (!(*pv >= 0.0f))
        *pv = 0.0f;
    if (*pv > 2048.0f)
        *pv = 2048.0f;
}

#endif /* BR_MATCHING_BUILD */
