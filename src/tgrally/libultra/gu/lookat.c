/* n64-cflags: -O3 */
/* lookat.c -- libultra's viewing matrices (gu/lookat.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { long m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
float sqrtf(float value);
/* -- end declarations -- */

/* WHAT IT DOES: Set a float matrix to the view from an eye point towards
 * an at point with the given up direction (right, up and look axes made
 * orthonormal, the eye's position folded into the last row). */
/* @implements 0x80264E50 tgr guLookAtF */
void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye,
	      float xAt,  float yAt,  float zAt,
	      float xUp,  float yUp,  float zUp)
{
	float len, xLook, yLook, zLook, xRight, yRight, zRight;

	guMtxIdentF(mf);

	xLook = xAt - xEye;
	yLook = yAt - yEye;
	zLook = zAt - zEye;

	len = -1.0 / sqrtf (xLook*xLook + yLook*yLook + zLook*zLook);
	xLook *= len;
	yLook *= len;
	zLook *= len;

	xRight = yUp * zLook - zUp * yLook;
	yRight = zUp * xLook - xUp * zLook;
	zRight = xUp * yLook - yUp * xLook;
	len = 1.0 / sqrtf (xRight*xRight + yRight*yRight + zRight*zRight);
	xRight *= len;
	yRight *= len;
	zRight *= len;

	xUp = yLook * zRight - zLook * yRight;
	yUp = zLook * xRight - xLook * zRight;
	zUp = xLook * yRight - yLook * xRight;
	len = 1.0 / sqrtf (xUp*xUp + yUp*yUp + zUp*zUp);
	xUp *= len;
	yUp *= len;
	zUp *= len;

	mf[0][0] = xRight;
	mf[1][0] = yRight;
	mf[2][0] = zRight;
	mf[3][0] = -(xEye * xRight + yEye * yRight + zEye * zRight);

	mf[0][1] = xUp;
	mf[1][1] = yUp;
	mf[2][1] = zUp;
	mf[3][1] = -(xEye * xUp + yEye * yUp + zEye * zUp);

	mf[0][2] = xLook;
	mf[1][2] = yLook;
	mf[2][2] = zLook;
	mf[3][2] = -(xEye * xLook + yEye * yLook + zEye * zLook);

	mf[0][3] = 0;
	mf[1][3] = 0;
	mf[2][3] = 0;
	mf[3][3] = 1;
}

/* WHAT IT DOES: guLookAtF into a fixed-point matrix. */
/* @implements 0x80265108 tgr guLookAt */
void guLookAt (Mtx *m, float xEye, float yEye, float zEye,
		float xAt,  float yAt,  float zAt,
		float xUp,  float yUp,  float zUp)
{
	Matrix mf;

	guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

	guMtxF2L(mf, m);
}
