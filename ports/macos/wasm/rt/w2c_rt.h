/* w2c_rt.h -- runtime contract of the C that ports/macos/wasm/w2c.py writes.
 *
 * The game's 32-bit address space is one reservation at a FIXED host
 * address (W_BASE), so a game address a becomes the host pointer W_BASE + a
 * with no base register and no bounds check: every address a u32 can hold,
 * plus the largest offset an access adds, lies inside the reservation.
 * Unmapped parts are PROT_NONE, so a wild access faults as it would have on
 * Windows. Everything here is port code; nothing in it is decomp.
 */
#ifndef W2C_RT_H
#define W2C_RT_H

#include <stdint.h>
#include <string.h>
#include <math.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;

#define W_BASE 0x40000000000ULL         /* 4 TB: macOS refuses fixed maps below 0x7000000000 */
#define W_P(a) ((void *)(uintptr_t)(W_BASE + (u64)(u32)(a)))

/* unaligned-tolerant typed access */
typedef u8  __attribute__((aligned(1), may_alias)) w_u8;
typedef s8  __attribute__((aligned(1), may_alias)) w_s8;
typedef u16 __attribute__((aligned(1), may_alias)) w_u16;
typedef s16 __attribute__((aligned(1), may_alias)) w_s16;
typedef u32 __attribute__((aligned(1), may_alias)) w_u32;
typedef s32 __attribute__((aligned(1), may_alias)) w_s32;
typedef u64 __attribute__((aligned(1), may_alias)) w_u64;
typedef f32 __attribute__((aligned(1), may_alias)) w_f32;
typedef f64 __attribute__((aligned(1), may_alias)) w_f64;

#define W_LD(t, a, off) (*(volatile w_##t *)(uintptr_t)(W_BASE + (u64)(u32)(a) + (off)))
#define W_ST(t, a, off, v) (*(volatile w_##t *)(uintptr_t)(W_BASE + (u64)(u32)(a) + (off)) = (t)(v))

/* The game ran threads (sound, network); each has its own shadow stack. */
extern _Thread_local u32 w_sp;

/* BR_TRACE=1: every translated function logs its entry (debugging) */
extern int w_tracing;
void w_trace(const char *fn);
#define W_TRACE(n) do { if (__builtin_expect(w_tracing, 0)) w_trace(n); } while (0)

/* traps */
_Noreturn void w_trap(const char *why);
void w_missing(const char *import);                 /* a host import not written */
void w_missing_game(u32 va, const char *name);      /* game code the tree lacks */

/* integer ops wasm traps on */
static inline u32 w_divs32(u32 a, u32 b) {
    if (!b) w_trap("integer divide by zero");
    if ((s32)a == INT32_MIN && (s32)b == -1) w_trap("integer overflow");
    return (u32)((s32)a / (s32)b);
}
static inline u32 w_divu32(u32 a, u32 b) { if (!b) w_trap("integer divide by zero"); return a / b; }
static inline u32 w_rems32(u32 a, u32 b) {
    if (!b) w_trap("integer divide by zero");
    if ((s32)b == -1) return 0;
    return (u32)((s32)a % (s32)b);
}
static inline u32 w_remu32(u32 a, u32 b) { if (!b) w_trap("integer divide by zero"); return a % b; }
static inline u64 w_divs64(u64 a, u64 b) {
    if (!b) w_trap("integer divide by zero");
    if ((s64)a == INT64_MIN && (s64)b == -1) w_trap("integer overflow");
    return (u64)((s64)a / (s64)b);
}
static inline u64 w_divu64(u64 a, u64 b) { if (!b) w_trap("integer divide by zero"); return a / b; }
static inline u64 w_rems64(u64 a, u64 b) {
    if (!b) w_trap("integer divide by zero");
    if ((s64)b == -1) return 0;
    return (u64)((s64)a % (s64)b);
}
static inline u64 w_remu64(u64 a, u64 b) { if (!b) w_trap("integer divide by zero"); return a % b; }
static inline u32 w_rotl32(u32 a, u32 b) { b &= 31; return b ? (a << b) | (a >> (32 - b)) : a; }
static inline u32 w_rotr32(u32 a, u32 b) { b &= 31; return b ? (a >> b) | (a << (32 - b)) : a; }
static inline u64 w_rotl64(u64 a, u64 b) { b &= 63; return b ? (a << b) | (a >> (64 - b)) : a; }
static inline u64 w_rotr64(u64 a, u64 b) { b &= 63; return b ? (a >> b) | (a << (64 - b)) : a; }
static inline u32 w_clz32(u32 a) { return a ? (u32)__builtin_clz(a) : 32; }
static inline u32 w_ctz32(u32 a) { return a ? (u32)__builtin_ctz(a) : 32; }
static inline u64 w_clz64(u64 a) { return a ? (u64)__builtin_clzll(a) : 64; }
static inline u64 w_ctz64(u64 a) { return a ? (u64)__builtin_ctzll(a) : 64; }

/* bit reinterpretation */
static inline u32 w_bits_f(f32 f) { u32 u; memcpy(&u, &f, 4); return u; }
static inline u64 w_bits_d(f64 d) { u64 u; memcpy(&u, &d, 8); return u; }
static inline f32 w_f_bits(u32 u) { f32 f; memcpy(&f, &u, 4); return f; }
static inline f64 w_d_bits(u64 u) { f64 d; memcpy(&d, &u, 8); return d; }

/* wasm min/max: NaN-propagating, -0 < +0 */
static inline f32 w_fminf(f32 a, f32 b) { return (a != a || b != b) ? NAN : (a == b ? (signbit(a) ? a : b) : (a < b ? a : b)); }
static inline f32 w_fmaxf(f32 a, f32 b) { return (a != a || b != b) ? NAN : (a == b ? (signbit(a) ? b : a) : (a > b ? a : b)); }
static inline f64 w_fmin(f64 a, f64 b) { return (a != a || b != b) ? NAN : (a == b ? (signbit(a) ? a : b) : (a < b ? a : b)); }
static inline f64 w_fmax(f64 a, f64 b) { return (a != a || b != b) ? NAN : (a == b ? (signbit(a) ? b : a) : (a > b ? a : b)); }

/* float -> int. Trapping forms trap as wasm does; the saturating forms clamp.
 * (MSVC's own conversions go through _ftol, which the x86 FPU answers with
 * 0x80000000 on overflow; clang lowers (int)f to the saturating form here.) */
#define W_TRUNC(name, T, F, lo, hi) \
    static inline T name(F x) { if (!(x > (F)(lo) && x < (F)(hi))) w_trap("float->int overflow"); return (T)x; }
W_TRUNC(w_trunc_s32_f_, s32, f32, -2147483904.0f, 2147483648.0f)
W_TRUNC(w_trunc_u32_f_, u32, f32, -1.0f, 4294967296.0f)
W_TRUNC(w_trunc_s32_d_, s32, f64, -2147483649.0, 2147483648.0)
W_TRUNC(w_trunc_u32_d_, u32, f64, -1.0, 4294967296.0)
W_TRUNC(w_trunc_s64_f_, s64, f32, -9223373136366403584.0f, 9223372036854775808.0f)
W_TRUNC(w_trunc_u64_f_, u64, f32, -1.0f, 18446744073709551616.0f)
W_TRUNC(w_trunc_s64_d_, s64, f64, -9223372036854777856.0, 9223372036854775808.0)
W_TRUNC(w_trunc_u64_d_, u64, f64, -1.0, 18446744073709551616.0)
#define w_trunc_s32_f(x) ((u32)w_trunc_s32_f_(x))
#define w_trunc_u32_f(x) ((u32)w_trunc_u32_f_(x))
#define w_trunc_s32_d(x) ((u32)w_trunc_s32_d_(x))
#define w_trunc_u32_d(x) ((u32)w_trunc_u32_d_(x))
#define w_trunc_s64_f(x) ((u64)w_trunc_s64_f_(x))
#define w_trunc_u64_f(x) ((u64)w_trunc_u64_f_(x))
#define w_trunc_s64_d(x) ((u64)w_trunc_s64_d_(x))
#define w_trunc_u64_d(x) ((u64)w_trunc_u64_d_(x))
#define W_SAT(name, T, F, lo, hi, tmin, tmax) \
    static inline T name(F x) { if (x != x) return 0; if (x <= (F)(lo)) return (T)(tmin); if (x >= (F)(hi)) return (T)(tmax); return (T)x; }
W_SAT(w_sat_s32_f_, s32, f32, -2147483648.0f, 2147483648.0f, INT32_MIN, INT32_MAX)
W_SAT(w_sat_u32_f_, u32, f32, 0.0f, 4294967296.0f, 0, UINT32_MAX)
W_SAT(w_sat_s32_d_, s32, f64, -2147483648.0, 2147483648.0, INT32_MIN, INT32_MAX)
W_SAT(w_sat_u32_d_, u32, f64, 0.0, 4294967296.0, 0, UINT32_MAX)
W_SAT(w_sat_s64_f_, s64, f32, -9223372036854775808.0f, 9223372036854775808.0f, INT64_MIN, INT64_MAX)
W_SAT(w_sat_u64_f_, u64, f32, 0.0f, 18446744073709551616.0f, 0, UINT64_MAX)
W_SAT(w_sat_s64_d_, s64, f64, -9223372036854775808.0, 9223372036854775808.0, INT64_MIN, INT64_MAX)
W_SAT(w_sat_u64_d_, u64, f64, 0.0, 18446744073709551616.0, 0, UINT64_MAX)
#define w_sat_s32_f(x) ((u32)w_sat_s32_f_(x))
#define w_sat_u32_f(x) ((u32)w_sat_u32_f_(x))
#define w_sat_s32_d(x) ((u32)w_sat_s32_d_(x))
#define w_sat_u32_d(x) ((u32)w_sat_u32_d_(x))
#define w_sat_s64_f(x) ((u64)w_sat_s64_f_(x))
#define w_sat_u64_f(x) ((u64)w_sat_u64_f_(x))
#define w_sat_s64_d(x) ((u64)w_sat_s64_d_(x))
#define w_sat_u64_d(x) ((u64)w_sat_u64_d_(x))

u32 w_memory_size(void);
u32 w_memory_grow(u32 pages);

/* ---- the function table: every function with a game address ---- */
/* cc: the function's x86 convention, bits 20-21 (1 thiscall, 2 fastcall)
 * plus the fastcall register-argument mask in the low bits */
typedef struct { u32 va; const char *sig; void *fn; const char *name; u32 cc; } w_fentry;
typedef struct { const char *name; const char *sig; void *fn; } w_hentry;
typedef struct { u32 addr; u32 off; u32 len; } w_segentry;
typedef struct { u32 addr; void *fn; } w_hostfix;
typedef struct { u32 addr; const char *name; } w_orphan;
extern const w_fentry w_functions[];
extern const w_hentry w_host_functions[];
extern const w_segentry w_port_segments[];
extern const w_hostfix w_port_hostfix[];
extern const w_orphan w_orphans[];
typedef struct { u32 slot; const char *name; } w_iatent;
extern const w_iatent w_iat[];
extern const u32 w_port_end;
void w_run_inits(void);

/* indirect calls dispatch on the game address */
const w_fentry *w_lookup(u32 addr);
u32 w_addr_of_host(void *fn, const char *sig, const char *name);
#define w_host_addr(cn) w_addr_of_host((void *)&h_##cn, 0, #cn)

/* signature bridging: args flattened to 32-bit stack words, as x86 cdecl */
static inline int w_flat_u32(u32 *w, int n, u32 v) { w[n] = v; return n + 1; }
static inline int w_flat_f32(u32 *w, int n, f32 v) { w[n] = w_bits_f(v); return n + 1; }
static inline int w_flat_u64(u32 *w, int n, u64 v) { w[n] = (u32)v; w[n + 1] = (u32)(v >> 32); return n + 2; }
static inline int w_flat_f64(u32 *w, int n, f64 v) { return w_flat_u64(w, n, w_bits_d(v)); }
static inline u32 w_unflat_u32(const u32 *w, int k) { return w[k]; }
static inline f32 w_unflat_f32(const u32 *w, int k) { return w_f_bits(w[k]); }
static inline u64 w_unflat_u64(const u32 *w, int k) { return w[k] | ((u64)w[k + 1] << 32); }
static inline f64 w_unflat_f64(const u32 *w, int k) { return w_d_bits(w_unflat_u64(w, k)); }
#define w_conv_u32_u32(x) (x)
#define w_conv_u64_u64(x) (x)
#define w_conv_f32_f32(x) (x)
#define w_conv_f64_f64(x) (x)
#define w_conv_u32_f32(x) w_f_bits(x)
#define w_conv_f32_u32(x) w_bits_f(x)
#define w_conv_u32_u64(x) ((u64)(x))
#define w_conv_u64_u32(x) ((u32)(x))
#define w_conv_f64_u32(x) ((u32)w_bits_d(x))
#define w_conv_u32_f64(x) ((f64)(x))
#define w_conv_f64_f32(x) ((f32)(x))
#define w_conv_f32_f64(x) ((f64)(x))
#define w_conv_u64_f64(x) w_d_bits(x)
#define w_conv_f64_u64(x) w_bits_d(x)
#define w_conv_u64_f32(x) ((f32)(x))
#define w_conv_f32_u64(x) ((u64)w_bits_f(x))
u64 w_generic_call(const w_fentry *e, const u32 *w, int n, const char *sig);
u64 w_x86_call(const w_fentry *e, u32 caller_cc, const char *caller_sig, const u32 *w);
typedef struct { const char *sig; u64 (*call)(void *fn, const u32 *w); } w_gcentry;
extern const w_gcentry w_gcalls[];

/* declarations the generated files need */
#define W_ICALL_DECL(sig) /* declared in w2c_icall.h */
#define W_ADAPT_DECL(a, b) /* declared in w2c_icall.h */
#include "w2c_icall.h"

#endif
