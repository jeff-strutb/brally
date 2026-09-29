/* n64-cflags: -O3 */
/* mtxxfml.c -- libultra's point transform by a fixed-point matrix.
 */

/* -- declarations -- */
typedef struct { long m[4][4]; } Mtx;
void guMtxL2F(float mf[4][4], Mtx *m);
void guMtxXFMF(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz);
/* -- end declarations -- */

/* WHAT IT DOES: guMtxXFMF through a fixed-point matrix. */
/* @implements 0x80267410 tgr guMtxXFML */
void guMtxXFML(Mtx *m, float x, float y, float z, float *ox, float *oy, float *oz)
{
	float mf[4][4];

	guMtxL2F(mf, m);
	guMtxXFMF(mf, x, y, z, ox, oy, oz);
}
