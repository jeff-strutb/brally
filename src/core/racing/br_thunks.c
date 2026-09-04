/* br_thunks.c -- racing.
 *
 * Ghidra-matched forwarders filed out of the address batches. Every function
 * carries its original address.
 */
#ifdef BR_MATCHING_BUILD

int FUN_1006e590();

/* WHAT IT DOES: thunk — forwards to the shared no-op at 0x1006E590. */
/* @implements 0x1005C440 glide BrThunk5C440 */

int BrThunk5C440(void)

{
  FUN_1006e590();
  return;
}

#endif /* BR_MATCHING_BUILD */
