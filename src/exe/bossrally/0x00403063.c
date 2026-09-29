/* WHAT IT DOES: return a fixed value the statically linked CRT expects from
 * this slot. Library code, not intro code. */
/* @implements 0x00403063 bossrally.exe CRT_ret_0x804 */
int CRT_ret_0x804(void) { return 0x804; }
