/* Hand-matched from disassembly — 0x10039F60
 * Copies a dword global, a byte global, and a zero-extended byte global into
 * three destination globals; returns 1. */
#ifdef BR_MATCHING_BUILD

extern int           DAT_10ac5a48;   /* src dword */
extern unsigned char DAT_10ac5a4c;   /* src byte  */
extern unsigned char DAT_10ac5a4d;   /* src byte -> widened */
extern int           DAT_10ac5bf8;   /* dst dword */
extern int           DAT_10ac5bfc;   /* dst dword (from byte) */
extern unsigned char DAT_10ac5c10;   /* dst byte  */

/* @implements 0x10039F60 glide FUN_10039f60 */
int FUN_10039f60(void)
{
  DAT_10ac5bf8 = DAT_10ac5a48;
  DAT_10ac5c10 = DAT_10ac5a4c;
  DAT_10ac5bfc = DAT_10ac5a4d;
  return 1;
}

#endif /* BR_MATCHING_BUILD */
