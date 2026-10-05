/* n64-cflags: -O3 */
/* normalize.c -- libultra's vector normalization (gu/normalize.c).
 */

/* -- declarations -- */
float sqrtf(float value);
/* -- end declarations -- */

/* WHAT IT DOES: Scale (*x, *y, *z) to unit length. */
/* @implements 0x802686C0 tgr guNormalize */
void guNormalize(float *x, float *y, float *z)
{
	float m;

	m = 1/sqrtf((*x)*(*x) + (*y)*(*y) + (*z)*(*z));
	*x *= m;
	*y *= m;
	*z *= m;
}
