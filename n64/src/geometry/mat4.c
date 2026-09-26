/* mat4.c -- 4x4 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
char * func_80260B20(char *param_1,char *param_2,int param_3);
/* -- end declarations -- */

/* WHAT IT DOES: Copy a 4x4 float matrix (64 bytes). */
/* @implements 0x80225350 tgr BrMat4Copy */
void BrMat4Copy(int param_1,int param_2)
{
  func_80260B20(param_1,param_2,0x40);
}
