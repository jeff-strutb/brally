/* msvc_intrinsics.h -- forced into every TU of the Mac 32-bit lane (port code).
 *
 * MSVC knows the prototype of every function on its intrinsic list, so the
 * decomp can write `#pragma intrinsic(sqrt)` and call sqrt() with no
 * <math.h> in sight -- the matching build gets FSQRT on a double.  clang has
 * no such knowledge: an undeclared sqrt() is implicitly `int sqrt()`, and
 * its double result was read back as an int.  BrSqrtF returned the square
 * root's integer truncation -- or garbage -- to every vector length in the
 * game.  These are the floating-point members of MSVC 5.0's intrinsic list,
 * with the CRT's own prototypes. */
#ifndef BR_MSVC_INTRINSICS_H
#define BR_MSVC_INTRINSICS_H
#ifdef __cplusplus
extern "C" {
#endif
double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double cos(double);
double cosh(double);
double exp(double);
double fabs(double);
double fmod(double, double);
double log(double);
double log10(double);
double pow(double, double);
double sin(double);
double sinh(double);
double sqrt(double);
double tan(double);
double tanh(double);
#ifdef __cplusplus
}
#endif
#endif
