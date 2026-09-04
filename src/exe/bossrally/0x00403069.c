#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return a fixed value the statically linked CRT expects from
 * this slot. Library code, not intro code. */
/* @implements 0x00403069 bossrally.exe CRT_ret_0x412 */
int CRT_ret_0x412(void) { return 0x412; }
#endif
