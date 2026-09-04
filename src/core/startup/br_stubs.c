/* br_stubs.c -- startup: the one-line stubs, nops and global accessors.
 *
 * Filed out of the address batches.  Each section keeps the declarations the
 * batch it came from made locally, so the compiler's view of each body is
 * unchanged.  These are whole original functions, not placeholders: the
 * shipped code really is this small.
 */
#ifdef BR_MATCHING_BUILD

/* ---- from slice2_12.c ---------------------------------------------- */

extern int g_br094294;

/* WHAT IT DOES: return the value of the global at g_br094294. */
/* @implements 0x100060A0 glide BrGetGlobal_94294 */

int BrGetGlobal_94294(void)

{
  return g_br094294;
}

#endif /* BR_MATCHING_BUILD */
