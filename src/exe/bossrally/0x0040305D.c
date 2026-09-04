#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return a fixed value the statically linked CRT expects from
 * this slot. Library code, not intro code. */
/* @implements 0x0040305D bossrally.exe CRT_ret_0x411 */
int CRT_ret_0x411(void) { return 0x411; }
#endif
