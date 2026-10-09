/* math.h: the port's C library (platform/libc) */
#ifndef PS1_MATH_H
#define PS1_MATH_H
#define M_PI 3.14159265358979323846
float  sqrtf(float x);
float  fabsf(float x);
float  fminf(float a, float b);
float  fmaxf(float a, float b);
long   lroundf(float x);
double fabs(double x);
double sqrt(double x);
double fmin(double a, double b);
double fmax(double a, double b);
double atan(double x);
double tan(double x);
double floor(double x);
float  floorf(float x);
#endif
