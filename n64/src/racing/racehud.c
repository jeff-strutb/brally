/* racehud.c -- the race's frame hook: extra layers drawn over the 3-D view
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8020037C(void);
void func_8022D7E0(int param_1,unsigned int param_2,unsigned int param_3,int param_4,int param_5);
void func_8023BF60(void);
extern int D_8026FF10;
/* -- end declarations -- */

/* WHAT IT DOES: The race's frame hook: unless the race has switched the
 * layers off, draws the extra model layer and the second overlay pass, each
 * after setting the fill colour it needs. */
/* @implements 0x802003E4 tgr BrRaceDrawLayers */
void BrRaceDrawLayers(void)
{
  if (D_8026FF10 == 0) {
    func_8022D7E0(0,0x80,0x80,0xf0,0xff);
    func_8020037C();
    func_8022D7E0(0,0,0,0xc0,0xff);
    func_8023BF60();
    func_8022D7E0(0,0,0x82,0,0xff);
  }
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept among
 * the race display code. */
/* @implements 0x8023870C tgr BrStub8023870C */
void BrStub8023870C(void)
{
}
