// dlldef.cpp - definitions for C++ DLL

#include <limits>
#include <istream>
#include <sstream>
#include <xstring>
#include <iomanip>
#include <fstream>
#include <locale>
#include <complex>

_STD_BEGIN


#pragma warning(disable:4231) /* the extern before template is a non-standard extension */
numeric_limits<char>;
numeric_limits<_Bool>;
numeric_limits<signed char>;
numeric_limits<unsigned char>;
numeric_limits<short>;
numeric_limits<unsigned short>;
numeric_limits<int>;
numeric_limits<unsigned int>;
numeric_limits<long>;
numeric_limits<unsigned long>;
numeric_limits<float>;
numeric_limits<double>;
numeric_limits<long double>;

complex<float>;
complex<double>;
complex<long double>;

_Smanip<int>;

template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, char *);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, char&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, signed char *);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, signed char&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, unsigned char *);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
					basic_istream<char, char_traits<char> >&, unsigned char&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
					basic_istream<wchar_t, char_traits<wchar_t> >&, wchar_t *);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
					basic_istream<wchar_t, char_traits<wchar_t> >&, wchar_t&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
					basic_istream<wchar_t, char_traits<wchar_t> >&, signed short *);

template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, const char *);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, char);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, const signed char *);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, const signed char);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, const unsigned char *);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
					basic_ostream<char, char_traits<char> >&, const unsigned char);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
					basic_ostream<wchar_t, char_traits<wchar_t> >&, const wchar_t *);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
					basic_ostream<wchar_t, char_traits<wchar_t> >&, const wchar_t);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
					basic_ostream<wchar_t, char_traits<wchar_t> >&, const signed short *);

template _CRTIMP2 basic_string<char, char_traits<char>, allocator<char> > operator+(
		const basic_string<char, char_traits<char>, allocator<char> >&,
	    const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 basic_string<char, char_traits<char>, allocator<char> > operator+(
    	const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 basic_string<char, char_traits<char>, allocator<char> > operator+(
		const char, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 basic_string<char, char_traits<char>, allocator<char> > operator+(
		const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 basic_string<char, char_traits<char>, allocator<char> > operator+(
	    const basic_string<char, char_traits<char>, allocator<char> >&,	const char);
template _CRTIMP2 bool operator==(
        const basic_string<char, char_traits<char>, allocator<char> >&,
        const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator==(
        const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator==(
        const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 bool operator!=(
        const basic_string<char, char_traits<char>, allocator<char> >&,
        const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator!=(
        const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator!=(
        const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 bool operator<(
        const basic_string<char, char_traits<char>, allocator<char> >&,
        const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator<(
		const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator<(
		const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 bool operator>(
		const basic_string<char, char_traits<char>, allocator<char> >&,
		const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator>(
		const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator>(
		const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 bool operator<=(
		const basic_string<char, char_traits<char>, allocator<char> >&,
		const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator<=(
		const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator<=(
		const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 bool operator>=(
		const basic_string<char, char_traits<char>, allocator<char> >&,
		const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator>=(
		const char *, const basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 bool operator>=(
		const basic_string<char, char_traits<char>, allocator<char> >&, const char *);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
		basic_istream<char, char_traits<char> >&,
		basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& getline(
		basic_istream<char, char_traits<char> >&,
		basic_string<char, char_traits<char>, allocator<char> >&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& getline(
		basic_istream<char, char_traits<char> >&,
		basic_string<char, char_traits<char>, allocator<char> >&, const char);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
		basic_ostream<char, char_traits<char> >&,
		const basic_string<char, char_traits<char>, allocator<char> >&);

template _CRTIMP2 basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> > operator+(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
	    const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> > operator+(
    	const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> > operator+(
		const wchar_t, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> > operator+(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> > operator+(
	    const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,	const wchar_t);
template _CRTIMP2 bool operator==(
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator==(
        const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator==(
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 bool operator!=(
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator!=(
        const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator!=(
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 bool operator<(
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
        const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator<(
		const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator<(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 bool operator>(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator>(
		const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator>(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 bool operator<=(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator<=(
		const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator<=(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 bool operator>=(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&,
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator>=(
		const wchar_t *, const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 bool operator>=(
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t *);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
		basic_istream<wchar_t, char_traits<wchar_t> >&,
		basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& getline(
		basic_istream<wchar_t, char_traits<wchar_t> >&,
		basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& getline(
		basic_istream<wchar_t, char_traits<wchar_t> >&,
		basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&, const wchar_t);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
		basic_ostream<wchar_t, char_traits<wchar_t> >&,
		const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >&);

template _CRTIMP2 complex<float>& operator+=(
		complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float>& operator-=(
		complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float>& operator*=(
		complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float>& operator/=(
		complex<float>&, const complex<float>&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
		basic_istream<char, char_traits<char> >&, complex<float>&);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
		basic_ostream<char, char_traits<char> >&, const complex<float>&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
		basic_istream<wchar_t, char_traits<wchar_t> >&, complex<float>&);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
		basic_ostream<wchar_t, char_traits<wchar_t> >&, const complex<float>&);

template _CRTIMP2 complex<double>& operator+=(
		complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double>& operator-=(
		complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double>& operator*=(
		complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double>& operator/=(
		complex<double>&, const complex<double>&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
		basic_istream<char, char_traits<char> >&, complex<double>&);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
		basic_ostream<char, char_traits<char> >&, const complex<double>&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
		basic_istream<wchar_t, char_traits<wchar_t> >&, complex<double>&);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
		basic_ostream<wchar_t, char_traits<wchar_t> >&, const complex<double>&);

template _CRTIMP2 complex<long double>& operator+=(
		complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double>& operator-=(
		complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double>& operator*=(
		complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double>& operator/=(
		complex<long double>&, const complex<long double>&);
template _CRTIMP2 basic_istream<char, char_traits<char> >& operator>>(
		basic_istream<char, char_traits<char> >&, complex<long double>&);
template _CRTIMP2 basic_ostream<char, char_traits<char> >& operator<<(
		basic_ostream<char, char_traits<char> >&, const complex<long double>&);
template _CRTIMP2 basic_istream<wchar_t, char_traits<wchar_t> >& operator>>(
		basic_istream<wchar_t, char_traits<wchar_t> >&, complex<long double>&);
template _CRTIMP2 basic_ostream<wchar_t, char_traits<wchar_t> >& operator<<(
		basic_ostream<wchar_t, char_traits<wchar_t> >&, const complex<long double>&);

template _CRTIMP2 float imag(const complex<float>&);
template _CRTIMP2 float real(const complex<float>&);
template _CRTIMP2 float _Fabs(const complex<float>&, int *);
template _CRTIMP2 complex<float> operator+(const complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float> operator+(const complex<float>&, const float&);
template _CRTIMP2 complex<float> operator+(const float&, const complex<float>&);
template _CRTIMP2 complex<float> operator-(const complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float> operator-(const complex<float>&, const float&);
template _CRTIMP2 complex<float> operator-(const float&, const complex<float>&);
template _CRTIMP2 complex<float> operator*(const complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float> operator*(const complex<float>&, const float&);
template _CRTIMP2 complex<float> operator*(const float&, const complex<float>&);
template _CRTIMP2 complex<float> operator/(const complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float> operator/(const complex<float>&, const float&);
template _CRTIMP2 complex<float> operator/(const float&, const complex<float>&);
template _CRTIMP2 complex<float> operator+(const complex<float>&);
template _CRTIMP2 complex<float> operator-(const complex<float>&);
template _CRTIMP2 bool operator==(const complex<float>&, const complex<float>&);
template _CRTIMP2 bool operator==(const complex<float>&, const float&);
template _CRTIMP2 bool operator==(const float&, const complex<float>&);
template _CRTIMP2 bool operator!=(const complex<float>&, const complex<float>&);
template _CRTIMP2 bool operator!=(const complex<float>&, const float&);
template _CRTIMP2 bool operator!=(const float&, const complex<float>&);
template _CRTIMP2 float abs(const complex<float>&);
template _CRTIMP2 float arg(const complex<float>&);
template _CRTIMP2 complex<float> conj(const complex<float>&);
template _CRTIMP2 complex<float> cos(const complex<float>&);
template _CRTIMP2 complex<float> cosh(const complex<float>&);
template _CRTIMP2 complex<float> exp(const complex<float>&);
template _CRTIMP2 complex<float> log(const complex<float>&);
template _CRTIMP2 complex<float> log10(const complex<float>&);
template _CRTIMP2 float norm(const complex<float>&);
template _CRTIMP2 complex<float> polar(const float&, const float&);
template _CRTIMP2 complex<float> polar(const float&);
template _CRTIMP2 complex<float> pow(const complex<float>&, const complex<float>&);
template _CRTIMP2 complex<float> pow(const complex<float>&, const float&);
template _CRTIMP2 complex<float> pow(const complex<float>&, int);
template _CRTIMP2 complex<float> pow(const float&, const complex<float>&);
template _CRTIMP2 complex<float> sin(const complex<float>&);
template _CRTIMP2 complex<float> sinh(const complex<float>&);
template _CRTIMP2 complex<float> sqrt(const complex<float>&);

template _CRTIMP2 double imag(const complex<double>&);
template _CRTIMP2 double real(const complex<double>&);
template _CRTIMP2 double _Fabs(const complex<double>&, int *);
template _CRTIMP2 complex<double> operator+(const complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double> operator+(const complex<double>&, const double&);
template _CRTIMP2 complex<double> operator+(const double&, const complex<double>&);
template _CRTIMP2 complex<double> operator-(const complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double> operator-(const complex<double>&, const double&);
template _CRTIMP2 complex<double> operator-(const double&, const complex<double>&);
template _CRTIMP2 complex<double> operator*(const complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double> operator*(const complex<double>&, const double&);
template _CRTIMP2 complex<double> operator*(const double&, const complex<double>&);
template _CRTIMP2 complex<double> operator/(const complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double> operator/(const complex<double>&, const double&);
template _CRTIMP2 complex<double> operator/(const double&, const complex<double>&);
template _CRTIMP2 complex<double> operator+(const complex<double>&);
template _CRTIMP2 complex<double> operator-(const complex<double>&);
template _CRTIMP2 bool operator==(const complex<double>&, const complex<double>&);
template _CRTIMP2 bool operator==(const complex<double>&, const double&);
template _CRTIMP2 bool operator==(const double&, const complex<double>&);
template _CRTIMP2 bool operator!=(const complex<double>&, const complex<double>&);
template _CRTIMP2 bool operator!=(const complex<double>&, const double&);
template _CRTIMP2 bool operator!=(const double&, const complex<double>&);
template _CRTIMP2 double abs(const complex<double>&);
template _CRTIMP2 double arg(const complex<double>&);
template _CRTIMP2 complex<double> conj(const complex<double>&);
template _CRTIMP2 complex<double> cos(const complex<double>&);
template _CRTIMP2 complex<double> cosh(const complex<double>&);
template _CRTIMP2 complex<double> exp(const complex<double>&);
template _CRTIMP2 complex<double> log(const complex<double>&);
template _CRTIMP2 complex<double> log10(const complex<double>&);
template _CRTIMP2 double norm(const complex<double>&);
template _CRTIMP2 complex<double> polar(const double&, const double&);
template _CRTIMP2 complex<double> polar(const double&);
template _CRTIMP2 complex<double> pow(const complex<double>&, const complex<double>&);
template _CRTIMP2 complex<double> pow(const complex<double>&, const double&);
template _CRTIMP2 complex<double> pow(const complex<double>&, int);
template _CRTIMP2 complex<double> pow(const double&, const complex<double>&);
template _CRTIMP2 complex<double> sin(const complex<double>&);
template _CRTIMP2 complex<double> sinh(const complex<double>&);
template _CRTIMP2 complex<double> sqrt(const complex<double>&);

template _CRTIMP2 long double imag(const complex<long double>&);
template _CRTIMP2 long double real(const complex<long double>&);
template _CRTIMP2 long double _Fabs(const complex<long double>&, int *);
template _CRTIMP2 complex<long double> operator+(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator+(const complex<long double>&, const long double&);
template _CRTIMP2 complex<long double> operator+(const long double&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator-(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator-(const complex<long double>&, const long double&);
template _CRTIMP2 complex<long double> operator-(const long double&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator*(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator*(const complex<long double>&, const long double&);
template _CRTIMP2 complex<long double> operator*(const long double&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator/(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator/(const complex<long double>&, const long double&);
template _CRTIMP2 complex<long double> operator/(const long double&, const complex<long double>&);
template _CRTIMP2 complex<long double> operator+(const complex<long double>&);
template _CRTIMP2 complex<long double> operator-(const complex<long double>&);
template _CRTIMP2 bool operator==(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 bool operator==(const complex<long double>&, const long double&);
template _CRTIMP2 bool operator==(const long double&, const complex<long double>&);
template _CRTIMP2 bool operator!=(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 bool operator!=(const complex<long double>&, const long double&);
template _CRTIMP2 bool operator!=(const long double&, const complex<long double>&);
template _CRTIMP2 long double abs(const complex<long double>&);
template _CRTIMP2 long double arg(const complex<long double>&);
template _CRTIMP2 complex<long double> conj(const complex<long double>&);
template _CRTIMP2 complex<long double> cos(const complex<long double>&);
template _CRTIMP2 complex<long double> cosh(const complex<long double>&);
template _CRTIMP2 complex<long double> exp(const complex<long double>&);
template _CRTIMP2 complex<long double> log(const complex<long double>&);
template _CRTIMP2 complex<long double> log10(const complex<long double>&);
template _CRTIMP2 long double norm(const complex<long double>&);
template _CRTIMP2 complex<long double> polar(const long double&, const long double&);
template _CRTIMP2 complex<long double> polar(const long double&);
template _CRTIMP2 complex<long double> pow(const complex<long double>&, const complex<long double>&);
template _CRTIMP2 complex<long double> pow(const complex<long double>&, const long double&);
template _CRTIMP2 complex<long double> pow(const complex<long double>&, int);
template _CRTIMP2 complex<long double> pow(const long double&, const complex<long double>&);
template _CRTIMP2 complex<long double> sin(const complex<long double>&);
template _CRTIMP2 complex<long double> sinh(const complex<long double>&);
template _CRTIMP2 complex<long double> sqrt(const complex<long double>&);

extern template basic_istringstream<char,struct char_traits<char>,class allocator<char> >;
extern template basic_istringstream<wchar_t,struct char_traits<wchar_t>,class allocator<wchar_t> >;
extern template basic_ostringstream<char,struct char_traits<char>,class allocator<char> >;
extern template basic_ostringstream<wchar_t,struct char_traits<wchar_t>,class allocator<wchar_t> >;

extern template basic_stringstream<char,struct char_traits<char>,class allocator<char> >;
extern template basic_stringstream<wchar_t,struct char_traits<wchar_t>,class allocator<wchar_t> >;

extern template basic_ifstream<char,struct char_traits<char> >;
extern template basic_ifstream<wchar_t,struct char_traits<wchar_t> >;
extern template basic_ofstream<char,struct char_traits<char> >;
extern template basic_ofstream<wchar_t,struct char_traits<wchar_t> >;

extern template basic_fstream<char,struct char_traits<char> >;
extern template basic_fstream<wchar_t,struct char_traits<wchar_t> >;

codecvt<char, char, int>;
codecvt<wchar_t, char, int>;

template locale::id codecvt<char,char,int>::id;

ctype<char>;
ctype<wchar_t>;

numpunct<char>;
numpunct<wchar_t>;

moneypunct<char, true>;
moneypunct<char, false>;
moneypunct<wchar_t, true>;
moneypunct<wchar_t, false>;

money_get<char, istreambuf_iterator<char, char_traits<char> > >;
money_get<wchar_t, istreambuf_iterator<wchar_t, char_traits<wchar_t> > >;
money_put<char, ostreambuf_iterator<char, char_traits<char> > >;
money_put<wchar_t, ostreambuf_iterator<wchar_t, char_traits<wchar_t> > >;

time_get<char, istreambuf_iterator<char, char_traits<char> > >;
time_get<wchar_t, istreambuf_iterator<wchar_t, char_traits<wchar_t> > >;
time_put<char, ostreambuf_iterator<char, char_traits<char> > >;
time_put<wchar_t, ostreambuf_iterator<wchar_t, char_traits<wchar_t> > >;

messages<char>;
messages<wchar_t>;

template const basic_string<char, char_traits<char>, allocator<char> >::size_type
                            basic_string<char, char_traits<char>, allocator<char> >::npos;

template const basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >::size_type
                            basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >::npos;
bad_alloc;
#pragma warning(default:4231) /* restore previous warning */

_STD_END
