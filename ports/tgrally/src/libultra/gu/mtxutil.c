/* n64-cflags: -O3 */
/* mtxutil.c -- libultra's matrix utilities (gu/mtxutil.c): float identity,
 * the fixed-point identity, and float to fixed-point conversion.
 */

/* -- declarations -- */
typedef float Matrix[4][4];
typedef struct { int m[4][4]; } Mtx;
#define FTOFIX32(x) (int)((x) * (float)0x00010000)
#define FIX32TOF(x) ((float)(x) / (float)0x00010000)
void guMtxIdentF(float mf[4][4]);
/* -- end declarations -- */

/* WHAT IT DOES: Convert a float matrix to the RSP's 16.16 fixed-point
 * layout: the integer halves of each row pair in the first 8 words, the
 * fraction halves in the last 8. */
/* @implements 0x80260ED0 tgr guMtxF2L */
void guMtxF2L(float mf[4][4], Mtx *m)
{
	int i, j;
	int e1,e2;
	int *ai,*af;

	ai=(int *) &m->m[0][0];
	af=(int *) &m->m[2][0];

	for (i=0; i<4; i++)
	for (j=0; j<2; j++) {
		e1=FTOFIX32(mf[i][j*2]);
		e2=FTOFIX32(mf[i][j*2+1]);
		/* RSP memory: big-endian words (tgr_core.h) */
		tgr_wr32(ai++, ( e1 & 0xffff0000 ) | ((e2 >> 16)&0xffff));
		tgr_wr32(af++, ((e1 << 16) & 0xffff0000) | (e2 & 0xffff));
	}
}

/* WHAT IT DOES: Set a float matrix to the identity. */
/* @implements 0x80260FD0 tgr guMtxIdentF */
void guMtxIdentF(float mf[4][4])
{
	int r, c;

	for (r = 0; r < 4; r++)
	    for (c = 0; c < 4; c++)
		if (r == c)
		    mf[r][c] = 1.0;
		else
		    mf[r][c] = 0.0;
}

/* WHAT IT DOES: Set a fixed-point matrix to the identity. */
/* @implements 0x80261058 tgr guMtxIdent */
void guMtxIdent(Mtx *m)
{
	Matrix mf;

	guMtxIdentF(mf);
	guMtxF2L(mf, m);
}

/* WHAT IT DOES: Convert a 16.16 fixed-point matrix back to floats. */
/* @implements 0x80261088 tgr guMtxL2F */
void guMtxL2F(float mf[4][4], Mtx *m)
{
	int i, j;
	unsigned int e1,e2;
	unsigned int *ai,*af;
	int q1,q2;

	ai=(unsigned int *) &m->m[0][0];
	af=(unsigned int *) &m->m[2][0];

	for (i=0; i<4; i++)
	for (j=0; j<2; j++) {
		e1 = (tgr_rd32(ai) & 0xffff0000) | ((tgr_rd32(af) >> 16) & 0xffff);
		e2 = ((tgr_rd32(ai++) << 16) & 0xffff0000) | (tgr_rd32(af++) & 0xffff);
		q1 = *((int *)&e1);
		q2 = *((int *)&e2);
		mf[i][j*2] = FIX32TOF(q1);
		mf[i][j*2+1] = FIX32TOF(q2);
	}
}
