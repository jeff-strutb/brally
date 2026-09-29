/* n64-cflags: -O1 -mips3 -32 */
/* llcvt.c -- libultra's 64-bit integer and floating-point conversions
 * (libc/llcvt.c).
 */

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: A double to a signed 64-bit integer. */
/* @implements 0x802669D0 tgr __d_to_ll */
long long __d_to_ll(double d)
{
	return d;
}

/* WHAT IT DOES: A float to a signed 64-bit integer. */
/* @implements 0x802669EC tgr __f_to_ll */
long long __f_to_ll(float f)
{
	return f;
}

/* WHAT IT DOES: A double to an unsigned 64-bit integer. */
/* @implements 0x80266A08 tgr __d_to_ull */
unsigned long long __d_to_ull(double d)
{
	return d;
}

/* WHAT IT DOES: A float to an unsigned 64-bit integer. */
/* @implements 0x80266AA8 tgr __f_to_ull */
unsigned long long __f_to_ull(float f)
{
	return f;
}

/* WHAT IT DOES: A signed 64-bit integer to a double. */
/* @implements 0x80266B44 tgr __ll_to_d */
double __ll_to_d(long long s)
{
	return s;
}

/* WHAT IT DOES: A signed 64-bit integer to a float. */
/* @implements 0x80266B5C tgr __ll_to_f */
float __ll_to_f(long long s)
{
	return s;
}

/* WHAT IT DOES: An unsigned 64-bit integer to a double. */
/* @implements 0x80266B74 tgr __ull_to_d */
double __ull_to_d(unsigned long long u)
{
	return u;
}

/* WHAT IT DOES: An unsigned 64-bit integer to a float. */
/* @implements 0x80266BA8 tgr __ull_to_f */
float __ull_to_f(unsigned long long u)
{
	return u;
}
