/* n64-cflags: -O3 */
/* mtxcatl.c -- libultra's fixed-point matrix product.
 */

/* -- declarations -- */
typedef struct { long m[4][4]; } Mtx;
void guMtxL2F(float mf[4][4], Mtx *m);
void guMtxF2L(float mf[4][4], Mtx *m);
void guMtxCatF(float mf[4][4], float nf[4][4], float res[4][4]);
/* -- end declarations -- */

/* WHAT IT DOES: res = m * n for fixed-point matrices (through floats). */
/* @implements 0x80267470 tgr guMtxCatL */
void guMtxCatL(Mtx *m, Mtx *n, Mtx *res)
{
	float mf[4][4], nf[4][4], resf[4][4];

	guMtxL2F(mf, m);
	guMtxL2F(nf, n);
	guMtxCatF(mf, nf, resf);
	guMtxF2L(resf, res);
}
