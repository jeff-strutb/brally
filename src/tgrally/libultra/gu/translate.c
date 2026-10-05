/* n64-cflags: -O3 */
/* translate.c -- libultra's translation matrices (gu/translate.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { long m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
/* -- end declarations -- */

/* WHAT IT DOES: Set a float matrix to a translation by (x, y, z). */
/* @implements 0x80260E30 tgr guTranslateF */
void guTranslateF(float mf[4][4], float x, float y, float z)
{
	guMtxIdentF(mf);

	mf[3][0] = x;
	mf[3][1] = y;
	mf[3][2] = z;
}

/* WHAT IT DOES: Set a fixed-point matrix to a translation by (x, y, z). */
/* @implements 0x80260E78 tgr guTranslate */
void guTranslate(Mtx *m, float x, float y, float z)
{
	Matrix mf;

	guTranslateF(mf, x, y, z);
	guMtxF2L(mf, m);
}
