/* Hand-matched from disassembly — 0x10058C70
 * fastcall: pointer arrives in ecx, five consecutive dwords zeroed, ret. */
#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: zero the five fields of a small record -- the compiler-
 * emitted constructor for it. */
/* @implements 0x10058C70 glide FUN_10058c70 */
int *__fastcall FUN_10058c70(int *p)
{
  p[0] = 0;
  p[1] = 0;
  p[2] = 0;
  p[3] = 0;
  p[4] = 0;
  return p;
}

#endif /* BR_MATCHING_BUILD */
