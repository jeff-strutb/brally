/* n64-cflags: -O3 */
/* mtxcat.c -- libultra's float matrix product (gu/mtxcat.c).
 */

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: res = mf * nf, through a temporary so res may be either
 * input. */
/* @implements 0x80260940 tgr guMtxCatF */
void guMtxCatF(float mf[4][4], float nf[4][4], float res[4][4])
{
	int i, j, k;
	float temp[4][4];

	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) {
			temp[i][j] = 0.0;
			for (k=0; k<4; k++) {
				temp[i][j] += mf[i][k] * nf[k][j];
			}
		}
	}

	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) {
			res[i][j] = temp[i][j];
		}
	}
}
