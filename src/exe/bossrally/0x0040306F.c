#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return a fixed value the statically linked CRT expects from
 * this slot. Library code, not intro code. */
/* @implements 0x0040306F bossrally.exe CRT_ret_0x404 */
int CRT_ret_0x404(void) { return 0x404; }
#endif
