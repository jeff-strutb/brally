/* n64-cflags: -O3 */
/* lookatref.c -- libultra's viewing matrices with reflection-map lights
 * (gu/lookatref.c).
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { long m[4][4]; } Mtx;
void guMtxIdentF(float mf[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
float sqrtf(float value);
typedef struct {
	unsigned char col[3];
	char pad1;
	unsigned char colc[3];
	char pad2;
	signed char dir[3];
	char pad3;
} Light_t;
typedef union {
	Light_t l;
	long long force_structure_alignment[2];
} Light;
typedef struct {
	Light l[2];
} LookAt;
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define FTOFRAC8(x) ((int) MIN(((x) * (128.0)), 127.0) & 0xff)
/* -- end declarations -- */

/* WHAT IT DOES: guLookAtF, also setting the two reflection-map lights to
 * the view's right and up axes (8-bit fractions) with their fixed
 * colours. */
/* @implements 0x80261150 tgr guLookAtReflectF */
void guLookAtReflectF(float mf[4][4], LookAt *l,
		      float xEye, float yEye, float zEye,
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

	l->l[0].l.dir[0] = FTOFRAC8(xRight);
	l->l[0].l.dir[1] = FTOFRAC8(yRight);
	l->l[0].l.dir[2] = FTOFRAC8(zRight);
	l->l[1].l.dir[0] = FTOFRAC8(xUp);
	l->l[1].l.dir[1] = FTOFRAC8(yUp);
	l->l[1].l.dir[2] = FTOFRAC8(zUp);
	l->l[0].l.col[0] = 0x00;
	l->l[0].l.col[1] = 0x00;
	l->l[0].l.col[2] = 0x00;
	l->l[0].l.pad1 = 0x00;
	l->l[0].l.colc[0] = 0x00;
	l->l[0].l.colc[1] = 0x00;
	l->l[0].l.colc[2] = 0x00;
	l->l[0].l.pad2 = 0x00;
	l->l[1].l.col[0] = 0x00;
	l->l[1].l.col[1] = 0x80;
	l->l[1].l.col[2] = 0x00;
	l->l[1].l.pad1 = 0x00;
	l->l[1].l.colc[0] = 0x00;
	l->l[1].l.colc[1] = 0x80;
	l->l[1].l.colc[2] = 0x00;
	l->l[1].l.pad2 = 0x00;

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

/* WHAT IT DOES: guLookAtReflectF into a fixed-point matrix. */
/* @implements 0x80261590 tgr guLookAtReflect */
void guLookAtReflect (Mtx *m, LookAt *l, float xEye, float yEye, float zEye,
		      float xAt,  float yAt,  float zAt,
		      float xUp,  float yUp,  float zUp)
{
	Matrix mf;

	guLookAtReflectF(mf, l, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);

	guMtxF2L(mf, m);
}
