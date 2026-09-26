/* entcolour.c -- car artwork records and their paint colours
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021D140(int param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80222D54(int param_1);
void func_8021D5E4(int param_1,int param_2,int param_3);
void func_802203F0(int param_1,int param_2);
/* -- end declarations -- */

/* WHAT IT DOES: Repaint a car's artwork record in the car's own colour (its
 * red, green and blue reduced to the 5-bit texture format), then refresh
 * what depends on it. */
/* @implements 0x80220398 tgr BrEntRefreshColour */
void BrEntRefreshColour(int param_1)
{
  func_8021D140(*(int *)(param_1 + 0x2078),(int)(unsigned int)*(unsigned char *)(param_1 + 0x2060) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2061) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2062) >> 3);
  func_80222D54(param_1);
}

/* WHAT IT DOES: Load a car into its slot and attach that car's artwork
 * record to the entity, repainting it. */
/* @implements 0x80220438 tgr BrEntLoadRecord */
void BrEntLoadRecord(int param_1,int param_2,int param_3)
{
  func_8021D5E4(param_2,param_3,0);
  func_802203F0(param_1,param_2);
}

/* WHAT IT DOES: Tell whether an entity slot is unused (it has no owner
 * yet). */
/* @implements 0x80220534 tgr BrEntIsFree */
int BrEntIsFree(int param_1)
{
  return *(int *)(param_1 + 0x18) == 0;
}
