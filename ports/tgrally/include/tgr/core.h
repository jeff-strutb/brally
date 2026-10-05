/* core.h -- the game's functions more than one file calls, declared once
 * (the decomp declared them in each file; natively their types matter).
 */
#ifndef TGR_CORE_H_GAME
#define TGR_CORE_H_GAME

/* the main loop's game mode: the function it runs once per frame */
typedef void (*BrMode)(void);
void BrModeSet(BrMode mode);
int  BrModeIs(BrMode mode);
void BrModeRun(void);
void BrFrameSetHook(BrMode hook);
void BrFrameSetHookLate(BrMode hook);
extern BrMode D_8031B318;               /* the game mode */
extern BrMode D_8031B31C;               /* run once while the next frame is built */
extern BrMode D_8031B320;               /* and again at its very end */

/* where the demo recordings sit in the cartridge */
#define BR_ROM_DEMOS 0xAD400

void BrFatal(char *msg);
#endif
