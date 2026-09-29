/* n64-cflags: -O3 */
/* sqrtf.c -- libultra's single-precision square root (gu/sqrtf.c): the
 * FPU's own instruction, through IDO's intrinsic.
 */

/* -- declarations -- */
float sqrtf(float value);
#pragma intrinsic (sqrtf)
/* -- end declarations -- */

/* WHAT IT DOES: The square root of value (sqrt.s). */
/* @implements 0x80261140 tgr sqrtf */
float sqrtf(float value)
{
	return sqrtf(value);
}
