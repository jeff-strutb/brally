/* n64-cflags: -O3 */
/* scale.c -- libultra's scale matrices (gu/scale.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { int m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
/* -- end declarations -- */

/* WHAT IT DOES: Set a float matrix to a scale by (x, y, z). */
/* @implements 0x80260800 tgr guScaleF */
void guScaleF(float mf[4][4], float x, float y, float z)
{
	guMtxIdentF(mf);

	mf[0][0] = x;
	mf[1][1] = y;
	mf[2][2] = z;
	mf[3][3] = 1;
}

/* WHAT IT DOES: Set a fixed-point matrix to a scale by (x, y, z). */
/* @implements 0x80260854 tgr guScale */
void guScale(Mtx *m, float x, float y, float z)
{
	Matrix mf;

	guScaleF(mf, x, y, z);
	guMtxF2L(mf, m);
}
