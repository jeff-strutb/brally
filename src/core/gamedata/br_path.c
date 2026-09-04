/* br_path.c -- gamedata.
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
#include <stdint.h>

#ifdef BR_MATCHING_BUILD


/* WHAT IT DOES: copy the last path component of `param_1` (after the final backslash)
 * into `param_2`. Both strlen and strcpy are the /Oi inline forms (repne scasb, rep
 * movsd + movsb). stdcall per the trailing ret 8. */
/* @implements 0x10008D70 glide BrPathBasename */

void __stdcall BrPathBasename(char *param_1,char *param_2)

{
  unsigned int uVar2;
  char *pcVar4;
  
  uVar2 = strlen(param_1);
  for (pcVar4 = param_1 + ((uVar2 + 1) - 2); (pcVar4 != param_1 && (pcVar4[-1] != '\\'));
      pcVar4 = pcVar4 + -1) {
  }
  strcpy(param_2,pcVar4);
  return;
}

#endif /* BR_MATCHING_BUILD */
