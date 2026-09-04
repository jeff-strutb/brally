/* br_netmsg.c -- net.
 *
 * The one-slot announcement mailbox: a message string the networking layer
 * picks up on its next pass, written under the message mutex.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#ifdef BR_MATCHING_BUILD
#include <windows.h>

extern char DAT_1021c9b0[];
extern int DAT_10226a38;
extern HANDLE DAT_10226a54;

/* WHAT IT DOES: post a message string for the networking layer to pick up,
 * under the message mutex, and raise the flag that says one is waiting. */
/* @implements 0x100038A0 glide FUN_100038a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_100038a0(char *param_1)

{
  WaitForSingleObject(DAT_10226a54, 0xffffffff);
  strcpy(DAT_1021c9b0, param_1);
  DAT_10226a38 = 1;
  ReleaseMutex(DAT_10226a54);
  return;
}

#endif /* BR_MATCHING_BUILD */
