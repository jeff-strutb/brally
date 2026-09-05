/* Matching TU for 0x10034870 -- transform a point by a 4x4 and divide by w. */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

/* WHAT IT DOES: full 4x4 point transform with perspective divide -- computes
 * w from the matrix's fourth column first, then writes x, y, z as each row
 * product plus translation, scaled by 1/w.  Column-major (m[row][col],
 * translation in m[3]). */
/* @implements 0x10034870 glide BrVec3TransformDivW */
void BrVec3TransformDivW(float *d, const float *s, const float (*m)[4])
{
    float x = s[0];
    float y = s[1];
    float z = s[2];
    float w = 1.0f / (y * m[1][3] + x * m[0][3] + z * m[2][3] + m[3][3]);
    /* Row 0's x product is named so it is evaluated FIRST (`fld st(4);
     * fmul [eax]`): inside the flat sum the canonicaliser puts the
     * zero-offset product last. */
    float tx = x * m[0][0];

    d[0] = (tx + y * m[1][0] + z * m[2][0] + m[3][0]) * w;
    d[1] = (x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1]) * w;
    d[2] = (x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2]) * w;
}

#endif /* BR_MATCHING_BUILD */
