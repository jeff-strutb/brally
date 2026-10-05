/* n64-cflags: -O3 */
/* perspective.c -- libultra's perspective projection (gu/perspective.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { long m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
float sqrtf(float value);
#pragma intrinsic (sqrtf)
typedef unsigned short u16;
float sinf(float x);
float cosf(float x);
#define NULL 0
/* -- end declarations -- */

/* WHAT IT DOES: Set a float matrix to a perspective projection (fovy in
 * degrees, aspect, near and far planes), every element scaled by scale,
 * and give the RSP's perspective normalisation for the near/far pair. */
/* @implements 0x80265180 tgr guPerspectiveF */
void guPerspectiveF(float mf[4][4], u16 *perspNorm, float fovy, float aspect, float near, float far, float scale)
{
	float cot;
	int i, j;

	guMtxIdentF(mf);

	fovy *= 3.1415926 / 180.0;
	cot = cosf (fovy/2) / sinf (fovy/2);

	mf[0][0] = cot / aspect;
	mf[1][1] = cot;
	mf[2][2] = (near + far) / (near - far);
	mf[2][3] = -1;
	mf[3][2] = (2 * near * far) / (near - far);
	mf[3][3] = 0;

	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		mf[i][j] *= scale;

	if (perspNorm != (u16 *) NULL) {
	    if (near+far<=2.0) {
		*perspNorm = (u16) 0xFFFF;
	    } else  {
		*perspNorm = (u16) ((2.0*65536.0)/(near+far));
		if (*perspNorm<=0)
		    *perspNorm = (u16) 0x0001;
	    }
	}
}

/* WHAT IT DOES: guPerspectiveF into a fixed-point matrix. */
/* @implements 0x802653B0 tgr guPerspective */
void guPerspective(Mtx *m, u16 *perspNorm, float fovy, float aspect, float near, float far, float scale)
{
	Matrix mf;

	guPerspectiveF(mf, perspNorm, fovy, aspect, near, far, scale);
	guMtxF2L(mf, m);
}
