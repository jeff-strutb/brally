#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return a fixed value the statically linked CRT expects from
 * this slot. Library code, not intro code. */
/* @implements 0x0040607D bossrally.exe CRT_ret_10 */
int CRT_ret_10(void) { return 10; }
#endif
