/* xprintf.c: libultra's formatted output (libc/xprintf.c, xlitob.c,
 * xldtob.c: _Printf, _Putfld, _Litob, _Ldtob and its helpers), which the
 * game's sprintf runs on.  The host's printf rounds differently (libultra's
 * "%.0f" of a value under 1 gives "0", for one), so the game's text comes
 * from this, not from libc.  It is checked against the ROM's own code by
 * tools/printfcheck.py.
 *
 * The algorithms are libultra's, step for step; only what the N64 did by
 * byte order (the double's high halfword as ps[0]) is done here on the
 * double's bit pattern. */
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tgr_libc.h"

#define FLAGS_SPACE 1
#define FLAGS_PLUS  2
#define FLAGS_MINUS 4
#define FLAGS_HASH  8
#define FLAGS_ZERO  16

typedef struct {
    union {
        int64_t s64;
        double f64;
    } value;
    char *buff;
    int part1_len;
    int num_leading_zeros;
    int part2_len;
    int num_mid_zeros;
    int part3_len;
    int num_trailing_zeros;
    int precision;
    int width;
    unsigned int size;
    unsigned int flags;
    char length;
} PrintfArgs;

static const char length_str[] = "hlL";
static const char flags_str[] = " +-#0";
static const unsigned int flags_arr[] = { FLAGS_SPACE, FLAGS_PLUS, FLAGS_MINUS, FLAGS_HASH, FLAGS_ZERO, 0 };
static const char spaces[] = "                                ";
static const char zeroes[] = "00000000000000000000000000000000";

/* ---- _Litob: an integer --------------------------------------------------- */
#define LITOB_LEN 0x18
static const char digs_lo[] = "0123456789abcdef";
static const char digs_hi[] = "0123456789ABCDEF";

static void litob(PrintfArgs *args, char type)
{
    char buff[LITOB_LEN];
    const char *num_map = type == 'X' ? digs_hi : digs_lo;
    int base = type == 'o' ? 8 : (type != 'x' && type != 'X') ? 10 : 16;
    int i = LITOB_LEN;
    uint64_t num = (uint64_t)args->value.s64;
    if ((type == 'd' || type == 'i') && args->value.s64 < 0)
        num = -num;
    if (num != 0 || args->precision != 0)
        buff[--i] = num_map[num % base];
    args->value.s64 = (int64_t)(num / base);
    while (args->value.s64 > 0 && i > 0) {
        lldiv_t qr = lldiv(args->value.s64, base);
        args->value.s64 = qr.quot;
        buff[--i] = num_map[qr.rem];
    }
    args->part2_len = LITOB_LEN - i;
    memcpy(args->buff, buff + i, args->part2_len);
    if (args->part2_len < args->precision)
        args->num_leading_zeros = args->precision - args->part2_len;
    if (args->precision < 0 && (args->flags & (FLAGS_ZERO | FLAGS_MINUS)) == FLAGS_ZERO) {
        i = args->width - args->part1_len - args->num_leading_zeros - args->part2_len;
        if (i > 0)
            args->num_leading_zeros += i;
    }
}

/* ---- _Ldtob: a double ------------------------------------------------------ */
#define LDTOB_LEN 0x20
#define LD_NAN    2
#define LD_INF    1
#define LD_FINITE (-1)

static const double pows[] = { 10e0, 10e1, 10e3, 10e7, 10e15, 10e31, 10e63, 10e127, 10e255 };

/* the exponent of the double in px (scaled there to [0.5, 1)) */
static int ldunscale(short *pex, PrintfArgs *px)
{
    uint64_t bits;
    unsigned int hi;
    short xchar;
    memcpy(&bits, &px->value.f64, 8);
    hi = (unsigned int)(bits >> 48);
    xchar = (short)((hi & 0x7FF0) >> 4);
    if (xchar == 0x7FF) {
        *pex = 0;
        return (hi & 0xF) || (bits & 0xFFFFFFFFFFFFull) ? LD_NAN : LD_INF;
    } else if (0 < xchar) {
        hi = (hi & ~0x7FF0u) | (0x3FFu << 4);
        bits = (bits & 0x0000FFFFFFFFFFFFull) | ((uint64_t)hi << 48);
        memcpy(&px->value.f64, &bits, 8);
        *pex = (short)(xchar - (0x3FF - 1));
        return LD_FINITE;
    }
    if (0 > xchar)
        return LD_NAN;
    *pex = 0;
    return 0;
}

static void genld(PrintfArgs *px, char code, const char *p, short nsig, short xexp)
{
    const char point = '.';
    if (nsig <= 0) {
        nsig = 1;
        p = "0";
    }
    if (code == 'f' || ((code == 'g' || code == 'G') && -4 <= xexp && xexp < px->precision)) {
        if (code != 'f') {
            if ((px->flags & FLAGS_HASH) == 0 && nsig < px->precision)
                px->precision = nsig;
            if ((px->precision -= xexp + 1) < 0)
                px->precision = 0;
        }
        if (xexp < 0) {
            px->buff[px->part2_len++] = '0';
            if (0 < px->precision || (px->flags & FLAGS_HASH))
                px->buff[px->part2_len++] = point;
            if (px->precision < -xexp - 1)
                xexp = (short)(-px->precision - 1);
            px->num_mid_zeros = -xexp - 1;
            px->precision += xexp + 1;
            if (px->precision < nsig)
                nsig = (short)px->precision;
            memcpy(&px->buff[px->part2_len], p, px->part3_len = nsig);
            px->num_trailing_zeros = px->precision - nsig;
        } else if (nsig < xexp + 1) {
            memcpy(&px->buff[px->part2_len], p, nsig);
            px->part2_len += nsig;
            px->num_mid_zeros = xexp - nsig + 1;
            if (0 < px->precision || (px->flags & FLAGS_HASH)) {
                px->buff[px->part2_len] = point;
                ++px->part3_len;
            }
            px->num_trailing_zeros = px->precision;
        } else {
            memcpy(&px->buff[px->part2_len], p, xexp + 1);
            px->part2_len += xexp + 1;
            nsig -= xexp + 1;
            if (0 < px->precision || (px->flags & FLAGS_HASH))
                px->buff[px->part2_len++] = point;
            if (px->precision < nsig)
                nsig = (short)px->precision;
            memcpy(&px->buff[px->part2_len], p + xexp + 1, nsig);
            px->part2_len += nsig;
            px->num_mid_zeros = px->precision - nsig;
        }
    } else {
        char *q;
        if (code == 'g' || code == 'G') {
            if (nsig < px->precision)
                px->precision = nsig;
            if (--px->precision < 0)
                px->precision = 0;
            code = code == 'g' ? 'e' : 'E';
        }
        px->buff[px->part2_len++] = *p++;
        if (0 < px->precision || (px->flags & FLAGS_HASH))
            px->buff[px->part2_len++] = point;
        if (0 < px->precision) {
            if (px->precision < --nsig)
                nsig = (short)px->precision;
            memcpy(&px->buff[px->part2_len], p, nsig);
            px->part2_len += nsig;
            px->num_mid_zeros = px->precision - nsig;
        }
        q = &px->buff[px->part2_len];
        *q++ = code;
        if (0 <= xexp) {
            *q++ = '+';
        } else {
            *q++ = '-';
            xexp = (short)-xexp;
        }
        if (100 <= xexp) {
            if (1000 <= xexp) {
                *q++ = (char)(xexp / 1000 + '0');
                xexp %= 1000;
            }
            *q++ = (char)(xexp / 100 + '0');
            xexp %= 100;
        }
        *q++ = (char)(xexp / 10 + '0');
        xexp %= 10;
        *q++ = (char)(xexp + '0');
        px->part3_len = (int)(q - &px->buff[px->part2_len]);
    }
    if ((px->flags & (FLAGS_ZERO | FLAGS_MINUS)) == FLAGS_ZERO) {
        int i = px->width - px->part1_len - px->part2_len - px->num_mid_zeros - px->part3_len -
                px->num_trailing_zeros;
        if (0 < i)
            px->num_leading_zeros = i;
    }
}

static void ldtob(PrintfArgs *args, char type)
{
    char buff[LDTOB_LEN];
    char *ptr = buff;
    double val = args->value.f64;
    short err, nsig, exp;
    int i, n, gen, j, lo, n2;
    double factor;
    char drop;

    if (args->precision < 0)
        args->precision = 6;
    else if (args->precision == 0 && (type == 'g' || type == 'G'))
        args->precision = 1;
    err = (short)ldunscale(&exp, args);
    if (err > 0) {
        memcpy(args->buff, err == LD_NAN ? "NaN" : "Inf", args->part2_len = 3);
        return;
    }
    if (err == 0) {
        nsig = 0;
        exp = 0;
    } else {
        if (val < 0)
            val = -val;
        exp = (short)(exp * 30103 / 100000 - 4);
        if (exp < 0) {
            n = (3 - exp) & ~3;
            exp = (short)-n;
            for (i = 0; n > 0; n >>= 1, i++)
                if (n & 1)
                    val *= pows[i];
        } else if (exp > 0) {
            factor = 1;
            exp &= ~3;
            for (n = exp, i = 0; n > 0; n >>= 1, i++)
                if (n & 1)
                    factor *= pows[i];
            val /= factor;
        }
        gen = (type == 'f' ? exp + 10 : 6) + args->precision;
        if (gen > 0x13)
            gen = 0x13;
        *ptr++ = '0';
        while (gen > 0 && 0 < val) {
            lo = (int)val;
            if ((gen -= 8) > 0)
                val = (val - lo) * 1.0e8;
            ptr += 8;
            for (j = 8; lo > 0 && --j >= 0;) {
                ldiv_t qr = ldiv(lo, 10);
                *--ptr = (char)(qr.rem + '0');
                lo = (int)qr.quot;
            }
            while (--j >= 0)
                *--ptr = '0';
            ptr += 8;
        }
        gen = (int)(ptr - &buff[1]);
        for (ptr = &buff[1], exp += 7; *ptr == '0'; ptr++)
            --gen, --exp;
        nsig = (short)((type == 'f' ? exp + 1 : (type == 'e' || type == 'E') ? 1 : 0) + args->precision);
        if (gen < nsig)
            nsig = (short)gen;
        if (nsig > 0) {
            drop = nsig < gen && ptr[nsig] > '4' ? '9' : '0';
            for (n2 = nsig; ptr[--n2] == drop;)
                nsig--;
            if (drop == '9')
                ptr[n2]++;
            if (n2 < 0) {
                --ptr;
                ++nsig;
                ++exp;
            }
        }
    }
    genld(args, type, ptr, nsig, exp);
}

/* ---- _Putfld: one conversion ----------------------------------------------- */
static void putfld(PrintfArgs *a, va_list *ap, char type, char *buff)
{
    a->part1_len = a->num_leading_zeros = a->part2_len = a->num_mid_zeros = a->part3_len =
        a->num_trailing_zeros = 0;
    switch (type) {
    case 'c':
        buff[a->part1_len++] = (char)va_arg(*ap, unsigned int);
        break;
    case 'd':
    case 'i':
        if (a->length == 'L')
            a->value.s64 = va_arg(*ap, long long);
        else
            a->value.s64 = va_arg(*ap, int);
        if (a->length == 'h')
            a->value.s64 = (short)a->value.s64;
        if (a->value.s64 < 0)
            buff[a->part1_len++] = '-';
        else if (a->flags & FLAGS_PLUS)
            buff[a->part1_len++] = '+';
        else if (a->flags & FLAGS_SPACE)
            buff[a->part1_len++] = ' ';
        a->buff = &buff[a->part1_len];
        litob(a, type);
        break;
    case 'x':
    case 'X':
    case 'u':
    case 'o':
        if (a->length == 'L')
            a->value.s64 = va_arg(*ap, long long);
        else
            a->value.s64 = va_arg(*ap, int);
        if (a->length == 'h')
            a->value.s64 = (unsigned short)a->value.s64;
        else if (a->length == 0)
            a->value.s64 = (unsigned int)a->value.s64;
        if (a->flags & FLAGS_HASH) {
            buff[a->part1_len++] = '0';
            if (type == 'x' || type == 'X')
                buff[a->part1_len++] = type;
        }
        a->buff = &buff[a->part1_len];
        litob(a, type);
        break;
    case 'e':
    case 'f':
    case 'g':
    case 'E':
    case 'G': {
        uint64_t bits;
        a->value.f64 = va_arg(*ap, double);
        memcpy(&bits, &a->value.f64, 8);
        if (bits >> 63)
            buff[a->part1_len++] = '-';
        else if (a->flags & FLAGS_PLUS)
            buff[a->part1_len++] = '+';
        else if (a->flags & FLAGS_SPACE)
            buff[a->part1_len++] = ' ';
        a->buff = &buff[a->part1_len];
        ldtob(a, type);
        break;
    }
    case 'n':
        if (a->length == 'h')
            *va_arg(*ap, unsigned short *) = (unsigned short)a->size;
        else if (a->length == 'L')
            *va_arg(*ap, unsigned long long *) = a->size;
        else
            *va_arg(*ap, unsigned int *) = a->size;
        break;
    case 'p':
        a->value.s64 = (int64_t)(intptr_t)va_arg(*ap, void *);
        a->buff = &buff[a->part1_len];
        litob(a, 'x');
        break;
    case 's':
        a->buff = va_arg(*ap, char *);
        a->part2_len = (int)strlen(a->buff);
        if (a->precision >= 0 && a->part2_len > a->precision)
            a->part2_len = a->precision;
        break;
    case '%':
        buff[a->part1_len++] = '%';
        break;
    default:
        buff[a->part1_len++] = type;
        break;
    }
}

/* ---- _Printf ------------------------------------------------------------------ */
typedef char *(*Prout)(char *, const char *, size_t);

#define ATOI(i, a)                                                                                     \
    for (i = 0; *a >= '0' && *a <= '9'; a++)                                                           \
        if (i < 999)                                                                                   \
            i = *a + i * 10 - '0';
#define PROUT(s, n)                                                                                    \
    if ((n) > 0) {                                                                                     \
        dst = prout(dst, (s), (n));                                                                    \
        if (dst != NULL)                                                                               \
            x.size += (n);                                                                             \
        else                                                                                           \
            return (int)x.size;                                                                        \
    }
#define PAD(m, src, cond)                                                                              \
    if ((cond) && (m) > 0) {                                                                           \
        int i_, j_;                                                                                    \
        for (j_ = (m); j_ > 0; j_ -= i_) {                                                             \
            i_ = (unsigned int)j_ > 32 ? 32 : j_;                                                      \
            PROUT(src, i_);                                                                            \
        }                                                                                              \
    }

static int xprintf(Prout prout, char *dst, const char *fmt, va_list ap)
{
    PrintfArgs x;
    const char *s;
    const char *fi;
    char c;
    char ac[32];
    va_list args;

    va_copy(args, ap);
    x.size = 0;
    for (;;) {
        s = fmt;
        while ((c = *s) != 0 && c != '%')
            s++;
        PROUT(fmt, (int)(s - fmt));
        if (c == 0) {
            va_end(args);
            return (int)x.size;
        }
        fmt = ++s;
        x.flags = 0;
        for (; (fi = strchr(flags_str, *fmt)) != NULL; fmt++)
            x.flags |= flags_arr[fi - flags_str];
        if (*fmt == '*') {
            x.width = va_arg(args, int);
            if (x.width < 0) {
                x.width = -x.width;
                x.flags |= FLAGS_MINUS;
            }
            fmt++;
        } else {
            ATOI(x.width, fmt);
        }
        if (*fmt != '.') {
            x.precision = -1;
        } else {
            fmt++;
            if (*fmt == '*') {
                x.precision = va_arg(args, int);
                fmt++;
            } else {
                ATOI(x.precision, fmt);
            }
        }
        if (strchr(length_str, *fmt) != NULL)
            x.length = *fmt++;
        else
            x.length = 0;
        if (x.length == 'l' && *fmt == 'l') {
            x.length = 'L';
            fmt++;
        }
        putfld(&x, &args, *fmt, ac);
        x.width -= x.part1_len + x.num_leading_zeros + x.part2_len + x.num_mid_zeros + x.part3_len +
                   x.num_trailing_zeros;
        PAD(x.width, spaces, !(x.flags & FLAGS_MINUS));
        PROUT(ac, x.part1_len);
        PAD(x.num_leading_zeros, zeroes, 1);
        PROUT(x.buff, x.part2_len);
        PAD(x.num_mid_zeros, zeroes, 1);
        PROUT(&x.buff[x.part2_len], x.part3_len);
        PAD(x.num_trailing_zeros, zeroes, 1);
        PAD(x.width, spaces, x.flags & FLAGS_MINUS);
        fmt++;
    }
}

static char *prout_sprintf(char *dst, const char *s, size_t n)
{
    return (char *)memcpy(dst, s, n) + n;
}

int tgr_vsprintf(char *dst, const char *fmt, va_list ap)
{
    int ans = xprintf(prout_sprintf, dst, fmt, ap);
    if (ans >= 0)
        dst[ans] = 0;
    return ans;
}

int tgr_sprintf(char *dst, const char *fmt, ...)
{
    va_list ap;
    int ans;
    va_start(ap, fmt);
    ans = tgr_vsprintf(dst, fmt, ap);
    va_end(ap);
    return ans;
}
