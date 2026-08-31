/* Hand-matched from disassembly — 0x1006CD80
 * fastcall: pointer in ecx, four fields zeroed then a self-pointer stored at
 * offset 0x10 (= p+0x14), returns this. */
#ifdef BR_MATCHING_BUILD

/* @implements 0x1006CD80 glide FUN_1006cd80 */
int *__fastcall FUN_1006cd80(int *p)
{
  p[2] = 0;   /* 0x08 */
  p[3] = 0;   /* 0x0c */
  p[0] = 0;
  p[1] = 0;   /* 0x04 */
  p[4] = (int)(p + 5);   /* [0x10] = p + 0x14 */
  return p;
}

#endif /* BR_MATCHING_BUILD */
