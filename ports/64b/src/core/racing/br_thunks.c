/* br_thunks.c -- racing.
 *
 * Ghidra-matched forwarders filed out of the address batches. Every function
 * carries its original address.
 */

/* FUN_1006e590: prototype in br_funcs.h */

/* WHAT IT DOES: thunk: forwards to the shared no-op at 0x1006E590. */
/* @implements 0x1005C440 glide BrThunk5C440 */

int BrThunk5C440(void)

{
  BrNop6E590();
  return;
}

