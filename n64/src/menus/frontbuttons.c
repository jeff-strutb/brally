/* frontbuttons.c -- the button prompts along the bottom of front-end screens
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8022F4F8(void);
void func_8022F514(void);
void func_8022F5D0(int param_1);
void func_8022F5DC(int param_1,int param_2,int param_3);
int func_8023DF9C();
extern int D_80271D70;
extern int D_80271D84;
extern int D_8028AAB4;
extern int D_80271FD0;
/* -- end declarations -- */

/* WHAT IT DOES: Draw the A and B button icons at the foot of the screen
 * with the prompts Select and Go Back. */
/* @implements 0x8020AAE0 tgr BrFrontPromptSelect */
void BrFrontPromptSelect(void)
{
  func_8022F4F8();
  func_8022F514();
  func_8023DF9C(&D_80271D70,100,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8023DF9C(&D_80271D84,0xa0,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8022F5D0(0xb);
  func_8022F5DC("%wwSelect",0x73,(D_8028AAB4 * 0x13) / 0x14 + -3);
  func_8022F5DC("%wwGo Back",0xaf,(D_8028AAB4 * 0x13) / 0x14 + -3);
}

/* WHAT IT DOES: Draw the A and B button icons at the foot of the screen
 * with the prompts Continue and Exit. */
/* @implements 0x8020AC18 tgr BrFrontPromptContinue */
void BrFrontPromptContinue(void)
{
  func_8022F4F8();
  func_8022F514();
  func_8023DF9C(&D_80271D70,0x66,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8023DF9C(&D_80271D84,0xb2,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8022F5D0(0xb);
  func_8022F5DC("%wwContinue",0x76,(D_8028AAB4 * 0x13) / 0x14 + -3);
  func_8022F5DC("%wwExit",0xc1,(D_8028AAB4 * 0x13) / 0x14 + -3);
}

/* WHAT IT DOES: Set the front-end flag that the season and track-select
 * screens pass to the menu drawer. */
/* @implements 0x8020AD50 tgr BrFrontSetMenuFlag */
void BrFrontSetMenuFlag(int param_1)
{
  D_80271FD0 = param_1;
}
