/* n64-cflags: -O3 */
/* rotate.c -- libultra's axis-angle rotation matrices (gu/rotate.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { long m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
void guNormalize(float *x, float *y, float *z);
float sinf(float x);
float cosf(float x);
/* -- end declarations -- */

/* WHAT IT DOES: Set a float matrix to a rotation of a degrees about the
 * axis (x, y, z) (normalized first). */
/* @implements 0x80260BC0 tgr guRotateF */
void guRotateF(float mf[4][4], float a, float x, float y, float z)
{
	static float dtor = 3.1415926 / 180.0;
	float sine;
	float cosine;
	float ab, bc, ca, t;

	guNormalize(&x, &y, &z);
	a *= dtor;
	sine = sinf(a);
	cosine = cosf(a);
	t = (1-cosine);
	ab = x*y*t;
	bc = y*z*t;
	ca = z*x*t;

	guMtxIdentF(mf);

	t = x*x;
	mf[0][0] = t+cosine*(1-t);
	mf[2][1] = bc-x*sine;
	mf[1][2] = bc+x*sine;

	t = y*y;
	mf[1][1] = t+cosine*(1-t);
	mf[2][0] = ca+y*sine;
	mf[0][2] = ca-y*sine;

	t = z*z;
	mf[2][2] = t+cosine*(1-t);
	mf[1][0] = ab-z*sine;
	mf[0][1] = ab+z*sine;
}

/* WHAT IT DOES: Set a fixed-point matrix to a rotation of a degrees about
 * the axis (x, y, z). */
/* @implements 0x80260D54 tgr guRotate */
void guRotate(Mtx *m, float a, float x, float y, float z)
{
	Matrix mf;

	guRotateF(mf, a, x, y, z);
	guMtxF2L(mf, m);
}
