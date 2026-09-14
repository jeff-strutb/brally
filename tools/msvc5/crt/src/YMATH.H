/* ymath.h internal header */
#ifndef _YMATH
#define _YMATH
#include <wctype.h>
#include <math.h>
#ifndef _YVALS
#include <yvals.h>
#endif
		/* MACROS */
#define _FINITE		-1
#define _INFCODE	1
#define _NANCODE	2
		/* TYPE DEFINITIONS */
typedef union {
	unsigned short _W[5];
	float _F;
	double _D;
	long double _L;
	} _Dconst;
_C_LIB_DECL
		/* double DECLARATIONS */
_CRTIMP2 double _Cosh(double, double);
_CRTIMP2 short __cdecl _Dtest(double *);
_CRTIMP2 short _Exp(double *, double, short);
_CRTIMP2 double _Log(double, int);
_CRTIMP2 double _Sin(double, unsigned int);
_CRTIMP2 double _Sinh(double, double);
extern _CRTIMP2 const _Dconst _Denorm, _Hugeval, _Inf, _Nan, _Snan;
		/* float DECLARATIONS */
_CRTIMP2 float _FCosh(float, float);
_CRTIMP2 short __cdecl _FDtest(float *);
_CRTIMP2 short _FExp(float *, float, short);
_CRTIMP2 float _FLog(float, int);
_CRTIMP2 float _FSin(float, unsigned int);
_CRTIMP2 float _FSinh(float, float);
extern _CRTIMP2 const _Dconst _FDenorm, _FInf, _FNan, _FSnan;
		/* long double DECLARATIONS */
_CRTIMP2 long double _LCosh(long double, long double);
_CRTIMP2 short __cdecl _LDtest(long double *);
_CRTIMP2 short _LExp(long double *, long double, short);
_CRTIMP2 long double _LLog(long double, int);
_CRTIMP2 long double _LSin(long double, unsigned int);
_CRTIMP2 long double _LSinh(long double, long double);
extern _CRTIMP2 const _Dconst _LDenorm, _LInf, _LNan, _LSnan;
_END_C_LIB_DECL
#endif /* _YMATH */

/*
 * Copyright (c) 1995 by P.J. Plauger.  ALL RIGHTS RESERVED. 
 * Consult your license regarding permissions and restrictions.
 */
