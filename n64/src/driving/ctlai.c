/* ctlai.c -- the computer drivers
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8022762C(int ctl);
/* -- end declarations -- */

/* WHAT IT DOES: Drive one computer-controlled car for this frame: the
 * out-of-line entry to the AI driver's main body. */
/* @implements 0x802288B4 tgr BrCtlAi */
void BrCtlAi(int ctl)
{
    func_8022762C(ctl);
}

/* WHAT IT DOES: Does nothing with its argument. An empty function the
 * retail build kept among the AI driver code. */
/* @implements 0x80228E44 tgr BrStub80228E44 */
void BrStub80228E44(int arg0)
{
}
