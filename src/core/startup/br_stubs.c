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

/* ---- from slice2_18.c ---------------------------------------------- */

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB3F glide BrNop_1002CB3F */

void BrNop_1002CB3F(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002CB44 glide BrNop_1002CB44 */

void BrNop_1002CB44(void)

{
  return;
}

/* ---- from slice2_19.c ---------------------------------------------- */

extern int DAT_106e8a1c;
extern int DAT_106e8698;

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E136 glide BrNop_1002E136 */

void BrNop_1002E136(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E2DE glide BrNop_1002E2DE */

void BrNop_1002E2DE(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002E2E3 glide BrNop_1002E2E3 */

void BrNop_1002E2E3(void)

{
  return;
}

/* WHAT IT DOES: store the argument into the global at 0x106E8A1C. */
/* @implements 0x1002E2E8 glide BrSet_106E8A1C */

void BrSet_106E8A1C(int param_1)

{
  DAT_106e8a1c = param_1;
  return;
}

/* WHAT IT DOES: store the argument into the global at 0x106E8698. */
/* @implements 0x1002E2F5 glide BrSet_106E8698 */

void BrSet_106E8698(int param_1)

{
  DAT_106e8698 = param_1;
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002EBCC glide BrNop_1002EBCC */

void BrNop_1002EBCC(void)

{
  return;
}

#endif /* BR_MATCHING_BUILD */
