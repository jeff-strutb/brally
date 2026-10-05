/* recfield.c -- small accessors on shared record layouts
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Return the unsigned 16-bit field at offset 2 of a record.
 * Nothing in the ROM calls it directly. */
/* @implements 0x8021EA80 tgr BrRecHalf1 */
unsigned short BrRecHalf1(int param_1)
{
  return *(unsigned short *)(param_1 + 2);
}

/* WHAT IT DOES: Return the unsigned 16-bit field at offset 2 of a record:
 * an identical copy of the accessor before it, kept as a separate entry. */
/* @implements 0x8021EA88 tgr BrRecHalf1Dup */
unsigned short BrRecHalf1Dup(int param_1)
{
  return *(unsigned short *)(param_1 + 2);
}
