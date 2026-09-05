/* Matching TU for 0x1000E270 -- project a point's x/y through the combined matrix. */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

extern float g_BrDrawCombined[4][4];           /* 0x106E78F0 */

/* WHAT IT DOES: takes a point record (x, y, z at +0/+4/+8) and writes its
 * transformed screen x and y (at +0xC and +0x10) through the first two
 * columns of the combined view matrix, translation included.  Only x and y
 * are produced; z/w are left alone. */
/* @implements 0x1000E270 glide BrPointProjectXY */
void BrPointProjectXY(float *v)
{
    /* Declaration order y, z, x and assignment order x, y, z: together they
     * put x in the fresh frame dword, y in the dead parameter slot, and z
     * on the x87 stack, with x copied through ecx first.  Any other pairing
     * measured 4-9 bytes off (all six declaration orders probed). */
    float y;
    float z;
    float x;

    x = v[0];
    y = v[1];
    z = v[2];

    v[3] = x * g_BrDrawCombined[0][0] + y * g_BrDrawCombined[1][0]
         + z * g_BrDrawCombined[2][0] + g_BrDrawCombined[3][0];
    v[4] = x * g_BrDrawCombined[0][1] + y * g_BrDrawCombined[1][1]
         + z * g_BrDrawCombined[2][1] + g_BrDrawCombined[3][1];
}

#endif /* BR_MATCHING_BUILD */
