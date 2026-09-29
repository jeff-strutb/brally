/* n64-cflags: -O1 -mips3 -32 */
/* ll.c -- libultra's 64-bit integer helpers (libc/ll.c): the calls IDO
 * makes for long long shifts, division, remainder and multiplication.
 */

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: a >> b, unsigned 64-bit. */
/* @implements 0x80266020 tgr __ull_rshift */
unsigned long long __ull_rshift(unsigned long long a0, unsigned long long a1)
{
	return a0 >> a1;
}

/* WHAT IT DOES: a % b, unsigned 64-bit. */
/* @implements 0x8026604C tgr __ull_rem */
unsigned long long __ull_rem(unsigned long long a0, unsigned long long a1)
{
	return a0 % a1;
}

/* WHAT IT DOES: a / b, unsigned 64-bit. */
/* @implements 0x80266088 tgr __ull_div */
unsigned long long __ull_div(unsigned long long a0, unsigned long long a1)
{
	return a0 / a1;
}

/* WHAT IT DOES: a << b, 64-bit. */
/* @implements 0x802660C4 tgr __ll_lshift */
unsigned long long __ll_lshift(unsigned long long a0, unsigned long long a1)
{
	return a0 << a1;
}

/* WHAT IT DOES: a % b, 64-bit (the dividend unsigned, as libultra has
 * it). */
/* @implements 0x802660F0 tgr __ll_rem */
long long __ll_rem(unsigned long long a0, long long a1)
{
	return a0 % a1;
}

/* WHAT IT DOES: a / b, signed 64-bit. */
/* @implements 0x8026612C tgr __ll_div */
long long __ll_div(long long a0, long long a1)
{
	return a0 / a1;
}

/* WHAT IT DOES: a * b, 64-bit. */
/* @implements 0x80266188 tgr __ll_mul */
unsigned long long __ll_mul(unsigned long long a0, unsigned long long a1)
{
	return a0 * a1;
}

/* WHAT IT DOES: 64-bit quotient and remainder by a 16-bit divisor. */
/* @implements 0x802661B8 tgr __ull_divremi */
void __ull_divremi(unsigned long long *div, unsigned long long *rem, unsigned long long a2, unsigned short a3)
{
	*div = a2 / a3;
	*rem = a2 % a3;
}

/* WHAT IT DOES: a mod b, the result taking the divisor's sign. */
/* @implements 0x80266218 tgr __ll_mod */
long long __ll_mod(long long a0, long long a1)
{
	long long tmp = a0 % a1;

	if ((tmp < 0 && a1 > 0) || (tmp > 0 && a1 < 0)) {
		tmp += a1;
	}
	return tmp;
}

/* WHAT IT DOES: a >> b, signed 64-bit. */
/* @implements 0x802662B4 tgr __ll_rshift */
long long __ll_rshift(long long a0, long long a1)
{
	return a0 >> a1;
}
