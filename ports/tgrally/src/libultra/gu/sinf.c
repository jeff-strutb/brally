/* n64-cflags: -O3 */
/* sinf.c -- libultra's single-precision sine (gu/sinf.c).
 */

/* -- declarations -- */
typedef union {             /* a double by its bit pattern, high word first */
	unsigned long long u;
	double d;
} du;
#define DU(hi, lo) {((unsigned long long)(hi) << 32) | (lo)}
typedef union {
	unsigned int i;
	float f;
} fu;
#define ROUND(d) (int)(((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
extern float __libm_qnan_f;
/* -- end declarations -- */

static const du P[] =
{
DU(0x3ff00000, 0x00000000),
DU(0xbfc55554, 0xbc83656d),
DU(0x3f8110ed, 0x3804c2a0),
DU(0xbf29f6ff, 0xeea56814),
DU(0x3ec5dbdf, 0x0e314bfe),
};

static const du rpi =
DU(0x3fd45f30, 0x6dc9c883);

static const du pihi =
DU(0x400921fb, 0x50000000);

static const du pilo =
DU(0x3e6110b4, 0x611a6263);

static const fu zero = {0x00000000};

/* WHAT IT DOES: sin(x) in double precision by Cody and Waite: x itself
 * below 2^-12, a degree-9 odd polynomial below 1.5, otherwise reduced by
 * multiples of pi first; a NaN gives the quiet NaN, |x| of 2^28 and above
 * gives 0. */
/* @implements 0x80261610 tgr sinf */
float sinf(float x)
{
	double dx, xsq, poly;
	double dn;
	int n;
	double result;
	int ix, xpt;

	ix = *(int *)&x;
	xpt = (ix >> 22);
	xpt &= 0x1ff;

	if ( xpt < 0xff )
	{
		dx = x;

		if ( xpt >= 0xe6 )
		{
			xsq = dx*dx;

			poly = ((P[4].d*xsq + P[3].d)*xsq + P[2].d)*xsq + P[1].d;

			result = dx + (dx*xsq)*poly;

			return ( (float)result );
		}

		return ( x );
	}

	if ( xpt < 0x136 )
	{
		dx = x;

		dn = dx*rpi.d;

		n = ROUND(dn);
		dn = n;

		dx = dx - dn*pihi.d;
		dx = dx - dn*pilo.d;

		xsq = dx*dx;

		poly = ((P[4].d*xsq + P[3].d)*xsq + P[2].d)*xsq + P[1].d;

		result = dx + (dx*xsq)*poly;


		if ( (n & 1) == 0 )
			return ( (float)result );

		return ( -(float)result );
	}

	if ( x != x )
	{
		return ( __libm_qnan_f );
	}

	return ( zero.f );
}
