/* n64-cflags: -O3 */
/* mtxxfm.c -- libultra's point transform by a float matrix (gu/mtxxfm.c).
 */

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Transform the point (x, y, z) by a float matrix (row
 * vectors, translation in the last row) into (*ox, *oy, *oz). */
/* @implements 0x802608A0 tgr guMtxXFMF */
void guMtxXFMF(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz)
{
	*ox = mf[0][0]*x + mf[1][0]*y + mf[2][0]*z + mf[3][0];
	*oy = mf[0][1]*x + mf[1][1]*y + mf[2][1]*z + mf[3][1];
	*oz = mf[0][2]*x + mf[1][2]*y + mf[2][2]*z + mf[3][2];
}
