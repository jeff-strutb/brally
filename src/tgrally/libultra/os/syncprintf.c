/* n64-cflags: -O3 */
/* syncprintf.c -- libultra's debug print entry points (os/syncprintf.c)
 * in the retail build: they take their arguments and print nothing.
 */

/* -- declarations -- */
/* -- end declarations -- */

/* WHAT IT DOES: Nothing: a printf-style debug entry point with no callers,
 * empty in the retail library. */
/* @implements 0x802607C0 tgr BrStub802607C0 */
void BrStub802607C0(const char *fmt, ...)
{
}

/* WHAT IT DOES: Nothing: the debug printf, empty in the retail library
 * (the game's messages still pass through it). */
/* @implements 0x802607DC tgr osSyncPrintf */
void osSyncPrintf(const char *fmt, ...)
{
}
